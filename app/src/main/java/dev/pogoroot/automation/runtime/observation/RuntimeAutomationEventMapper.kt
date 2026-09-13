package dev.pogoroot.automation.runtime.observation

import dev.pogoroot.automation.bridge.RuntimeAutomationEventPayload
import dev.pogoroot.automation.bridge.RuntimeAutomationEventType
import dev.pogoroot.automation.events.AutomationEvent
import dev.pogoroot.automation.events.AutomationEventType

/** Maps known native automation telemetry to the app-owned event/toast model. */
internal fun RuntimeAutomationEventPayload.toAutomationEvent(): AutomationEvent? = when (type) {
    RuntimeAutomationEventType.POKEMON_FOUND -> AutomationEvent(
        AutomationEventType.INFO,
        "Catch started: ${pokemonName()}",
    )
    RuntimeAutomationEventType.POKEMON_CAUGHT -> AutomationEvent(
        AutomationEventType.CAUGHT,
        "Catch success: ${pokemonName()}",
    )
    RuntimeAutomationEventType.POKEMON_FLED -> AutomationEvent(
        AutomationEventType.RAN_AWAY,
        "${pokemonName()} fled",
    )
    RuntimeAutomationEventType.POKEMON_TRANSFERRED -> AutomationEvent(
        AutomationEventType.TRANSFERRED,
        "Transfer success: ${pokemonName()}",
    )
    RuntimeAutomationEventType.POKEMON_TRANSFER_TRIGGERED -> AutomationEvent(
        AutomationEventType.INFO,
        "Transfer started: ${pokemonName()}",
    )
    RuntimeAutomationEventType.POKEMON_TRANSFER_FAILED -> AutomationEvent(
        AutomationEventType.ERROR,
        "Transfer failed: ${pokemonName()}",
    )
    RuntimeAutomationEventType.ITEM_DISCARD_TRIGGERED -> AutomationEvent(
        AutomationEventType.INFO,
        "Discard started: ${itemName()} ×$secondaryId",
    )
    RuntimeAutomationEventType.ITEM_DISCARDED -> AutomationEvent(
        AutomationEventType.DISCARDED,
        "Discard success: ${itemName()} ×$secondaryId",
    )
    RuntimeAutomationEventType.ITEM_DISCARD_FAILED -> AutomationEvent(
        AutomationEventType.ERROR,
        "Discard failed: ${itemName()} ×$secondaryId" + detail.takeIf { it.isNotBlank() }
            ?.let { " — $it" }.orEmpty(),
    )
    RuntimeAutomationEventType.SPIN_STARTED -> AutomationEvent(AutomationEventType.INFO, "Spin started")
    RuntimeAutomationEventType.SPIN_COMPLETED -> AutomationEvent(AutomationEventType.SPUN, "Spin success")
    RuntimeAutomationEventType.SPIN_FAILED -> AutomationEvent(
        AutomationEventType.ERROR, "Spin failed (result $secondaryId)",
    )
    RuntimeAutomationEventType.SPIN_BUBBLES_FAILED -> AutomationEvent(
        AutomationEventType.ERROR, "Spin succeeded; game reward bubbles unavailable",
    )
    RuntimeAutomationEventType.CATCH_FAILED -> AutomationEvent(
        AutomationEventType.ERROR, "Catch unsuccessful: ${pokemonName()}",
    )
    RuntimeAutomationEventType.UNKNOWN -> null
}

private fun RuntimeAutomationEventPayload.pokemonName(): String = subjectName.ifBlank { "Pokémon" }
private fun RuntimeAutomationEventPayload.itemName(): String = subjectName.ifBlank { "Item" }
