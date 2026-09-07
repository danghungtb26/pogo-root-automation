package dev.pogoroot.automation.core.automation

import dev.pogoroot.automation.core.model.GeoPoint

sealed interface AutomationAction {
    data class MoveTo(
        val target: GeoPoint,
        val mode: MovementMode = MovementMode.WALK,
    ) : AutomationAction

    data class OpenEncounter(
        val spawnId: String,
    ) : AutomationAction

    data class Catch(
        val encounterId: String,
        val reason: CatchReason,
    ) : AutomationAction

    data class Spin(
        val fortId: String,
    ) : AutomationAction

    data class DiscardItem(
        val itemId: Int,
        val amount: Int,
    ) : AutomationAction

    data class TransferPokemon(
        val pokemonId: String,
    ) : AutomationAction

    data class Alert(
        val kind: AlertKind,
        val message: String,
    ) : AutomationAction
}

enum class MovementMode {
    WALK,
    TELEPORT,
}

enum class CatchReason {
    SHUNDO,
    SHINY,
    HUNDO,
    IV_THRESHOLD,
    CATCH_ALL,
}

enum class AlertKind {
    SHUNDO,
}

data class AutomationActionResult(
    val action: AutomationAction,
    val success: Boolean,
    val message: String? = null,
)
