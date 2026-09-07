package dev.pogoroot.automation.headless

import dev.pogoroot.automation.adapter.GameAdapter
import dev.pogoroot.automation.core.automation.AutomationAction
import dev.pogoroot.automation.core.automation.AutomationActionResult
import dev.pogoroot.automation.core.automation.InventoryPlanner
import dev.pogoroot.automation.core.automation.TransferPlanner

class MaintenanceAutomationRunner(
    private val adapter: GameAdapter,
    private val notifier: AutomationEventNotifier? = null,
    private val inventoryPlanner: InventoryPlanner = InventoryPlanner(),
    private val transferPlanner: TransferPlanner = TransferPlanner(),
) {
    fun runOnce(config: HeadlessAutomationConfig): List<AutomationActionResult> {
        if (!config.enabled) return emptyList()

        val results = mutableListOf<AutomationActionResult>()
        val policy = config.toAutomationPolicy()

        if (config.autoDiscard && config.discardLimits.isNotEmpty()) {
            adapter.readInventory().getOrNull()?.let { inventory ->
                for (action in inventoryPlanner.plan(inventory, policy.inventoryPolicy)) {
                    val result = adapter.execute(action).getOrNull() ?: continue
                    results += result
                    if (result.success) {
                        notifier?.show(
                            AutomationEvent.ITEM_DISCARDED,
                            "${action.amount} × item ${action.itemId}",
                        )
                    }
                }
            }
        }

        if (config.autoTransfer) {
            adapter.readPokemonStorage().getOrNull()?.let { storage ->
                val byId = storage.pokemon.associateBy { it.pokemonId }
                for (action in transferPlanner.plan(storage, policy.transferPolicy)) {
                    val result = adapter.execute(action).getOrNull() ?: continue
                    results += result
                    if (result.success) {
                        val name = byId[action.pokemonId]?.speciesName ?: action.pokemonId
                        notifier?.show(AutomationEvent.POKEMON_TRANSFERRED, name)
                    }
                }
            }
        }

        return results
    }
}
