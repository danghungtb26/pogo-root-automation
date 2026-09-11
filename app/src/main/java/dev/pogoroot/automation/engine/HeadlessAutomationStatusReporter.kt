package dev.pogoroot.automation.engine

import java.util.concurrent.atomic.AtomicReference
import dev.pogoroot.automation.events.AutomationEvent
import dev.pogoroot.automation.events.AutomationEventSink
import dev.pogoroot.automation.events.AutomationEventType
import dev.pogoroot.automation.runtime.RuntimeControlSnapshot
import dev.pogoroot.automation.runtime.RuntimeFeatureModuleSnapshot
import dev.pogoroot.automation.runtime.RuntimeLifecycleCoordinator
import dev.pogoroot.automation.runtime.structured.StructuredAutomationTick

/**
 * Owns the observable [HeadlessAutomationStatus] and all of its transitions
 * (running/idle/diagnostic-pending/tick/error). Extracted from the engine so the
 * loop keeps only lifecycle + cycle orchestration.
 */
internal class HeadlessAutomationStatusReporter(
    private val runtimeCoordinator: RuntimeLifecycleCoordinator,
    private val eventSink: AutomationEventSink,
) {
    private val status = AtomicReference(HeadlessAutomationStatus())

    fun current(): HeadlessAutomationStatus = status.get()

    fun setRunning(running: Boolean) {
        status.updateAndGet { it.copy(running = running, updatedAtEpochMs = now()) }
    }

    fun snapshot(): HeadlessAutomationStatus {
        val runtime = runtimeCoordinator.snapshot()
        return status.get().copy(
            enabled = AutomationRunState.isActive(),
            runtimeControlState = runtime.state.name,
            runtimeModules = runtime.moduleStates(),
            updatedAtEpochMs = now(),
        )
    }

    fun publishIdle() {
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

    fun publishRuntimeDiagnosticPending(runtimeSessionId: String?) {
        val runtime = runtimeCoordinator.snapshot()
        status.updateAndGet {
            it.copy(
                running = true,
                enabled = true,
                runtimeSessionId = runtimeSessionId,
                runtimeControlState = runtime.state.name,
                runtimeModules = runtime.moduleStates(),
                runtimeStrongIdentityVerified = false,
                runtimeCapabilities = emptySet(),
                runtimeMutationPermissionGranted = false,
                runtimeLifecycle = "WAITING_FOR_DIAGNOSTIC",
                runtimeSuspended = false,
                observationSeq = null,
                lastAction = "runtime-started",
                lastError = runtime.lastError ?: "runtime managed diagnostic pending",
                updatedAtEpochMs = now(),
            )
        }
    }

    fun publishTick(tick: StructuredAutomationTick) {
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

    fun recordError(
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

    private fun now(): Long = System.currentTimeMillis()
}
