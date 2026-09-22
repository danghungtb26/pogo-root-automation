bool send_runtime_ui_status(ProbeContext &context);

std::atomic<uint64_t> g_runtime_applied_desired_revision{0U};
std::atomic<uint64_t> g_runtime_expired_desired_revision{0U};
extern std::atomic<bool> g_runtime_active;

struct RuntimeDesiredApplyResult {
    bool catch_spin_changed = false;
    bool discard_changed = false;
    bool transfer_changed = false;
};

RuntimeDesiredApplyResult apply_runtime_desired_config_mirrors(
    const pogo_runtime_desired_state::State &desired
) {
    const bool enabled = desired.enabled;
    RuntimeDesiredApplyResult result;
    std::lock_guard<std::recursive_mutex> applied_lock(g_runtime_applied_config_mutex);

    const bool catch_changed = !g_runtime_catch_spin_config.configured ||
        g_runtime_catch_spin_config.revision != desired.config_revision ||
        g_runtime_catch_spin_config.armed != (enabled && desired.catch_spin_armed) ||
        g_runtime_catch_spin_config.auto_catch != (enabled && desired.auto_catch) ||
        g_runtime_catch_spin_config.auto_spin != (enabled && desired.auto_spin) ||
        g_runtime_catch_spin_config.auto_encounter != (enabled && desired.auto_encounter) ||
        g_runtime_catch_spin_config.catch_all != desired.catch_all ||
        g_runtime_catch_spin_config.auto_walk_to_fort !=
            (enabled && desired.auto_walk_to_fort) ||
        g_runtime_catch_spin_config.spin_settle_delay_ms != desired.spin_settle_delay_ms ||
        g_runtime_catch_spin_config.catch_settle_delay_ms != desired.catch_settle_delay_ms;
    g_runtime_catch_spin_config.revision = desired.config_revision;
    g_runtime_catch_spin_config.configured = true;
    g_runtime_catch_spin_config.armed = enabled && desired.catch_spin_armed;
    g_runtime_catch_spin_config.auto_catch = enabled && desired.auto_catch;
    g_runtime_catch_spin_config.auto_spin = enabled && desired.auto_spin;
    g_runtime_catch_spin_config.auto_encounter = enabled && desired.auto_encounter;
    g_runtime_catch_spin_config.catch_all = desired.catch_all;
    g_runtime_catch_spin_config.auto_walk_to_fort = enabled && desired.auto_walk_to_fort;
    g_runtime_catch_spin_config.spin_settle_delay_ms = desired.spin_settle_delay_ms;
    g_runtime_catch_spin_config.catch_settle_delay_ms = desired.catch_settle_delay_ms;

    const bool discard_changed = !g_runtime_discard_config.configured ||
        g_runtime_discard_config.revision != desired.config_revision ||
        g_runtime_discard_config.auto_discard != (enabled && desired.auto_discard) ||
        g_runtime_discard_config.limit_count != desired.discard_limits.size();
    g_runtime_discard_config.revision = desired.config_revision;
    g_runtime_discard_config.configured = true;
    g_runtime_discard_config.auto_discard = enabled && desired.auto_discard;
    g_runtime_discard_config.limit_count = static_cast<uint32_t>(desired.discard_limits.size());
    for (uint32_t index = 0U; index < g_runtime_discard_config.limit_count; ++index) {
        const auto &limit = desired.discard_limits[index];
        const bool differs = g_runtime_discard_config.limits[index].item_id !=
                static_cast<int32_t>(limit.first) ||
            g_runtime_discard_config.limits[index].max_count !=
                static_cast<int32_t>(limit.second);
        if (differs) result.discard_changed = true;
        g_runtime_discard_config.limits[index] = pogo_runtime_discard_config::Limit{
            static_cast<int32_t>(limit.first),
            static_cast<int32_t>(limit.second),
        };
    }

    const bool transfer_changed = !g_runtime_transfer_config.configured ||
        g_runtime_transfer_config.revision != desired.config_revision ||
        g_runtime_transfer_config.auto_transfer != (enabled && desired.auto_transfer) ||
        g_runtime_transfer_config.keep_unknown_iv != desired.keep_unknown_iv ||
        g_runtime_transfer_config.minimum_iv_percent_to_keep !=
            desired.minimum_iv_percent_to_keep ||
        g_runtime_transfer_config.keep_shiny != desired.keep_shiny ||
        g_runtime_transfer_config.keep_hundo != desired.keep_hundo ||
        g_runtime_transfer_config.keep_special_background != desired.keep_special_background ||
        g_runtime_transfer_config.keep_favorite != desired.keep_favorite ||
        g_runtime_transfer_config.keep_legendary != desired.keep_legendary ||
        g_runtime_transfer_config.keep_mythical != desired.keep_mythical;
    g_runtime_transfer_config.revision = desired.config_revision;
    g_runtime_transfer_config.configured = true;
    g_runtime_transfer_config.auto_transfer = enabled && desired.auto_transfer;
    g_runtime_transfer_config.keep_unknown_iv = desired.keep_unknown_iv;
    g_runtime_transfer_config.minimum_iv_percent_to_keep = desired.minimum_iv_percent_to_keep;
    g_runtime_transfer_config.keep_shiny = desired.keep_shiny;
    g_runtime_transfer_config.keep_hundo = desired.keep_hundo;
    g_runtime_transfer_config.keep_special_background = desired.keep_special_background;
    g_runtime_transfer_config.keep_favorite = desired.keep_favorite;
    g_runtime_transfer_config.keep_legendary = desired.keep_legendary;
    g_runtime_transfer_config.keep_mythical = desired.keep_mythical;

    result.catch_spin_changed = catch_changed;
    result.discard_changed = result.discard_changed || discard_changed;
    result.transfer_changed = transfer_changed;
    return result;
}

