extern std::atomic<bool> g_runtime_active;

bool send_runtime_control_completed(
    int fd,
    const char *request_id,
    const char *message
);

bool run_runtime_snapshot(ProbeContext &context, const char *request_id) {
    if (!g_runtime_active.load()) {
        return send_runtime_rejected(
            context.fd,
            request_id,
            "runtime_inactive",
            "runtime host must be STARTed before requesting a snapshot"
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
    if (inventory_ok && !send_runtime_inventory_payload(context, inventory)) return false;

    pogo_runtime::RuntimeNearbyObservation nearby;
    pogo_runtime::RuntimeFortsObservation forts;
    const bool map_ok = request_main_thread_map_snapshot(context, &nearby, &forts);
    if (map_ok) {
        // Publish auxiliary state before NEARBY so Kotlin's selected nearby
        // observation sees forts and ball counts from the same pull cycle.
        if (binding.forts_read_verified && !send_runtime_forts_payload(context, forts)) {
            return false;
        }
        if (binding.map_entity_read_verified && !send_runtime_nearby_payload(context, nearby)) {
            return false;
        }
    }
    __android_log_print(
        (map_ok || inventory_ok) ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "runtime snapshot request=%s map=%d nearby=%zu forts=%zu inventory=%d items=%zu",
        request_id == nullptr ? "<unknown>" : request_id,
        map_ok ? 1 : 0,
        nearby.spawns.size(),
        forts.forts.size(),
        inventory_ok ? 1 : 0,
        inventory.items.size()
    );
    return send_runtime_control_completed(
        context.fd,
        request_id,
        map_ok || inventory_ok
            ? "runtime world snapshot completed"
            : "runtime world snapshot unavailable; binding is not ready"
    );
}
