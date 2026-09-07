package dev.pogoroot.automation.core.automation

import dev.pogoroot.automation.core.model.GeoPoint

sealed interface AutomationAction {
    data class MoveTo(
        val target: GeoPoint,
        val mode: MovementMode = MovementMode.WALK,
    ) : AutomationAction

    data class OpenEncounter(
        val spawnId: String,
    ) : AutomationAction {
        init { require(spawnId.isNotBlank()) { "spawnId must not be blank" } }
    }

    data class Catch(
        val encounterId: String,
        val reason: CatchReason,
    ) : AutomationAction {
        init { require(encounterId.isNotBlank()) { "encounterId must not be blank" } }
    }

    data class Spin(
        val fortId: String,
    ) : AutomationAction {
        init { require(fortId.isNotBlank()) { "fortId must not be blank" } }
    }

    data class DiscardItem(
        val itemId: Int,
        val amount: Int,
    ) : AutomationAction {
        init {
            require(itemId > 0) { "itemId must be positive" }
            require(amount > 0) { "amount must be positive" }
        }
    }

    data class TransferPokemon(
        val pokemonId: String,
    ) : AutomationAction {
        init { require(pokemonId.isNotBlank()) { "pokemonId must not be blank" } }
    }

    data class UseBerry(
        val encounterId: String,
        val berryType: BerryType,
    ) : AutomationAction {
        init { require(encounterId.isNotBlank()) { "encounterId must not be blank" } }
    }

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

enum class BerryType {
    RAZZ,
    NANAB,
    PINAP,
    GOLDEN_RAZZ,
    SILVER_PINAP,
}

enum class AlertKind {
    SHUNDO,
}

data class AutomationActionResult(
    val action: AutomationAction,
    val success: Boolean,
    val message: String? = null,
)
