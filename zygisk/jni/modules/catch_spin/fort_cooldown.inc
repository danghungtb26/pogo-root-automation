// Native fort cooldown state. The server response is the source of truth for
// the expiry; the local MapPokestop object is refreshed as a second layer so
// future map reads observe the same state as the game UI.

namespace {

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

void refresh_runtime_pokestop_cooldown(
    ProbeContext &context,
    const std::string &fort_id,
    int64_t cooldown_complete_ms
) {
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    const Il2CppApi &api = context.il2cpp_api;
    if (fort_id.empty() || cooldown_complete_ms <= 0 ||
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
        return;
    }
    void *pokestop = find_runtime_pokestop_by_id(context, fort_id);
    if (pokestop == nullptr) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native spin cooldown refresh skipped fort=%s reason=pokestop_not_found",
            fort_id.c_str()
        );
        return;
    }
    long cooldown_timestamp_ms = static_cast<long>(cooldown_complete_ms);
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
}

}  // namespace
