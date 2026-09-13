// Native event boundary between catch-spin and transfer. The catch module only
// reports an authoritative captured storage id; transfer owns all policy and
// release decisions from this point onward.

namespace pogo_runtime_module {
bool auto_transfer_module_enabled();
}

void on_runtime_catch_succeeded_on_main_thread(
    ProbeContext &context,
    uint64_t captured_pokemon_id
) {
    if (captured_pokemon_id == 0U) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native auto transfer skipped reason=captured_id_missing"
        );
        return;
    }
    const RuntimeTransferConfig config = runtime_transfer_config_snapshot();
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "native auto transfer trigger id=%llu configured=%d auto=%d revision=%llu",
        static_cast<unsigned long long>(captured_pokemon_id),
        config.configured ? 1 : 0,
        config.auto_transfer ? 1 : 0,
        static_cast<unsigned long long>(config.revision)
    );
    if (!config.configured || !config.auto_transfer ||
        !pogo_runtime_module::auto_transfer_module_enabled()) {
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "native auto transfer skipped id=%llu reason=disabled_or_config_missing",
            static_cast<unsigned long long>(captured_pokemon_id)
        );
        return;
    }
    if (runtime_transfer_blocks_catch_spin()) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native auto transfer skipped id=%llu reason=transfer_in_flight_or_blocked",
            static_cast<unsigned long long>(captured_pokemon_id)
        );
        return;
    }
    const MainThreadActionOutcome outcome = start_runtime_transfer_on_main_thread(
        context,
        captured_pokemon_id,
        ""
    );
    __android_log_print(
        outcome == MainThreadActionOutcome::kInvoked ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "native auto transfer start id=%llu outcome=%u",
        static_cast<unsigned long long>(captured_pokemon_id),
        static_cast<unsigned>(outcome)
    );
}
