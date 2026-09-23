// InventoryCache timestamp is the local freshness boundary for discard.
// GetItemCount is useful telemetry, but a successful getter alone does not
// establish a baseline after reconnect or prove that a previous mutation has
// reconciled.

struct RuntimeInventoryReadiness {
    bool baseline_valid = false;
    uint64_t owner_generation = 0U;
    uint64_t observation_generation = 0U;
    int64_t latest_timestamp = 0;
};

std::mutex g_runtime_inventory_readiness_mutex;
RuntimeInventoryReadiness g_runtime_inventory_readiness;

bool read_runtime_inventory_cache_timestamp(
    const Il2CppApi &api,
    const RuntimeBinding &binding,
    int64_t *timestamp
) {
    if (timestamp == nullptr || !binding.inventory_cache_read_verified ||
        binding.inventory_cache == nullptr || binding.inventory_cache_get_latest_timestamp == nullptr ||
        api.runtime_invoke == nullptr || api.object_unbox == nullptr) return false;
    void *exception = nullptr;
    void *boxed_timestamp = api.runtime_invoke(
        binding.inventory_cache_get_latest_timestamp,
        binding.inventory_cache,
        nullptr,
        &exception
    );
    if (exception != nullptr || boxed_timestamp == nullptr) return false;
    void *unboxed = api.object_unbox(boxed_timestamp);
    if (!is_probable_managed_pointer(unboxed)) return false;
    memcpy(timestamp, unboxed, sizeof(*timestamp));
    return *timestamp > 0;
}

bool update_runtime_inventory_readiness(
    const Il2CppApi &api,
    const RuntimeBinding &binding,
    uint64_t *observation_generation,
    int64_t *latest_timestamp
) {
    if (observation_generation != nullptr) *observation_generation = 0U;
    if (latest_timestamp != nullptr) *latest_timestamp = 0;
    int64_t timestamp = 0;
    if (!read_runtime_inventory_cache_timestamp(api, binding, &timestamp)) return false;
    std::lock_guard<std::mutex> lock(g_runtime_inventory_readiness_mutex);
    if (g_runtime_inventory_readiness.owner_generation != binding.owner_generation) {
        g_runtime_inventory_readiness = RuntimeInventoryReadiness{};
        g_runtime_inventory_readiness.owner_generation = binding.owner_generation;
    }
    if (g_runtime_inventory_readiness.latest_timestamp > timestamp) {
        g_runtime_inventory_readiness.baseline_valid = false;
        return false;
    }
    if (g_runtime_inventory_readiness.latest_timestamp != timestamp) {
        ++g_runtime_inventory_readiness.observation_generation;
        g_runtime_inventory_readiness.latest_timestamp = timestamp;
    }
    g_runtime_inventory_readiness.baseline_valid = true;
    if (observation_generation != nullptr) {
        *observation_generation = g_runtime_inventory_readiness.observation_generation;
    }
    if (latest_timestamp != nullptr) *latest_timestamp = timestamp;
    return true;
}

bool runtime_inventory_baseline_ready() {
    std::lock_guard<std::mutex> lock(g_runtime_inventory_readiness_mutex);
    return g_runtime_inventory_readiness.baseline_valid;
}

uint64_t runtime_inventory_observation_generation() {
    std::lock_guard<std::mutex> lock(g_runtime_inventory_readiness_mutex);
    return g_runtime_inventory_readiness.observation_generation;
}

void reset_runtime_inventory_readiness() {
    std::lock_guard<std::mutex> lock(g_runtime_inventory_readiness_mutex);
    g_runtime_inventory_readiness = RuntimeInventoryReadiness{};
}
