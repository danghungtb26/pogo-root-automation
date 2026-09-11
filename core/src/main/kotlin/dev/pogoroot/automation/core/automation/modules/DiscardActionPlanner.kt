package dev.pogoroot.automation.core.automation.modules

import dev.pogoroot.automation.core.automation.AutomationAction
import dev.pogoroot.automation.core.automation.InventoryPlanner
import dev.pogoroot.automation.core.model.GameLifecycleState

/** discard module: recycle inventory items in the overworld. */
class DiscardActionPlanner(
    private val inventoryPlanner: InventoryPlanner = InventoryPlanner(),
) : ModuleActionPlanner {
    override fun plan(context: PlanningContext): List<AutomationAction> {
        if (context.snapshot.lifecycleState != GameLifecycleState.OVERWORLD) return emptyList()
        if (!context.policy.autoDiscard) return emptyList()
        val inventory = context.snapshot.inventory ?: return emptyList()
        return inventoryPlanner.plan(inventory, context.policy.inventoryPolicy)
    }
}
