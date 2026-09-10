package dev.pogoroot.automation.headless

import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicReference

data class HeadlessAutomationStatus(
    val running: Boolean = false,
    val enabled: Boolean = false,
    val runtimeSessionId: String? = null,
    val runtimeControlState: String = RuntimeControlState.DETACHED.name,
    val runtimeModules: Map<String, String> = emptyMap(),
    val runtimeStrongIdentityVerified: Boolean = false,
    val runtimeCapabilities: Set<String> = emptySet(),
    val runtimeMutationPermissionGranted: Boolean = false,
    val runtimeLifecycle: String? = null,
    val runtimeSuspended: Boolean = false,
    val observationSeq: Long? = null,
    val lastAction: String? = null,
    val lastError: String? = null,
    val updatedAtEpochMs: Long = System.currentTimeMillis(),
)

/**
 * Structured-only automation loop. The service-owned runtime coordinator
 * controls the injected host and synchronizes each native feature module from
 * persisted settings while gameplay mutations remain serialized by
 * [StructuredAutomationController].
 */
class HeadlessAutomationEngine(
    private val configRepository: AutomationConfigRepository,
    private val runtimeCoordinator: RuntimeLifecycleCoordinator,
    private val structuredController: StructuredAutomationController,
    private val eventSink: AutomationEventSink = AutomationEventSink { },
) {
    private val executor = Executors.newSingleThreadExecutor()
    private val loopActive = AtomicBoolean(false)
    private val status = AtomicReference(HeadlessAutomationStatus())

    fun start() {
        if (!loopActive.compareAndSet(false, true)) return
        status.updateAndGet { it.copy(running = true, updatedAtEpochMs = now()) }
        executor.execute { runLoop() }
    }

    fun stop() {
        loopActive.set(false)
        status.updateAndGet { it.copy(running = false, updatedAtEpochMs = now()) }
    }

    fun shutdown() {
        loopActive.set(false)
        runCatching { runtimeCoordinator.ensureIdle().getOrThrow() }
        structuredController.stop()
        runtimeCoordinator.shutdown()
        executor.shutdownNow()
    }

    fun snapshot(): HeadlessAutomationStatus {
        val runtime = runtimeCoordinator.snapshot()
        return status.get().copy(
            runtimeControlState = runtime.state.name,
            runtimeModules = runtime.moduleStates(),
            updatedAtEpochMs = now(),
        )
    }

    private fun runLoop() {
        while (loopActive.get()) {
            val config = configRepository.read()
            if (!config.enabled) {
                runtimeCoordinator.ensureIdle()
                    .onFailure { error ->
                        recordError(
                            message = "runtime stop: ${error.message ?: error::class.java.simpleName}",
                            enabled = false,
                        )
                    }
                publishIdle()
                sleepInterruptibly(700L)
                continue
            }

            if (!runtimeCoordinator.connected) {
                // Drop any stale game-adapter session before the broker/runtime
                // reconnect path creates a fresh runtime session.
                structuredController.stop()
            }
            runtimeCoordinator.ensureRunning(config)
                .onSuccess {
                    structuredController.tick(config)
                        .onSuccess(::publishTick)
                        .onFailure { error ->
                            val runner = structuredController.snapshot()
                            recordError(
                                message = "runtime bridge: ${error.message ?: error::class.java.simpleName}",
                                runtimeSessionId = runner.runtimeSessionId,
                                runtimeSuspended = runner.suspended,
                                observationSeq = runner.lastObservationSeq,
                            )
                        }
                }
                .onFailure { error ->
                    recordError(
                        message = "runtime start: ${error.message ?: error::class.java.simpleName}",
                        runtimeSessionId = runtimeCoordinator.snapshot().runtimeSessionId,
                    )
                }
            sleepInterruptibly(config.loopIntervalMs)
        }

        runCatching { runtimeCoordinator.ensureIdle().getOrThrow() }
        structuredController.stop()
        status.updateAndGet { it.copy(running = false, updatedAtEpochMs = now()) }
    }

    private fun publishIdle() {
        val runtime = runtimeCoordinator.snapshot()
        status.updateAndGet {
            it.copy(
                running = true,
                enabled = false,
                runtimeSessionId = runtime.runtimeSessionId,
                runtimeControlState = runtime.state.name,
                runtimeModules = runtime.moduleStates(),
                runtimeStrongIdentityVerified = false,
                runtimeCapabilities = emptySet(),
                runtimeMutationPermissionGranted = false,
                runtimeLifecycle = null,
                runtimeSuspended = false,
                observationSeq = null,
                lastAction = "idle",
                lastError = runtime.lastError,
                updatedAtEpochMs = now(),
            )
        }
    }

    private fun publishTick(tick: StructuredAutomationTick) {
        val runtime = runtimeCoordinator.snapshot()
        val moduleErrors = runtime.modules.values
            .mapNotNull(RuntimeFeatureModuleSnapshot::lastError)
            .distinct()
        status.updateAndGet {
            it.copy(
                running = true,
                enabled = true,
                runtimeSessionId = tick.runtimeSessionId,
                runtimeControlState = runtime.state.name,
                runtimeModules = runtime.moduleStates(),
                runtimeStrongIdentityVerified = tick.strongIdentityVerified,
                runtimeCapabilities = tick.runtimeCapabilities,
                runtimeMutationPermissionGranted = tick.mutationPermissionGranted,
                runtimeLifecycle = tick.lifecycleState.name,
                runtimeSuspended = tick.suspended,
                observationSeq = tick.observationSeq,
                lastAction = tick.lastAction,
                lastError = tick.lastError ?: moduleErrors.firstOrNull(),
                updatedAtEpochMs = now(),
            )
        }
    }

    private fun recordError(
        message: String,
        enabled: Boolean = true,
        runtimeSessionId: String? = status.get().runtimeSessionId,
        runtimeSuspended: Boolean = status.get().runtimeSuspended,
        observationSeq: Long? = status.get().observationSeq,
    ) {
        val runtime = runtimeCoordinator.snapshot()
        status.updateAndGet {
            it.copy(
                running = true,
                enabled = enabled,
                runtimeSessionId = runtimeSessionId,
                runtimeControlState = runtime.state.name,
                runtimeModules = runtime.moduleStates(),
                runtimeStrongIdentityVerified = false,
                runtimeCapabilities = emptySet(),
                runtimeMutationPermissionGranted = false,
                runtimeLifecycle = "ERROR",
                runtimeSuspended = runtimeSuspended,
                observationSeq = observationSeq,
                lastError = message,
                updatedAtEpochMs = now(),
            )
        }
        eventSink.publish(AutomationEvent(AutomationEventType.ERROR, message))
    }

    private fun RuntimeControlSnapshot.moduleStates(): Map<String, String> =
        modules.values.associate { it.module.name to it.state.name }

    private fun sleepInterruptibly(durationMs: Long) {
        if (durationMs <= 0L) return
        try {
            Thread.sleep(durationMs)
        } catch (_: InterruptedException) {
            Thread.currentThread().interrupt()
        }
    }

    private fun now(): Long = System.currentTimeMillis()
}
