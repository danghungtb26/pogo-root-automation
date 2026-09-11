extern std::atomic<bool> g_runtime_active;

bool send_runtime_control_completed(
    int fd,
    const char *request_id,
    const char *message
);

bool run_runtime_scan_map(
    ProbeContext &context,
    const char *request_id,
    uint64_t cycle_id
) {
    if (!g_runtime_active.load()) {
        return send_runtime_rejected(
            context.fd,
            request_id,
            "runtime_inactive",
            "runtime host must be STARTed before SCAN_MAP"
        );
    }
    if (cycle_id == 0U) {
        return send_runtime_rejected(
            context.fd,
            request_id,
            "invalid_scan_cycle",
            "SCAN_MAP requires a positive cycle_id"
        );
    }

    const RuntimeBinding before = runtime_binding_snapshot(context);
    if (!before.map_entity_read_verified && !context.observation_running.load()) {
        discover_scene_runtime_owners(context);
    }
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    if (binding.map_entity_read_verified != before.map_entity_read_verified) {
        send_runtime_capability_update(context.fd, binding);
    }

    pogo_runtime::RuntimeInventoryObservation inventory;
    const bool inventory_ok = request_main_thread_inventory(context, &inventory);
    pogo_runtime::RuntimeNearbyObservation nearby;
    pogo_runtime::RuntimeFortsObservation forts;
    const bool map_ok = request_main_thread_map_snapshot(context, &nearby, &forts);

    bool has_player_position = map_ok && nearby.has_player_position;
    double player_latitude = nearby.player_latitude;
    double player_longitude = nearby.player_longitude;
    if (!has_player_position) {
        has_player_position = request_main_thread_player_position(
            context,
            &player_latitude,
            &player_longitude
        );
    }

    pogo_runtime::RuntimeCatchSpinObservation response{};
    response.cycle_id = cycle_id;
    response.nearby_available = map_ok && binding.map_entity_read_verified;
    response.nearby = nearby;
    response.forts_available = map_ok && binding.forts_read_verified;
    response.forts = forts;
    response.inventory_available = inventory_ok;
    response.inventory = inventory;
    response.has_player_position = has_player_position;
    response.player_latitude = player_latitude;
    response.player_longitude = player_longitude;

    if (!send_runtime_catch_spin_payload(context, response)) return false;
    __android_log_print(
        (response.nearby_available || response.forts_available || response.inventory_available)
            ? ANDROID_LOG_INFO
            : ANDROID_LOG_WARN,
        kLogTag,
        "runtime SCAN_MAP completed cycle=%llu map=%d nearby=%d forts=%d inventory=%d player=%d",
        static_cast<unsigned long long>(cycle_id),
        map_ok ? 1 : 0,
        response.nearby_available ? 1 : 0,
        response.forts_available ? 1 : 0,
        response.inventory_available ? 1 : 0,
        response.has_player_position ? 1 : 0
    );
    return send_runtime_control_completed(
        context.fd,
        request_id,
        "SCAN_MAP completed; REQUEST_CATCH_SPIN sent"
    );
}
