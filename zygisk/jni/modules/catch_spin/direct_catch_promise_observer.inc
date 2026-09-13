// Reads and owns the Promise returned by MapPokemon.TryCapture. The Promise is
// retained with an ABI-width IL2CPP GC handle, polled only on Unity's main
// thread, and converted to the shared authoritative catch-result wire values.

namespace {
constexpr uint64_t kDirectCatchPromiseTimeoutNs = 15000000000ULL;

void refresh_map_after_direct_catch(
    ProbeContext &context,
    uint64_t encounter_id,
    uint32_t status
);

const char *direct_catch_status_name(uint32_t status) {
    switch (status) {
        case 0U: return "CATCH_ERROR";
        case 1U: return "CATCH_SUCCESS";
        case 2U: return "CATCH_ESCAPE";
        case 3U: return "CATCH_FLEE";
        case 4U: return "CATCH_MISSED";
        case 5U: return "CATCH_ITEM_REPLACEMENT";
        default: return "UNKNOWN";
    }
}

uint32_t direct_catch_outcome_wire(uint32_t status) {
    switch (status) {
        case 1U: return 1U;  // CatchOutcome.CAUGHT.
        case 2U: return 3U;  // CatchOutcome.BREAKOUT.
        case 3U: return 4U;  // CatchOutcome.FLED.
        case 4U: return 2U;  // CatchOutcome.MISSED.
        default: return 0U;
    }
}

bool direct_catch_status_removes_map_entity(uint32_t status) {
    return status == 1U || status == 3U;
}

void free_direct_catch_handle(ProbeContext &context, Il2CppGcHandle handle) {
    if (handle != 0U && context.il2cpp_api.gchandle_free != nullptr) {
        context.il2cpp_api.gchandle_free(handle);
    }
}

void mark_direct_catch_outcome_verified(ProbeContext &context) {
    std::lock_guard<std::mutex> lock(context.binding_mutex);
    context.binding.direct_catch_outcome_verified = true;
}

void set_pending_direct_catch_terminal(
    ProbeContext &context,
    uint32_t phase,
    uint32_t catch_outcome_wire,
    const std::string &captured_pokemon_id,
    const char *error_code,
    const char *message
) {
    std::lock_guard<std::mutex> lock(context.pending_direct_catch_mutex);
    if (!context.pending_direct_catch.active || context.pending_direct_catch.terminal) return;
    context.pending_direct_catch.terminal = true;
    context.pending_direct_catch.phase = phase;
    context.pending_direct_catch.catch_outcome_wire = catch_outcome_wire;
    context.pending_direct_catch.captured_pokemon_id = captured_pokemon_id;
    context.pending_direct_catch.error_code = error_code == nullptr ? "" : error_code;
    context.pending_direct_catch.message = message == nullptr ? "" : message;
    if (context.pending_direct_catch.command_id.empty() &&
        catch_outcome_wire != 1U && catch_outcome_wire != 4U) {
        (void)send_runtime_automation_event(
            context, pogo_runtime::RuntimeAutomationEventType::kCatchFailed,
            context.pending_direct_catch.encounter_id, catch_outcome_wire
        );
    }
}

void publish_pending_direct_catch_result(
    ProbeContext &context,
    const PendingDirectCatch &terminal
) {
    if (terminal.command_id.empty()) return;
    const bool sent = send_runtime_result(
        context.fd,
        terminal.command_id.c_str(),
        terminal.phase,
        terminal.error_code.c_str(),
        terminal.message.c_str(),
        terminal.catch_outcome_wire,
        terminal.captured_pokemon_id.empty() ? nullptr : terminal.captured_pokemon_id.c_str()
    );
    __android_log_print(
        sent ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "runtime direct catch late result command=%s phase=%u outcome=%u captured_id=%s sent=%d",
        terminal.command_id.c_str(),
        terminal.phase,
        terminal.catch_outcome_wire,
        terminal.captured_pokemon_id.empty() ? "0" : terminal.captured_pokemon_id.c_str(),
        sent ? 1 : 0
    );
}
}  // namespace

