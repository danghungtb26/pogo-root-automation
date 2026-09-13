namespace {
bool runtime_managed_configs_ready() {
    const RuntimeCatchSpinConfig catch_spin = runtime_catch_spin_config_snapshot();
    const RuntimeDiscardConfig discard = runtime_discard_config_snapshot();
    const RuntimeTransferConfig transfer = runtime_transfer_config_snapshot();
    if (!catch_spin.configured || !discard.configured || !transfer.configured) {
        return false;
    }
    const uint64_t revision = catch_spin.revision;
    return revision > 0U &&
        discard.revision == revision &&
        transfer.revision == revision;
}

void maybe_log_managed_config_gate() {
    static std::atomic<uint64_t> last_log_ns{0U};
    const uint64_t now = static_cast<uint64_t>(now_elapsed_nanos());
    constexpr uint64_t kIntervalNs = 5'000'000'000ULL;
    const uint64_t last = last_log_ns.load(std::memory_order_relaxed);
    if (last != 0U && now - last < kIntervalNs) return;
    last_log_ns.store(now, std::memory_order_relaxed);
    const RuntimeCatchSpinConfig catch_spin = runtime_catch_spin_config_snapshot();
    const RuntimeDiscardConfig discard = runtime_discard_config_snapshot();
    const RuntimeTransferConfig transfer = runtime_transfer_config_snapshot();
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "auto module sync deferred reason=managed_config_pending catch=%d transfer=%d discard=%d revision=%llu/%llu/%llu",
        catch_spin.configured ? 1 : 0,
        transfer.configured ? 1 : 0,
        discard.configured ? 1 : 0,
        static_cast<unsigned long long>(catch_spin.revision),
        static_cast<unsigned long long>(transfer.revision),
        static_cast<unsigned long long>(discard.revision)
    );
}
}  // namespace
