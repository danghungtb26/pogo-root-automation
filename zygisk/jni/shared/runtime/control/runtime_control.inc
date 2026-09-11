#include "../../bridge_kotlin/runtime_control_protocol.h"

std::atomic<bool> g_runtime_active{false};
constexpr uint32_t kRuntimeControlCompletedPhase = 3U;  // CommandPhase.COMPLETED

bool send_runtime_control_completed(
    int fd,
    const char *request_id,
    const char *message
) {
    return send_runtime_result(
        fd,
        request_id,
        kRuntimeControlCompletedPhase,
        "",
        message == nullptr ? "runtime control completed" : message
    );
}

bool runtime_control_identity_matches(
    const ProbeContext &context,
    const pogo_runtime_control::Request &request
) {
    return request.pid == static_cast<uint32_t>(context.pid) &&
        request.process_name == context.process_name &&
        request.package_name == context.process_name;
}

bool runtime_module_identity_matches(
    const ProbeContext &context,
    const pogo_runtime_module::Request &request
) {
    return request.pid == static_cast<uint32_t>(context.pid) &&
        request.process_name == context.process_name &&
        request.package_name == context.process_name;
}

void discard_pending_throw_events(ProbeContext &context) {
    RuntimeThrowEventRecord discarded{};
    while (context.throw_events.pop(&discarded)) {
        // Throw hooks intentionally remain installed once initialized. Drop
        // diagnostics accumulated while THROW_ASSIST is disabled so a later
        // enable never replays stale user activity.
    }
}

bool activate_runtime(ProbeContext &context, const char *request_id) {
    if (!pogo_runtime_module::module_registry_initialized()) {
        return send_runtime_rejected(
            context.fd,
            request_id,
            "runtime_module_registry_uninitialized",
            "runtime feature module registry failed to initialize"
        );
    }

    if (g_runtime_active.load()) {
        const RuntimeBinding binding = runtime_binding_snapshot(context);
        send_runtime_capability_update(context.fd, binding);
        return send_runtime_control_completed(
            context.fd,
            request_id,
            "runtime host already ready"
        );
    }

    // START is intentionally native-only. The probe can reach this path while
    // il2cpp_init and Unity graphics/scene initialization are still in flight;
    // managed discovery at that point can kill the target process. The
    // controller issues DIAGNOSTIC after a short post-start settling period.
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "runtime host start requested; managed diagnostic deferred"
    );
    discard_pending_throw_events(context);
    g_runtime_active.store(true);
    g_runtime_explicit_snapshot_mode.store(true);
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "runtime host ready; managed binding remains deferred observer=%d",
        context.observation_running.load() ? 1 : 0
    );
    return send_runtime_control_completed(
        context.fd,
        request_id,
        "runtime host ready; managed diagnostic is deferred"
    );
}

bool stop_runtime(ProbeContext &context, const char *request_id) {
    __android_log_print(ANDROID_LOG_INFO, kLogTag, "runtime host stop requested");
    pogo_runtime_module::disable_all_modules(context);
    g_runtime_active.store(false);
    g_runtime_explicit_snapshot_mode.store(false);
    discard_pending_throw_events(context);
    return send_runtime_control_completed(
        context.fd,
        request_id,
        "runtime feature modules stopped; process attachment remains idle"
    );
}
bool run_runtime_control_diagnostic(ProbeContext &context, const char *request_id) {
    // Re-running discovery rebuilds RuntimeBinding and invokes il2cpp on this
    // thread. It is only safe when no feature module is observing (the observer
    // touches il2cpp on its own thread and holds resolved owner pointers). While
    // a module is enabled the start-time binding stays frozen; otherwise a
    // DIAGNOSTIC re-probe lets a client pick up a binding that resolved late
    // (e.g. a Zenject service not yet in the container at START) without a full
    // STOP/START, then re-advertises capabilities to reflect it.
    if (g_runtime_active.load() && pogo_runtime_module::any_enabled()) {
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "runtime managed diagnostic skipped; active modules observing and binding stays frozen"
        );
        return send_runtime_control_completed(
            context.fd,
            request_id,
            "runtime host is ready with modules observing; binding stays frozen"
        );
    }
    const bool ready = run_managed_runtime_diagnostic(context);
    if (!ready) {
        return send_runtime_rejected(
            context.fd,
            request_id,
            "runtime_diagnostic_failed",
            "managed runtime diagnostic could not verify the initialized runtime"
        );
    }
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    send_runtime_capability_update(context.fd, binding);
    return send_runtime_control_completed(
        context.fd,
        request_id,
        "managed runtime diagnostic completed; capabilities refreshed"
    );
}
bool handle_runtime_control(
    ProbeContext &context,
    const pogo_runtime_control::Request &request
) {
    if (!runtime_control_identity_matches(context, request)) {
        return send_runtime_rejected(
            context.fd,
            request.request_id.c_str(),
            "runtime_identity_mismatch",
            "runtime control identity does not match target process"
        );
    }
    const uint64_t now = static_cast<uint64_t>(now_elapsed_nanos());
    if (request.expires_at_elapsed_ns <= now) {
        return send_runtime_rejected(
            context.fd,
            request.request_id.c_str(),
            "runtime_control_expired",
            "runtime control request expired before execution"
        );
    }

    switch (request.action) {
        case pogo_runtime_control::Action::kStart:
            return activate_runtime(context, request.request_id.c_str());
        case pogo_runtime_control::Action::kStop:
            return stop_runtime(context, request.request_id.c_str());
        case pogo_runtime_control::Action::kDiagnostic:
            return run_runtime_control_diagnostic(context, request.request_id.c_str());
        case pogo_runtime_control::Action::kSnapshot:
            return run_runtime_snapshot(context, request.request_id.c_str());
        case pogo_runtime_control::Action::kScanMap:
            return run_runtime_scan_map(
                context,
                request.request_id.c_str(),
                request.cycle_id
            );
    }
    return false;
}

