package dev.pogoroot.automation.core.automation.modules

import dev.pogoroot.automation.core.automation.AutomationAction
import dev.pogoroot.automation.core.model.GameLifecycleState

/**
 * encounter module: in-encounter assistance. Currently the berry assist — used
 * only when the shared catch decision says this encounter will be caught, so the
 * berry is thrown right before the catch_spin throw. Does not represent a
 * guaranteed server-side capture result.
 */
class EncounterActionPlanner : ModuleActionPlanner {
    override fun plan(context: PlanningContext): List<AutomationAction> {
        if (context.snapshot.lifecycleState != GameLifecycleState.ENCOUNTER) return emptyList()
        val encounter = context.snapshot.encounter ?: return emptyList()
        val decision = context.encounterCatchDecision ?: return emptyList()
        if (!decision.shouldCatch || decision.reason == null) return emptyList()
        val berryType = context.policy.berryType ?: return emptyList()
        return listOf(
            AutomationAction.UseBerry(encounterId = encounter.encounterId, berryType = berryType),
        )
    }
}
