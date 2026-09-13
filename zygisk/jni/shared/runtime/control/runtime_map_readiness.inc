namespace {
bool valid_map_coordinate(double latitude, double longitude) {
    return std::isfinite(latitude) && std::isfinite(longitude) &&
        latitude >= -90.0 && latitude <= 90.0 &&
        longitude >= -180.0 && longitude <= 180.0 &&
        !(latitude == 0.0 && longitude == 0.0);
}
}  // namespace

namespace pogo_runtime_module {
bool map_binding_ready(const RuntimeBinding &binding) {
    return binding.map_entity_read_verified && binding.map_entity_service != nullptr &&
        binding.map_entity_get_cells != nullptr;
}

void reset_map_live_verified(ProbeContext &context) {
    context.map_live_verified.store(false, std::memory_order_release);
}

bool probe_map_live(ProbeContext &context) {
    if (context.map_live_verified.load(std::memory_order_acquire)) return true;
    if (!map_binding_ready(runtime_binding_snapshot(context))) return false;

    pogo_runtime::RuntimeNearbyObservation nearby{};
    pogo_runtime::RuntimeFortsObservation forts{};
    if (!request_main_thread_map_snapshot(context, &nearby, &forts)) {
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "map live probe rejected reason=snapshot_unavailable"
        );
        return false;
    }

    bool has_player_position = nearby.has_player_position &&
        valid_map_coordinate(nearby.player_latitude, nearby.player_longitude);
    if (!has_player_position) {
        double player_latitude = 0.0;
        double player_longitude = 0.0;
        has_player_position = request_main_thread_player_position(
            context,
            &player_latitude,
            &player_longitude
        ) && valid_map_coordinate(player_latitude, player_longitude);
    }
    if (nearby.is_complete) {
        context.map_live_verified.store(true, std::memory_order_release);
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "map live probe accepted reason=nearby_complete spawns=%zu player=%d",
            nearby.spawns.size(),
            has_player_position ? 1 : 0
        );
        return true;
    }

    if (!has_player_position) {
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "map live probe rejected reason=nearby_incomplete_and_player_unavailable"
        );
        return false;
    }

    context.map_live_verified.store(true, std::memory_order_release);
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "map live probe accepted reason=player_position_verified"
    );
    return true;
}

bool map_runtime_ready(ProbeContext &context) {
    if (!map_binding_ready(runtime_binding_snapshot(context))) return false;
    return context.map_live_verified.load(std::memory_order_acquire);
}
}  // namespace pogo_runtime_module
