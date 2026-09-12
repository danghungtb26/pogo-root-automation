package dev.pogoroot.automation.engine

import dev.pogoroot.automation.config.HeadlessAutomationConfig
import dev.pogoroot.automation.runtime.RuntimeLifecycleCoordinator
import dev.pogoroot.automation.runtime.structured.StructuredAutomationController

/** How the engine loop should pace itself after one cycle. */
internal enum class CycleWait { NORMAL, WAIT_FOR_RESULT, WAIT_AFTER_ACTION }

/**
 * One controller cycle: ensure the runtime is connected/ready and drain native
 * observations/results. Catch-spin scanning and mutation are owned by the native
 * observer; this loop remains for encounter and maintenance modules.
 */
internal class AutomationCycle(
    private val runtimeCoordinator: RuntimeLifecycleCoordinator,
    private val structuredController: StructuredAutomationController,
    private val statusReporter: HeadlessAutomationStatusReporter,
) {
    fun run(config: HeadlessAutomationConfig): CycleWait {
        if (!runtimeCoordinator.connected) {
            // Drop any stale game-adapter session before the broker/runtime
            // reconnect path creates a fresh runtime session.
            structuredController.stop()
        }
        var wait = CycleWait.NORMAL
        runtimeCoordinator.ensureRunning(config)
            .onSuccess { ready ->
                val configSync = runtimeCoordinator.syncCatchSpinConfig(
                    config = config,
                    armed = CatchSpinArmState.isArmed(),
                )
                if (configSync.isFailure) {
                    val error = configSync.exceptionOrNull()
                    structuredController.stop()
                    statusReporter.recordError(
                        message = "catch_spin config sync: " +
                            (error?.message ?: error?.javaClass?.simpleName ?: "unknown error"),
                        runtimeSessionId = ready.runtimeSessionId,
                    )
                    return@onSuccess
                }
                val transferConfigSync = runtimeCoordinator.syncTransferConfig(config)
                if (transferConfigSync.isFailure) {
                    val error = transferConfigSync.exceptionOrNull()
                    structuredController.stop()
                    statusReporter.recordError(
                        message = "transfer config sync: " +
                            (error?.message ?: error?.javaClass?.simpleName ?: "unknown error"),
                        runtimeSessionId = ready.runtimeSessionId,
                    )
                    return@onSuccess
                }
                if (!ready.strongIdentityVerified || ready.capabilities.isEmpty()) {
                    // START is intentionally probe-only. Do not let the structured
                    // adapter refresh or submit commands until the delayed automatic
                    // (or explicit) DIAGNOSTIC publishes verified capabilities.
                    structuredController.stop()
                    statusReporter.publishRuntimeDiagnosticPending(ready.runtimeSessionId)
                } else {
                    val wasAwaitingAction = structuredController.awaitingActionResult()
                    val tickSucceeded = tickAndPublish(config, "runtime bridge")
                    if (tickSucceeded && structuredController.awaitingActionResult()) {
                        wait = CycleWait.WAIT_FOR_RESULT
                    }
                    if (tickSucceeded && wasAwaitingAction && wait != CycleWait.WAIT_FOR_RESULT) {
                        wait = CycleWait.WAIT_AFTER_ACTION
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

}
