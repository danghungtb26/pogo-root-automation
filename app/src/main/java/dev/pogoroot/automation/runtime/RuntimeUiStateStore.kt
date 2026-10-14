package dev.pogoroot.automation.runtime

import dev.pogoroot.automation.bridge.BridgeEvent
import dev.pogoroot.automation.bridge.RuntimeBootstrapStatus
import dev.pogoroot.automation.bridge.RuntimeBootstrapPhase
import dev.pogoroot.automation.bridge.RuntimeThrowDiagnostic
import dev.pogoroot.automation.bridge.RuntimeUiStatus
import dev.pogoroot.automation.core.model.MapTargetObservation
import dev.pogoroot.automation.events.AutomationEvent

sealed interface RuntimeUiEvent {
    data class Bootstrap(val value: RuntimeBootstrapStatus) : RuntimeUiEvent
    data class Ready(val value: BridgeEvent.RuntimeReady) : RuntimeUiEvent
    data class ObservationSequence(val value: Long) : RuntimeUiEvent
    data class Status(val value: RuntimeUiStatus) : RuntimeUiEvent
    data class Automation(val value: AutomationEvent) : RuntimeUiEvent
    data class MapTarget(val value: MapTargetObservation) : RuntimeUiEvent
    data class ThrowDiagnostic(val value: RuntimeThrowDiagnostic) : RuntimeUiEvent
    data class Error(val message: String) : RuntimeUiEvent
}

data class RuntimeUiState(
    val runtimeSessionId: String? = null,
    val bootstrap: RuntimeBootstrapStatus? = null,
    val ready: BridgeEvent.RuntimeReady? = null,
    val nativeStatus: RuntimeUiStatus? = null,
    val lastObservationSeq: Long? = null,
    val lastAutomationEvent: AutomationEvent? = null,
    val mapTarget: MapTargetObservation? = null,
    val throwDiagnostic: RuntimeThrowDiagnostic? = null,
    val lastError: String? = null,
)

/** Session-scoped UI state; raw nearby/encounter/inventory payloads never enter it. */
class RuntimeUiStateStore {
    private var state = RuntimeUiState()

    @Synchronized
    fun accept(event: RuntimeUiEvent) {
        state = when (event) {
            is RuntimeUiEvent.Bootstrap -> if (state.runtimeSessionId == event.value.sessionId) {
                if (event.value.phase == RuntimeBootstrapPhase.LIVE_READY) {
                    state.copy(bootstrap = event.value)
                } else {
                    state.copy(bootstrap = event.value, ready = null, nativeStatus = null,
                        lastObservationSeq = null, mapTarget = null)
                }
            } else {
                RuntimeUiState(runtimeSessionId = event.value.sessionId, bootstrap = event.value)
            }
            is RuntimeUiEvent.Ready -> state.copy(
                runtimeSessionId = event.value.runtimeSessionId,
                ready = event.value,
                nativeStatus = null,
                lastObservationSeq = null,
                mapTarget = null,
                throwDiagnostic = null,
                lastError = null,
            )
            is RuntimeUiEvent.ObservationSequence -> state.copy(lastObservationSeq = event.value)
            is RuntimeUiEvent.Status -> state.copy(
                runtimeSessionId = event.value.runtimeSessionId,
                nativeStatus = event.value,
                lastError = event.value.errorMessage ?: event.value.errorCode,
            )
            is RuntimeUiEvent.Automation -> state.copy(lastAutomationEvent = event.value)
            is RuntimeUiEvent.MapTarget -> state.copy(mapTarget = event.value)
            is RuntimeUiEvent.ThrowDiagnostic -> state.copy(throwDiagnostic = event.value)
            is RuntimeUiEvent.Error -> state.copy(lastError = event.message)
        }
    }

    @Synchronized
    fun reset(reason: String? = null) {
        state = RuntimeUiState(lastError = reason)
    }

    @Synchronized
    fun snapshot(): RuntimeUiState = state
}
