package dev.pogoroot.automation.engine

import android.util.Log
import java.util.concurrent.atomic.AtomicLong
import dev.pogoroot.automation.config.HeadlessAutomationConfig
import dev.pogoroot.automation.runtime.RuntimeLifecycleCoordinator
import dev.pogoroot.automation.runtime.structured.StructuredAutomationController

/** How the engine loop should pace itself after one cycle. */
internal enum class CycleWait { NORMAL, WAIT_FOR_RESULT, WAIT_AFTER_ACTION }

/**
 * One automation cycle: ensure the runtime is connected/ready, drain a pending
 * action result, and — only while the catch_spin cluster is armed — pull one
 * correlated SCAN_MAP world snapshot and dispatch the planned mutation. Owns the
 * per-session cycle counter. Extracted from [HeadlessAutomationEngine] so the loop
 * keeps only lifecycle + pacing.
 */
internal class AutomationCycle(
    private val runtimeCoordinator: RuntimeLifecycleCoordinator,
    private val structuredController: StructuredAutomationController,
    private val statusReporter: HeadlessAutomationStatusReporter,
) {
    private val snapshotCycle = AtomicLong(0L)

    fun resetCycleCounter() {
        snapshotCycle.set(0L)
    }

    fun run(config: HeadlessAutomationConfig): CycleWait {
        if (!runtimeCoordinator.connected) {
            // Drop any stale game-adapter session before the broker/runtime
            // reconnect path creates a fresh runtime session.
            structuredController.stop()
        }
        var wait = CycleWait.NORMAL
        runtimeCoordinator.ensureRunning(config)
            .onSuccess { ready ->
                if (!ready.strongIdentityVerified || ready.capabilities.isEmpty()) {
                    // START is intentionally probe-only. Do not let the structured
                    // adapter refresh or submit commands until the delayed automatic
                    // (or explicit) DIAGNOSTIC publishes verified capabilities.
                    structuredController.stop()
                    statusReporter.publishRuntimeDiagnosticPending(ready.runtimeSessionId)
                } else {
                    val wasAwaitingAction = structuredController.awaitingActionResult()
                    val beforeScan = tickAndPublish(config, "runtime bridge")
                    if (beforeScan && structuredController.awaitingActionResult()) {
                        wait = CycleWait.WAIT_FOR_RESULT
                    }
                    if (beforeScan && wasAwaitingAction && wait != CycleWait.WAIT_FOR_RESULT) {
                        wait = CycleWait.WAIT_AFTER_ACTION
                    }
                    // The runtime stays connected/warm while Pokémon GO is
                    // foreground, but the SCAN_MAP cycle (and its counter) only
                    // starts once the catch_spin cluster is armed. While disarmed we
                    // neither scan nor dispatch.
                    if (wait == CycleWait.NORMAL && CatchSpinArmState.isArmed()) {
                        val cycle = snapshotCycle.incrementAndGet()
                        runtimeCoordinator.requestCatchSpinScan(cycle)
                            .onFailure { error ->
                                Log.w(
                                    LOG_TAG,
                                    "automation SCAN_MAP failed cycle=$cycle " +
                                        "error=${error.message ?: error::class.java.simpleName}",
                                )
                            }
                        tickAndPublish(config, "runtime scan response")
                    }
                }
            }
            .onFailure { error ->
                statusReporter.recordError(
                    message = "runtime start: ${error.message ?: error::class.java.simpleName}",
                    runtimeSessionId = runtimeCoordinator.snapshot().runtimeSessionId,
                )
            }
        return wait
    }

    private fun tickAndPublish(config: HeadlessAutomationConfig, errorContext: String): Boolean {
        val result = structuredController.tick(config)
        result
            .onSuccess(statusReporter::publishTick)
            .onFailure { error ->
                val runner = structuredController.snapshot()
                statusReporter.recordError(
                    message = "$errorContext: ${error.message ?: error::class.java.simpleName}",
                    runtimeSessionId = runner.runtimeSessionId,
                    runtimeSuspended = runner.suspended,
                    observationSeq = runner.lastObservationSeq,
                )
            }
        return result.isSuccess
    }

    private companion object {
        const val LOG_TAG = "PogoRootAutomation"
    }
}
