#include "../../shared/runtime/inventory/runtime_inventory_full.inc"

namespace {
constexpr uint64_t kCatchSpinScanEveryTicks = 20U;  // ~2 s at a 100 ms tick.
constexpr uint64_t kCatchSpinActionRetryDelayNs = 250000000ULL;
constexpr uint64_t kCatchSpinPostCatchDelayMs = 1000U;
constexpr double kCatchSpinFortInteractionRangeMeters = 80.0;

struct RuntimeCatchSpinCoordinatorState {
    std::mutex mutex;
    uint64_t cycle_id = 0U;
    uint64_t settle_until_ns = 0U;
    bool suspended = false;
    bool out_of_balls = false;
    bool item_inventory_full = false;
    std::unordered_set<uint64_t> requested_catch_ids;
    RuntimeFortCooldownDeadlines fort_cooldown_deadlines;
};

RuntimeCatchSpinCoordinatorState g_runtime_catch_spin_coordinator;

bool runtime_catch_spin_valid_coordinate(double latitude, double longitude) {
    return std::isfinite(latitude) && latitude >= -90.0 && latitude <= 90.0 &&
        std::isfinite(longitude) && longitude >= -180.0 && longitude <= 180.0;
}

#include "requested_catch_ids.inc"

// Returns -1 only when the inventory source is unavailable. The native inventory
// reader emits non-zero stacks only, so an absent Poké Ball item is a verified
// zero and must switch the policy to the spin branch.
int32_t runtime_pokeball_count(
    const pogo_runtime::RuntimeCatchSpinObservation &snapshot
) {
    if (!snapshot.inventory_available) return -1;
    return runtime_inventory_item_count(snapshot.inventory, kRuntimePokeBallItemId);
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
        send_runtime_capability_update(
            context.fd,
            binding,
            context.map_live_verified.load(std::memory_order_acquire)
        );
    }

    pogo_runtime::RuntimeInventoryObservation inventory;
    const bool inventory_ok = request_main_thread_inventory(context, &inventory);
    pogo_runtime::RuntimeNearbyObservation nearby;
    pogo_runtime::RuntimeFortsObservation forts;
    const bool map_ok = request_main_thread_map_snapshot(context, &nearby, &forts);

