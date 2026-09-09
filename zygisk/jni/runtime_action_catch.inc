constexpr bool kCatchExecutionEnabled = false;

bool parse_runtime_catch_command(
    const std::vector<uint8_t> &command,
    RuntimeActionCommand *parsed
) {
    if (parsed == nullptr) return false;
    *parsed = RuntimeActionCommand{};
    size_t offset = 0U;
    uint32_t message_type = 0U;
    uint32_t payload_version = 0U;
    uint32_t catch_reason = 0U;
    uint8_t lifecycle_present = 0U;
    if (!read_be32(command, &offset, &message_type) ||
        !read_be64(command, &offset, &parsed->message_seq) ||
        !read_be32(command, &offset, &payload_version) ||
        !read_string(command, &offset, &parsed->session_id) ||
        !read_string(command, &offset, &parsed->command_id) ||
        !read_be32(command, &offset, &parsed->action_tag) ||
        (parsed->action_tag != 3U && parsed->action_tag != 9U) ||
        !read_string(command, &offset, &parsed->encounter_id) ||
        !read_be32(command, &offset, &catch_reason) ||
        !read_be64(command, &offset, &parsed->based_on_observation_seq) ||
        offset >= command.size()) return false;

    lifecycle_present = command[offset++];
    if (lifecycle_present > 1U) return false;
    parsed->expected_lifecycle_present = lifecycle_present != 0U;
    if (parsed->expected_lifecycle_present &&
        !read_be32(command, &offset, &parsed->expected_lifecycle)) return false;
    if (!read_be64(command, &offset, &parsed->expires_at_elapsed_ns) ||
        !read_be32(command, &offset, &parsed->pid) ||
        !read_string(command, &offset, &parsed->process_name) ||
        !read_string(command, &offset, &parsed->package_name) ||
        !read_string(command, &offset, &parsed->build_fingerprint)) return false;

    // The default Catch wire format has no throw profile. Keep the reason
    // validated without exposing it as an execution parameter in native.
    if (catch_reason == 0U || catch_reason > 5U) return false;
    return message_type == kBridgeCommandType && payload_version == 1U &&
        parsed->message_seq > 0U && !parsed->session_id.empty() &&
        !parsed->command_id.empty() && !parsed->encounter_id.empty() &&
        parsed->based_on_observation_seq > 0U && offset == command.size();
}

RuntimeActionResult execute_runtime_catch(
    ProbeContext &context,
    const RuntimeActionCommand &command
) {
    RuntimeActionResult result{};
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    if (!kCatchExecutionEnabled || !binding.exact_build_verified ||
        !binding.catch_throw_verified || binding.pokeball_service == nullptr ||
        binding.pokeball_service_throw == nullptr) {
        result.error_code = "binding_unavailable";
        result.message = "catch throw execution is disabled until a live throw postcondition is verified";
        return result;
    }
    if (command.pid != static_cast<uint32_t>(context.pid) ||
        command.process_name != context.process_name ||
        command.package_name != context.process_name) {
        result.error_code = "runtime_identity_mismatch";
        result.message = "command identity does not match target process";
        return result;
    }
    if (command.build_fingerprint != kVerifiedBuildFingerprint) {
        result.error_code = "build_fingerprint_mismatch";
        result.message = "command build fingerprint is not the verified binding build";
        return result;
    }
    if (!command.expected_lifecycle_present || command.expected_lifecycle != 5U) {
        result.error_code = "lifecycle_guard_failed";
        result.message = "Catch requires the ENCOUNTER lifecycle";
        return result;
    }
    if (command.expires_at_elapsed_ns <= static_cast<uint64_t>(now_elapsed_nanos())) {
        result.error_code = "command_expired";
        result.message = "runtime command expired before invocation";
        return result;
    }
    if (command.command_id == context.last_action_command_id) {
        result.error_code = "duplicate_command";
        result.message = "command was already attempted in this runtime session";
        return result;
    }
    uint64_t encounter_id = 0U;
    if (!parse_decimal_u64(command.encounter_id, &encounter_id)) {
        result.error_code = "invalid_catch_command";
        result.message = "encounter id is invalid";
        return result;
    }

    context.last_action_command_id = command.command_id;
    const MainThreadActionOutcome outcome = request_main_thread_catch(context, encounter_id);
    if (outcome == MainThreadActionOutcome::kInvoked) {
        result.phase = kCommandPhaseIndeterminate;
        result.error_code = "throw_outcome_unavailable";
        result.message = "PokeballService.Throw was invoked; catch outcome is not yet observed";
    } else if (outcome == MainThreadActionOutcome::kIndeterminate) {
        result.phase = kCommandPhaseIndeterminate;
        result.error_code = "invoke_outcome_indeterminate";
        result.message = "PokeballService.Throw may have run but the main-thread result was lost";
    } else {
        result.error_code = "catch_target_unavailable";
        result.message = "active encounter ball or target position is unavailable";
    }
    return result;
}
