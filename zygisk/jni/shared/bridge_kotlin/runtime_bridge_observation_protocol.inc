bool send_runtime_encounter_observation(
    int fd,
    BrokerContext &context,
    const std::vector<uint8_t> &encounter_payload,
    uint64_t observed_at_epoch_ms,
    uint64_t observed_at_elapsed_ns
) {
    if (encounter_payload.empty() || encounter_payload.size() > kBridgeHardMessageBytes) return false;
    const uint64_t message_seq = context.next_message_seq++;
    std::vector<uint8_t> payload;
    append_u32(&payload, 1U);
    append_string(&payload, context.runtime_session_id);
    append_u64(&payload, message_seq);
    append_u32(&payload, pogo_runtime::kEncounterObservationType);
    append_u32(&payload, pogo_runtime::kRuntimeEncounterPayloadVersion);
    append_u32(&payload, static_cast<uint32_t>(encounter_payload.size()));
    payload.insert(payload.end(), encounter_payload.begin(), encounter_payload.end());
    append_u64(&payload, observed_at_epoch_ms);
    append_u64(&payload, observed_at_elapsed_ns);
    append_u32(&payload, static_cast<uint32_t>(context.pid));
    append_string(&payload, context.process_name);
    append_string(&payload, context.process_name);
    append_string(&payload, kVerifiedBuildFingerprint);
    append_optional_absent(&payload);
    append_optional_absent(&payload);
    payload.push_back(1U);
    append_u32(&payload, 5U);
    return send_bridge_frame(fd, 3U, message_seq, payload);
}

bool send_runtime_nearby_observation(
    int fd,
    BrokerContext &context,
    const std::vector<uint8_t> &nearby_payload,
    uint64_t observed_at_epoch_ms,
    uint64_t observed_at_elapsed_ns
) {
    if (nearby_payload.empty() || nearby_payload.size() > kBridgeHardMessageBytes) return false;
    const uint64_t message_seq = context.next_message_seq++;
    std::vector<uint8_t> payload;
    append_u32(&payload, 1U);
    append_string(&payload, context.runtime_session_id);
    append_u64(&payload, message_seq);
    append_u32(&payload, 2U);
    append_u32(&payload, pogo_runtime::kRuntimeNearbyPayloadVersion);
    append_u32(&payload, static_cast<uint32_t>(nearby_payload.size()));
    payload.insert(payload.end(), nearby_payload.begin(), nearby_payload.end());
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
    return send_bridge_frame(fd, 3U, message_seq, payload);
}

bool send_runtime_forts_observation(
    int fd,
    BrokerContext &context,
    const std::vector<uint8_t> &forts_payload,
    uint64_t observed_at_epoch_ms,
    uint64_t observed_at_elapsed_ns
) {
    if (forts_payload.empty() || forts_payload.size() > kBridgeHardMessageBytes) return false;
    const uint64_t message_seq = context.next_message_seq++;
    std::vector<uint8_t> payload;
    append_u32(&payload, 1U);
    append_string(&payload, context.runtime_session_id);
    append_u64(&payload, message_seq);
    append_u32(&payload, pogo_runtime::kFortsObservationType);
    append_u32(&payload, pogo_runtime::kRuntimeFortsPayloadVersion);
    append_u32(&payload, static_cast<uint32_t>(forts_payload.size()));
    payload.insert(payload.end(), forts_payload.begin(), forts_payload.end());
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
    return send_bridge_frame(fd, 3U, message_seq, payload);
}

