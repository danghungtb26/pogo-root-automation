package dev.pogoroot.automation.headless

import dev.pogoroot.automation.bridge.BridgeEvent
import dev.pogoroot.automation.core.automation.ActionExecutionPhase
import dev.pogoroot.automation.core.automation.ActionRequest
import dev.pogoroot.automation.core.automation.AutomationAction

internal fun AutomationAction.structuredSemanticName(): String = when (this) {
    is AutomationAction.Catch -> "TRY_CATCH"
    is AutomationAction.Spin -> "TRY_SPIN"
    else -> this::class.simpleName ?: "ACTION"
}

internal fun publishTerminalActionResult(
    eventSink: AutomationEventSink,
    request: ActionRequest,
    result: BridgeEvent.AutomationCommandResult,
    phase: ActionExecutionPhase,
    label: String,
) {
    val successful = phase == ActionExecutionPhase.COMPLETED
    val type = if (successful) {
        if (request.action is AutomationAction.Spin) AutomationEventType.SPUN
        else AutomationEventType.INFO
    } else {
        AutomationEventType.ERROR
    }
    val detail = result.message?.takeIf { it.isNotBlank() } ?: phase.name
    eventSink.publish(AutomationEvent(type, "$label: $detail"))
}
