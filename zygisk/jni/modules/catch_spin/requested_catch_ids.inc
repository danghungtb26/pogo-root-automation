// Catch/spin target selection and requested-id memory for the native catch_spin loop.

const pogo_runtime::RuntimeNearbySpawnObservation *select_catch_target(
    const pogo_runtime::RuntimeCatchSpinObservation &snapshot,
    const std::unordered_set<uint64_t> &requested_catch_ids
) {
    if (!snapshot.nearby_available || !snapshot.nearby.is_complete) return nullptr;
    const pogo_runtime::RuntimeNearbySpawnObservation *selected = nullptr;
    for (const auto &spawn : snapshot.nearby.spawns) {
        if (spawn.spawn_id == 0U || !runtime_catch_spin_valid_coordinate(
                spawn.latitude, spawn.longitude)) continue;
        if (requested_catch_ids.find(spawn.spawn_id) != requested_catch_ids.end()) continue;
        if (selected == nullptr || spawn.spawn_id < selected->spawn_id) selected = &spawn;
    }
    return selected;
}

const pogo_runtime::RuntimeFortObservation *select_spin_target(
    const pogo_runtime::RuntimeCatchSpinObservation &snapshot,
    RuntimeFortCooldownDeadlines *fort_cooldown_deadlines,
    uint64_t now_epoch_ms,
    double *selected_distance_meters,
    size_t *in_range_count
) {
    if (selected_distance_meters != nullptr) *selected_distance_meters = 0.0;
    if (in_range_count != nullptr) *in_range_count = 0U;
    if (!snapshot.forts_available || !snapshot.has_player_position ||
        !runtime_catch_spin_valid_coordinate(
            snapshot.player_latitude, snapshot.player_longitude
        )) return nullptr;

    constexpr double kEarthRadiusMeters = 6371000.0;
    constexpr double kDegreesToRadians = 0.017453292519943295;
    const double player_latitude = snapshot.player_latitude * kDegreesToRadians;
    const double player_longitude = snapshot.player_longitude * kDegreesToRadians;
    const pogo_runtime::RuntimeFortObservation *selected = nullptr;
    double selected_distance = 0.0;
    for (const auto &fort : snapshot.forts.forts) {
        if (fort.fort_id.empty() || !fort.spin_available ||
            !runtime_catch_spin_valid_coordinate(fort.latitude, fort.longitude)) continue;
        if (runtime_fort_cooldown_active(
                fort_cooldown_deadlines,
                fort.fort_id,
                now_epoch_ms,
                nullptr
            )) {
            continue;
        }
        const double fort_latitude = fort.latitude * kDegreesToRadians;
        const double fort_longitude = fort.longitude * kDegreesToRadians;
        const double delta_latitude = fort_latitude - player_latitude;
        const double delta_longitude = fort_longitude - player_longitude;
        const double haversine = std::sin(delta_latitude / 2.0) *
                std::sin(delta_latitude / 2.0) +
            std::cos(player_latitude) * std::cos(fort_latitude) *
                std::sin(delta_longitude / 2.0) * std::sin(delta_longitude / 2.0);
        const double distance = kEarthRadiusMeters * 2.0 *
            std::asin(std::sqrt(std::min(1.0, std::max(0.0, haversine))));
        if (distance > kCatchSpinFortInteractionRangeMeters) continue;
        if (in_range_count != nullptr) ++(*in_range_count);
        if (selected == nullptr || distance < selected_distance ||
            (distance == selected_distance && fort.fort_id < selected->fort_id)) {
            selected = &fort;
            selected_distance = distance;
        }
    }
    if (selected_distance_meters != nullptr && selected != nullptr) {
        *selected_distance_meters = selected_distance;
    }
    return selected;
}

void prune_requested_catch_ids(
    std::unordered_set<uint64_t> *requested_catch_ids,
    const pogo_runtime::RuntimeCatchSpinObservation &snapshot
) {
    if (requested_catch_ids == nullptr || requested_catch_ids->empty()) return;
    if (!snapshot.nearby_available || !snapshot.nearby.is_complete) return;

    std::unordered_set<uint64_t> visible;
    visible.reserve(snapshot.nearby.spawns.size());
    for (const auto &spawn : snapshot.nearby.spawns) {
        if (spawn.spawn_id != 0U) visible.insert(spawn.spawn_id);
    }
    for (auto it = requested_catch_ids->begin(); it != requested_catch_ids->end(); ) {
        if (visible.find(*it) == visible.end()) {
            it = requested_catch_ids->erase(it);
        } else {
            ++it;
        }
    }
}

void remember_requested_catch_skip(uint64_t spawn_id) {
    if (spawn_id == 0U) return;

    std::lock_guard<std::mutex> lock(g_runtime_catch_spin_coordinator.mutex);
    const auto inserted = g_runtime_catch_spin_coordinator.requested_catch_ids.insert(spawn_id);
    if (!inserted.second) return;

    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "native catch_spin skip remembered spawn=%llu memory_size=%zu",
        static_cast<unsigned long long>(spawn_id),
        g_runtime_catch_spin_coordinator.requested_catch_ids.size()
    );
}
