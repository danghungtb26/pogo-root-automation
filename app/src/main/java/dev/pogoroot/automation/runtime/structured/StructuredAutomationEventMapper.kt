package dev.pogoroot.automation.runtime.structured

import dev.pogoroot.automation.bridge.RuntimeAutomationEventPayload
import dev.pogoroot.automation.bridge.RuntimeAutomationEventType
import dev.pogoroot.automation.events.AutomationEvent
import dev.pogoroot.automation.events.AutomationEventType

/** Maps known native automation telemetry to the app-owned event/toast model. */
internal fun RuntimeAutomationEventPayload.toAutomationEvent(): AutomationEvent? = when (type) {
    RuntimeAutomationEventType.POKEMON_FOUND -> AutomationEvent(
        AutomationEventType.INFO,
        "Found Pokémon #${secondaryId.toUnsignedDecimal()} to catch",
    )
    RuntimeAutomationEventType.POKEMON_CAUGHT -> AutomationEvent(
        AutomationEventType.CAUGHT,
        if (secondaryId != 0L) {
            "Catch success: Pokémon #${secondaryId.toUnsignedDecimal()}"
        } else {
            "Catch success"
        },
    )
    RuntimeAutomationEventType.POKEMON_FLED -> AutomationEvent(
        AutomationEventType.RAN_AWAY,
        "Pokémon fled",
    )
    RuntimeAutomationEventType.POKEMON_TRANSFERRED -> AutomationEvent(
        AutomationEventType.TRANSFERRED,
        "Transfer success: Pokémon #${primaryId.toUnsignedDecimal()}",
    )
    RuntimeAutomationEventType.POKEMON_TRANSFER_TRIGGERED -> AutomationEvent(
        AutomationEventType.INFO,
        "Transfer started: Pokémon #${primaryId.toUnsignedDecimal()}",
    )
    RuntimeAutomationEventType.POKEMON_TRANSFER_FAILED -> AutomationEvent(
        AutomationEventType.ERROR,
        "Transfer failed: Pokémon #${primaryId.toUnsignedDecimal()}",
    )
    RuntimeAutomationEventType.ITEM_DISCARD_TRIGGERED -> AutomationEvent(
        AutomationEventType.INFO,
        "Discard started: item #${primaryId.toUnsignedDecimal()} x$secondaryId",
    )
    RuntimeAutomationEventType.ITEM_DISCARDED -> AutomationEvent(
        AutomationEventType.DISCARDED,
        "Discard success: item #${primaryId.toUnsignedDecimal()} x$secondaryId",
    )
    RuntimeAutomationEventType.ITEM_DISCARD_FAILED -> AutomationEvent(
        AutomationEventType.ERROR,
        "Discard failed: item #${primaryId.toUnsignedDecimal()} x$secondaryId",
    )
    RuntimeAutomationEventType.UNKNOWN -> null
}

private fun Long.toUnsignedDecimal(): String = toULong().toString()
