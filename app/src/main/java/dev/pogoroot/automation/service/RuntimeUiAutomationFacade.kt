package dev.pogoroot.automation.service

import android.util.Log
import dev.pogoroot.automation.bridge.RuntimeNativeLifecycle
import dev.pogoroot.automation.bridge.RuntimeBootstrapPhase
import dev.pogoroot.automation.config.AutomationConfigRepository
import dev.pogoroot.automation.engine.CatchSpinArmState
import dev.pogoroot.automation.config.HeadlessAutomationConfig
import dev.pogoroot.automation.config.RuntimeDesiredStateMapper
import dev.pogoroot.automation.data.MapTargetRepository
import dev.pogoroot.automation.events.AutomationEventSink
import dev.pogoroot.automation.location.NativeWalkCandidateReceiver
import dev.pogoroot.automation.root.DesiredStateReceipt
import dev.pogoroot.automation.root.RuntimeUiClient
import dev.pogoroot.automation.runtime.RuntimeUiState
import dev.pogoroot.automation.runtime.observation.RuntimeUiEventRouter
import java.util.concurrent.Executors
import java.util.concurrent.Future
import java.util.concurrent.ScheduledFuture
import java.util.concurrent.TimeUnit

data class RuntimeUiAutomationStatus(
    val running: Boolean,
    val enabled: Boolean,
    val catchSpinArmed: Boolean,
    val runtimeSessionId: String?,
    val runtimeControlState: String,
    val runtimeModules: Map<String, String>,
    val runtimeStrongIdentityVerified: Boolean,
    val runtimeCapabilities: Set<String>,
    val runtimeMutationPermissionGranted: Boolean,
    val runtimeLifecycle: String?,
    val runtimeSuspended: Boolean,
    val observationSeq: Long?,
    val lastAction: String?,
    val lastError: String?,
)

/**
 * Service-side UI facade. It submits durable intent and renders native-owned
 * status; it does not schedule gameplay or infer readiness from a receipt.
 */
