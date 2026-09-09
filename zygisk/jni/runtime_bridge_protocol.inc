void append_optional_absent(std::vector<uint8_t> *output) {
    output->push_back(0U);
}

enum class CompanionMessageType {
    kRuntimeEvent,
    kRuntimeResult,
    kRuntimeCapabilities,
    kRuntimeObservation,
};

bool read_length_prefixed_payload(int fd, std::vector<uint8_t> *payload) {
    uint8_t length_bytes[4]{};
    if (payload == nullptr || !read_full(fd, length_bytes, sizeof(length_bytes))) return false;
    const uint32_t length = (static_cast<uint32_t>(length_bytes[0]) << 24U) |
        (static_cast<uint32_t>(length_bytes[1]) << 16U) |
        (static_cast<uint32_t>(length_bytes[2]) << 8U) |
        static_cast<uint32_t>(length_bytes[3]);
    if (length > kBridgeHardMessageBytes) return false;
    payload->resize(length);
    return length == 0U || read_full(fd, payload->data(), payload->size());
}

bool read_companion_message(
    int fd,
    CompanionMessageType *message_type,
    RuntimeEvent *runtime_event,
    std::vector<uint8_t> *runtime_result,
    std::vector<uint8_t> *runtime_capabilities,
    std::vector<uint8_t> *runtime_observation
) {
    uint8_t magic_bytes[4]{};
    if (message_type == nullptr || !read_full(fd, magic_bytes, sizeof(magic_bytes))) return false;
    const uint32_t magic = (static_cast<uint32_t>(magic_bytes[0]) << 24U) |
        (static_cast<uint32_t>(magic_bytes[1]) << 16U) |
        (static_cast<uint32_t>(magic_bytes[2]) << 8U) |
        static_cast<uint32_t>(magic_bytes[3]);
    if (magic == kRuntimeEventMagic) {
        if (runtime_event == nullptr) return false;
        memset(runtime_event, 0, sizeof(*runtime_event));
        memcpy(runtime_event, magic_bytes, sizeof(magic_bytes));
        if (!read_full(
                fd,
                reinterpret_cast<uint8_t *>(runtime_event) + sizeof(magic_bytes),
                sizeof(*runtime_event) - sizeof(magic_bytes)
            )) return false;
        *message_type = CompanionMessageType::kRuntimeEvent;
        return true;
    }
    if (magic == kRuntimeResultMagic) {
        if (!read_length_prefixed_payload(fd, runtime_result)) return false;
        *message_type = CompanionMessageType::kRuntimeResult;
        return true;
    }
    if (magic == kRuntimeCapabilitiesMagic) {
        if (!read_length_prefixed_payload(fd, runtime_capabilities)) return false;
        *message_type = CompanionMessageType::kRuntimeCapabilities;
        return true;
    }
    if (magic == pogo_runtime::kRuntimeObservationMagic) {
        if (!read_length_prefixed_payload(fd, runtime_observation)) return false;
        *message_type = CompanionMessageType::kRuntimeObservation;
        return true;
    }
    return false;
}

bool broker_is_current(const char *runtime_session_id) {
    std::lock_guard<std::mutex> lock(g_broker_mutex);
    return runtime_session_id != nullptr &&
        g_broker_running &&
        g_broker_session_id == runtime_session_id;
}

bool send_runtime_ready_with_values(
    int fd,
    BrokerContext &context,
    const char *fingerprint,
    const std::vector<std::string> &capabilities,
    bool strong_identity
) {
    const uint64_t message_seq = context.next_message_seq++;
    std::vector<uint8_t> payload;
    append_u32(&payload, 1U);
    append_string(&payload, context.runtime_session_id);
    append_u64(&payload, message_seq);
    append_u32(&payload, static_cast<uint32_t>(context.pid));
    append_string(&payload, context.process_name);
    append_string(&payload, context.process_name);

    append_string(&payload, fingerprint);
    append_u32(&payload, static_cast<uint32_t>(capabilities.size()));
    for (const std::string &capability : capabilities) append_string(&payload, capability.c_str());
    append_optional_absent(&payload);  // version name
    append_optional_absent(&payload);  // version code
    append_u64(&payload, static_cast<uint64_t>(now_epoch_millis()));
    append_u64(&payload, static_cast<uint64_t>(now_elapsed_nanos()));
    payload.push_back(strong_identity ? 1U : 0U);
    return send_bridge_frame(fd, 2U, message_seq, payload);
}

bool send_runtime_ready(int fd, BrokerContext &context) {
    char fingerprint[192]{};
    snprintf(fingerprint, sizeof(fingerprint), "unverified|%s", context.process_name);
    return send_runtime_ready_with_values(fd, context, fingerprint, {}, false);
}

bool send_command_result(
    int fd,
    BrokerContext &context,
    const char *command_id,
    uint32_t phase,
    const char *error_code,
    const char *message
) {
    if (command_id == nullptr || command_id[0] == '\0') return false;
    const uint64_t message_seq = context.next_message_seq++;
    std::vector<uint8_t> payload;
    append_u32(&payload, 1U);
    append_string(&payload, context.runtime_session_id);
    append_u64(&payload, message_seq);
    append_string(&payload, command_id);
    append_u32(&payload, phase);
    append_nullable_string(&payload, error_code);
    append_nullable_string(&payload, message);
    append_u64(&payload, static_cast<uint64_t>(now_epoch_millis()));
    append_u64(&payload, static_cast<uint64_t>(now_elapsed_nanos()));
    const bool sent = send_bridge_frame(fd, kBridgeCommandResultType, message_seq, payload);
    __android_log_print(
        sent ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "runtime result forwarded command=%s phase=%u seq=%llu sent=%d",
        command_id,
        phase,
        static_cast<unsigned long long>(message_seq),
        sent ? 1 : 0
    );
    return sent;
}

bool send_runtime_command(int fd, uint16_t message_type, uint64_t message_seq, const std::vector<uint8_t> &payload) {
    std::vector<uint8_t> command;
    command.reserve(12U + payload.size());
    append_u32(&command, static_cast<uint32_t>(message_type));
    append_u64(&command, message_seq);
    command.insert(command.end(), payload.begin(), payload.end());
    return send_internal_message(fd, kRuntimeCommandMagic, command);
}

[[maybe_unused]] bool send_runtime_map_target_observation(
    int fd,
    const pogo_runtime::MapTargetObservation &observation
) {
    std::vector<uint8_t> envelope;
    if (!pogo_runtime::encode_runtime_map_target_observation(
            observation,
            static_cast<uint64_t>(now_epoch_millis()),
            static_cast<uint64_t>(now_elapsed_nanos()),
            &envelope
        )) return false;
    return send_internal_message(fd, pogo_runtime::kRuntimeObservationMagic, envelope);
}

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
    std::string *message
) {
    size_t offset = 0U;
    uint32_t version = 0U;
    if (command_id == nullptr || phase == nullptr || error_code == nullptr || message == nullptr ||
        !read_be32(payload, &offset, &version) || version != 1U ||
        !read_string(payload, &offset, command_id) ||
        !read_be32(payload, &offset, phase) ||
        !read_string(payload, &offset, error_code) ||
        !read_string(payload, &offset, message)) return false;
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
