bool send_runtime_navigation_observation(
    int fd, BrokerContext &context, const std::vector<uint8_t> &navigation_payload,
    uint64_t epoch_ms, uint64_t elapsed_ns
) {
    if (navigation_payload.empty() || navigation_payload.size() > kBridgeHardMessageBytes) return false;
    const uint64_t seq = context.next_message_seq++;
    std::vector<uint8_t> payload;
    append_u32(&payload, 1U);
    append_string(&payload, context.runtime_session_id);
    append_u64(&payload, seq);
    append_u32(&payload, pogo_runtime::kNavigationObservationType);
    append_u32(&payload, pogo_runtime::kNavigationPayloadVersion);
    append_u32(&payload, static_cast<uint32_t>(navigation_payload.size()));
    payload.insert(payload.end(), navigation_payload.begin(), navigation_payload.end());
    append_u64(&payload, epoch_ms);
    append_u64(&payload, elapsed_ns);
    append_u32(&payload, static_cast<uint32_t>(context.pid));
    append_string(&payload, context.process_name);
    append_string(&payload, context.process_name);
    append_string(&payload, kVerifiedBuildFingerprint);
    append_optional_absent(&payload);
    append_optional_absent(&payload);
    append_optional_absent(&payload);  // Navigation is not a lifecycle observation.
    return send_bridge_frame(fd, 3U, seq, payload);
}