bool handle_runtime_module_control(
    ProbeContext &context,
    const pogo_runtime_module::Request &request
) {
    if (!runtime_module_identity_matches(context, request)) {
        return send_runtime_rejected(
            context.fd,
            request.request_id.c_str(),
            "runtime_identity_mismatch",
            "runtime module identity does not match target process"
        );
    }
    const uint64_t now = static_cast<uint64_t>(now_elapsed_nanos());
    if (request.expires_at_elapsed_ns <= now) {
        return send_runtime_rejected(
            context.fd,
            request.request_id.c_str(),
            "runtime_module_control_expired",
            "runtime module request expired before execution"
        );
    }

    const bool enable = request.action == pogo_runtime_module::Action::kEnable;
    if (enable && !g_runtime_active.load()) {
        return send_runtime_rejected(
            context.fd,
            request.request_id.c_str(),
            "runtime_inactive",
            "runtime host must be STARTed before enabling a feature module"
        );
    }

    std::string error_code;
    std::string message;
    const bool changed = pogo_runtime_module::set_module_enabled(
        context,
        request.module,
        enable,
        &error_code,
        &message
    );
    __android_log_print(
        changed ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "runtime module control module=%s enabled=%d success=%d",
        pogo_runtime_module::module_name(request.module),
        enable ? 1 : 0,
        changed ? 1 : 0
    );
    if (!changed) {
        return send_runtime_rejected(
            context.fd,
            request.request_id.c_str(),
            error_code.empty() ? "runtime_module_control_failed" : error_code.c_str(),
            message.empty() ? "runtime feature module could not change state" : message.c_str()
        );
    }
    const RuntimeBinding updated_binding = runtime_binding_snapshot(context);
    send_runtime_capability_update(context.fd, updated_binding);
    if (request.module == pogo_runtime_module::Module::kEncounter) {
        discard_pending_throw_events(context);
    }
    return send_runtime_control_completed(
        context.fd,
        request.request_id.c_str(),
        message.c_str()
    );
}

bool parse_gameplay_action_tag(
    const std::vector<uint8_t> &command,
    uint32_t *action_tag
) {
    if (action_tag == nullptr) return false;
    size_t offset = 0U;
    uint32_t message_type = 0U;
    uint64_t message_seq = 0U;
    uint32_t payload_version = 0U;
    std::string session_id;
    std::string command_id;
    return read_be32(command, &offset, &message_type) &&
        read_be64(command, &offset, &message_seq) &&
        read_be32(command, &offset, &payload_version) &&
        read_string(command, &offset, &session_id) &&
        read_string(command, &offset, &command_id) &&
        read_be32(command, &offset, action_tag) &&
        message_type == kBridgeCommandType && payload_version == 1U &&
        message_seq > 0U && !session_id.empty() && !command_id.empty();
}

