// Inventory reader for the discard module.
//
// Reads item counts via IItemBag.GetItemCount(Item) over a curated set of
// commonly-discardable item ids. ItemBag is a data service (not a Unity
// MonoBehaviour), so this is invoked from the observer's attached il2cpp thread
// by DiscardModule::observe(). A full enumeration would use
// NianticInventoryCache.GetCurrentItems instead. Depends on the shared
// read_runtime_item_count helper (runtime_map_forts.inc).
bool read_runtime_inventory(
    const Il2CppApi &api,
    const RuntimeBinding &binding,
    pogo_runtime::RuntimeInventoryObservation *observation
) {
    if (observation == nullptr || !binding.inventory_read_verified ||
        binding.item_bag == nullptr || binding.item_bag_get_item_count == nullptr ||
        api.runtime_invoke == nullptr || api.object_unbox == nullptr) return false;
    static const int32_t kInventoryItemIds[] = {
        1, 2, 3, 4,               // Poké / Great / Ultra / Master Ball
        101, 102, 103, 104,       // Potion / Super / Hyper / Max Potion
        201, 202,                 // Revive / Max Revive
        701, 703, 705, 706, 708,  // Razz / Nanab / Pinap / Golden Razz / Silver Pinap
    };
    int32_t item_ids[sizeof(kInventoryItemIds) / sizeof(int32_t) + kMaxRuntimeDiscardLimits]{};
    size_t item_id_count = 0U;
    for (int32_t item_id : kInventoryItemIds) item_ids[item_id_count++] = item_id;
    const RuntimeDiscardConfig discard_config = runtime_discard_config_snapshot();
    if (discard_config.configured) {
        for (uint32_t index = 0U; index < discard_config.limit_count; ++index) {
            const int32_t configured_item_id = discard_config.limits[index].item_id;
            bool already_present = false;
            for (size_t existing = 0U; existing < item_id_count; ++existing) {
                if (item_ids[existing] == configured_item_id) {
                    already_present = true;
                    break;
                }
            }
            if (!already_present && item_id_count < sizeof(item_ids) / sizeof(int32_t)) {
                item_ids[item_id_count++] = configured_item_id;
            }
        }
    }
    observation->items.clear();
    int64_t total = 0;
    bool read_failed = false;
    for (size_t index = 0U; index < item_id_count; ++index) {
        const int32_t item_id = item_ids[index];
        const int32_t count = read_runtime_item_count(api, binding, item_id);
        if (count < 0) {
            read_failed = true;
            __android_log_print(
                ANDROID_LOG_WARN,
                kLogTag,
                "runtime inventory item read failed item_id=%d",
                item_id
            );
            continue;
        }
        if (count == 0) continue;
        observation->items.push_back({item_id, count});
        total += count;
    }
    const RuntimeInventoryBallCounts balls = runtime_inventory_ball_counts(*observation);
    __android_log_print(
        read_failed ? ANDROID_LOG_WARN : ANDROID_LOG_INFO,
        kLogTag,
        "runtime inventory read stacks=%zu curated_total=%lld "
        "poke_ball_count=%d great_ball_count=%d ultra_ball_count=%d master_ball_count=%d "
        "read_failed=%d",
        observation->items.size(),
        static_cast<long long>(total),
        balls.poke_ball,
        balls.great_ball,
        balls.ultra_ball,
        balls.master_ball,
        read_failed ? 1 : 0
    );
    if (read_failed || total > 2147483647LL) return false;
    // The bag capacity is not read yet; report used==capacity==sum so the Kotlin
    // mapper's used<=capacity invariant holds. Discard uses per-item limits, not
    // capacity, so this does not affect the discard decision.
    observation->used_slots = static_cast<int32_t>(total);
    observation->capacity = static_cast<int32_t>(total);
    return true;
}
