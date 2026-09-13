package dev.pogoroot.automation.engine

import java.util.concurrent.atomic.AtomicReference
import dev.pogoroot.automation.events.AutomationEvent
import dev.pogoroot.automation.events.AutomationEventSink
import dev.pogoroot.automation.events.AutomationEventType
import dev.pogoroot.automation.runtime.RuntimeControlSnapshot
import dev.pogoroot.automation.runtime.RuntimeLifecycleCoordinator
import dev.pogoroot.automation.runtime.observation.RuntimeObservationTick

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
            runtimeModules = emptyMap(),
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
                runtimeModules = emptyMap(),
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
                runtimeModules = emptyMap(),
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

    fun publishTick(tick: RuntimeObservationTick) {
        val runtime = runtimeCoordinator.snapshot()
        status.updateAndGet {
            it.copy(
                running = true,
                enabled = true,
                runtimeSessionId = tick.runtimeSessionId,
                runtimeControlState = runtime.state.name,
                runtimeModules = emptyMap(),
                runtimeStrongIdentityVerified = tick.strongIdentityVerified,
                runtimeCapabilities = tick.runtimeCapabilities,
                runtimeMutationPermissionGranted = false,
                runtimeLifecycle = tick.lifecycleState.name,
                runtimeSuspended = false,
                observationSeq = tick.observationSeq,
                lastAction = tick.observationSeq?.let { "observing" },
                lastError = tick.lastError ?: runtime.lastError,
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
                runtimeModules = emptyMap(),
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

    private fun now(): Long = System.currentTimeMillis()
}