bool begin_pending_direct_catch(
    ProbeContext &context,
    const std::string &command_id,
    uint64_t encounter_id
) {
    if (encounter_id == 0U) return false;
    std::lock_guard<std::mutex> lock(context.pending_direct_catch_mutex);
    if (context.pending_direct_catch.active) return false;
    context.pending_direct_catch = PendingDirectCatch{};
    context.pending_direct_catch.active = true;
    context.pending_direct_catch.encounter_id = encounter_id;
    context.pending_direct_catch.command_id = command_id;
    context.pending_direct_catch.requested_at_ns =
        static_cast<uint64_t>(now_elapsed_nanos());
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "runtime direct catch promise tracking started id=%llu command=%s",
        static_cast<unsigned long long>(encounter_id),
        command_id.empty() ? "<native-auto>" : command_id.c_str()
    );
    return true;
}

bool runtime_direct_catch_is_pending(ProbeContext &context) {
    std::lock_guard<std::mutex> lock(context.pending_direct_catch_mutex);
    return context.pending_direct_catch.active && !context.pending_direct_catch.terminal;
}

void abandon_pending_direct_catch(ProbeContext &context, const char *reason) {
    Il2CppGcHandle handle = 0U;
    uint64_t encounter_id = 0U;
    {
        std::lock_guard<std::mutex> lock(context.pending_direct_catch_mutex);
        if (!context.pending_direct_catch.active) return;
        handle = context.pending_direct_catch.promise_gc_handle;
        encounter_id = context.pending_direct_catch.encounter_id;
        context.pending_direct_catch = PendingDirectCatch{};
    }
    free_direct_catch_handle(context, handle);
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "runtime direct catch promise tracking cleared id=%llu reason=%s",
        static_cast<unsigned long long>(encounter_id),
        reason == nullptr ? "unspecified" : reason
    );
}

bool retain_pending_direct_catch(
    ProbeContext &context,
    uint64_t encounter_id,
    void *promise
) {
    const Il2CppApi &api = context.il2cpp_api;
    if (promise == nullptr || api.gchandle_new == nullptr || api.gchandle_get_target == nullptr ||
        api.gchandle_free == nullptr) return false;
    const Il2CppGcHandle handle = api.gchandle_new(promise, false);
    if (handle == 0U) return false;
    bool retained = false;
    {
        std::lock_guard<std::mutex> lock(context.pending_direct_catch_mutex);
        if (context.pending_direct_catch.active &&
            context.pending_direct_catch.encounter_id == encounter_id &&
            context.pending_direct_catch.promise_gc_handle == 0U) {
            context.pending_direct_catch.promise_gc_handle = handle;
            retained = true;
        }
    }
    if (!retained) {
        free_direct_catch_handle(context, handle);
        return false;
    }
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "runtime direct catch promise retained id=%llu promise=%p gc_handle=0x%llx",
        static_cast<unsigned long long>(encounter_id),
        promise,
        static_cast<unsigned long long>(handle)
    );
    return true;
}