bool send_runtime_inventory_observation(
    int fd,
    BrokerContext &context,
    const std::vector<uint8_t> &inventory_payload,
    uint64_t observed_at_epoch_ms,
    uint64_t observed_at_elapsed_ns
) {
    if (inventory_payload.empty() || inventory_payload.size() > kBridgeHardMessageBytes) return false;
    const uint64_t message_seq = context.next_message_seq++;
    std::vector<uint8_t> payload;
    append_u32(&payload, 1U);
    append_string(&payload, context.runtime_session_id);
    append_u64(&payload, message_seq);
    append_u32(&payload, pogo_runtime::kInventoryObservationType);
    append_u32(&payload, pogo_runtime::kRuntimeInventoryPayloadVersion);
    append_u32(&payload, static_cast<uint32_t>(inventory_payload.size()));
    payload.insert(payload.end(), inventory_payload.begin(), inventory_payload.end());
    append_u64(&payload, observed_at_epoch_ms);
    append_u64(&payload, observed_at_elapsed_ns);
    append_u32(&payload, static_cast<uint32_t>(context.pid));
    append_string(&payload, context.process_name);
    append_string(&payload, context.process_name);
    append_string(&payload, kVerifiedBuildFingerprint);
    append_optional_absent(&payload);
    append_optional_absent(&payload);
    payload.push_back(1U);
    append_u32(&payload, 5U);
    return send_bridge_frame(fd, 3U, message_seq, payload);
}

bool send_runtime_lifecycle_observation(
    int fd,
    BrokerContext &context,
    const std::vector<uint8_t> &lifecycle_payload,
    uint32_t lifecycle_wire,
    uint64_t observed_at_epoch_ms,
    uint64_t observed_at_elapsed_ns
) {
    const uint64_t message_seq = context.next_message_seq++;
    std::vector<uint8_t> payload;
    append_u32(&payload, 1U);
    append_string(&payload, context.runtime_session_id);
    append_u64(&payload, message_seq);
    append_u32(&payload, pogo_runtime::kLifecycleObservationType);
    append_u32(&payload, pogo_runtime::kLifecyclePayloadVersion);
    append_u32(&payload, static_cast<uint32_t>(lifecycle_payload.size()));
    payload.insert(payload.end(), lifecycle_payload.begin(), lifecycle_payload.end());
    append_u64(&payload, observed_at_epoch_ms);
    append_u64(&payload, observed_at_elapsed_ns);
    append_u32(&payload, static_cast<uint32_t>(context.pid));
    append_string(&payload, context.process_name);
    append_string(&payload, context.process_name);
    append_string(&payload, kVerifiedBuildFingerprint);
    append_optional_absent(&payload);
    append_optional_absent(&payload);
    payload.push_back(1U);
    append_u32(&payload, lifecycle_wire);
    return send_bridge_frame(fd, 3U, message_seq, payload);
}

bool parse_runtime_observation_envelope(
    const std::vector<uint8_t> &envelope,
    uint32_t *observation_type,
    uint32_t *payload_version,
    std::vector<uint8_t> *payload,
    uint64_t *observed_at_epoch_ms,
    uint64_t *observed_at_elapsed_ns
) {
    size_t offset = 0U;
    uint32_t envelope_version = 0U;
    uint32_t parsed_observation_type = 0U;
    uint32_t parsed_payload_version = 0U;
    uint32_t payload_size = 0U;
    if (observation_type == nullptr || payload_version == nullptr || payload == nullptr ||
        observed_at_epoch_ms == nullptr || observed_at_elapsed_ns == nullptr ||
        !read_be32(envelope, &offset, &envelope_version) ||
        !read_be32(envelope, &offset, &parsed_observation_type) ||
        !read_be32(envelope, &offset, &parsed_payload_version) ||
        !read_be32(envelope, &offset, &payload_size) ||
        envelope_version != pogo_runtime::kRuntimeObservationEnvelopeVersion ||
        payload_size > kBridgeHardMessageBytes || offset > envelope.size() ||
        envelope.size() - offset < payload_size) return false;

    payload->assign(envelope.data() + offset, envelope.data() + offset + payload_size);
    offset += payload_size;
    if (!read_be64(envelope, &offset, observed_at_epoch_ms) ||
        !read_be64(envelope, &offset, observed_at_elapsed_ns) || offset != envelope.size()) {
        return false;
    }
    *observation_type = parsed_observation_type;
    *payload_version = parsed_payload_version;
    return true;
}