    bool has_player_position = nearby.has_player_position && runtime_catch_spin_valid_coordinate(
        nearby.player_latitude, nearby.player_longitude
    );
    double player_latitude = nearby.player_latitude;
    double player_longitude = nearby.player_longitude;
    if (!has_player_position) {
        has_player_position = request_main_thread_player_position(
            context, &player_latitude, &player_longitude
        );
        __android_log_print(
            has_player_position ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
            kLogTag,
            "native catch_spin player position fallback success=%d lat=%.7f lng=%.7f",
            has_player_position ? 1 : 0,
            player_latitude,
            player_longitude
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
    (void)send_runtime_automation_event(
        context,
        pogo_runtime::RuntimeAutomationEventType::kPokemonFound,
        target.spawn_id,
        static_cast<uint64_t>(target.species_id)
    );
    const MainThreadActionOutcome outcome = request_main_thread_direct_catch(
        context, target.spawn_id
    );
    switch (outcome) {
        case MainThreadActionOutcome::kInvoked:
            g_runtime_catch_spin_coordinator.requested_catch_ids.insert(target.spawn_id);
            g_runtime_catch_spin_coordinator.suspended = true;
            __android_log_print(
                ANDROID_LOG_INFO,
                kLogTag,
                "native catch_spin catch requested spawn=%llu memory_size=%zu; waiting for TryCapture Promise",
                static_cast<unsigned long long>(target.spawn_id),
                g_runtime_catch_spin_coordinator.requested_catch_ids.size()
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
    if (runtime_spin_is_pending(context)) {
        runtime_catch_spin_log_rejected("spin", "fort_search_promise_pending");
        return;
    }
    const MainThreadActionOutcome outcome = request_main_thread_spin(context, fort.fort_id);
    switch (outcome) {
        case MainThreadActionOutcome::kInvoked:
            g_runtime_catch_spin_coordinator.out_of_balls = false;
            __android_log_print(
                ANDROID_LOG_INFO,
                kLogTag,
                "native catch_spin FortSearch requested fort=%s; waiting for Promise result",
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
    g_runtime_catch_spin_coordinator.item_inventory_full = false;
    g_runtime_catch_spin_coordinator.requested_catch_ids.clear();
    g_runtime_catch_spin_coordinator.fort_cooldown_deadlines.clear();
}

void run_runtime_catch_spin_observer_tick(ObserverTickContext &tick) {
    if (tick.tick % kCatchSpinScanEveryTicks != 0U) return;
    const uint64_t pending_encounter_id = runtime_pending_encounter_id(tick.context);
    if (pending_encounter_id != 0U) {
        const MainThreadActionOutcome encounter_poll = request_main_thread_poll_encounter(
            tick.context,
            pending_encounter_id
        );
        if (encounter_poll == MainThreadActionOutcome::kIndeterminate) {
            runtime_catch_spin_log_rejected("encounter", "promise_poll_failed");
            remember_requested_catch_skip(pending_encounter_id);
        }
    }
    const MainThreadActionOutcome promise_poll = request_main_thread_poll_direct_catch(tick.context);
    if (promise_poll != MainThreadActionOutcome::kRejected) {
        PendingDirectCatch terminal;
        if (consume_terminal_pending_direct_catch(tick.context, &terminal)) {
            std::lock_guard<std::mutex> lock(g_runtime_catch_spin_coordinator.mutex);
            const bool transfer_busy = runtime_transfer_blocks_catch_spin();
            // A transfer in flight is a separate native gate. Do not copy it
            // into `suspended`, whose indeterminate state is intentionally
            // sticky; otherwise a successful ReleasePokemon Promise would
            // leave catch-spin suspended forever.
            g_runtime_catch_spin_coordinator.suspended = false;
            const bool catch_success = terminal.catch_outcome_wire == 1U;
            if (catch_success) {
                runtime_catch_spin_settle(kCatchSpinPostCatchDelayMs);
            }
            __android_log_print(
                terminal.catch_outcome_wire == 1U ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
                kLogTag,
                "native catch_spin TryCapture Promise settled spawn=%llu outcome=%u "
                "map sync attempted transfer_busy=%d post_catch_delay_ms=%llu",
                static_cast<unsigned long long>(terminal.encounter_id),
                terminal.catch_outcome_wire,
                transfer_busy ? 1 : 0,
                static_cast<unsigned long long>(
                    catch_success ? kCatchSpinPostCatchDelayMs : 0U
                )
            );
        }
    }
    if (runtime_spin_is_pending(tick.context)) {
        const MainThreadActionOutcome spin_poll = request_main_thread_poll_spin(tick.context);
        if (spin_poll != MainThreadActionOutcome::kRejected) {
            std::string fort_id;
            int32_t server_result = 0;
            int64_t cooldown_complete_ms = 0;
            if (consume_terminal_runtime_spin(
                    tick.context, &fort_id, &server_result, &cooldown_complete_ms
                )) {
                std::lock_guard<std::mutex> lock(g_runtime_catch_spin_coordinator.mutex);
                const long long epoch_now = now_epoch_millis();
                const uint64_t now_epoch_ms = epoch_now > 0
                    ? static_cast<uint64_t>(epoch_now) : 0U;
                remember_runtime_fort_cooldown(
                    &g_runtime_catch_spin_coordinator.fort_cooldown_deadlines,
                    fort_id,
                    cooldown_complete_ms,
                    now_epoch_ms
                );
                runtime_catch_spin_settle(
                    runtime_catch_spin_config_snapshot().spin_settle_delay_ms
                );
                if (server_result == 4) {
                    g_runtime_catch_spin_coordinator.item_inventory_full = true;
                } else if (server_result == 1) {
                    g_runtime_catch_spin_coordinator.item_inventory_full = false;
                }
                __android_log_print(
                    server_result == 1 ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
                    kLogTag,
                    "native catch_spin FortSearch Promise settled fort=%s result=%d(%s) success=%d "
                    "inventory_full=%d cooldown_complete_ms=%lld",
                    fort_id.c_str(),
                    server_result,
                    server_result == 1 ? "SUCCESS" : "REJECTED_OR_FAILED",
                    server_result == 1 ? 1 : 0,
                    g_runtime_catch_spin_coordinator.item_inventory_full ? 1 : 0,
                    static_cast<long long>(cooldown_complete_ms)
                );
            }
        }
    }
    std::lock_guard<std::mutex> lock(g_runtime_catch_spin_coordinator.mutex);

    const RuntimeCatchSpinConfig config = runtime_catch_spin_config_snapshot();
    if (!g_runtime_active.load() || !config.configured || !config.armed ||
        !((config.auto_catch && config.catch_all) || config.auto_spin ||
            config.auto_walk_to_fort)) return;
    if (tick.context.active_encounter_id.load() != 0U) {
        runtime_catch_spin_log_rejected("scan", "active_encounter");
        return;
    }
    if (runtime_direct_catch_is_pending(tick.context)) {
        runtime_catch_spin_log_rejected("scan", "direct_catch_promise_pending");
        return;
    }
    if (runtime_spin_is_pending(tick.context)) {
        runtime_catch_spin_log_rejected("scan", "fort_search_promise_pending");
        return;
    }
    if (runtime_encounter_request_is_pending(tick.context)) {
        runtime_catch_spin_log_rejected("scan", "encounter_promise_pending");
        return;
    }
    if (g_runtime_catch_spin_coordinator.suspended || runtime_transfer_blocks_catch_spin() ||
        runtime_discard_blocks_catch_spin()) return;

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
    (void)send_runtime_catch_spin_payload(tick.context, snapshot);

    const int32_t ball_count = runtime_pokeball_count(snapshot);
    const int32_t total_ball_count = snapshot.inventory_available
        ? runtime_inventory_total_ball_count(snapshot.inventory) : -1;
    __android_log_print(
        ball_count < 0 ? ANDROID_LOG_WARN : ANDROID_LOG_INFO,
        kLogTag,
        "native catch_spin ball inventory_available=%d poke_ball_count=%d "
        "total_ball_count=%d inventory_stacks=%zu",
        snapshot.inventory_available ? 1 : 0,
        ball_count,
        total_ball_count,
        snapshot.inventory.items.size()
    );
    if (ball_count == 0) g_runtime_catch_spin_coordinator.out_of_balls = true;
    if (ball_count > 0) g_runtime_catch_spin_coordinator.out_of_balls = false;
    if (binding.inventory_full_read_verified) {
        g_runtime_catch_spin_coordinator.item_inventory_full =
            runtime_item_bag_is_full(tick.context.il2cpp_api, binding);
    }

    prune_requested_catch_ids(
        &g_runtime_catch_spin_coordinator.requested_catch_ids,
        snapshot
    );

    const bool catch_enabled = config.auto_catch && config.catch_all;
    const auto *target = select_catch_target(
        snapshot,
        g_runtime_catch_spin_coordinator.requested_catch_ids
    );
    if (catch_enabled && target == nullptr && !snapshot.nearby.spawns.empty() &&
        !g_runtime_catch_spin_coordinator.requested_catch_ids.empty()) {
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "native catch_spin no new catch target nearby=%zu requested_memory=%zu",
            snapshot.nearby.spawns.size(),
            g_runtime_catch_spin_coordinator.requested_catch_ids.size()
        );
    }
    if (catch_enabled && target != nullptr && ball_count > 0 &&
        !g_runtime_catch_spin_coordinator.out_of_balls) {
        runtime_catch_spin_execute_catch(tick.context, binding, *target);
        return;
    }

    const bool force_spin_for_balls = catch_enabled && target != nullptr &&
        g_runtime_catch_spin_coordinator.out_of_balls;
    if (!config.auto_spin && !force_spin_for_balls) return;
    if (g_runtime_catch_spin_coordinator.item_inventory_full) {
        runtime_catch_spin_log_rejected("spin", "inventory_full");
        return;
    }
    double fort_distance_meters = 0.0;
    size_t in_range_forts = 0U;
    const long long epoch_now = now_epoch_millis();
    const uint64_t now_epoch_ms = epoch_now > 0
        ? static_cast<uint64_t>(epoch_now) : 0U;
    const auto *fort = select_spin_target(
        snapshot,
        &g_runtime_catch_spin_coordinator.fort_cooldown_deadlines,
        now_epoch_ms,
        &fort_distance_meters,
        &in_range_forts
    );
    if (fort == nullptr) {
        runtime_catch_spin_log_rejected(
            "spin",
            snapshot.has_player_position
                ? "no_available_fort_in_interaction_range"
                : "player_position_unavailable"
        );
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "native catch_spin spin filter forts=%zu in_range=%zu player=%d range_m=%.1f",
            snapshot.forts.forts.size(),
            in_range_forts,
            snapshot.has_player_position ? 1 : 0,
            kCatchSpinFortInteractionRangeMeters
        );
        return;
    }
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "native catch_spin spin target selected fort=%s distance_m=%.1f in_range=%zu range_m=%.1f",
        fort->fort_id.c_str(),
        fort_distance_meters,
        in_range_forts,
        kCatchSpinFortInteractionRangeMeters
    );
    runtime_catch_spin_execute_spin(tick.context, config, *fort);
}
