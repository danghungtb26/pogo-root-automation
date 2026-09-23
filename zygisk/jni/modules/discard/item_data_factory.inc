// Builds verified ItemInventoryItemWidget.ItemData for IItemBag.RecycleItem by reusing
// rows from ItemInventoryService.ListSortedPlayerInventory. Fail closed when lookup fails.

void *create_recycle_item_data_on_main_thread(
    ProbeContext &context,
    int32_t item_id,
    int32_t stack_count,
    void **exception
) {
    if (exception != nullptr) *exception = nullptr;
    if (context.il2cpp_api.object_new == nullptr || item_id <= 0 || stack_count <= 0) return nullptr;

    RuntimeBinding binding{};
    {
        std::lock_guard<std::mutex> lock(context.binding_mutex);
        binding = context.binding;
    }
    if (binding.item_data_class == nullptr) return nullptr;

    void *inventory_item_data = lookup_recycle_item_data_from_inventory_service(
        context,
        binding,
        item_id,
        exception
    );
    if (exception != nullptr && *exception != nullptr) {
        if (exception != nullptr) *exception = nullptr;
    } else if (is_probable_managed_pointer(inventory_item_data)) {
        {
            std::lock_guard<std::mutex> lock(context.binding_mutex);
            context.binding.item_inventory_service = binding.item_inventory_service;
            context.binding.item_inventory_get_expiring_items =
                binding.item_inventory_get_expiring_items;
            if (binding.item_inventory_filter_delegate != nullptr) {
                context.binding.item_inventory_filter_delegate =
                    binding.item_inventory_filter_delegate;
            }
        }
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "native discard ItemData source=inventory item_id=%d item_data=%p",
            item_id,
            inventory_item_data
        );
        return inventory_item_data;
    }

    __android_log_print(
        ANDROID_LOG_WARN,
        kLogTag,
        "native discard inventory ItemData unavailable item_id=%d; skipping RecycleItem",
        item_id
    );
    if (exception != nullptr) *exception = nullptr;
    return nullptr;
}

void *resolve_recycle_expiring_items_copy(
    const Il2CppApi &api,
    const RuntimeBinding &binding,
    void **exception
) {
    if (exception != nullptr) *exception = nullptr;
    if (binding.item_inventory_service != nullptr &&
        binding.item_inventory_get_expiring_items != nullptr &&
        api.runtime_invoke != nullptr) {
        void *expiring_items = api.runtime_invoke(
            binding.item_inventory_get_expiring_items,
            binding.item_inventory_service,
            nullptr,
            exception
        );
        if ((exception == nullptr || *exception == nullptr) &&
            is_probable_managed_pointer(expiring_items)) {
            return expiring_items;
        }
        if (exception != nullptr) *exception = nullptr;
    }
    // The supplier and an empty HashSet have the right generic shape but are
    // not proven to be the service generation used to create ItemData. Passing
    // either one can make RecycleItem observe an unrelated expiration context.
    // Keep the mutation unavailable until the genuine service copy is available.
    return nullptr;
}
