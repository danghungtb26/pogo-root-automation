package dev.pogoroot.automation.core.automation

import dev.pogoroot.automation.core.model.InventorySnapshot

class InventoryPlanner {
    fun plan(
        inventory: InventorySnapshot,
        policy: InventoryPolicy,
    ): List<AutomationAction.DiscardItem> = inventory.items.mapNotNull { stack ->
        val maxCount = policy.maxCountByItemId[stack.itemId] ?: return@mapNotNull null
        val amount = stack.count - maxCount
        if (amount <= 0) {
            null
        } else {
            AutomationAction.DiscardItem(
                itemId = stack.itemId,
                amount = amount,
            )
        }
    }
}
