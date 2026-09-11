package dev.pogoroot.automation.core.automation.modules

import dev.pogoroot.automation.core.automation.AlertKind
import dev.pogoroot.automation.core.automation.AutomationAction
import dev.pogoroot.automation.core.model.GameLifecycleState

/**
 * encounter module: the encounter-based flow. In the overworld it opens the
 * soonest-expiring spawn (when auto-encounter is on and direct-map catch-all is
 * not); inside an encounter it plans the GO snapshot, the berry assist, a shundo
 * alert, and the catch throw. Direct-map catch and spin belong to catch_spin.
 *
 * The throw is a client-side intent; it does not represent a guaranteed
 * server-side capture result.
 */
class EncounterActionPlanner : ModuleActionPlanner {
    override fun plan(context: PlanningContext): List<AutomationAction> =
        when (context.snapshot.lifecycleState) {
            GameLifecycleState.OVERWORLD -> planOverworldOpen(context)
            GameLifecycleState.ENCOUNTER -> planEncounter(context)
            else -> emptyList()
        }

    private fun planOverworldOpen(context: PlanningContext): List<AutomationAction> {
        val policy = context.policy
        // Open only when auto-encounter is on and direct-map catch-all is not (the
        // two target selections are mutually exclusive).
        if (!policy.autoEncounter || (policy.autoCatch && policy.catchPolicy.catchAll)) {
            return emptyList()
        }
        if (context.snapshot.outOfBalls) return emptyList()
        val target = context.overworldTargetSpawn ?: return emptyList()
        return listOf(AutomationAction.OpenEncounter(target.spawnId))
    }

    private fun planEncounter(context: PlanningContext): List<AutomationAction> {
        val encounter = context.snapshot.encounter ?: return emptyList()
        val policy = context.policy
        val actions = mutableListOf<AutomationAction>()
        if (policy.autoSnapshotDuringEncounter) {
            actions += AutomationAction.TakeEncounterSnapshot(
                encounterId = encounter.encounterId,
                encounterMode = policy.snapshotEncounterMode,
            )
        }
        val decision = context.encounterCatchDecision
        if (decision != null && decision.shouldCatch && decision.reason != null) {
            policy.berryType?.let { berryType ->
                actions += AutomationAction.UseBerry(
                    encounterId = encounter.encounterId,
                    berryType = berryType,
                )
            }
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
}