RuntimeDirectCatchPromisePollResult poll_pending_direct_catch_on_main_thread(
    ProbeContext &context
) {
    PendingDirectCatch pending;
    {
        std::lock_guard<std::mutex> lock(context.pending_direct_catch_mutex);
        pending = context.pending_direct_catch;
    }
    if (!pending.active) return RuntimeDirectCatchPromisePollResult::kNoPending;
    if (pending.terminal) return RuntimeDirectCatchPromisePollResult::kCompleted;
    const uint64_t now = static_cast<uint64_t>(now_elapsed_nanos());
    if (pending.requested_at_ns != 0U && now >= pending.requested_at_ns &&
        now - pending.requested_at_ns > kDirectCatchPromiseTimeoutNs) {
        set_pending_direct_catch_terminal(
            context,
            kCommandPhaseIndeterminate,
            0U,
            "",
            "direct_catch_promise_timeout",
            "TryCapture Promise did not complete before the native observation deadline"
        );
        return RuntimeDirectCatchPromisePollResult::kTimedOut;
    }
    if (pending.promise_gc_handle == 0U) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "runtime direct catch promise waiting for retained handle id=%llu",
            static_cast<unsigned long long>(pending.encounter_id)
        );
        return RuntimeDirectCatchPromisePollResult::kWaiting;
    }
    const Il2CppApi &api = context.il2cpp_api;
    void *promise = api.gchandle_get_target == nullptr
        ? nullptr : api.gchandle_get_target(pending.promise_gc_handle);
    if (promise == nullptr) {
        set_pending_direct_catch_terminal(
            context,
            kCommandPhaseIndeterminate,
            0U,
            "",
            "direct_catch_promise_target_unavailable",
            "TryCapture Promise GC target is no longer available"
        );
        return RuntimeDirectCatchPromisePollResult::kFailed;
    }
    bool complete_called = false;
    bool error_called = false;
    void *completed_value = nullptr;
    void *error_value = nullptr;
    int32_t complete_offset = -1;
    int32_t error_offset = -1;
    int32_t value_offset = -1;
    const bool fields_read =
        read_runtime_promise_field(api, promise, "completeCalled", &complete_called, &complete_offset) &&
        read_runtime_promise_field(api, promise, "errorCalled", &error_called, &error_offset) &&
        read_runtime_promise_field(api, promise, "completedValue", &completed_value, &value_offset) &&
        read_runtime_promise_field(api, promise, "errorValue", &error_value);
    uint32_t poll_count = 0U;
    {
        std::lock_guard<std::mutex> lock(context.pending_direct_catch_mutex);
        if (context.pending_direct_catch.active &&
            context.pending_direct_catch.promise_gc_handle == pending.promise_gc_handle) {
            poll_count = ++context.pending_direct_catch.poll_count;
        }
    }
    __android_log_print(
        fields_read ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "runtime direct catch promise state id=%llu promise=%p complete=%d error=%d value=%p error_value=%p poll=%u offsets=0x%x/0x%x/0x%x",
        static_cast<unsigned long long>(pending.encounter_id),
        promise,
        complete_called ? 1 : 0,
        error_called ? 1 : 0,
        completed_value,
        error_value,
        poll_count,
        complete_offset,
        error_offset,
        value_offset
    );
    if (!fields_read) {
        set_pending_direct_catch_terminal(
            context,
            kCommandPhaseIndeterminate,
            0U,
            "",
            "direct_catch_promise_layout_unavailable",
            "TryCapture Promise completion fields could not be read"
        );
        return RuntimeDirectCatchPromisePollResult::kFailed;
    }
    if (error_called) {
        set_pending_direct_catch_terminal(
            context,
            kCommandPhaseIndeterminate,
            0U,
            "",
            "direct_catch_promise_failed",
            "TryCapture Promise completed with an error"
        );
        return RuntimeDirectCatchPromisePollResult::kFailed;
    }
    if (!complete_called) return RuntimeDirectCatchPromisePollResult::kWaiting;
    if (completed_value == nullptr || api.object_get_class == nullptr) {
        set_pending_direct_catch_terminal(
            context,
            kCommandPhaseIndeterminate,
            0U,
            "",
            "direct_catch_result_unavailable",
            "TryCapture Promise completed without CatchPokemonOutProto"
        );
        return RuntimeDirectCatchPromisePollResult::kFailed;
    }
    if (!runtime_object_is_exact_class(
            api,
            completed_value,
            "Holoholo.Rpc",
            "CatchPokemonOutProto"
        )) {
        set_pending_direct_catch_terminal(
            context,
            kCommandPhaseIndeterminate,
            0U,
            "",
            "direct_catch_result_type_mismatch",
            "TryCapture Promise returned an unexpected managed result type"
        );
        return RuntimeDirectCatchPromisePollResult::kFailed;
    }
    Il2CppClass *result_class = api.object_get_class(completed_value);
    const void *status_field = find_runtime_promise_field(api, result_class, "Status");
    const void *captured_id_field = find_runtime_promise_field(
        api, result_class, "CapturedPokemonId"
    );
    if (status_field == nullptr || captured_id_field == nullptr || api.field_get_value == nullptr) {
        set_pending_direct_catch_terminal(
            context,
            kCommandPhaseIndeterminate,
            0U,
            "",
            "direct_catch_result_layout_unavailable",
            "CatchPokemonOutProto fields could not be read"
        );
        return RuntimeDirectCatchPromisePollResult::kFailed;
    }
    int32_t status_value = 0;
    uint64_t captured_pokemon_id = 0U;
    api.field_get_value(completed_value, const_cast<void *>(status_field), &status_value);
    api.field_get_value(completed_value, const_cast<void *>(captured_id_field), &captured_pokemon_id);
    const uint32_t status = status_value < 0 ? 0U : static_cast<uint32_t>(status_value);
    const uint32_t outcome_wire = direct_catch_outcome_wire(status);
    const std::string captured_id = captured_pokemon_id == 0U
        ? "" : std::to_string(captured_pokemon_id);
    const int32_t status_offset = api.field_get_offset == nullptr
        ? -1 : api.field_get_offset(status_field);
    const int32_t captured_offset = api.field_get_offset == nullptr
        ? -1 : api.field_get_offset(captured_id_field);
    __android_log_print(
        outcome_wire == 0U ? ANDROID_LOG_WARN : ANDROID_LOG_INFO,
        kLogTag,
        "runtime direct catch promise result id=%llu status=%u(%s) captured_id=%s offsets=status:0x%x captured:0x%x",
        static_cast<unsigned long long>(pending.encounter_id),
        status,
        direct_catch_status_name(status),
        captured_id.empty() ? "0" : captured_id.c_str(),
        status_offset,
        captured_offset
    );
    mark_direct_catch_outcome_verified(context);
    if (outcome_wire == 0U) {
        set_pending_direct_catch_terminal(
            context,
            kCommandPhaseIndeterminate,
            0U,
            "",
            "direct_catch_status_unhandled",
            "TryCapture Promise completed with a non-terminal catch status"
        );
        return RuntimeDirectCatchPromisePollResult::kFailed;
    }
    if (direct_catch_status_removes_map_entity(status)) {
        refresh_map_after_direct_catch(context, pending.encounter_id, status);
    }
    // Native auto-catch has no Kotlin command id, so publish a dedicated
    // telemetry event for the app-owned custom toast. Manual command catches
    // already produce their normal controller result and must not duplicate it.
    if (pending.command_id.empty()) {
        if (status == 1U) {
            (void)send_runtime_automation_event(
                context,
                pogo_runtime::RuntimeAutomationEventType::kPokemonCaught,
                pending.encounter_id,
                captured_pokemon_id
            );
        } else if (status == 3U) {
            (void)send_runtime_automation_event(
                context,
                pogo_runtime::RuntimeAutomationEventType::kPokemonFled,
                pending.encounter_id
            );
        }
    }
    if (status == 1U && captured_pokemon_id != 0U) {
        // The native transfer module consumes the same authoritative catch
        // result before this Promise is released. Kotlin is not involved in
        // the auto-transfer decision or trigger.
        on_runtime_catch_succeeded_on_main_thread(context, captured_pokemon_id);
    }
    set_pending_direct_catch_terminal(
        context,
        kCommandPhaseCompleted,
        outcome_wire,
        captured_id,
        "",
        captured_id.empty()
            ? "TryCapture Promise completed and map sync was requested"
            : "TryCapture Promise completed with a captured Pokémon id"
    );
    PendingDirectCatch terminal;
    {
        std::lock_guard<std::mutex> lock(context.pending_direct_catch_mutex);
        terminal = context.pending_direct_catch;
    }
    publish_pending_direct_catch_result(context, terminal);
    return RuntimeDirectCatchPromisePollResult::kCompleted;
}

bool consume_terminal_pending_direct_catch(
    ProbeContext &context,
    PendingDirectCatch *terminal
) {
    if (terminal == nullptr) return false;
    Il2CppGcHandle handle = 0U;
    {
        std::lock_guard<std::mutex> lock(context.pending_direct_catch_mutex);
        if (!context.pending_direct_catch.active || !context.pending_direct_catch.terminal) return false;
        *terminal = context.pending_direct_catch;
        handle = context.pending_direct_catch.promise_gc_handle;
        context.pending_direct_catch = PendingDirectCatch{};
    }
    free_direct_catch_handle(context, handle);
    return true;
}
