package dev.pogoroot.automation.core.automation.modules

import dev.pogoroot.automation.core.automation.AlertKind
import dev.pogoroot.automation.core.automation.AutomationAction
import dev.pogoroot.automation.core.automation.AutomationPolicy
import dev.pogoroot.automation.core.automation.AutomationSnapshot
import dev.pogoroot.automation.core.automation.CatchDecision
import dev.pogoroot.automation.core.automation.CatchMode
import dev.pogoroot.automation.core.automation.CatchReason
import dev.pogoroot.automation.core.model.EncounterSnapshot
import dev.pogoroot.automation.core.model.GameLifecycleState
import dev.pogoroot.automation.core.model.NearbySpawn

/**
 * catch_spin module: map targeting (direct-map catch / open encounter / spin) in
 * the overworld, and the catch throw (+ optional GO snapshot + shundo alert) inside
 * an encounter. The berry that precedes a throw is owned by the encounter module.
 *
 * The out-of-balls signal is internal to this module: the native catch executor
 * throws a Poké Ball and reports out_of_balls when none remain; while set (via
 * [AutomationSnapshot.outOfBalls]) catching is skipped and a spin is forced to farm
 * balls. Catch and spin both live here, so this coupling stays within the module.
 */
class CatchSpinActionPlanner : ModuleActionPlanner {
    override fun plan(context: PlanningContext): List<AutomationAction> =
        when (context.snapshot.lifecycleState) {
            GameLifecycleState.ENCOUNTER ->
                planEncounter(context.snapshot.encounter, context.policy, context.encounterCatchDecision)
            GameLifecycleState.OVERWORLD ->
                planOverworld(context.snapshot, context.policy)
            else -> emptyList()
        }

    private fun planOverworld(
        snapshot: AutomationSnapshot,
        policy: AutomationPolicy,
    ): List<AutomationAction> {
        val outOfBalls = snapshot.outOfBalls
        val actions = mutableListOf<AutomationAction>()

        val catchAllTarget = if (policy.autoCatch && policy.catchPolicy.catchAll) {
            soonestExpiringSpawn(snapshot)
        } else {
            null
        }
        val encounterTarget =
            if (policy.autoEncounter && !(policy.autoCatch && policy.catchPolicy.catchAll)) {
                soonestExpiringSpawn(snapshot)
            } else {
                null
            }
        val catchIntended = catchAllTarget != null || encounterTarget != null

        if (catchAllTarget != null && !outOfBalls) {
            // Nearby map state has no IV/shiny metadata, so direct catch is
            // limited to catch-all until encounter metadata is requested.
            actions += AutomationAction.Catch(
                encounterId = catchAllTarget.spawnId,
                reason = CatchReason.CATCH_ALL,
                mode = CatchMode.DIRECT_MAP,
            )
        }
        if (encounterTarget != null && !outOfBalls) {
            actions += AutomationAction.OpenEncounter(encounterTarget.spawnId)
        }

        // Spin on the normal toggle, or force one when a catch was suppressed
        // because the pouch is out of catch balls.
        val forceSpinForBalls = outOfBalls && catchIntended
        if (policy.autoSpin || forceSpinForBalls) {
            snapshot.forts?.forts
                ?.asSequence()
                ?.filter { it.spinAvailable }
                ?.forEach { actions += AutomationAction.Spin(it.fortId) }
        }
        return actions
    }

    private fun planEncounter(
        encounter: EncounterSnapshot?,
        policy: AutomationPolicy,
        decision: CatchDecision?,
    ): List<AutomationAction> {
        if (encounter == null) return emptyList()
        val actions = mutableListOf<AutomationAction>()
        if (policy.autoSnapshotDuringEncounter) {
            actions += AutomationAction.TakeEncounterSnapshot(
                encounterId = encounter.encounterId,
                encounterMode = policy.snapshotEncounterMode,
            )
        }
        // Berry (encounter module) is planned separately and ordered before this
        // throw by the coordinator. Here: shundo alert + the catch throw itself.
        if (decision != null && decision.shouldCatch && decision.reason != null) {
            if (encounter.isShundo) {
                actions += AutomationAction.Alert(
                    kind = AlertKind.SHUNDO,
                    message = "Shundo detected: ${encounter.speciesName} (${encounter.encounterId})",
                )
            }
            actions += AutomationAction.Catch(
                encounterId = encounter.encounterId,
                reason = decision.reason,
                closePreviewAfterCaught = policy.autoCloseCatchPreview,
                throwProfile = policy.catchPolicy.throwProfile,
            )
        }
        return actions
    }

    private fun soonestExpiringSpawn(snapshot: AutomationSnapshot): NearbySpawn? {
        val nearby = snapshot.nearby ?: return null
        return nearby.spawns
            .asSequence()
            .filter { spawn ->
                val expiresAt = spawn.expiresAtEpochMs
                expiresAt == null || expiresAt > nearby.observedAtEpochMs
            }
            .minByOrNull { it.expiresAtEpochMs ?: Long.MAX_VALUE }
    }
}
