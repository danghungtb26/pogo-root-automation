bool send_runtime_automation_event_observation(
    int fd,
    BrokerContext &context,
    const std::vector<uint8_t> &event_payload,
    uint64_t observed_at_epoch_ms,
    uint64_t observed_at_elapsed_ns
) {
    if (event_payload.empty() || event_payload.size() > kBridgeHardMessageBytes) return false;
    const uint64_t message_seq = context.next_message_seq++;
    std::vector<uint8_t> payload;
    append_u32(&payload, 1U);
    append_string(&payload, context.runtime_session_id);
    append_u64(&payload, message_seq);
    append_u32(&payload, pogo_runtime::kAutomationEventObservationType);
    append_u32(&payload, pogo_runtime::kRuntimeAutomationEventPayloadVersion);
    append_u32(&payload, static_cast<uint32_t>(event_payload.size()));
    payload.insert(payload.end(), event_payload.begin(), event_payload.end());
    append_u64(&payload, observed_at_epoch_ms);
    append_u64(&payload, observed_at_elapsed_ns);
    append_u32(&payload, static_cast<uint32_t>(context.pid));
    append_string(&payload, context.process_name);
    append_string(&payload, context.process_name);
    append_string(&payload, kVerifiedBuildFingerprint);
    append_optional_absent(&payload);
    append_optional_absent(&payload);
    payload.push_back(1U);
    append_u32(&payload, 4U);
    const bool sent = send_bridge_frame(fd, 3U, message_seq, payload);
    __android_log_print(
        sent ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "bridge automation event forwarded seq=%llu bytes=%zu sent=%d",
        static_cast<unsigned long long>(message_seq),
        event_payload.size(),
        sent ? 1 : 0
    );
    return sent;
}
