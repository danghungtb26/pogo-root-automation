void append_optional_absent(std::vector<uint8_t> *output) {
    output->push_back(0U);
}

enum class CompanionMessageType {
    kRuntimeEvent,
    kRuntimeResult,
    kRuntimeCapabilities,
    kRuntimeUiStatus,
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
    std::vector<uint8_t> *runtime_ui_status,
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
    if (magic == kRuntimeUiStatusMagic) {
        if (!read_length_prefixed_payload(fd, runtime_ui_status)) return false;
        *message_type = CompanionMessageType::kRuntimeUiStatus;
        return true;
    }
    if (magic == pogo_runtime::kRuntimeObservationMagic) {
        if (!read_length_prefixed_payload(fd, runtime_observation)) return false;
        *message_type = CompanionMessageType::kRuntimeObservation;
        return true;
    }
    return false;
}

bool rewrite_runtime_ui_status_session(
    const std::vector<uint8_t> &input,
    const char *runtime_session_id,
    std::vector<uint8_t> *output
) {
    if (runtime_session_id == nullptr || runtime_session_id[0] == '\0' || output == nullptr) {
        return false;
    }
    const std::string session(runtime_session_id);
    if (session.size() > pogo_runtime_ui_status::kMaxLogicalIdBytes) return false;

    size_t offset = 0U;
    uint32_t payload_version = 0U;
    uint32_t marker = 0U;
    uint32_t schema_version = 0U;
    uint32_t placeholder_length = 0U;
    if (!read_be32(input, &offset, &payload_version) ||
        !read_be32(input, &offset, &marker) ||
        !read_be32(input, &offset, &schema_version) ||
        !read_be32(input, &offset, &placeholder_length) ||
        payload_version != pogo_runtime_ui_status::kPayloadVersion ||
        marker != pogo_runtime_ui_status::kMarker ||
        schema_version != pogo_runtime_ui_status::kSchemaVersion ||
        placeholder_length == 0U ||
        placeholder_length > pogo_runtime_ui_status::kMaxLogicalIdBytes ||
        offset > input.size() || input.size() - offset < placeholder_length) return false;

    output->clear();
    output->reserve(input.size() - placeholder_length + session.size());
    output->insert(output->end(), input.begin(), input.begin() + 12U);
    append_u32(output, static_cast<uint32_t>(session.size()));
    output->insert(output->end(), session.begin(), session.end());
    output->insert(
        output->end(),
        input.begin() + offset + placeholder_length,
        input.end()
    );
    return output->size() <= kBridgeHardMessageBytes;
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
    const char *message,
    uint32_t catch_outcome_wire = 0U,
    const char *captured_pokemon_id = nullptr
) {
    if (command_id == nullptr || command_id[0] == '\0') return false;
    if (catch_outcome_wire > 6U) return false;
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
    // Matches BridgePayloadDecoder.readCommandResult: nullable catch outcome,
    // then (when a captured id follows) an ext-block marker set to 0 (no
    // throw/snapshot) and the nullable captured pokemon id.
    const bool has_captured =
        captured_pokemon_id != nullptr && captured_pokemon_id[0] != '\0';
    if (catch_outcome_wire != 0U) {
        payload.push_back(1U);
        append_u32(&payload, catch_outcome_wire);
    } else if (has_captured) {
        payload.push_back(0U);  // catch outcome absent, so the ext block can follow
    }
    if (has_captured) {
        payload.push_back(0U);  // ext marker: no throw/snapshot outcome present
        append_nullable_string(&payload, captured_pokemon_id);
    }
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
