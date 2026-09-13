package dev.pogoroot.automation.engine

import dev.pogoroot.automation.config.HeadlessAutomationConfig
import dev.pogoroot.automation.runtime.RuntimeLifecycleCoordinator
import dev.pogoroot.automation.runtime.observation.RuntimeObservationRouter

/**
 * One controller cycle: ensure the runtime is connected/ready, push native config,
 * and drain observations for app-side features (events, map target, fort walk).
 */
internal class AutomationCycle(
    private val runtimeCoordinator: RuntimeLifecycleCoordinator,
    private val observationRouter: RuntimeObservationRouter,
    private val statusReporter: HeadlessAutomationStatusReporter,
) {
    fun run(config: HeadlessAutomationConfig) {
        if (!runtimeCoordinator.connected) {
            observationRouter.stop()
        }
        runtimeCoordinator.ensureRunning(config)
            .onSuccess { ready ->
                if (!ready.strongIdentityVerified) {
                    observationRouter.stop()
                    statusReporter.publishRuntimeDiagnosticPending(ready.runtimeSessionId)
                } else {
                    observationRouter.tick()
                        .onSuccess(statusReporter::publishTick)
                        .onFailure { error ->
                            statusReporter.recordError(
                                message = "runtime bridge: ${error.message ?: error::class.java.simpleName}",
                                runtimeSessionId = ready.runtimeSessionId,
                            )
                        }
                }
            }
            .onFailure { error ->
                observationRouter.stop()
                statusReporter.recordError(
                    message = "runtime start: ${error.message ?: error::class.java.simpleName}",
                    runtimeSessionId = runtimeCoordinator.snapshot().runtimeSessionId,
                )
            }
    }
}
