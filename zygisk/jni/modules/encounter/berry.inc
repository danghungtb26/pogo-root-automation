bool berry_item_id(uint32_t berry_wire, int32_t *item_id) {
    if (item_id == nullptr) return false;
    switch (berry_wire) {
        case 1U:  // BerryType.RAZZ
            *item_id = 701;
            return true;
        case 2U:  // BerryType.NANAB
            *item_id = 703;
            return true;
        case 3U:  // BerryType.PINAP
            *item_id = 705;
            return true;
        case 4U:  // BerryType.GOLDEN_RAZZ
            *item_id = 706;
            return true;
        case 5U:  // BerryType.SILVER_PINAP
            *item_id = 708;
            return true;
        default:
            return false;
    }
}

bool parse_runtime_use_berry_command(
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
        parsed->action_tag != 8U ||
        !read_string(command, &offset, &parsed->encounter_id) ||
        !read_be32(command, &offset, &parsed->berry_wire) ||
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

    return message_type == kBridgeCommandType &&
        payload_version == 1U &&
        parsed->message_seq > 0U &&
        !parsed->session_id.empty() &&
        !parsed->command_id.empty() &&
        !parsed->encounter_id.empty() &&
        parsed->based_on_observation_seq > 0U &&
        offset == command.size();
}

MainThreadActionOutcome use_runtime_berry_on_main_thread(
    ProbeContext &context,
    uint64_t encounter_id,
    int32_t item_id
) {
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    const Il2CppApi &api = context.il2cpp_api;
    if (encounter_id == 0U || item_id <= 0 ||
        (binding.encounter_pokemon == nullptr && binding.encounter_state == nullptr) ||
        binding.item_bag == nullptr ||
        binding.use_item_on_pokemon == nullptr || api.runtime_invoke == nullptr) {
        return MainThreadActionOutcome::kRejected;
    }
    pogo_runtime::RuntimeEncounterObservation active{};
    if (!read_active_runtime_encounter(
            api,
            binding.encounter_pokemon,
            binding.encounter_state,
            &active
        ) || active.encounter_id != encounter_id) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "runtime berry rejected stale encounter requested=%llu active=%llu",
            static_cast<unsigned long long>(encounter_id),
            static_cast<unsigned long long>(active.encounter_id)
        );
        return MainThreadActionOutcome::kRejected;
    }
    void *arguments[] = {&item_id, &encounter_id};
    void *exception = nullptr;
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "runtime action invoking action=USE_BERRY main_thread item=%d encounter=%llu owner=%p method=%p",
        item_id,
        static_cast<unsigned long long>(encounter_id),
        binding.item_bag,
        binding.use_item_on_pokemon
    );
    void *promise = api.runtime_invoke(
        binding.use_item_on_pokemon,
        binding.item_bag,
        arguments,
        &exception
    );
    if (exception != nullptr) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "runtime action invoke exception action=USE_BERRY exception=%p",
            exception
        );
        return MainThreadActionOutcome::kIndeterminate;
    }
    return promise == nullptr
        ? MainThreadActionOutcome::kIndeterminate
        : MainThreadActionOutcome::kInvoked;
}

RuntimeActionResult execute_runtime_use_berry(
    ProbeContext &context,
    const RuntimeActionCommand &command
) {
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
    if (!binding.exact_build_verified || !binding.use_berry_verified ||
        binding.item_bag == nullptr || binding.use_item_on_pokemon == nullptr) {
        result.error_code = "binding_unavailable";
        result.message = "verified IItemBag.UseItemOnPokemon binding is unavailable";
        return result;
    }
    if (!command.expected_lifecycle_present || command.expected_lifecycle != 5U) {
        result.error_code = "lifecycle_guard_failed";
        result.message = "UseBerry requires the ENCOUNTER lifecycle";
        return result;
    }
    const uint64_t now = static_cast<uint64_t>(now_elapsed_nanos());
    if (command.expires_at_elapsed_ns <= now) {
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
    int32_t item_id = 0;
    if (!parse_decimal_u64(command.encounter_id, &encounter_id) ||
        !berry_item_id(command.berry_wire, &item_id)) {
        result.error_code = "invalid_berry_command";
        result.message = "encounter id or berry type is invalid";
        return result;
    }
    context.last_action_command_id = command.command_id;
    const MainThreadActionOutcome outcome = request_main_thread_use_berry(
        context, encounter_id, item_id
    );
    if (outcome == MainThreadActionOutcome::kInvoked) {
        result.phase = kCommandPhaseIndeterminate;
        result.error_code = "async_outcome_unavailable";
        result.message = "UseItemOnPokemon invoked on the Unity main thread; Promise outcome is not yet observed";
    } else if (outcome == MainThreadActionOutcome::kIndeterminate) {
        result.phase = kCommandPhaseIndeterminate;
        result.error_code = "invoke_outcome_indeterminate";
        result.message = "UseItemOnPokemon may have run but the main-thread result was lost";
    } else {
        result.error_code = "berry_target_unavailable";
        result.message = "active encounter or Unity main-thread bridge is unavailable";
    }
    return result;
}
