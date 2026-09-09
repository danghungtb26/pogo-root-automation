constexpr uint64_t kRuntimeMapPollIntervalNs = 2'000'000'000ULL;
constexpr uint64_t kRuntimeOwnerRefreshMissingIntervalNs = 5'000'000'000ULL;
constexpr uint64_t kRuntimeOwnerRefreshHealthyIntervalNs = 30'000'000'000ULL;

std::atomic<uint64_t> g_last_runtime_map_poll_ns{0U};
std::atomic<uint64_t> g_last_runtime_owner_refresh_ns{0U};

bool request_main_thread_map_snapshot_throttled(
    ProbeContext &context,
    pogo_runtime::RuntimeNearbyObservation *observation,
    pogo_runtime::RuntimeFortsObservation *forts
) {
    // Nearby/fort enumeration walks managed collections on the Unity main
    // thread. Never run it while an encounter is active and keep overworld
    // polling at a human-scale cadence instead of every ~300 ms.
    if (context.active_encounter_id.load() != 0U) return false;
    const uint64_t now = static_cast<uint64_t>(now_elapsed_nanos());
    const uint64_t last = g_last_runtime_map_poll_ns.load();
    if (last != 0U && now > last && now - last < kRuntimeMapPollIntervalNs) return false;
    g_last_runtime_map_poll_ns.store(now);
    return request_main_thread_map_snapshot(context, observation, forts);
}

bool refresh_runtime_encounter_owners_throttled(ProbeContext &context) {
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    const bool owners_missing = binding.encounter_pokemon == nullptr ||
        binding.encounter_state == nullptr ||
        binding.encounter_interaction == nullptr ||
        binding.pokeball_service == nullptr;
    const uint64_t interval = owners_missing
        ? kRuntimeOwnerRefreshMissingIntervalNs
        : kRuntimeOwnerRefreshHealthyIntervalNs;
    const uint64_t now = static_cast<uint64_t>(now_elapsed_nanos());
    const uint64_t last = g_last_runtime_owner_refresh_ns.load();
    if (last != 0U && now > last && now - last < interval) return false;
    g_last_runtime_owner_refresh_ns.store(now);
    return refresh_runtime_encounter_owners(context);
}
