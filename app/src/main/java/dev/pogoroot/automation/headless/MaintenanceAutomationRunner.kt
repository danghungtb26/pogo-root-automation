package dev.pogoroot.automation.headless

import dev.pogoroot.automation.core.automation.InventoryPlanner
import dev.pogoroot.automation.core.automation.TransferPlanner

class MaintenanceAutomationRunner(
    private val runtime: MaintenanceRuntime = MaintenanceRuntimeBridge(),
    private val inventoryPlanner: InventoryPlanner = InventoryPlanner(),
    private val transferPlanner: TransferPlanner = TransferPlanner(),
    private val eventSink: AutomationEventSink = AutomationEventSink { },
) {
    private var lastProcessedSnapshotAt = 0L

    fun runOnce(config: HeadlessAutomationConfig): Result<MaintenanceRunResult> = runCatching {
        if (!config.autoDiscard && !config.autoTransfer) {
            return@runCatching MaintenanceRunResult()
        }

        val snapshot = runtime.readSnapshot().getOrThrow()
        val now = System.currentTimeMillis()
        check(snapshot.isFresh(now)) { "maintenance snapshot is stale" }
        snapshot.error?.let { error(it) }

        if (snapshot.observedAtEpochMs <= lastProcessedSnapshotAt) {
            return@runCatching MaintenanceRunResult(skippedDuplicateSnapshot = true)
        }

        val policy = config.toCorePolicy()
        var discardedStacks = 0
        var discardedItems = 0
        var transferredPokemon = 0

        if (config.autoDiscard) {
            check(snapshot.discardBindingsReady) { "discard runtime bindings are not ready" }
            val inventory = snapshot.inventory ?: error("inventory snapshot unavailable")
            val actions = inventoryPlanner.plan(inventory, policy.inventoryPolicy)
            actions.take(MAX_DISCARD_ACTIONS_PER_PASS).forEach { action ->
                runtime.execute(action).getOrThrow()
                discardedStacks += 1
                discardedItems += action.amount
                eventSink.publish(
                    AutomationEvent(
                        AutomationEventType.DISCARDED,
                        "Discarded ${action.amount}x item #${action.itemId}",
                    ),
                )
            }
        }

        if (config.autoTransfer) {
            check(snapshot.transferBindingsReady) { "transfer runtime bindings are not ready" }
            val storage = snapshot.storage ?: error("Pokemon storage snapshot unavailable")
            val actions = transferPlanner.plan(storage, policy.transferPolicy)
            actions.take(MAX_TRANSFER_ACTIONS_PER_PASS).forEach { action ->
                runtime.execute(action).getOrThrow()
                transferredPokemon += 1
                eventSink.publish(
                    AutomationEvent(
                        AutomationEventType.TRANSFERRED,
                        "Transferred Pokemon ${action.pokemonId}",
                    ),
                )
            }
        }

        lastProcessedSnapshotAt = snapshot.observedAtEpochMs
        MaintenanceRunResult(
            discardedStacks = discardedStacks,
            discardedItems = discardedItems,
            transferredPokemon = transferredPokemon,
        )
    }

    data class MaintenanceRunResult(
        val discardedStacks: Int = 0,
        val discardedItems: Int = 0,
        val transferredPokemon: Int = 0,
        val skippedDuplicateSnapshot: Boolean = false,
    )

    companion object {
        private const val MAX_DISCARD_ACTIONS_PER_PASS = 12
        private const val MAX_TRANSFER_ACTIONS_PER_PASS = 20
    }
}
