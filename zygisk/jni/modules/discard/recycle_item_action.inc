// Validate a real inventory row before the only RecycleItem invocation.
void *recycle_runtime_item_on_main_thread(
    ProbeContext &context,
    int32_t item_id,
    int32_t amount,
    void **exception,
    bool *invoked,
    const char **failure
) {
    RuntimeBinding binding = runtime_binding_snapshot(context);
    const Il2CppApi &api = context.il2cpp_api;
    if (exception != nullptr) *exception = nullptr;
    *invoked = false;
    *failure = "Inventory binding unavailable";
    if (!binding.discard_verified || binding.item_bag == nullptr ||
        binding.item_bag_recycle_item == nullptr || binding.item_data_class == nullptr ||
        api.runtime_invoke == nullptr ||
        item_id <= 0 || amount <= 0) return nullptr;

    const int32_t stack_count = read_runtime_item_count(api, binding, item_id);
    *failure = "Item count changed; waiting for fresh inventory";
    if (stack_count <= 0 || amount > stack_count) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native discard stack unavailable item_id=%d amount=%d stack_count=%d",
            item_id,
            amount,
            stack_count
        );
        return nullptr;
    }

    *failure = "Inventory item data or usable filter unavailable";
    void *item_data = create_recycle_item_data_on_main_thread(
        context,
        item_id,
        stack_count,
        exception
    );
    if (exception != nullptr && *exception != nullptr) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native discard ItemData build exception item_id=%d stack_count=%d exception=%p",
            item_id,
            stack_count,
            *exception
        );
        return nullptr;
    }
    if (!is_probable_managed_pointer(item_data)) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native discard ItemData unavailable item_id=%d stack_count=%d",
            item_id,
            stack_count
        );
        return nullptr;
    }
    {
        std::lock_guard<std::mutex> lock(context.binding_mutex);
        binding = context.binding;
    }
    int32_t verified_item_id = 0;
    int32_t verified_count = 0;
    uint8_t verified_recyclable = 0U;
    *failure = "Item is not recyclable or inventory row is stale";
    if (api.object_get_class == nullptr || api.object_get_class(item_data) != binding.item_data_class ||
        !read_runtime_field(api, item_data, "item", &verified_item_id) ||
        !read_runtime_field(api, item_data, "count", &verified_count) ||
        !read_runtime_field(api, item_data, "recyclable", &verified_recyclable) ||
        verified_item_id != item_id || verified_count != stack_count ||
        verified_count < amount || verified_recyclable != 1U) return nullptr;
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "native discard ItemData verify item_id=%d row_item=%d row_count=%d recyclable=%u",
        item_id,
        verified_item_id,
        verified_count,
        static_cast<unsigned>(verified_recyclable)
    );

    void *expiring_exception = nullptr;
    *failure = "Expiring item state unavailable";
    void *expiring_items = resolve_recycle_expiring_items_copy(
        api,
        binding,
        &expiring_exception
    );
    if (expiring_exception != nullptr) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native discard expiring items unavailable item_id=%d exception=%p",
            item_id,
            expiring_exception
        );
        expiring_items = nullptr;
    }
    if (!is_probable_managed_pointer(expiring_items)) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native discard expiring set missing item_id=%d supplier=%p hash_set=%p ctor=%p",
            item_id,
            binding.hash_set_item_supplier,
            binding.hash_set_item_class,
            binding.hash_set_item_ctor
        );
        return nullptr;
    }
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "native discard invoke prep item_id=%d amount=%d stack_count=%d item_data=%p expiring=%p",
        item_id,
        amount,
        stack_count,
        item_data,
        expiring_items
    );

    int32_t num_to_recycle = amount;
    void *arguments[] = {item_data, &num_to_recycle, expiring_items};
    *failure = "RecycleItem result unknown; discard paused until restart";
    *invoked = true;
    void *promise = api.runtime_invoke(
        binding.item_bag_recycle_item,
        binding.item_bag,
        arguments,
        exception
    );
    if (exception != nullptr && *exception != nullptr) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native discard RecycleItem exception item_id=%d amount=%d stack_count=%d exception=%p",
            item_id,
            amount,
            stack_count,
            *exception
        );
        return nullptr;
    }
    return promise;
}

