package dev.pogoroot.automation.core.automation.modules

import dev.pogoroot.automation.core.automation.AutomationAction
import dev.pogoroot.automation.core.automation.TransferPlanner
import dev.pogoroot.automation.core.model.GameLifecycleState

/** transfer module: release stored Pokémon in the overworld. */
class TransferActionPlanner(
    private val transferPlanner: TransferPlanner = TransferPlanner(),
) : ModuleActionPlanner {
    override fun plan(context: PlanningContext): List<AutomationAction> {
        if (context.snapshot.lifecycleState != GameLifecycleState.OVERWORLD) return emptyList()
        if (!context.policy.autoTransfer) return emptyList()
        val storage = context.snapshot.storage ?: return emptyList()
        return transferPlanner.plan(storage, context.policy.transferPolicy)
    }
}
