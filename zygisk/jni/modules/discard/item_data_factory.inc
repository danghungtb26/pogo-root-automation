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

void *create_expiring_item_hash_set_via_supplier(
    const Il2CppApi &api,
    const RuntimeBinding &binding,
    void **exception
) {
    if (exception != nullptr) *exception = nullptr;
    if (binding.hash_set_item_supplier == nullptr || api.runtime_invoke == nullptr) return nullptr;
    const uint8_t include_all = 0U;
    void *arguments[] = {const_cast<uint8_t *>(&include_all)};
    void *expiring_items = api.runtime_invoke(
        binding.hash_set_item_supplier,
        nullptr,
        arguments,
        exception
    );
    if (exception != nullptr && *exception != nullptr) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native discard hash set supplier exception exception=%p",
            *exception
        );
        return nullptr;
    }
    if (!is_probable_managed_pointer(expiring_items)) return nullptr;
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "native discard hash set supplier ok set=%p",
        expiring_items
    );
    return expiring_items;
}

void *create_empty_expiring_item_hash_set(
    const Il2CppApi &api,
    const RuntimeBinding &binding,
    void **exception
) {
    if (exception != nullptr) *exception = nullptr;
    if (binding.hash_set_item_class == nullptr || binding.hash_set_item_ctor == nullptr ||
        api.object_new == nullptr || api.runtime_invoke == nullptr) return nullptr;
    void *empty_set = api.object_new(binding.hash_set_item_class);
    if (!is_probable_managed_pointer(empty_set)) return nullptr;
    (void)api.runtime_invoke(binding.hash_set_item_ctor, empty_set, nullptr, exception);
    if (exception != nullptr && *exception != nullptr) return nullptr;
    return empty_set;
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
    void *supplier_set = create_expiring_item_hash_set_via_supplier(api, binding, exception);
    if (exception != nullptr && *exception != nullptr) {
        if (exception != nullptr) *exception = nullptr;
    } else if (is_probable_managed_pointer(supplier_set)) {
        return supplier_set;
    }
    return create_empty_expiring_item_hash_set(api, binding, exception);
}
