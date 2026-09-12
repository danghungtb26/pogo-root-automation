// Manual transfer command execution. The actual GetPokemon -> ReleasePokemon
// call and Promise lifecycle are shared with auto transfer and are marshalled
// to Unity's main thread through the transfer observer state machine.

RuntimeActionResult execute_runtime_transfer(
    ProbeContext &context,
    const RuntimeActionCommand &command
) {
    RuntimeActionResult result{};
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    if (!kTransferExecutionEnabled || !binding.exact_build_verified || !binding.transfer_verified) {
        result.error_code = "binding_unavailable";
        result.message = "pokemon transfer is unavailable for this verified build";
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
        result.message = "transfer requires the OVERWORLD lifecycle";
        return result;
    }
    if (command.expires_at_elapsed_ns <= static_cast<uint64_t>(now_elapsed_nanos())) {
        result.error_code = "command_expired";
        result.message = "transfer command expired before invocation";
        return result;
    }
    if (command.command_id == context.last_action_command_id) {
        result.error_code = "duplicate_command";
        result.message = "command was already attempted in this runtime session";
        return result;
    }
    uint64_t pokemon_id = 0U;
    if (!parse_decimal_u64(command.transfer_pokemon_id, &pokemon_id)) {
        result.error_code = "invalid_transfer_command";
        result.message = "pokemon id is invalid";
        return result;
    }
    context.last_action_command_id = command.command_id;
    const MainThreadActionOutcome outcome = request_main_thread_start_runtime_transfer(
        context,
        pokemon_id,
        command.command_id.c_str()
    );
    if (outcome == MainThreadActionOutcome::kInvoked) {
        result.phase = kCommandPhaseIndeterminate;
        result.error_code = "transfer_promise_pending";
        result.message = "pokemon release was invoked; native is waiting for ReleasePokemon Promise";
    } else if (outcome == MainThreadActionOutcome::kIndeterminate) {
        result.phase = kCommandPhaseIndeterminate;
        result.error_code = "invoke_outcome_indeterminate";
        result.message = "release may have run but the result was lost";
    } else {
        result.error_code = "transfer_target_unavailable";
        result.message = "the requested pokemon could not be released";
    }
    return result;
}
