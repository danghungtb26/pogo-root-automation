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

    /**
     * Balls usable by the current auto-catch path. The DIRECT_MAP catch flow
     * always throws a Poké Ball (native `DirectMapPokeballThrow.ball_type = 1`),
     * so the out-of-balls guard counts only Poké Balls. Great/Ultra/Master are
     * not consumed by this flow and must not make the gate think a catch is
     * possible. Revisit this if the native throw learns to pick a better ball.
     */
    fun catchBallCount(): Int =
        items.filter { it.itemId == CATCH_BALL_ITEM_ID }.sumOf { it.count }
}

/** Item ID of the ball consumed by the auto-catch (DIRECT_MAP) flow: Poké Ball. */
const val CATCH_BALL_ITEM_ID: Int = 1
