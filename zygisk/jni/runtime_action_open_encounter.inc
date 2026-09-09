constexpr bool kOpenEncounterExecutionEnabled = true;

bool parse_runtime_open_encounter_command(
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
        parsed->action_tag != 2U ||
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

bool wait_for_runtime_encounter(ProbeContext &context, uint64_t encounter_id) {
    const Il2CppApi &api = context.il2cpp_api;
    if (encounter_id == 0U || api.domain_get == nullptr || api.thread_attach == nullptr ||
        api.thread_detach == nullptr) return false;
    for (size_t attempt = 0U; attempt < 40U; ++attempt) {
        refresh_runtime_encounter_owners(context);
        const RuntimeBinding binding = runtime_binding_snapshot(context);
        Il2CppDomain *domain = api.domain_get();
        Il2CppThread *thread = domain == nullptr ? nullptr : api.thread_attach(domain);
        if (thread != nullptr) {
            pogo_runtime::RuntimeEncounterObservation observation{};
            const bool active = read_active_runtime_encounter(
                api,
                binding.encounter_pokemon,
                binding.encounter_state,
                &observation
            );
            api.thread_detach(thread);
            if (active && observation.encounter_id == encounter_id) return true;
        }
        usleep(100000U);
    }
    return false;
}

RuntimeActionResult execute_runtime_open_encounter(
    ProbeContext &context,
    const RuntimeActionCommand &command
) {
    // Prefer the client-owned DynamicTappablesService pipeline. Some live map
    // states keep eghx empty while the corresponding WildMapPokemon still has
    // its verified TapGesture/OnTap contract, so that exact contract is the
    // guarded fallback below.
    RuntimeActionResult result{};
    if (command.session_id.empty()) {
        result.error_code = "runtime_session_missing";
        result.message = "command does not carry a runtime session";
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
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    const bool dynamic_binding_available = binding.dynamic_tappables_service != nullptr &&
        binding.dynamic_tappables_tap != nullptr;
    const bool map_pokemon_binding_available = binding.map_pokemon_on_tapped != nullptr &&
        binding.map_pokemon_tap_event_verified && binding.unity_find_objects_of_type != nullptr;
    if (!kOpenEncounterExecutionEnabled || !binding.exact_build_verified ||
        (!dynamic_binding_available && !map_pokemon_binding_available)) {
        result.error_code = "binding_unavailable";
        result.message = "no verified encounter tap binding is available for this runtime";
        return result;
    }
    if (!command.expected_lifecycle_present || command.expected_lifecycle != 4U) {
        result.error_code = "lifecycle_guard_failed";
        result.message = "OpenEncounter requires the OVERWORLD lifecycle";
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
        result.error_code = "invalid_encounter_command";
        result.message = "encounter id is invalid";
        return result;
    }
    context.last_action_command_id = command.command_id;
    const MainThreadActionOutcome outcome = request_main_thread_open_encounter(
        context, encounter_id
    );
    if (outcome == MainThreadActionOutcome::kInvoked) {
        if (wait_for_runtime_encounter(context, encounter_id)) {
            result.phase = kCommandPhaseCompleted;
            result.error_code = "encounter_opened";
            result.message = "verified runtime tap opened the requested encounter";
        } else {
            result.phase = kCommandPhaseIndeterminate;
            result.error_code = "encounter_open_postcondition_timeout";
            result.message = "verified runtime tap was invoked but encounter lifecycle did not confirm";
        }
    } else if (outcome == MainThreadActionOutcome::kIndeterminate) {
        result.phase = kCommandPhaseIndeterminate;
        result.error_code = "invoke_outcome_indeterminate";
        result.message = "verified runtime tap may have run but the main-thread result was lost";
    } else {
        result.error_code = "encounter_target_unavailable";
        result.message = "the requested map spawn is no longer available or its tap contract rejected the command";
    }
    return result;
}
