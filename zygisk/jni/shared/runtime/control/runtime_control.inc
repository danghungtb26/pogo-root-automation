#include "../../bridge_kotlin/runtime_control_protocol.h"

std::atomic<bool> g_runtime_active{false};
constexpr uint32_t kRuntimeControlCompletedPhase = 3U;  // CommandPhase.COMPLETED

#include "runtime_map_maintenance.inc"

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
        send_runtime_capability_update(
            context.fd,
            binding,
            context.map_live_verified.load(std::memory_order_acquire)
        );
        return send_runtime_control_completed(
            context.fd,
            request_id,
            "runtime host already ready"
        );
    }

    // START is intentionally native-only. The probe can reach this path while
    // il2cpp_init and Unity graphics/scene initialization are still in flight;
    // managed discovery at that point can kill the target process. The
    // native control loop schedules discovery after a post-start settling period.
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "runtime host start requested; managed diagnostic deferred"
    );
    discard_pending_throw_events(context);
    pogo_runtime_module::reset_map_live_verified(context);
    g_runtime_active.store(true);
    g_runtime_readiness_retry.start(static_cast<uint64_t>(now_elapsed_nanos()));
    g_runtime_explicit_snapshot_mode.store(true);
    reset_runtime_catch_spin_config();
    reset_runtime_transfer_config();
    reset_runtime_discard_config();
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
    g_runtime_readiness_retry.stop();
    __android_log_print(ANDROID_LOG_INFO, kLogTag, "runtime host stop requested");
    pogo_runtime_module::disable_all_modules(context);
    pogo_runtime_module::reset_map_live_verified(context);
    g_runtime_active.store(false);
    g_runtime_explicit_snapshot_mode.store(false);
    discard_pending_throw_events(context);
    reset_runtime_catch_spin_config();
    reset_runtime_transfer_config();
    reset_runtime_discard_config();
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
    g_runtime_readiness_retry.completed();
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    pogo_runtime_module::probe_map_live(context);
    send_runtime_capability_update(
        context.fd,
        binding,
        context.map_live_verified.load(std::memory_order_acquire)
    );
    pogo_runtime_module::sync_auto_enabled_modules(context);
    return send_runtime_control_completed(
        context.fd,
        request_id,
        "managed runtime diagnostic completed; capabilities refreshed"
    );
}
bool handle_runtime_control(
    ProbeContext &context,
    const pogo_runtime_control::Request &request,
    const std::vector<uint8_t> &command
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

    // Every control action is module-owned: the runtime-core module owns the
    // lifecycle actions (START/STOP/DIAGNOSTIC) and feature modules own theirs
    // (e.g. catch_spin owns CONFIG_SET). The action arrives as a raw wire number and
    // is routed uniformly through the owner map; an action no module owns is
    // rejected fail-closed inside dispatch_control_action. No lifecycle switch.
    return pogo_runtime_module::dispatch_control_action(
        context,
        request.action_wire,
        command,
        request.request_id.c_str(),
        request.cycle_id
    );
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

    return send_runtime_rejected(
        context.fd,
        request.request_id.c_str(),
        "native_module_lifecycle_owned",
        "feature lifecycle follows native config; send CONFIG_SET or host STOP"
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
        pollfd descriptor{};
        descriptor.fd = context.fd;
        descriptor.events = POLLIN;
        const int polled = poll(&descriptor, 1, kMapReadinessPollTimeoutMs);
        if (polled < 0) {
            if (errno == EINTR) {
                maybe_probe_pending_map_readiness(context);
                continue;
            }
            break;
        }

        if (polled > 0) {
            if ((descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) break;
            if ((descriptor.revents & POLLIN) == 0) {
                maybe_probe_pending_map_readiness(context);
                continue;
            }

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
                static_cast<unsigned int>(control.action_wire)
            );
            if (!handle_runtime_control(context, control, command)) break;
            continue;
        }

        pogo_runtime_module::Request module_control;
        if (pogo_runtime_module::parse(command, &module_control)) {
            if (!handle_runtime_module_control(context, module_control)) break;
            continue;
        }

        // CONFIG_SET has its own marker and payload because it carries a full
        // versioned snapshot, not the small ENABLE/DISABLE module-control
        // request. Route it through the catch_spin owner map before falling
        // through to gameplay parsing.
        pogo_runtime_catch_spin_config::Request catch_spin_config;
        if (pogo_runtime_catch_spin_config::parse(command, &catch_spin_config)) {
            if (!pogo_runtime_module::dispatch_control_action(
                    context,
                    pogo_runtime_catch_spin_config::kConfigSetAction,
                    command,
                    catch_spin_config.request_id.c_str(),
                    0U
                )) break;
            continue;
        }

        pogo_runtime_transfer_config::Request transfer_config;
        if (pogo_runtime_transfer_config::parse(command, &transfer_config)) {
            if (!pogo_runtime_module::dispatch_control_action(
                    context,
                    pogo_runtime_transfer_config::kConfigSetAction,
                    command,
                    transfer_config.request_id.c_str(),
                    0U
                )) break;
            continue;
        }

        pogo_runtime_discard_config::Request discard_config;
        if (pogo_runtime_discard_config::parse(command, &discard_config)) {
            if (!pogo_runtime_module::dispatch_control_action(
                    context,
                    pogo_runtime_discard_config::kConfigSetAction,
                    command,
                    discard_config.request_id.c_str(),
                    0U
                )) break;
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
        // Route to the module that owns this action tag via the registry owner map.
        // Ownership (action_ownership), parsing and execution live inside each
        // module; the control loop keeps no per-action switch or enable gate.
        RuntimeActionResult action_result;
        std::string action_command_id = prefix.command_id;
        std::string reject_reason;
        const pogo_runtime_module::GameplayDispatchStatus dispatch_status =
            pogo_runtime_module::dispatch_gameplay_action(
                context, action_tag, command, &action_result, &action_command_id, &reject_reason
            );
        if (dispatch_status == pogo_runtime_module::GameplayDispatchStatus::kModuleDisabled) {
            if (!send_runtime_rejected(
                    context.fd,
                    action_command_id.c_str(),
                    "runtime_module_disabled",
                    reject_reason.c_str()
                )) break;
            continue;
        }
        if (dispatch_status == pogo_runtime_module::GameplayDispatchStatus::kHandled) {
            if (!send_runtime_result(
                    context.fd,
                    action_command_id.c_str(),
                    action_result.phase,
                    action_result.error_code,
                    action_result.message,
                    action_result.catch_outcome_wire,
                    action_result.captured_pokemon_id.c_str()
                )) break;
            continue;
        }
        // Unowned action tag (or the owning module could not parse the frame):
        // fail-closed until a verified native client-owned executor is added.
        if (!send_runtime_rejected(
                context.fd,
                prefix.command_id.c_str(),
                "binding_not_implemented",
                "runtime module has no verified binding for this gameplay action"
            )) break;
        }

        maybe_probe_pending_map_readiness(context);
    }

    pogo_runtime_module::disable_all_modules(context);
    pogo_runtime_module::reset_map_live_verified(context);
    g_runtime_active.store(false);
    g_runtime_explicit_snapshot_mode.store(false);
    shutdown_main_thread_bridge(context);
}

#include "runtime_binding_probe.inc"
