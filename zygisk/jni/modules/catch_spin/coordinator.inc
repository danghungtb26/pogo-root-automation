// Native catch-spin owner. This is deliberately driven from the shared observer
// tick: Kotlin supplies a revisioned policy snapshot, while native reads the
// verified game state, filters candidates, and invokes the matching action.

namespace {
constexpr uint64_t kCatchSpinScanEveryTicks = 20U;  // ~2 s at a 100 ms tick.
constexpr uint64_t kCatchSpinActionRetryDelayNs = 250000000ULL;

struct RuntimeCatchSpinCoordinatorState {
    std::mutex mutex;
    uint64_t cycle_id = 0U;
    uint64_t settle_until_ns = 0U;
    bool suspended = false;
    bool out_of_balls = false;
};

RuntimeCatchSpinCoordinatorState g_runtime_catch_spin_coordinator;

bool runtime_catch_spin_valid_coordinate(double latitude, double longitude) {
    return std::isfinite(latitude) && latitude >= -90.0 && latitude <= 90.0 &&
        std::isfinite(longitude) && longitude >= -180.0 && longitude <= 180.0;
}

const pogo_runtime::RuntimeNearbySpawnObservation *select_catch_target(
    const pogo_runtime::RuntimeCatchSpinObservation &snapshot
) {
    if (!snapshot.nearby_available || !snapshot.nearby.is_complete) return nullptr;
    const pogo_runtime::RuntimeNearbySpawnObservation *selected = nullptr;
    for (const auto &spawn : snapshot.nearby.spawns) {
        if (spawn.spawn_id == 0U || !runtime_catch_spin_valid_coordinate(
                spawn.latitude, spawn.longitude)) continue;
        if (selected == nullptr || spawn.spawn_id < selected->spawn_id) selected = &spawn;
    }
    return selected;
}

const pogo_runtime::RuntimeFortObservation *select_spin_target(
    const pogo_runtime::RuntimeCatchSpinObservation &snapshot
) {
    if (!snapshot.forts_available) return nullptr;
    const pogo_runtime::RuntimeFortObservation *selected = nullptr;
    for (const auto &fort : snapshot.forts.forts) {
        if (fort.fort_id.empty() || !fort.spin_available) continue;
        if (selected == nullptr || fort.fort_id < selected->fort_id) selected = &fort;
    }
    return selected;
}

// Returns -1 only when the inventory source is unavailable. The native inventory
// reader emits non-zero stacks only, so an absent Poké Ball item is a verified
// zero and must switch the policy to the spin branch.
int32_t runtime_pokeball_count(
    const pogo_runtime::RuntimeCatchSpinObservation &snapshot
) {
    if (!snapshot.inventory_available) return -1;
    for (const auto &item : snapshot.inventory.items) {
        if (item.item_id == 1) return item.count;
    }
    return 0;
}

bool runtime_catch_spin_read_snapshot(
    ProbeContext &context,
    uint64_t cycle_id,
    pogo_runtime::RuntimeCatchSpinObservation *snapshot
) {
    if (snapshot == nullptr || cycle_id == 0U) return false;
    const RuntimeBinding before = runtime_binding_snapshot(context);
    if (!before.map_entity_read_verified) {
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

    bool has_player_position = nearby.has_player_position;
    double player_latitude = nearby.player_latitude;
    double player_longitude = nearby.player_longitude;
    if (!has_player_position) {
        has_player_position = request_main_thread_player_position(
            context, &player_latitude, &player_longitude
        );
    }

    *snapshot = pogo_runtime::RuntimeCatchSpinObservation{};
    snapshot->cycle_id = cycle_id;
    snapshot->nearby_available = map_ok && binding.map_entity_read_verified;
    snapshot->nearby = nearby;
    snapshot->forts_available = binding.forts_read_verified;
    snapshot->forts = forts;
    snapshot->inventory_available = inventory_ok;
    snapshot->inventory = inventory;
    snapshot->has_player_position = has_player_position;
    snapshot->player_latitude = player_latitude;
    snapshot->player_longitude = player_longitude;
    return true;
}

void runtime_catch_spin_log_rejected(const char *action, const char *reason) {
    __android_log_print(
        ANDROID_LOG_WARN,
        kLogTag,
        "native catch_spin action=%s rejected reason=%s",
        action,
        reason
    );
}

void runtime_catch_spin_settle(uint64_t delay_ms) {
    const uint64_t now = static_cast<uint64_t>(now_elapsed_nanos());
    const uint64_t delay_ns = delay_ms > UINT64_MAX / 1000000ULL
        ? UINT64_MAX
        : delay_ms * 1000000ULL;
    g_runtime_catch_spin_coordinator.settle_until_ns =
        UINT64_MAX - now < delay_ns ? UINT64_MAX : now + delay_ns;
}

void runtime_catch_spin_execute_catch(
    ProbeContext &context,
    const RuntimeBinding &binding,
    const pogo_runtime::RuntimeNearbySpawnObservation &target
) {
    if (!binding.direct_catch_verified || binding.map_pokemon_try_capture == nullptr) {
        runtime_catch_spin_log_rejected("catch", "direct_catch_binding_unavailable");
        return;
    }
    if (!begin_pending_direct_catch(context, "", target.spawn_id)) {
        runtime_catch_spin_log_rejected("catch", "direct_catch_promise_pending");
        return;
    }
    const MainThreadActionOutcome outcome = request_main_thread_direct_catch(
        context, target.spawn_id
    );
    switch (outcome) {
        case MainThreadActionOutcome::kInvoked:
            g_runtime_catch_spin_coordinator.suspended = true;
            __android_log_print(
                ANDROID_LOG_INFO,
                kLogTag,
                "native catch_spin catch invoked spawn=%llu; waiting for TryCapture Promise",
                static_cast<unsigned long long>(target.spawn_id)
            );
            break;
        case MainThreadActionOutcome::kIndeterminate:
            abandon_pending_direct_catch(context, "direct_catch_invocation_indeterminate");
            g_runtime_catch_spin_coordinator.suspended = true;
            runtime_catch_spin_log_rejected("catch", "main_thread_outcome_indeterminate");
            break;
        case MainThreadActionOutcome::kOutOfBalls:
            abandon_pending_direct_catch(context, "out_of_balls");
            g_runtime_catch_spin_coordinator.out_of_balls = true;
            runtime_catch_spin_log_rejected("catch", "out_of_balls");
            break;
        case MainThreadActionOutcome::kEncounterRequested:
            abandon_pending_direct_catch(context, "encounter_requested");
            g_runtime_catch_spin_coordinator.settle_until_ns =
                static_cast<uint64_t>(now_elapsed_nanos()) + kCatchSpinActionRetryDelayNs;
            __android_log_print(
                ANDROID_LOG_INFO,
                kLogTag,
                "native catch_spin background encounter requested spawn=%llu",
                static_cast<unsigned long long>(target.spawn_id)
            );
            break;
        case MainThreadActionOutcome::kRejected:
        default:
            abandon_pending_direct_catch(context, "target_unavailable");
            runtime_catch_spin_log_rejected("catch", "target_unavailable");
            break;
    }
}

void runtime_catch_spin_execute_spin(
    ProbeContext &context,
    const RuntimeCatchSpinConfig &config,
    const pogo_runtime::RuntimeFortObservation &fort
) {
    if (!config.auto_spin && !g_runtime_catch_spin_coordinator.out_of_balls) return;
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    if (!binding.spin_verified) {
        runtime_catch_spin_log_rejected("spin", "spin_binding_unavailable");
        return;
    }
    const MainThreadActionOutcome outcome = request_main_thread_spin(context, fort.fort_id);
    switch (outcome) {
        case MainThreadActionOutcome::kInvoked:
            g_runtime_catch_spin_coordinator.out_of_balls = false;
            runtime_catch_spin_settle(config.spin_settle_delay_ms);
            __android_log_print(
                ANDROID_LOG_INFO,
                kLogTag,
                "native catch_spin spin invoked fort=%s",
                fort.fort_id.c_str()
            );
            break;
        case MainThreadActionOutcome::kIndeterminate:
            g_runtime_catch_spin_coordinator.suspended = true;
            runtime_catch_spin_log_rejected("spin", "main_thread_outcome_indeterminate");
            break;
        case MainThreadActionOutcome::kRejected:
        default:
            runtime_catch_spin_log_rejected("spin", "fort_unavailable_or_cooling_down");
            break;
    }
}
}  // namespace

void reset_runtime_catch_spin_coordinator() {
    std::lock_guard<std::mutex> lock(g_runtime_catch_spin_coordinator.mutex);
    g_runtime_catch_spin_coordinator.cycle_id = 0U;
    g_runtime_catch_spin_coordinator.settle_until_ns = 0U;
    g_runtime_catch_spin_coordinator.suspended = false;
    g_runtime_catch_spin_coordinator.out_of_balls = false;
}

void run_runtime_catch_spin_observer_tick(ObserverTickContext &tick) {
    if (tick.tick % kCatchSpinScanEveryTicks != 0U) return;
    const MainThreadActionOutcome promise_poll = request_main_thread_poll_direct_catch(tick.context);
    if (promise_poll != MainThreadActionOutcome::kRejected) {
        PendingDirectCatch terminal;
        if (consume_terminal_pending_direct_catch(tick.context, &terminal)) {
            std::lock_guard<std::mutex> lock(g_runtime_catch_spin_coordinator.mutex);
            g_runtime_catch_spin_coordinator.suspended = false;
            runtime_catch_spin_settle(1000U);
            __android_log_print(
                terminal.catch_outcome_wire == 1U ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
                kLogTag,
                "native catch_spin TryCapture Promise settled spawn=%llu outcome=%u; map sync attempted",
                static_cast<unsigned long long>(terminal.encounter_id),
                terminal.catch_outcome_wire
            );
        }
    }
    std::lock_guard<std::mutex> lock(g_runtime_catch_spin_coordinator.mutex);

    const RuntimeCatchSpinConfig config = runtime_catch_spin_config_snapshot();
    if (!g_runtime_active.load() || !config.configured || !config.armed ||
        !((config.auto_catch && config.catch_all) || config.auto_spin)) return;
    if (tick.context.active_encounter_id.load() != 0U) return;
    if (g_runtime_catch_spin_coordinator.suspended) return;

    const uint64_t now = static_cast<uint64_t>(now_elapsed_nanos());
    if (now < g_runtime_catch_spin_coordinator.settle_until_ns) return;
    const RuntimeBinding binding = runtime_binding_snapshot(tick.context);
    if (!binding.exact_build_verified) return;

    const uint64_t cycle_id = ++g_runtime_catch_spin_coordinator.cycle_id;
    pogo_runtime::RuntimeCatchSpinObservation snapshot;
    if (!runtime_catch_spin_read_snapshot(tick.context, cycle_id, &snapshot)) {
        runtime_catch_spin_log_rejected("scan", "snapshot_read_failed");
        return;
    }
    __android_log_print(
        snapshot.nearby_available && snapshot.nearby.is_complete
            ? ANDROID_LOG_INFO
            : ANDROID_LOG_WARN,
        kLogTag,
        "native catch_spin scan cycle=%llu nearby_success=%d nearby=%zu "
        "forts_available=%d forts=%zu complete=%d player=%d",
        static_cast<unsigned long long>(cycle_id),
        snapshot.nearby_available ? 1 : 0,
        snapshot.nearby.spawns.size(),
        snapshot.forts_available ? 1 : 0,
        snapshot.forts.forts.size(),
        snapshot.nearby.is_complete ? 1 : 0,
        snapshot.has_player_position ? 1 : 0
    );
    // Keep the observation as best-effort telemetry for encounter/discard/transfer
    // consumers. The catch/spin decision below must not depend on Kotlin or on the
    // bridge socket being connected.
    (void)send_runtime_catch_spin_payload(tick.context, snapshot);

    const int32_t ball_count = runtime_pokeball_count(snapshot);
    if (ball_count == 0) g_runtime_catch_spin_coordinator.out_of_balls = true;
    if (ball_count > 0) g_runtime_catch_spin_coordinator.out_of_balls = false;

    const auto *target = select_catch_target(snapshot);
    const bool catch_enabled = config.auto_catch && config.catch_all;
    if (catch_enabled && target != nullptr && ball_count > 0 &&
        !g_runtime_catch_spin_coordinator.out_of_balls) {
        runtime_catch_spin_execute_catch(tick.context, binding, *target);
        return;
    }

    const bool force_spin_for_balls = catch_enabled && target != nullptr &&
        g_runtime_catch_spin_coordinator.out_of_balls;
    if (!config.auto_spin && !force_spin_for_balls) return;
    const auto *fort = select_spin_target(snapshot);
    if (fort == nullptr) {
        runtime_catch_spin_log_rejected("spin", "no_available_fort");
        return;
    }
    runtime_catch_spin_execute_spin(tick.context, config, *fort);
}
