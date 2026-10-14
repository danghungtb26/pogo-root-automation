std::mutex g_runtime_bootstrap_status_mutex;
uint64_t g_runtime_bootstrap_status_sequence = 0U;

bool publish_runtime_bootstrap_status(
    ProbeContext &context,
    pogo_runtime_bootstrap::Status status
) {
    std::lock_guard<std::mutex> lock(g_runtime_bootstrap_status_mutex);
    const auto current = g_runtime_bootstrap_phase.load(std::memory_order_acquire);
    if (current == pogo_runtime_bootstrap::Phase::kFailed ||
        static_cast<uint32_t>(status.phase) <= static_cast<uint32_t>(current) ||
        (status.phase != pogo_runtime_bootstrap::Phase::kFailed &&
         static_cast<uint32_t>(status.phase) !=
            static_cast<uint32_t>(current) + 1U)) {
        return false;
    }

    // The companion owns the authoritative session ID. This internal identity
    // is bound to the process socket and replaced by the broker after validation.
    status.session_id = std::string("native-pending|") + context.process_name;
    status.pid = context.pid;
    status.process_name = context.process_name;
    status.sequence = g_runtime_bootstrap_status_sequence + 1U;
    status.observed_at_epoch_ms = static_cast<uint64_t>(now_epoch_millis());
    status.observed_at_elapsed_ns = static_cast<uint64_t>(now_elapsed_nanos());
    std::vector<uint8_t> payload;
    if (!pogo_runtime_bootstrap::encode(status, &payload) ||
        !send_internal_message(context.fd, kRuntimeBootstrapStatusMagic, payload)) {
        return false;
    }
    g_runtime_bootstrap_status_sequence = status.sequence;
    g_runtime_bootstrap_phase.store(status.phase, std::memory_order_release);
    return true;
}
