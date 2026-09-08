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
        /**
         * Ask a verified client-owned runtime to close the post-catch preview
         * only after it confirms [CatchOutcome.CAUGHT].
         */
        val closePreviewAfterCaught: Boolean = false,
        /**
         * Optional client-owned throw intent. This is never a guarantee that
         * the requested quality or curve will be returned by the game.
         */
        val throwProfile: ThrowProfile = ThrowProfile(),
    ) : AutomationAction {
        init { require(encounterId.isNotBlank()) { "encounterId must not be blank" } }
    }

    /** Trigger Pokémon GO's own Snapshot flow while an encounter is active. */
    data class TakeEncounterSnapshot(
        val encounterId: String,
        val encounterMode: EncounterMode = EncounterMode.STANDARD,
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

/** Semantic result emitted by a client-owned catch binding. */
enum class CatchOutcome {
    CAUGHT,
    MISSED,
    BREAKOUT,
    FLED,
    NO_BALL,
    INDETERMINATE,
}

enum class ThrowQualityTarget {
    ANY,
    NICE,
    GREAT,
    EXCELLENT,
}

enum class CurvePreference {
    ANY,
    STRAIGHT,
    CURVE,
}

enum class EncounterMode {
    STANDARD,
    AR_PLUS,
}

/**
 * A request to a verified client-owned throw pipeline. It deliberately does
 * not contain a force-hit/guaranteed-result flag.
 */
data class ThrowProfile(
    val qualityTarget: ThrowQualityTarget = ThrowQualityTarget.ANY,
    val curvePreference: CurvePreference = CurvePreference.ANY,
    val encounterMode: EncounterMode = EncounterMode.STANDARD,
) {
    val isDefault: Boolean
        get() = this == DEFAULT

    val requiresStructuredOutcome: Boolean
        get() = qualityTarget != ThrowQualityTarget.ANY || curvePreference != CurvePreference.ANY

    companion object {
        val DEFAULT = ThrowProfile()
    }
}

enum class ThrowQuality {
    NONE,
    NICE,
    GREAT,
    EXCELLENT,
    UNKNOWN,
}

enum class CurveOutcome {
    UNKNOWN,
    STRAIGHT,
    CURVE,
}

/** Actual evidence returned by a client-owned throw binding. */
data class ThrowOutcome(
    val hit: Boolean?,
    val quality: ThrowQuality = ThrowQuality.UNKNOWN,
    val curve: CurveOutcome = CurveOutcome.UNKNOWN,
) {
    init {
        require(quality != ThrowQuality.EXCELLENT || hit == true) {
            "a miss cannot report excellent quality"
        }
    }
}

/** Metadata for a completed GO Snapshot; the media bytes stay out of bridge frames. */
data class EncounterSnapshotResult(
    val encounterId: String,
    val mediaReference: String? = null,
) {
    init {
        require(encounterId.isNotBlank()) { "encounterId must not be blank" }
        require(mediaReference == null || mediaReference.isNotBlank()) {
            "mediaReference must not be blank when present"
        }
    }
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
