constexpr const char *kRuntimeDiagnosticAlert = "__runtime_diagnostic_v1__";

bool parse_runtime_diagnostic_command(
    const std::vector<uint8_t> &command,
    std::string *command_id
) {
    if (command_id == nullptr) return false;
    size_t offset = 0U;
    uint32_t message_type = 0U;
    uint64_t message_seq = 0U;
    uint32_t payload_version = 0U;
    std::string session_id;
    uint32_t action_tag = 0U;
    uint32_t alert_kind = 0U;
    std::string alert_message;
    uint64_t observation_seq = 0U;
    uint8_t lifecycle_present = 0U;
    uint32_t lifecycle = 0U;
    uint64_t expires_at_elapsed_ns = 0U;
    uint32_t pid = 0U;
    std::string process_name;
    std::string package_name;
    std::string build_fingerprint;
    if (!read_be32(command, &offset, &message_type) ||
        !read_be64(command, &offset, &message_seq) ||
        !read_be32(command, &offset, &payload_version) ||
        !read_string(command, &offset, &session_id) ||
        !read_string(command, &offset, command_id) ||
        !read_be32(command, &offset, &action_tag) ||
        !read_be32(command, &offset, &alert_kind) ||
        !read_string(command, &offset, &alert_message) ||
        !read_be64(command, &offset, &observation_seq) ||
        offset >= command.size()) return false;
    lifecycle_present = command[offset++];
    if (lifecycle_present != 0U) {
        if (!read_be32(command, &offset, &lifecycle)) return false;
    }
    if (!read_be64(command, &offset, &expires_at_elapsed_ns) ||
        !read_be32(command, &offset, &pid) ||
        !read_string(command, &offset, &process_name) ||
        !read_string(command, &offset, &package_name) ||
        !read_string(command, &offset, &build_fingerprint)) return false;

    (void)message_seq;
    (void)alert_kind;
    (void)observation_seq;
    (void)lifecycle;
    (void)expires_at_elapsed_ns;
    (void)pid;
    (void)process_name;
    (void)package_name;
    (void)build_fingerprint;
    return message_type == kBridgeCommandType && payload_version == 1U &&
        !session_id.empty() && !command_id->empty() && action_tag == 7U &&
        alert_message == kRuntimeDiagnosticAlert && offset == command.size();
}

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
    // Mark before invocation so a retry with the same id can never invoke the
    // client-owned method twice after a lost result.
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

bool send_runtime_diagnostic_result(int fd, const char *command_id, bool success) {
    if (command_id == nullptr || command_id[0] == '\0') return false;
    std::vector<uint8_t> result;
    append_u32(&result, 1U);
    append_string(&result, command_id);
    append_u32(&result, kCommandPhaseRejected);
    append_string(&result, success ? "runtime_diagnostic_complete" : "runtime_diagnostic_failed");
    append_string(
        &result,
        success
            ? "managed runtime diagnostic completed; see logcat for signatures"
            : "managed runtime diagnostic could not inspect the initialized runtime"
    );
    return send_internal_message(fd, kRuntimeResultMagic, result);
}

bool send_runtime_capability_update(int fd, const RuntimeBinding &binding) {
    if (!binding.exact_build_verified) return false;
    std::vector<std::string> capabilities;
    if (binding.use_berry_verified) capabilities.emplace_back("USE_BERRY");
    if (binding.encounter_read_verified) {
        capabilities.emplace_back("READ_LIFECYCLE");
        capabilities.emplace_back("ENCOUNTER");
    }
    if (binding.map_entity_read_verified) capabilities.emplace_back("READ_NEARBY");
    if (binding.forts_read_verified) capabilities.emplace_back("READ_FORTS");
    if (kOpenEncounterExecutionEnabled && binding.dynamic_tappables_service != nullptr &&
        binding.dynamic_tappables_tap != nullptr) {
        capabilities.emplace_back("OPEN_ENCOUNTER");
    }
    if (kCatchExecutionEnabled && binding.catch_throw_verified) {
        capabilities.emplace_back("CATCH");
    }
    if (kDirectCatchExecutionEnabled && binding.direct_catch_verified) {
        capabilities.emplace_back("DIRECT_CATCH");
    }
    if (kSpinExecutionEnabled && binding.spin_verified) capabilities.emplace_back("SPIN");
    if (capabilities.empty()) return false;
    std::vector<uint8_t> payload;
    append_u32(&payload, 1U);
    append_string(&payload, kVerifiedBuildFingerprint);
    append_u32(&payload, static_cast<uint32_t>(capabilities.size()));
    for (const std::string &capability : capabilities) append_string(&payload, capability.c_str());
    payload.push_back(1U);
    return send_internal_message(fd, kRuntimeCapabilitiesMagic, payload);
}

