package dev.pogoroot.automation.service

import android.util.Log
import dev.pogoroot.automation.bridge.RuntimeNativeLifecycle
import dev.pogoroot.automation.config.AutomationConfigRepository
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
    eventSink: AutomationEventSink,
    mapTargetRepository: MapTargetRepository,
    walkCandidateReceiver: NativeWalkCandidateReceiver,
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
    )
    @Volatile private var serviceRunning = false
    @Volatile private var lastSubmittedRevision: Long? = null
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
        val disabled = configRepository.update { it.copy(enabled = false) }
        runCatching {
            executor.submit {
                lastSubmittedRevision = null
                client.submitDesiredState(RuntimeDesiredStateMapper.map(disabled))
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
        val config = configRepository.read()
        if (lastSubmittedRevision != config.configRevision) {
            val result = client.submitDesiredState(RuntimeDesiredStateMapper.map(config))
            if (result.isSuccess) {
                lastSubmittedRevision = config.configRevision
            } else {
                Log.w(
                    LOG_TAG,
                    "desired state submit failed revision=${config.configRevision}",
                    result.exceptionOrNull(),
                )
            }
        }
        router.tick().onFailure { error ->
            Log.w(LOG_TAG, "runtime UI event poll failed", error)
        }
    }

    private fun submitCurrentDesiredStateOnRuntimeThread(): Result<DesiredStateReceipt> {
        val config = configRepository.read()
        return client.submitDesiredState(RuntimeDesiredStateMapper.map(config)).also { result ->
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
        val ready = native?.ready == true && native.strongIdentityVerified
        return RuntimeUiAutomationStatus(
            running = serviceRunning,
            enabled = config.enabled,
            runtimeSessionId = native?.runtimeSessionId ?: state.runtimeSessionId,
            runtimeControlState = lifecycle.toControlState(client.connected),
            runtimeModules = native?.modules?.associate { it.moduleName to it.state.name }.orEmpty(),
            runtimeStrongIdentityVerified = native?.strongIdentityVerified == true,
            runtimeCapabilities = native?.capabilities ?: state.ready?.capabilities.orEmpty(),
            runtimeMutationPermissionGranted = ready,
            runtimeLifecycle = lifecycle?.name,
            runtimeSuspended = config.enabled && !ready,
            observationSeq = state.lastObservationSeq,
            lastAction = state.lastAutomationEvent?.message,
            lastError = state.lastError ?: native?.errorMessage ?: native?.errorCode,
        )
    }

    private fun RuntimeNativeLifecycle?.toControlState(connected: Boolean): String = when (this) {
        null -> if (connected) "ATTACHED_IDLE" else "DETACHED"
        RuntimeNativeLifecycle.ATTACHED_IDLE -> "ATTACHED_IDLE"
        RuntimeNativeLifecycle.STARTING,
        RuntimeNativeLifecycle.DIAGNOSTIC_PENDING,
        RuntimeNativeLifecycle.BINDING_READY,
        RuntimeNativeLifecycle.APPLYING,
        -> "STARTING"
        RuntimeNativeLifecycle.READY -> "RUNNING"
        RuntimeNativeLifecycle.STOPPING -> "STOPPING"
        RuntimeNativeLifecycle.ERROR -> "ERROR"
        RuntimeNativeLifecycle.UNKNOWN -> "ATTACHED_IDLE"
    }

    private companion object {
        const val LOG_TAG = "PogoRootAutomation"
        const val POLL_INTERVAL_MS = 500L
        const val SHUTDOWN_TIMEOUT_MS = 5_000L
    }
}
