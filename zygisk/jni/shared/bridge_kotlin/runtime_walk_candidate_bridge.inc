bool send_runtime_walk_candidate_observation(
    int fd,
    BrokerContext &context,
    const std::vector<uint8_t> &candidate_payload,
    uint64_t epoch_ms,
    uint64_t elapsed_ns
) {
    if (candidate_payload.empty() || candidate_payload.size() > kBridgeHardMessageBytes) return false;
    const uint64_t sequence = context.next_message_seq++;
    std::vector<uint8_t> payload;
    append_u32(&payload, 1U);
    append_string(&payload, context.runtime_session_id);
    append_u64(&payload, sequence);
    append_u32(&payload, pogo_runtime::kPointWalkCandidateObservationType);
    append_u32(&payload, pogo_runtime::kPointWalkCandidatePayloadVersion);
    append_u32(&payload, static_cast<uint32_t>(candidate_payload.size()));
    payload.insert(payload.end(), candidate_payload.begin(), candidate_payload.end());
    append_u64(&payload, epoch_ms);
    append_u64(&payload, elapsed_ns);
    append_u32(&payload, static_cast<uint32_t>(context.pid));
    append_string(&payload, context.process_name);
    append_string(&payload, context.process_name);
    append_string(&payload, kVerifiedBuildFingerprint);
    append_optional_absent(&payload);
    append_optional_absent(&payload);
    append_optional_absent(&payload);
    return send_bridge_frame(fd, 3U, sequence, payload);
}
