// Native fort cooldown state. The server response is the source of truth for
// the expiry; the local MapPokestop object is refreshed as a second layer so
// future map reads observe the same state as the game UI.

extern std::atomic<bool> g_runtime_active;
namespace {
RuntimeFortRefreshSchedule g_runtime_fort_refresh; // Unity main-thread only.

using RuntimeFortCooldownDeadlines = std::unordered_map<std::string, uint64_t>;

bool runtime_fort_cooldown_active(
    RuntimeFortCooldownDeadlines *deadlines,
    const std::string &fort_id,
    uint64_t now_epoch_ms,
    uint64_t *remaining_ms
) {
    if (remaining_ms != nullptr) *remaining_ms = 0U;
    if (deadlines == nullptr || fort_id.empty()) return false;
    const auto found = deadlines->find(fort_id);
    if (found == deadlines->end()) return false;
    if (now_epoch_ms >= found->second) {
        deadlines->erase(found);
        return false;
    }
    if (remaining_ms != nullptr) *remaining_ms = found->second - now_epoch_ms;
    return true;
}

void remember_runtime_fort_cooldown(
    RuntimeFortCooldownDeadlines *deadlines,
    const std::string &fort_id,
    int64_t cooldown_complete_ms,
    uint64_t now_epoch_ms
) {
    if (deadlines == nullptr || fort_id.empty() || cooldown_complete_ms <= 0) return;
    const uint64_t deadline = static_cast<uint64_t>(cooldown_complete_ms);
    if (deadline <= now_epoch_ms) {
        deadlines->erase(fort_id);
        return;
    }
    (*deadlines)[fort_id] = deadline;
}

bool refresh_runtime_pokestop_cooldown(
    ProbeContext &context,
    const std::string &fort_id,
    int64_t cooldown_complete_ms
) {
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    const Il2CppApi &api = context.il2cpp_api;
    if (!binding.exact_build_verified || fort_id.empty() || cooldown_complete_ms <= 0 ||
        binding.map_pokestop_set_cooldown_time == nullptr ||
        api.runtime_invoke == nullptr) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native spin cooldown refresh skipped fort=%s complete_ms=%lld setter=%p",
            fort_id.c_str(),
            static_cast<long long>(cooldown_complete_ms),
            binding.map_pokestop_set_cooldown_time
        );
        return false;
    }
    void *pokestop = find_runtime_pokestop_by_id(context, fort_id);
    if (pokestop == nullptr) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native spin cooldown refresh skipped fort=%s reason=pokestop_not_found",
            fort_id.c_str()
        );
        return false;
    }
    int64_t cooldown_timestamp_ms = cooldown_complete_ms;
    void *arguments[] = {&cooldown_timestamp_ms};
    void *exception = nullptr;
    api.runtime_invoke(
        binding.map_pokestop_set_cooldown_time,
        pokestop,
        arguments,
        &exception
    );
    __android_log_print(
        exception == nullptr ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "native spin cooldown refresh fort=%s complete_ms=%lld setter=%p exception=%p",
        fort_id.c_str(),
        static_cast<long long>(cooldown_complete_ms),
        binding.map_pokestop_set_cooldown_time,
        exception
    );
    return exception == nullptr;
}

void maintain_runtime_map_after_actions(ProbeContext &context) {
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    if (!binding.exact_build_verified || !g_runtime_active.load()) return;
    const uint64_t now = static_cast<uint64_t>(now_epoch_millis());
    retry_runtime_map_refresh(context, now);
    for (const auto &due : g_runtime_fort_refresh.due(now)) {
        // The map may already carry a newer cooldown (e.g. a manual spin).
        // Never clear it using an older response from automation.
        void *pokestop = find_runtime_pokestop_by_id(context, due.id);
        void *proto = nullptr;
        int64_t live_deadline = 0;
        if (pokestop == nullptr ||
            !read_runtime_field(context.il2cpp_api, pokestop, "<FortData>k__BackingField", &proto) ||
            !runtime_object_is_exact_class(context.il2cpp_api, proto, "Holoholo.Rpc", "PokemonFortProto") ||
            !read_runtime_field(context.il2cpp_api, proto, "CooldownCompleteMs", &live_deadline)) continue;
        if (live_deadline > 0 && static_cast<uint64_t>(live_deadline) > now) {
            g_runtime_fort_refresh.schedule(due.id, static_cast<uint64_t>(live_deadline));
            continue;
        }
        // Replay the expired server timestamp through the game's setter so
        // its visual cooldown can finish. IsCoolingDown remains the spin gate.
        if (refresh_runtime_pokestop_cooldown(context, due.id, static_cast<int64_t>(due.deadline)) &&
            refresh_runtime_map_after_action(context, "fort cooldown expired")) {
            g_runtime_fort_refresh.completed(due);
        }
    }
}

}  // namespace