void runtime_command_channel_loop(ProbeContext &context) {
    while (kill(getpid(), 0) == 0) {
        std::vector<uint8_t> command;
        if (!read_internal_message(context.fd, kRuntimeCommandMagic, &command)) break;

        pogo_runtime::RuntimeCommandPrefix prefix;
        if (!pogo_runtime::parse_runtime_command_prefix(command, &prefix)) break;
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "runtime command received seq=%llu command=%s bytes=%zu",
            static_cast<unsigned long long>(prefix.message_seq),
            prefix.command_id.c_str(),
            command.size()
        );
        std::string diagnostic_command_id;
        if (parse_runtime_diagnostic_command(command, &diagnostic_command_id) &&
            diagnostic_command_id == prefix.command_id) {
            // The diagnostic is explicitly read-only and can be requested only
            // after the controller has connected to this already-running game
            // process. It never publishes a gameplay capability.
            const bool success = run_managed_runtime_diagnostic(context);
            if (success) {
                const RuntimeBinding binding = runtime_binding_snapshot(context);
                const bool capabilities_sent = send_runtime_capability_update(context.fd, binding);
                __android_log_print(
                    capabilities_sent ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
                    kLogTag,
                    "runtime diagnostic capabilities sent=%d berry=%d encounter_read=%d",
                    capabilities_sent ? 1 : 0,
                    binding.use_berry_verified ? 1 : 0,
                    binding.encounter_read_verified ? 1 : 0
                );
            }
            if (!send_runtime_diagnostic_result(context.fd, prefix.command_id.c_str(), success)) break;
            if (success) start_runtime_observation(context);
            continue;
        }
        RuntimeActionCommand action;
        if (parse_runtime_catch_command(command, &action)) {
            const RuntimeActionResult result = execute_runtime_catch(context, action);
            if (!send_runtime_result(
                    context.fd,
                    action.command_id.c_str(),
                    result.phase,
                    result.error_code,
                    result.message
                )) break;
            continue;
        }
        if (parse_runtime_spin_command(command, &action)) {
            const RuntimeActionResult result = execute_runtime_spin(context, action);
            if (!send_runtime_result(
                    context.fd,
                    action.command_id.c_str(),
                    result.phase,
                    result.error_code,
                    result.message
                )) break;
            continue;
        }
        if (parse_runtime_open_encounter_command(command, &action)) {
            const RuntimeActionResult result = execute_runtime_open_encounter(context, action);
            if (!send_runtime_result(
                    context.fd,
                    action.command_id.c_str(),
                    result.phase,
                    result.error_code,
                    result.message
                )) break;
            continue;
        }
        if (parse_runtime_use_berry_command(command, &action)) {
            const RuntimeActionResult result = execute_runtime_use_berry(context, action);
            if (!send_runtime_result(
                    context.fd,
                    action.command_id.c_str(),
                    result.phase,
                    result.error_code,
                    result.message
                )) break;
            continue;
        }
        // Unsupported or malformed commands remain fail-closed. A native
        // binding must be added for each action before it can reach IL2CPP.
        if (!send_runtime_rejected(
                context.fd,
                prefix.command_id.c_str(),
                "binding_not_implemented",
                "runtime has no verified binding for this action"
        )) break;
    }
    stop_runtime_observation(context);
    shutdown_main_thread_bridge(context);
}

void *binding_probe_thread(void *opaque_context) {
    auto *context = static_cast<ProbeContext *>(opaque_context);
    if (context == nullptr) return nullptr;

    RuntimeEvent event{};
    event.magic = kRuntimeEventMagic;
    event.protocol_version = kRuntimeProtocolVersion;
    event.event_type = static_cast<uint32_t>(RuntimeEventType::kBindingProbe);
    event.pid = context->pid;
    event.il2cpp_required_symbol_count = kRequiredIl2cppCoreSymbolCount;
    copy_string(event.process_name, sizeof(event.process_name), context->process_name);
    copy_string(event.translation_layer, sizeof(event.translation_layer), "none");

    for (int attempt = 0; attempt < kProbeAttempts; ++attempt) {
        const bool il2cpp_loaded = find_mapping_path("libil2cpp.so", event.il2cpp_path, sizeof(event.il2cpp_path));
        const bool unity_loaded = find_mapping_path("libunity.so", event.unity_path, sizeof(event.unity_path));
        if (il2cpp_loaded) event.probe_flags |= kIl2cppLoaded;
        if (unity_loaded) event.probe_flags |= kUnityLoaded;
        if (mapping_contains("libhoudini")) {
            event.probe_flags |= kHoudiniTranslation;
            copy_string(event.translation_layer, sizeof(event.translation_layer), "houdini");
        } else if (mapping_contains("libndk_translation")) {
            event.probe_flags |= kNdkTranslation;
            copy_string(event.translation_layer, sizeof(event.translation_layer), "ndk_translation");
        }
        if (il2cpp_loaded) {
            Il2CppApi resolved_api{};
            const bool api_ready = probe_il2cpp_runtime(&event, &resolved_api);
            copy_string(context->il2cpp_path, sizeof(context->il2cpp_path), event.il2cpp_path);
            if (api_ready) {
                context->il2cpp_api = resolved_api;
                context->il2cpp_api_ready = true;
            }
            break;
        }
        usleep(kProbeDelayUs);
    }

    event.probe_flags |= kProbeComplete;
    if (!write_full(context->fd, &event, sizeof(event))) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "failed to publish binding probe for pid=%d", event.pid);
    } else {
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "probe: il2cpp=%d symbols=%u core_required=%u assemblies=%u classes=%u candidates=%u translation=%s",
            (event.probe_flags & kIl2cppLoaded) != 0U,
            event.il2cpp_symbol_count,
            event.il2cpp_required_symbol_count,
            event.assembly_count,
            event.class_count,
            event.candidate_class_count,
            event.translation_layer
        );
    }
    runtime_command_channel_loop(*context);
    close(context->fd);
    delete context;
    return nullptr;
}
