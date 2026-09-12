package dev.pogoroot.automation.core.automation

import dev.pogoroot.automation.core.location.GeoMath
import dev.pogoroot.automation.core.model.Fort
import dev.pogoroot.automation.core.model.FortType
import dev.pogoroot.automation.core.model.GameLifecycleState
import dev.pogoroot.automation.core.model.GeoPoint

/** Commands sent to the app-side mock-location controller. */
sealed interface AutoFortNavigationCommand {
    data class WalkTo(
        val fortId: String,
        val target: GeoPoint,
    ) : AutoFortNavigationCommand

    data class Stop(
        val reason: String,
    ) : AutoFortNavigationCommand
}

/** Terminal signals from the native catch/encounter flow. */
enum class AutoFortNavigationSignal {
    POKEMON_FOUND,
    POKEMON_CAUGHT,
    POKEMON_FLED,
}

/**
 * Coordinates the no-Pokémon walk without owning movement or catch execution.
 * It intentionally waits for a fresh empty scan after a terminal catch result.
 */
class AutoFortNavigationCoordinator(
    private val commandSink: (AutoFortNavigationCommand) -> Unit,
    private val arrivalToleranceMeters: Double = DEFAULT_ARRIVAL_TOLERANCE_METERS,
) {
    private var enabled = false
    private var catchInProgress = false
    private var waitingForEmptyScan = false
    private var targetFortId: String? = null
    private var targetPoint: GeoPoint? = null
    private var lastArrivedFortId: String? = null
    private var lastCommand: AutoFortNavigationCommand? = null

    init {
        require(arrivalToleranceMeters.isFinite() && arrivalToleranceMeters >= 0.0) {
            "arrivalToleranceMeters must be finite and non-negative"
        }
    }

    fun setEnabled(value: Boolean) {
        if (enabled == value) return
        enabled = value
        if (!value) {
            clearNavigationState()
            emit(AutoFortNavigationCommand.Stop("feature disabled"))
        }
    }

    fun onSnapshot(snapshot: AutomationSnapshot) {
        if (!enabled) return

        if (snapshot.lifecycleState == GameLifecycleState.ENCOUNTER) {
            catchInProgress = true
            waitingForEmptyScan = true
            emit(AutoFortNavigationCommand.Stop("encounter in progress"))
            return
        }
        if (snapshot.lifecycleState != GameLifecycleState.OVERWORLD) {
            emit(AutoFortNavigationCommand.Stop("overworld state unavailable"))
            return
        }

        val nearby = snapshot.nearby
        if (nearby == null) {
            emit(AutoFortNavigationCommand.Stop("nearby map unavailable"))
            return
        }
        if (!nearby.isComplete) {
            emit(AutoFortNavigationCommand.Stop("nearby map incomplete"))
            return
        }

        val player = nearby.playerPosition
        if (player == null) {
            emit(AutoFortNavigationCommand.Stop("player position unavailable"))
            return
        }

        if (nearby.spawns.isNotEmpty()) {
            catchInProgress = true
            waitingForEmptyScan = true
            emit(AutoFortNavigationCommand.Stop("pokemon nearby"))
            return
        }
        if (catchInProgress || waitingForEmptyScan) return

        markArrivalIfNeeded(player)
        if (targetFortId != null) {
            val currentTargetId = targetFortId ?: return
            val currentTarget = targetPoint ?: return
            emit(AutoFortNavigationCommand.WalkTo(currentTargetId, currentTarget))
            return
        }

        val fortSnapshot = snapshot.forts
        if (fortSnapshot == null) {
            emit(AutoFortNavigationCommand.Stop("fort map unavailable"))
            return
        }

        val candidate = selectFort(fortSnapshot.forts, player)
        if (candidate == null) {
            emit(AutoFortNavigationCommand.Stop("no available PokéStop"))
            return
        }

        targetFortId = candidate.fortId
        targetPoint = candidate.position
        emit(AutoFortNavigationCommand.WalkTo(candidate.fortId, candidate.position))
    }

    fun onSignal(signal: AutoFortNavigationSignal) {
        when (signal) {
            AutoFortNavigationSignal.POKEMON_FOUND -> {
                catchInProgress = true
                waitingForEmptyScan = true
                emit(AutoFortNavigationCommand.Stop("pokemon found"))
            }

            AutoFortNavigationSignal.POKEMON_CAUGHT,
            AutoFortNavigationSignal.POKEMON_FLED,
            -> {
                catchInProgress = false
                waitingForEmptyScan = false
            }
        }
    }

    fun reset() {
        enabled = false
        clearNavigationState()
        emit(AutoFortNavigationCommand.Stop("automation reset"))
    }

    private fun markArrivalIfNeeded(player: GeoPoint) {
        val currentTargetId = targetFortId ?: return
        val currentTarget = targetPoint ?: return
        if (GeoMath.distanceMeters(player, currentTarget) > arrivalToleranceMeters) return

        lastArrivedFortId = currentTargetId
        targetFortId = null
        targetPoint = null
    }

    private fun selectFort(forts: List<Fort>, player: GeoPoint): Fort? {
        val candidates = forts.asSequence()
            .filter { it.type == FortType.POKESTOP && it.spinAvailable }
            .sortedBy { GeoMath.distanceMeters(player, it.position) }
            .toList()
        if (candidates.isEmpty()) return null

        val alternatives = candidates.filter { it.fortId != lastArrivedFortId }
        if (lastArrivedFortId != null && alternatives.isEmpty()) return null

        val alternate = alternatives.firstOrNull()
        val awayFromCurrent = alternatives.firstOrNull {
            GeoMath.distanceMeters(player, it.position) > arrivalToleranceMeters
        }
        return awayFromCurrent ?: alternate ?: candidates.first()
    }

    private fun clearNavigationState() {
        catchInProgress = false
        waitingForEmptyScan = false
        targetFortId = null
        targetPoint = null
        lastArrivedFortId = null
    }

    private fun emit(command: AutoFortNavigationCommand) {
        if (lastCommand == command) return
        lastCommand = command
        commandSink(command)
    }

    private companion object {
        const val DEFAULT_ARRIVAL_TOLERANCE_METERS = 12.0
    }
}
