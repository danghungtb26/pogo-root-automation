struct RuntimeActionCommand {
    uint64_t message_seq = 0U;
    std::string session_id;
    std::string command_id;
    uint32_t action_tag = 0U;
    std::string encounter_id;
    uint32_t berry_wire = 0U;
    bool close_preview_after_caught = false;
    bool has_throw_profile = false;
    uint32_t throw_quality_wire = 0U;
    uint32_t throw_curve_wire = 0U;
    uint32_t throw_encounter_mode_wire = 0U;
    uint64_t based_on_observation_seq = 0U;
    bool expected_lifecycle_present = false;
    uint32_t expected_lifecycle = 0U;
    uint64_t expires_at_elapsed_ns = 0U;
    uint32_t pid = 0U;
    std::string process_name;
    std::string package_name;
    std::string build_fingerprint;
};

bool send_runtime_result(
    int fd,
    const char *command_id,
    uint32_t phase,
    const char *error_code,
    const char *message,
    uint32_t catch_outcome_wire = 0U
) {
    if (command_id == nullptr || command_id[0] == '\0') return false;
    std::vector<uint8_t> result;
    append_u32(&result, 1U);
    append_string(&result, command_id);
    append_u32(&result, phase);
    append_string(&result, error_code);
    append_string(&result, message);
    if (catch_outcome_wire != 0U) {
        if (catch_outcome_wire > 6U) return false;
        result.push_back(1U);
        append_u32(&result, catch_outcome_wire);
    }
    return send_internal_message(fd, kRuntimeResultMagic, result);
}

bool send_runtime_rejected(int fd, const char *command_id, const char *error_code, const char *message) {
    return send_runtime_result(
        fd,
        command_id,
        kCommandPhaseRejected,
        error_code == nullptr ? "runtime_command_rejected" : error_code,
        message == nullptr ? "runtime rejected command" : message
    );
}

bool parse_decimal_u64(const std::string &value, uint64_t *result) {
    if (result == nullptr || value.empty() || value.size() > 20U) return false;
    uint64_t parsed = 0U;
    for (const char character : value) {
        if (character < '0' || character > '9') return false;
        const uint64_t digit = static_cast<uint64_t>(character - '0');
        if (parsed > (UINT64_MAX - digit) / 10U) return false;
        parsed = parsed * 10U + digit;
    }
    if (parsed == 0U) return false;
    *result = parsed;
    return true;
}

struct RuntimeActionResult {
    uint32_t phase = kCommandPhaseRejected;
    const char *error_code = "runtime_command_rejected";
    const char *message = "runtime rejected command";
    // 0 means that the native runtime did not receive an authoritative catch
    // outcome. Values 1..6 mirror BridgePayloadCodecSupport.CatchOutcome.
    uint32_t catch_outcome_wire = 0U;
};