void runtime_command_channel_loop(ProbeContext &context) {
    while (kill(getpid(), 0) == 0) {
        std::vector<uint8_t> command;
        if (!read_internal_message(context.fd, kRuntimeCommandMagic, &command)) break;

        pogo_runtime_control::Request control;
        if (pogo_runtime_control::parse(command, &control)) {
            __android_log_print(
                ANDROID_LOG_INFO,
                kLogTag,
                "runtime control received seq=%llu request=%s action=%u",
                static_cast<unsigned long long>(control.message_seq),
                control.request_id.c_str(),
                static_cast<unsigned int>(control.action)
            );
            if (!handle_runtime_control(context, control)) break;
            continue;
        }

        pogo_runtime_module::Request module_control;
        if (pogo_runtime_module::parse(command, &module_control)) {
            if (!handle_runtime_module_control(context, module_control)) break;
            continue;
        }

        pogo_runtime::RuntimeCommandPrefix prefix;
        if (!pogo_runtime::parse_runtime_command_prefix(command, &prefix)) break;
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "runtime gameplay command received seq=%llu command=%s bytes=%zu host=%d",
            static_cast<unsigned long long>(prefix.message_seq),
            prefix.command_id.c_str(),
            command.size(),
            g_runtime_active.load() ? 1 : 0
        );
        if (!g_runtime_active.load()) {
            if (!send_runtime_rejected(
                    context.fd,
                    prefix.command_id.c_str(),
                    "runtime_inactive",
                    "runtime host is idle; send START runtime control first"
                )) break;
            continue;
        }

        uint32_t action_tag = 0U;
        if (!parse_gameplay_action_tag(command, &action_tag)) break;
        std::string module_reason;
        if (!pogo_runtime_module::gameplay_action_allowed(action_tag, &module_reason)) {
            if (!send_runtime_rejected(
                    context.fd,
                    prefix.command_id.c_str(),
                    "runtime_module_disabled",
                    module_reason.c_str()
                )) break;
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
                    result.message,
                    result.catch_outcome_wire,
                    result.captured_pokemon_id.c_str()
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
        if (parse_runtime_discard_command(command, &action)) {
            const RuntimeActionResult result = execute_runtime_discard(context, action);
            if (!send_runtime_result(
                    context.fd,
                    action.command_id.c_str(),
                    result.phase,
                    result.error_code,
                    result.message
                )) break;
            continue;
        }
        if (parse_runtime_transfer_command(command, &action)) {
            const RuntimeActionResult result = execute_runtime_transfer(context, action);
            if (!send_runtime_result(
                    context.fd,
                    action.command_id.c_str(),
                    result.phase,
                    result.error_code,
                    result.message
                )) break;
            continue;
        }
        // Any other gameplay action remains fail-closed until a verified native
        // client-owned executor is added.
        if (!send_runtime_rejected(
                context.fd,
                prefix.command_id.c_str(),
                "binding_not_implemented",
                "runtime module has no verified binding for this gameplay action"
            )) break;
    }

    pogo_runtime_module::disable_all_modules(context);
    g_runtime_active.store(false);
    g_runtime_explicit_snapshot_mode.store(false);
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
        const bool il2cpp_loaded = find_mapping_path(
            "libil2cpp.so", event.il2cpp_path, sizeof(event.il2cpp_path)
        );
        const bool unity_loaded = find_mapping_path(
            "libunity.so", event.unity_path, sizeof(event.unity_path)
        );
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
            copy_string(
                context->il2cpp_path,
                sizeof(context->il2cpp_path),
                event.il2cpp_path
            );
            if (api_ready) {
                context->il2cpp_api = resolved_api;
                context->il2cpp_api_ready = true;
                break;
            }
        }
        usleep(kProbeDelayUs);
    }

    event.probe_flags |= kProbeComplete;
    if (!write_full(context->fd, &event, sizeof(event))) {
        __android_log_print(
            ANDROID_LOG_ERROR,
            kLogTag,
            "failed to publish binding probe for pid=%d",
            event.pid
        );
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

    const bool modules_registered = pogo_runtime_module::initialize_runtime_modules(context->fd);
    __android_log_print(
        modules_registered ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR,
        kLogTag,
        "runtime host module registration complete success=%d",
        modules_registered ? 1 : 0
    );

    // Zygisk host is attached but feature modules remain disabled until the
    // foreground service explicitly enables them.
    runtime_command_channel_loop(*context);
    close(context->fd);
    delete context;
    return nullptr;
}
