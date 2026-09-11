package dev.pogoroot.automation.core.automation.modules

import dev.pogoroot.automation.core.automation.AutomationAction
import dev.pogoroot.automation.core.automation.CatchMode
import dev.pogoroot.automation.core.automation.CatchReason
import dev.pogoroot.automation.core.model.GameLifecycleState

/**
 * catch_spin module: map automation only — direct-map catch (TRY_CATCH) and spin
 * (TRY_SPIN), in the overworld. The encounter-based flow (open, throw, snapshot,
 * berry) belongs to the encounter module.
 *
 * The out-of-balls signal is internal here: the native catch executor throws a
 * Poké Ball and reports out_of_balls when none remain; while set (via
 * [dev.pogoroot.automation.core.automation.AutomationSnapshot.outOfBalls]) catching
 * is skipped and a spin is forced to farm balls. Catch and spin both live here, so
 * that coupling stays within the module.
 */
class CatchSpinActionPlanner : ModuleActionPlanner {
    override fun plan(context: PlanningContext): List<AutomationAction> {
        val snapshot = context.snapshot
        val policy = context.policy
        // Master arm: the cluster does nothing until the Automation switch is on.
        if (!snapshot.catchSpinArmed) return emptyList()
        if (snapshot.lifecycleState != GameLifecycleState.OVERWORLD) return emptyList()

        val outOfBalls = snapshot.outOfBalls
        val actions = mutableListOf<AutomationAction>()

        // Direct-map catch of the soonest-expiring spawn, catch-all only (nearby map
        // state has no IV/shiny metadata). The target is chosen by the coordinator.
        val target = context.overworldTargetSpawn
        if (policy.autoCatch && policy.catchPolicy.catchAll && target != null && !outOfBalls) {
            actions += AutomationAction.Catch(
                encounterId = target.spawnId,
                reason = CatchReason.CATCH_ALL,
                mode = CatchMode.DIRECT_MAP,
            )
        }

        // Spin on the normal toggle, or force one when a catch/open was suppressed
        // because the pouch is out of catch balls.
        val forceSpinForBalls = outOfBalls && context.overworldCatchIntended
        if (policy.autoSpin || forceSpinForBalls) {
            snapshot.forts?.forts
                ?.asSequence()
                ?.filter { it.spinAvailable }
                ?.forEach { actions += AutomationAction.Spin(it.fortId) }
        }
        return actions
    }
}
