namespace {
bool read_runtime_managed_bool(
    const Il2CppApi &api,
    const void *method,
    void *instance
) {
    if (method == nullptr || api.runtime_invoke == nullptr || api.object_unbox == nullptr) {
        return false;
    }
    void *exception = nullptr;
    void *boxed = api.runtime_invoke(method, instance, nullptr, &exception);
    if (exception != nullptr || !is_probable_managed_pointer(boxed)) return false;
    void *unboxed = api.object_unbox(boxed);
    if (!is_probable_managed_pointer(unboxed)) return false;
    uint8_t value = 0U;
    memcpy(&value, unboxed, sizeof(value));
    return value != 0U;
}

int32_t read_runtime_managed_int32(
    const Il2CppApi &api,
    const void *method,
    void *instance
) {
    if (method == nullptr || api.runtime_invoke == nullptr || api.object_unbox == nullptr) {
        return -1;
    }
    void *exception = nullptr;
    void *boxed = api.runtime_invoke(method, instance, nullptr, &exception);
    if (exception != nullptr || !is_probable_managed_pointer(boxed)) return -1;
    void *unboxed = api.object_unbox(boxed);
    if (!is_probable_managed_pointer(unboxed)) return -1;
    int32_t value = 0;
    memcpy(&value, unboxed, sizeof(value));
    return value;
}
}  // namespace

bool runtime_item_bag_is_full(const Il2CppApi &api, const RuntimeBinding &binding) {
    if (binding.inventory_full_read_verified && binding.player_service != nullptr &&
        binding.player_get_bag_is_full != nullptr) {
        if (read_runtime_managed_bool(api, binding.player_get_bag_is_full, binding.player_service)) {
            return true;
        }
    }
    if (binding.item_bag != nullptr && binding.item_bag_get_total_items != nullptr &&
        binding.player_service != nullptr && binding.player_get_max_items != nullptr) {
        const int32_t total = read_runtime_managed_int32(
            api,
            binding.item_bag_get_total_items,
            binding.item_bag
        );
        const int32_t max_items = read_runtime_managed_int32(
            api,
            binding.player_get_max_items,
            binding.player_service
        );
        if (total >= 0 && max_items > 0) return total >= max_items;
    }
    return false;
}
