package dev.pogoroot.automation.core.automation

import dev.pogoroot.automation.core.model.InventorySnapshot
import dev.pogoroot.automation.core.model.ItemStack
import org.junit.Assert.assertEquals
import org.junit.Test

class InventoryPlannerTest {
    private val planner = InventoryPlanner()

    @Test
    fun `discards only amount above configured maximum`() {
        val actions = planner.plan(
            inventory = InventorySnapshot(
                observedAtEpochMs = 1_000L,
                usedSlots = 300,
                capacity = 350,
                items = listOf(
                    ItemStack(itemId = 1, itemName = "Poke Ball", count = 200),
                    ItemStack(itemId = 2, itemName = "Great Ball", count = 50),
                ),
            ),
            policy = InventoryPolicy(
                maxCountByItemId = mapOf(
                    1 to 120,
                    2 to 50,
                ),
            ),
        )

        assertEquals(
            listOf(AutomationAction.DiscardItem(itemId = 1, amount = 80)),
            actions,
        )
    }
}