class RuntimeUiAutomationFacade(
    private val configRepository: AutomationConfigRepository,
    private val catchSpinArmState: CatchSpinArmState,
    eventSink: AutomationEventSink,
    mapTargetRepository: MapTargetRepository,
    walkCandidateReceiver: NativeWalkCandidateReceiver,
    private val onRuntimeInactive: (String) -> Unit = {},
    private val client: RuntimeUiClient = RuntimeUiClient(),
) {
    private val executor = Executors.newSingleThreadScheduledExecutor()
    private val router = RuntimeUiEventRouter(
        bridge = client,
        eventSink = eventSink,
        onMapTarget = { target ->
            if (configRepository.read().mapTapWalkEnabled) {
                mapTargetRepository.publish(target)
            }
        },
        onPointWalkCandidate = walkCandidateReceiver::receive,
        onPointWalkCandidateReset = walkCandidateReceiver::reset,
        onRuntimeSessionChanged = { previous, current ->
            if (previous != null && previous != current) disarmForRuntimeBoundary("runtime_session_changed")
        },
        onRuntimeSessionLost = { reason ->
            disarmForRuntimeBoundary(reason)
            onRuntimeInactive(reason)
        },
    )
    @Volatile private var serviceRunning = false
    @Volatile private var lastSubmittedRevision: Long? = null
    private var nextConnectAttemptNs = 0L
    private var connectRetryMs = 500L
    private var poll: ScheduledFuture<*>? = null

    fun start() {
        if (serviceRunning) return
        serviceRunning = true
        poll = executor.scheduleWithFixedDelay(
            ::pollRuntime,
            0L,
            POLL_INTERVAL_MS,
            TimeUnit.MILLISECONDS,
        )
    }

    fun enable(transform: (HeadlessAutomationConfig) -> HeadlessAutomationConfig = { it }):
        HeadlessAutomationConfig = configRepository.update { current ->
        transform(current).copy(enabled = true)
    }

    fun disable(): HeadlessAutomationConfig = configRepository.update { it.copy(enabled = false) }

    fun armCurrentSession(): Result<DesiredStateReceipt> = onRuntimeThread {
        val sessionId = router.snapshot().runtimeSessionId
            ?: return@onRuntimeThread Result.failure(
                IllegalStateException("No active runtime session is available to arm."),
            )
        catchSpinArmState.arm(sessionId)
        val config = configRepository.update { it.copy(enabled = true) }
        lastSubmittedRevision = null
        client.submitDesiredState(mapDesiredState(config, sessionId)).onFailure {
            catchSpinArmState.disarm("arm_submit_failed")
            val disarmed = configRepository.update { it }
            val desired = mapDesiredState(disarmed, sessionId = null)
            client.rememberDesiredState(desired)
            lastSubmittedRevision = null
            if (client.connected) client.submitDesiredState(desired)
        }
    }

    fun disarm(reason: String): Result<DesiredStateReceipt>? {
        if (!catchSpinArmState.disarm(reason)) return null
        val config = configRepository.update { it }
        lastSubmittedRevision = null
        val desired = mapDesiredState(config, router.snapshot().runtimeSessionId)
        client.rememberDesiredState(desired)
        return if (client.connected) {
            client.submitDesiredState(desired)
        } else {
            null
        }
    }

    fun updateConfig(transform: (HeadlessAutomationConfig) -> HeadlessAutomationConfig):
        HeadlessAutomationConfig = configRepository.update(transform)

    fun submitCurrentDesiredStateAsync() {
        executor.execute { submitCurrentDesiredStateOnRuntimeThread() }
    }

    fun submitCurrentDesiredState(): Result<DesiredStateReceipt> = onRuntimeThread {
        lastSubmittedRevision = null
        submitCurrentDesiredStateOnRuntimeThread()
    }

    fun requestRuntimeDiagnostic(): Result<Unit> = onRuntimeThread {
        client.requestRuntimeDiagnostic()
    }

    fun snapshot(): RuntimeUiAutomationStatus {
        val config = configRepository.read()
        return snapshot(config, router.snapshot())
    }

    fun shutdown() {
        serviceRunning = false
        poll?.cancel(true)
        poll = null
        catchSpinArmState.disarm("service_shutdown")
        val disabled = configRepository.update { it.copy(enabled = false) }
        runCatching {
            executor.submit {
                lastSubmittedRevision = null
                client.submitDesiredState(mapDesiredState(disabled, router.snapshot().runtimeSessionId))
                router.stop()
            }.get(SHUTDOWN_TIMEOUT_MS, TimeUnit.MILLISECONDS)
        }.onFailure { error ->
            Log.w(LOG_TAG, "runtime UI shutdown did not receive a disabled-state receipt", error)
            router.reset("service stopped")
            client.disconnect()
        }
        executor.shutdownNow()
    }

    private fun pollRuntime() {
        if (System.nanoTime() < nextConnectAttemptNs) return
        val config = configRepository.read()
        if (lastSubmittedRevision != config.configRevision) {
            val result = client.submitDesiredState(
                mapDesiredState(config, router.snapshot().runtimeSessionId),
            )
            if (result.isSuccess) {
                lastSubmittedRevision = config.configRevision
            } else {
                Log.w(
                    LOG_TAG,
                    "desired state submit failed revision=${config.configRevision}",
                    result.exceptionOrNull(),
                )
                if (!client.connected) {
                    scheduleConnectRetry()
                    return
                }
            }
        }
        router.tick().onSuccess {
            nextConnectAttemptNs = 0L
            connectRetryMs = 500L
        }.onFailure { error ->
            Log.w(LOG_TAG, "runtime UI event poll failed", error)
            scheduleConnectRetry()
        }
    }

    private fun scheduleConnectRetry() {
        nextConnectAttemptNs = System.nanoTime() + TimeUnit.MILLISECONDS.toNanos(connectRetryMs)
        connectRetryMs = (connectRetryMs * 2).coerceAtMost(8_000L)
    }

    private fun submitCurrentDesiredStateOnRuntimeThread(): Result<DesiredStateReceipt> {
        val config = configRepository.read()
        return client.submitDesiredState(
            mapDesiredState(config, router.snapshot().runtimeSessionId),
        ).also { result ->
            if (result.isSuccess) lastSubmittedRevision = config.configRevision
        }
    }

    private fun <T> onRuntimeThread(block: () -> Result<T>): Result<T> = runCatching {
        val future: Future<Result<T>> = executor.submit<Result<T>> { block() }
        future.get(SHUTDOWN_TIMEOUT_MS, TimeUnit.MILLISECONDS)
    }.getOrElse { Result.failure(it) }

    private fun snapshot(
        config: HeadlessAutomationConfig,
        state: RuntimeUiState,
    ): RuntimeUiAutomationStatus {
        val native = state.nativeStatus
        val lifecycle = native?.nativeLifecycle
        val bootstrap = state.bootstrap
        val ready = native?.ready == true && native.strongIdentityVerified
        return RuntimeUiAutomationStatus(
            running = serviceRunning,
            enabled = config.enabled,
            catchSpinArmed = catchSpinArmState.snapshot(
                native?.runtimeSessionId ?: state.runtimeSessionId,
            ),
            runtimeSessionId = native?.runtimeSessionId ?: state.runtimeSessionId,
            runtimeControlState = bootstrap?.phase.toControlState(),
            runtimeModules = native?.modules?.associate { it.moduleName to it.state.name }.orEmpty(),
            runtimeStrongIdentityVerified = native?.strongIdentityVerified == true,
            runtimeCapabilities = native?.capabilities ?: state.ready?.capabilities.orEmpty(),
            runtimeMutationPermissionGranted = ready,
            runtimeLifecycle = lifecycle?.name ?: bootstrap?.phase?.name,
            runtimeSuspended = config.enabled && !ready,
            observationSeq = state.lastObservationSeq,
            lastAction = state.lastAutomationEvent?.message,
            lastError = bootstrap?.takeIf { it.phase == RuntimeBootstrapPhase.FAILED }?.let {
                "${it.errorCode}: ${it.message.orEmpty()} (recoverable=${it.recoverable})"
            } ?: state.lastError ?: native?.errorMessage ?: native?.errorCode,
        )
    }

    private fun RuntimeBootstrapPhase?.toControlState(): String = when (this) {
        null -> "WAITING_FOR_GAME"
        RuntimeBootstrapPhase.CREATED -> "ATTACHING"
        RuntimeBootstrapPhase.CORE_LOADED,
        RuntimeBootstrapPhase.LOADER_OBSERVER_ACTIVE,
        RuntimeBootstrapPhase.IL2CPP_LIBRARY_SEEN,
        RuntimeBootstrapPhase.IL2CPP_INITIALIZED -> "INITIALIZING"
        RuntimeBootstrapPhase.BUILD_VERIFIED,
        RuntimeBootstrapPhase.MAIN_THREAD_READY,
        RuntimeBootstrapPhase.BINDINGS_READY -> "VERIFYING"
        RuntimeBootstrapPhase.LIVE_READY -> "LIVE_READY"
        RuntimeBootstrapPhase.FAILED -> "FAILED"
    }

    private fun mapDesiredState(
        config: HeadlessAutomationConfig,
        sessionId: String?,
    ) = RuntimeDesiredStateMapper.map(config, catchSpinArmState.snapshot(sessionId))

    private fun disarmForRuntimeBoundary(reason: String) {
        if (!catchSpinArmState.disarm(reason)) return
        val config = configRepository.update { it }
        lastSubmittedRevision = null
        val desired = mapDesiredState(config, sessionId = null)
        client.rememberDesiredState(desired)
        if (client.connected) {
            runCatching { client.submitDesiredState(desired) }
        }
    }

    private companion object {
        const val LOG_TAG = "PogoRootAutomation"
        const val POLL_INTERVAL_MS = 500L
        const val SHUTDOWN_TIMEOUT_MS = 5_000L
    }
}
