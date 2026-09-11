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
    observation->items.clear();
    int64_t total = 0;
    for (int32_t item_id : kInventoryItemIds) {
        const int32_t count = read_runtime_item_count(api, binding, item_id);
        if (count <= 0) continue;
        observation->items.push_back({item_id, count});
        total += count;
    }
    // The bag capacity is not read yet; report used==capacity==sum so the Kotlin
    // mapper's used<=capacity invariant holds. Discard uses per-item limits, not
    // capacity, so this does not affect the discard decision.
    observation->used_slots = static_cast<int32_t>(total);
    observation->capacity = static_cast<int32_t>(total);
    return true;
}
