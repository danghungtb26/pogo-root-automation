package dev.pogoroot.automation.core.model

data class ItemStack(
    val itemId: Int,
    val itemName: String,
    val count: Int,
) {
    init {
        require(count >= 0) { "count must be non-negative" }
    }
}

data class InventorySnapshot(
    val observedAtEpochMs: Long,
    val usedSlots: Int,
    val capacity: Int,
    val items: List<ItemStack>,
) {
    val freeSlots: Int
        get() = (capacity - usedSlots).coerceAtLeast(0)
}
