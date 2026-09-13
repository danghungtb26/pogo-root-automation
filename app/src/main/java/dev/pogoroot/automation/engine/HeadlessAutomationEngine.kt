package dev.pogoroot.automation.engine

import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicBoolean
import dev.pogoroot.automation.config.AutomationConfigRepository
import dev.pogoroot.automation.events.AutomationEventSink
import dev.pogoroot.automation.runtime.RuntimeControlState
import dev.pogoroot.automation.runtime.RuntimeLifecycleCoordinator
import dev.pogoroot.automation.runtime.observation.RuntimeObservationRouter

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
 * Headless automation loop: runtime lifecycle + observation drain pacing.
 * Native owns module enable/disable and gameplay mutations.
 */
class HeadlessAutomationEngine(
    private val configRepository: AutomationConfigRepository,
    private val runtimeCoordinator: RuntimeLifecycleCoordinator,
    private val observationRouter: RuntimeObservationRouter,
    eventSink: AutomationEventSink = AutomationEventSink { },
) {
    private val executor = Executors.newSingleThreadExecutor()
    private val loopActive = AtomicBoolean(false)
    private val resetRequested = AtomicBoolean(false)
    private val statusReporter = HeadlessAutomationStatusReporter(runtimeCoordinator, eventSink)
    private val cycle = AutomationCycle(runtimeCoordinator, observationRouter, statusReporter)

    fun activate() {
        configRepository.update { it.copy(enabled = true) }
        AutomationRunState.setActive(true)
        start()
    }

    fun deactivate() {
        configRepository.update { it.copy(enabled = false) }
        AutomationRunState.setActive(false)
        resetRequested.set(true)
    }

    fun start() {
        if (!loopActive.compareAndSet(false, true)) return
        statusReporter.setRunning(true)
        executor.execute { runLoop() }
    }

    fun stop() {
        AutomationRunState.setActive(false)
        resetRequested.set(true)
        loopActive.set(false)
        statusReporter.setRunning(false)
    }

    fun shutdown() {
        AutomationRunState.setActive(false)
        resetRequested.set(true)
        loopActive.set(false)
        runCatching { runtimeCoordinator.ensureIdle().getOrThrow() }
        observationRouter.stop()
        runtimeCoordinator.shutdown()
        executor.shutdownNow()
    }

    fun snapshot(): HeadlessAutomationStatus = statusReporter.snapshot()

    /** Push config to native on the engine thread (overlay/API path). */
    fun pushRuntimeConfigs() {
        executor.execute {
            if (!AutomationRunState.isActive()) return@execute
            runtimeCoordinator.pushRuntimeConfigs(configRepository.read())
        }
    }

    private fun runLoop() {
        while (loopActive.get()) {
            if (resetRequested.compareAndSet(true, false)) {
                observationRouter.stopNavigation()
                val stopped = runtimeCoordinator.ensureIdle()
                if (stopped.isFailure) {
                    // Keep the control connection for retry; disconnecting here
                    // would lose the ability to stop a still-running native host.
                    resetRequested.set(true)
                    statusReporter.recordError("runtime stop: ${stopped.exceptionOrNull()?.message}")
                    sleepInterruptibly(700L)
                    continue
                }
                observationRouter.resetForAutomationDisable()
            }
            val config = configRepository.read()
            if (!AutomationRunState.isActive()) {
                runtimeCoordinator.ensureIdle()
                    .onFailure { error ->
                        statusReporter.recordError(
                            message = "runtime stop: ${error.message ?: error::class.java.simpleName}",
                            enabled = false,
                        )
                    }
                statusReporter.publishIdle()
                sleepInterruptibly(700L)
                continue
            }

            cycle.run(config)
            sleepInterruptibly(config.loopIntervalMs)
        }

        runCatching { runtimeCoordinator.ensureIdle().getOrThrow() }
        observationRouter.stop()
        statusReporter.setRunning(false)
    }

    private fun sleepInterruptibly(durationMs: Long) {
        if (durationMs <= 0L) return
        try {
            Thread.sleep(durationMs)
        } catch (_: InterruptedException) {
            Thread.currentThread().interrupt()
        }
    }
}