bool send_map_target_observation(
    int fd,
    BrokerContext &context,
    const std::vector<uint8_t> &map_target_payload,
    uint64_t observed_at_epoch_ms,
    uint64_t observed_at_elapsed_ns
) {
    if (map_target_payload.empty() || map_target_payload.size() > kBridgeHardMessageBytes) return false;
    const uint64_t message_seq = context.next_message_seq++;
    std::vector<uint8_t> payload;
    append_u32(&payload, 1U);
    append_string(&payload, context.runtime_session_id);
    append_u64(&payload, message_seq);
    append_u32(&payload, pogo_runtime::kMapTargetObservationType);
    append_u32(&payload, pogo_runtime::kMapTargetPayloadVersion);
    append_u32(&payload, static_cast<uint32_t>(map_target_payload.size()));
    payload.insert(payload.end(), map_target_payload.begin(), map_target_payload.end());
    append_u64(&payload, observed_at_epoch_ms);
    append_u64(&payload, observed_at_elapsed_ns);
    append_u32(&payload, static_cast<uint32_t>(context.pid));
    append_string(&payload, context.process_name);
    append_string(&payload, context.process_name);
    char fingerprint[192]{};
    snprintf(fingerprint, sizeof(fingerprint), "unverified|%s", context.process_name);
    append_string(&payload, fingerprint);
    append_optional_absent(&payload);  // player latitude
    append_optional_absent(&payload);  // player longitude
    append_optional_absent(&payload);  // lifecycle
    return send_bridge_frame(fd, 3U, message_seq, payload);
}

bool parse_runtime_result(
    const std::vector<uint8_t> &payload,
    std::string *command_id,
    uint32_t *phase,
    std::string *error_code,
    std::string *message,
    uint32_t *catch_outcome_wire = nullptr,
    std::string *captured_pokemon_id = nullptr
) {
    size_t offset = 0U;
    uint32_t version = 0U;
    if (command_id == nullptr || phase == nullptr || error_code == nullptr || message == nullptr ||
        !read_be32(payload, &offset, &version) || version != 1U ||
        !read_string(payload, &offset, command_id) ||
        !read_be32(payload, &offset, phase) ||
        !read_string(payload, &offset, error_code) ||
        !read_string(payload, &offset, message)) return false;
    if (catch_outcome_wire != nullptr) *catch_outcome_wire = 0U;
    if (captured_pokemon_id != nullptr) captured_pokemon_id->clear();
    // Optional trailing block: presence byte for the catch outcome (+u32 when
    // set) followed by a presence byte for the captured pokemon id (+string when
    // set). Written by send_runtime_result only when at least one is present.
    if (offset < payload.size()) {
        const uint8_t has_catch = payload[offset++];
        if (has_catch > 1U) return false;
        if (has_catch == 1U) {
            if (catch_outcome_wire == nullptr || !read_be32(payload, &offset, catch_outcome_wire) ||
                *catch_outcome_wire == 0U || *catch_outcome_wire > 6U) return false;
        }
        if (offset >= payload.size()) return false;
        const uint8_t has_captured = payload[offset++];
        if (has_captured > 1U) return false;
        if (has_captured == 1U) {
            std::string id;
            if (!read_string(payload, &offset, &id) || id.empty()) return false;
            if (captured_pokemon_id != nullptr) *captured_pokemon_id = std::move(id);
        }
    }
    return offset == payload.size();
}

bool parse_runtime_capabilities(
    const std::vector<uint8_t> &payload,
    std::string *fingerprint,
    std::vector<std::string> *capabilities,
    bool *strong_identity
) {
    size_t offset = 0U;
    uint32_t version = 0U;
    uint32_t capability_count = 0U;
    if (fingerprint == nullptr || capabilities == nullptr || strong_identity == nullptr ||
        !read_be32(payload, &offset, &version) || version != 1U ||
        !read_string(payload, &offset, fingerprint) ||
        !read_be32(payload, &offset, &capability_count) || capability_count > 64U) return false;
    capabilities->clear();
    for (uint32_t index = 0U; index < capability_count; ++index) {
        std::string capability;
        if (!read_string(payload, &offset, &capability) || capability.empty()) return false;
        capabilities->push_back(std::move(capability));
    }
    if (offset >= payload.size()) return false;
    *strong_identity = payload[offset++] != 0U;
    return offset == payload.size() && !fingerprint->empty();
}
