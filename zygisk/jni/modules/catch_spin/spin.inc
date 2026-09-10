constexpr bool kSpinExecutionEnabled = true;

bool parse_runtime_spin_command(
    const std::vector<uint8_t> &command,
    RuntimeActionCommand *parsed
) {
    if (parsed == nullptr) return false;
    *parsed = RuntimeActionCommand{};
    size_t offset = 0U;
    uint32_t message_type = 0U;
    uint32_t payload_version = 0U;
    uint8_t lifecycle_present = 0U;
    if (!read_be32(command, &offset, &message_type) ||
        !read_be64(command, &offset, &parsed->message_seq) ||
        !read_be32(command, &offset, &payload_version) ||
        !read_string(command, &offset, &parsed->session_id) ||
        !read_string(command, &offset, &parsed->command_id) ||
        !read_be32(command, &offset, &parsed->action_tag) ||
        parsed->action_tag != 4U ||
        !read_string(command, &offset, &parsed->encounter_id) ||
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
    return message_type == kBridgeCommandType && payload_version == 1U &&
        parsed->message_seq > 0U && !parsed->session_id.empty() &&
        !parsed->command_id.empty() && !parsed->encounter_id.empty() &&
        parsed->based_on_observation_seq > 0U && offset == command.size();
}

RuntimeActionResult execute_runtime_spin(
    ProbeContext &context,
    const RuntimeActionCommand &command
) {
    RuntimeActionResult result{};
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    if (!kSpinExecutionEnabled || !binding.exact_build_verified || !binding.spin_verified) {
        result.error_code = "binding_unavailable";
        result.message = "direct PokéStop spin is unavailable for this verified build";
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
    if (!command.expected_lifecycle_present || command.expected_lifecycle != 4U) {
        result.error_code = "lifecycle_guard_failed";
        result.message = "Spin requires the OVERWORLD lifecycle";
        return result;
    }
    if (command.expires_at_elapsed_ns <= static_cast<uint64_t>(now_elapsed_nanos())) {
        result.error_code = "command_expired";
        result.message = "spin command expired before invocation";
        return result;
    }
    if (command.command_id == context.last_action_command_id) {
        result.error_code = "duplicate_command";
        result.message = "command was already attempted in this runtime session";
        return result;
    }
    context.last_action_command_id = command.command_id;
    const MainThreadActionOutcome outcome = request_main_thread_spin(
        context, command.encounter_id
    );
    if (outcome == MainThreadActionOutcome::kInvoked) {
        result.phase = kCommandPhaseCompleted;
        result.error_code = "spin_invoked";
        result.message = "PoiItemSpinner.Spin was invoked; fort state will be refreshed from the next map observation";
    } else if (outcome == MainThreadActionOutcome::kIndeterminate) {
        result.phase = kCommandPhaseIndeterminate;
        result.error_code = "invoke_outcome_indeterminate";
        result.message = "PoiItemSpinner.Spin may have run but the main-thread result was lost";
    } else {
        result.error_code = "spin_target_unavailable";
        result.message = "the requested PokéStop is unavailable or cooling down";
    }
    return result;
}