void invalidate_runtime_desired_apply() {
    g_runtime_applied_desired_revision.store(0U, std::memory_order_release);
    g_runtime_expired_desired_revision.store(0U, std::memory_order_release);
}

void reconcile_runtime_desired_state(ProbeContext &context) {
    if (!g_runtime_active.load(std::memory_order_acquire)) return;

    RuntimeDesiredStateSnapshot snapshot;
    if (!snapshot_runtime_desired_state(&snapshot)) return;
    const uint64_t now = static_cast<uint64_t>(now_elapsed_nanos());
    if (snapshot.state.config_revision == 0U || snapshot.expires_at_ns == 0U) return;
    if (snapshot.expires_at_ns <= now) {
        const uint64_t expired_revision =
            g_runtime_expired_desired_revision.load(std::memory_order_acquire);
        if (expired_revision == snapshot.state.config_revision) return;
        pogo_runtime_module::disable_all_modules(context);
        reset_runtime_catch_spin_config();
        reset_runtime_transfer_config();
        reset_runtime_discard_config();
        g_runtime_applied_desired_revision.store(0U, std::memory_order_release);
        g_runtime_expired_desired_revision.store(
            snapshot.state.config_revision,
            std::memory_order_release
        );
        (void)send_runtime_ui_status(context);
        return;
    }
    g_runtime_expired_desired_revision.store(0U, std::memory_order_release);

    const uint64_t applied_revision =
        g_runtime_applied_desired_revision.load(std::memory_order_acquire);
    if (!snapshot.state.enabled) {
        if (applied_revision == snapshot.state.config_revision) return;
        pogo_runtime_module::disable_all_modules(context);
        (void)apply_runtime_desired_config_mirrors(snapshot.state);
        g_runtime_applied_desired_revision.store(
            snapshot.state.config_revision,
            std::memory_order_release
        );
        (void)send_runtime_ui_status(context);
        return;
    }

    const RuntimeBinding binding = runtime_binding_snapshot(context);
    if (!binding.exact_build_verified || !pogo_runtime_module::map_runtime_ready(context)) {
        return;
    }
    if (applied_revision == snapshot.state.config_revision) {
        pogo_runtime_module::sync_auto_enabled_modules(context);
        return;
    }

    const RuntimeDesiredApplyResult result =
        apply_runtime_desired_config_mirrors(snapshot.state);
    g_runtime_applied_desired_revision.store(
        snapshot.state.config_revision,
        std::memory_order_release
    );
    if (result.catch_spin_changed) reset_runtime_catch_spin_coordinator();
    if (result.discard_changed) reset_runtime_discard_state(context);
    pogo_runtime_module::sync_auto_enabled_modules(context);
    (void)send_runtime_ui_status(context);
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "desired state reconciled revision=%llu catch=%d discard=%d transfer=%d",
        static_cast<unsigned long long>(snapshot.state.config_revision),
        result.catch_spin_changed ? 1 : 0,
        result.discard_changed ? 1 : 0,
        result.transfer_changed ? 1 : 0
    );
}
