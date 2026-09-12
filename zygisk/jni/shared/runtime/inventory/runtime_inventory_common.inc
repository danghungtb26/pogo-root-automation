// Shared inventory semantics for native readers and telemetry.
//
// These values are Holoholo.Rpc.Item enum values from the pinned PoGo build,
// not UI category ids or item-list indexes.
constexpr int32_t kRuntimePokeBallItemId = 1;
constexpr int32_t kRuntimeGreatBallItemId = 2;
constexpr int32_t kRuntimeUltraBallItemId = 3;
constexpr int32_t kRuntimeMasterBallItemId = 4;

struct RuntimeInventoryBallCounts {
    int32_t poke_ball = 0;
    int32_t great_ball = 0;
    int32_t ultra_ball = 0;
    int32_t master_ball = 0;
};

inline int32_t runtime_inventory_item_count(
    const pogo_runtime::RuntimeInventoryObservation &inventory,
    int32_t item_id
) {
    for (const pogo_runtime::RuntimeInventoryItemObservation &item : inventory.items) {
        if (item.item_id == item_id) return item.count;
    }
    return 0;
}

inline RuntimeInventoryBallCounts runtime_inventory_ball_counts(
    const pogo_runtime::RuntimeInventoryObservation &inventory
) {
    RuntimeInventoryBallCounts counts;
    counts.poke_ball = runtime_inventory_item_count(inventory, kRuntimePokeBallItemId);
    counts.great_ball = runtime_inventory_item_count(inventory, kRuntimeGreatBallItemId);
    counts.ultra_ball = runtime_inventory_item_count(inventory, kRuntimeUltraBallItemId);
    counts.master_ball = runtime_inventory_item_count(inventory, kRuntimeMasterBallItemId);
    return counts;
}

inline int32_t runtime_inventory_total_ball_count(
    const pogo_runtime::RuntimeInventoryObservation &inventory
) {
    const RuntimeInventoryBallCounts counts = runtime_inventory_ball_counts(inventory);
    const int64_t total = static_cast<int64_t>(counts.poke_ball) +
        static_cast<int64_t>(counts.great_ball) +
        static_cast<int64_t>(counts.ultra_ball) +
        static_cast<int64_t>(counts.master_ball);
    return total > 2147483647LL ? 2147483647 : static_cast<int32_t>(total);
}
