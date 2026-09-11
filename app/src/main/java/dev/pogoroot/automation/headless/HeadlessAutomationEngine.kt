package dev.pogoroot.automation.headless

import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicBoolean

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
 * Structured-only automation loop. This is a thin lifecycle + pacing shell:
 * [HeadlessAutomationStatusReporter] owns the observable status and
 * [AutomationCycle] owns the per-cycle scan/dispatch work. The engine only starts
 * the runtime, drives idle vs. active pacing, and tears down on shutdown.
 */
class HeadlessAutomationEngine(
    private val configRepository: AutomationConfigRepository,
    private val runtimeCoordinator: RuntimeLifecycleCoordinator,
    private val structuredController: StructuredAutomationController,
    eventSink: AutomationEventSink = AutomationEventSink { },
) {
    private val executor = Executors.newSingleThreadExecutor()
    private val loopActive = AtomicBoolean(false)
    private val resetRequested = AtomicBoolean(false)
    private val statusReporter = HeadlessAutomationStatusReporter(runtimeCoordinator, eventSink)
    private val cycle = AutomationCycle(runtimeCoordinator, structuredController, statusReporter)

    fun activate() {
        AutomationRunState.setActive(true)
        start()
    }

    fun deactivate() {
        AutomationRunState.setActive(false)
        cycle.resetCycleCounter()
        resetRequested.set(true)
    }

    fun start() {
        if (!loopActive.compareAndSet(false, true)) return
        statusReporter.setRunning(true)
        executor.execute { runLoop() }
    }

    fun stop() {
        AutomationRunState.setActive(false)
        cycle.resetCycleCounter()
        resetRequested.set(true)
        loopActive.set(false)
        statusReporter.setRunning(false)
    }

    fun shutdown() {
        AutomationRunState.setActive(false)
        cycle.resetCycleCounter()
        resetRequested.set(true)
        loopActive.set(false)
        runCatching { runtimeCoordinator.ensureIdle().getOrThrow() }
        structuredController.stop()
        runtimeCoordinator.shutdown()
        executor.shutdownNow()
    }

    fun snapshot(): HeadlessAutomationStatus = statusReporter.snapshot()

    private fun runLoop() {
        while (loopActive.get()) {
            if (resetRequested.compareAndSet(true, false)) {
                structuredController.resetForAutomationDisable()
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

            when (cycle.run(config)) {
                CycleWait.WAIT_FOR_RESULT -> sleepInterruptibly(200L)
                CycleWait.WAIT_AFTER_ACTION -> sleepInterruptibly(config.loopIntervalMs)
                CycleWait.NORMAL -> sleepInterruptibly(
                    if (structuredController.awaitingActionResult()) 200L else config.loopIntervalMs,
                )
            }
        }

        runCatching { runtimeCoordinator.ensureIdle().getOrThrow() }
        structuredController.stop()
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
