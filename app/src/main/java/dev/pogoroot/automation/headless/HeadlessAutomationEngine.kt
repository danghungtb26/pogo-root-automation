package dev.pogoroot.automation.headless

import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicReference

data class HeadlessAutomationStatus(
    val running: Boolean = false,
    val enabled: Boolean = false,
    val runtimeSessionId: String? = null,
    val runtimeStrongIdentityVerified: Boolean = false,
    val runtimeLifecycle: String? = null,
    val runtimeSuspended: Boolean = false,
    val observationSeq: Long? = null,
    val lastAction: String? = null,
    val lastError: String? = null,
    val updatedAtEpochMs: Long = System.currentTimeMillis(),
)

/**
 * Structured-only automation loop. Runtime observations and mutations always
 * cross the bridge and are serialized by [StructuredAutomationController].
 */
class HeadlessAutomationEngine(
    private val configRepository: AutomationConfigRepository,
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
        structuredController.stop()
        executor.shutdownNow()
    }

    fun snapshot(): HeadlessAutomationStatus = status.get().copy(updatedAtEpochMs = now())

    private fun runLoop() {
        while (loopActive.get()) {
            val config = configRepository.read()
            if (!config.enabled) {
                structuredController.stop()
                status.updateAndGet {
                    it.copy(
                        running = true,
                        enabled = false,
                        runtimeSessionId = null,
                        runtimeStrongIdentityVerified = false,
                        runtimeLifecycle = "DISCONNECTED",
                        runtimeSuspended = false,
                        observationSeq = null,
                        lastAction = "idle",
                        updatedAtEpochMs = now(),
                    )
                }
                sleepInterruptibly(700L)
                continue
            }

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
            sleepInterruptibly(config.loopIntervalMs)
        }

        structuredController.stop()
        status.updateAndGet { it.copy(running = false, updatedAtEpochMs = now()) }
    }

    private fun publishTick(tick: StructuredAutomationTick) {
        status.updateAndGet {
            it.copy(
                running = true,
                enabled = true,
                runtimeSessionId = tick.runtimeSessionId,
                runtimeStrongIdentityVerified = tick.strongIdentityVerified,
                runtimeLifecycle = tick.lifecycleState.name,
                runtimeSuspended = tick.suspended,
                observationSeq = tick.observationSeq,
                lastAction = tick.lastAction,
                lastError = tick.lastError,
                updatedAtEpochMs = now(),
            )
        }
    }

    private fun recordError(
        message: String,
        runtimeSessionId: String? = status.get().runtimeSessionId,
        runtimeSuspended: Boolean = status.get().runtimeSuspended,
        observationSeq: Long? = status.get().observationSeq,
    ) {
        status.updateAndGet {
            it.copy(
                running = true,
                enabled = true,
                runtimeSessionId = runtimeSessionId,
                runtimeStrongIdentityVerified = false,
                runtimeLifecycle = "ERROR",
                runtimeSuspended = runtimeSuspended,
                observationSeq = observationSeq,
                lastError = message,
                updatedAtEpochMs = now(),
            )
        }
        eventSink.publish(AutomationEvent(AutomationEventType.ERROR, message))
    }

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
