// Owns the Promise returned by PoiItemSpinner.ckko(). The server-side Spin
// request is asynchronous, so the native observer keeps the Promise and reads
// its completion fields only from Unity's main thread.

namespace {
constexpr uint64_t kRuntimeSpinPromiseTimeoutNs = 15000000000ULL;

struct PendingRuntimeSpin {
    bool active = false;
    bool terminal = false;
    std::string fort_id;
    std::string command_id;
    Il2CppGcHandle promise_gc_handle = 0U;
    Il2CppGcHandle spinner_gc_handle = 0U;
    uint64_t requested_at_ns = 0U;
    uint32_t poll_count = 0U;
    int32_t server_result = 0;
    int64_t cooldown_complete_ms = 0;
    std::string error_code;
    std::string message;
};

std::mutex g_runtime_spin_mutex;
PendingRuntimeSpin g_runtime_spin;

const char *runtime_spin_result_name(int32_t result) {
    switch (result) {
        case 0: return "NO_RESULT_SET";
        case 1: return "SUCCESS";
        case 2: return "OUT_OF_RANGE";
        case 3: return "IN_COOLDOWN_PERIOD";
        case 4: return "INVENTORY_FULL";
        case 5: return "EXCEEDED_DAILY_LIMIT";
        case 6: return "POI_INACCESSIBLE";
        default: return "UNKNOWN";
    }
}

void free_runtime_spin_handles(
    ProbeContext &context,
    Il2CppGcHandle promise_handle,
    Il2CppGcHandle spinner_handle
) {
    if (promise_handle != 0U && context.il2cpp_api.gchandle_free != nullptr) {
        context.il2cpp_api.gchandle_free(promise_handle);
    }
    if (spinner_handle != 0U && context.il2cpp_api.gchandle_free != nullptr) {
        context.il2cpp_api.gchandle_free(spinner_handle);
    }
}

void publish_runtime_spin_terminal(
    ProbeContext &context,
    const PendingRuntimeSpin &terminal
) {
    if (terminal.command_id.empty()) return;
    const bool sent = send_runtime_result(
        context.fd,
        terminal.command_id.c_str(),
        kCommandPhaseCompleted,
        terminal.error_code.c_str(),
        terminal.message.c_str()
    );
    __android_log_print(
        sent ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "native spin late result command=%s fort=%s server_result=%d(%s) sent=%d",
        terminal.command_id.c_str(),
        terminal.fort_id.c_str(),
        terminal.server_result,
        runtime_spin_result_name(terminal.server_result),
        sent ? 1 : 0
    );
}

void mark_runtime_spin_terminal(
    ProbeContext &context,
    int32_t server_result,
    const char *error_code,
    const char *message
) {
    PendingRuntimeSpin terminal;
    {
        std::lock_guard<std::mutex> lock(g_runtime_spin_mutex);
        if (!g_runtime_spin.active || g_runtime_spin.terminal) return;
        g_runtime_spin.terminal = true;
        g_runtime_spin.server_result = server_result;
        g_runtime_spin.error_code = error_code == nullptr ? "" : error_code;
        g_runtime_spin.message = message == nullptr ? "" : message;
        terminal = g_runtime_spin;
    }
    publish_runtime_spin_terminal(context, terminal);
}
}  // namespace

bool begin_pending_runtime_spin(
    ProbeContext &context,
    const std::string &fort_id,
    const char *command_id
) {
    (void)context;
    if (fort_id.empty()) return false;
    std::lock_guard<std::mutex> lock(g_runtime_spin_mutex);
    if (g_runtime_spin.active) {
        return !g_runtime_spin.terminal && g_runtime_spin.fort_id == fort_id;
    }
    g_runtime_spin = PendingRuntimeSpin{};
    g_runtime_spin.active = true;
    g_runtime_spin.fort_id = fort_id;
    g_runtime_spin.command_id = command_id == nullptr ? "" : command_id;
    g_runtime_spin.requested_at_ns = static_cast<uint64_t>(now_elapsed_nanos());
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "native spin promise tracking started fort=%s command=%s",
        fort_id.c_str(),
        g_runtime_spin.command_id.empty() ? "<native-auto>" : g_runtime_spin.command_id.c_str()
    );
    return true;
}

bool runtime_spin_is_pending(ProbeContext &context) {
    (void)context;
    std::lock_guard<std::mutex> lock(g_runtime_spin_mutex);
    return g_runtime_spin.active && !g_runtime_spin.terminal;
}

void abandon_pending_runtime_spin(ProbeContext &context, const char *reason) {
    Il2CppGcHandle promise_handle = 0U;
    Il2CppGcHandle spinner_handle = 0U;
    std::string fort_id;
    {
        std::lock_guard<std::mutex> lock(g_runtime_spin_mutex);
        if (!g_runtime_spin.active) return;
        promise_handle = g_runtime_spin.promise_gc_handle;
        spinner_handle = g_runtime_spin.spinner_gc_handle;
        fort_id = g_runtime_spin.fort_id;
        g_runtime_spin = PendingRuntimeSpin{};
    }
    free_runtime_spin_handles(context, promise_handle, spinner_handle);
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "native spin promise tracking cleared fort=%s reason=%s",
        fort_id.c_str(),
        reason == nullptr ? "unspecified" : reason
    );
}

bool retain_pending_runtime_spin(
    ProbeContext &context,
    const std::string &fort_id,
    void *spinner,
    void *promise
) {
    const Il2CppApi &api = context.il2cpp_api;
    if (fort_id.empty() || !is_probable_managed_pointer(spinner) ||
        !is_probable_managed_pointer(promise) || api.gchandle_new == nullptr ||
        api.gchandle_get_target == nullptr || api.gchandle_free == nullptr) return false;
    const Il2CppGcHandle spinner_handle = api.gchandle_new(spinner, false);
    if (spinner_handle == 0U) return false;
    const Il2CppGcHandle promise_handle = api.gchandle_new(promise, false);
    if (promise_handle == 0U) {
        free_runtime_spin_handles(context, 0U, spinner_handle);
        return false;
    }
    bool retained = false;
    {
        std::lock_guard<std::mutex> lock(g_runtime_spin_mutex);
        if (g_runtime_spin.active && !g_runtime_spin.terminal &&
            g_runtime_spin.fort_id == fort_id &&
            g_runtime_spin.promise_gc_handle == 0U &&
            g_runtime_spin.spinner_gc_handle == 0U) {
            g_runtime_spin.promise_gc_handle = promise_handle;
            g_runtime_spin.spinner_gc_handle = spinner_handle;
            retained = true;
        }
    }
    if (!retained) {
        free_runtime_spin_handles(context, promise_handle, spinner_handle);
        return false;
    }
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "native spin FortSearch RPC 101 sent fort=%s promise=%p promise_gc=0x%llx spinner_gc=0x%llx",
        fort_id.c_str(),
        promise,
        static_cast<unsigned long long>(promise_handle),
        static_cast<unsigned long long>(spinner_handle)
    );
    return true;
}

RuntimeSpinPromisePollResult poll_pending_runtime_spin_on_main_thread(
    ProbeContext &context
) {
    PendingRuntimeSpin pending;
    {
        std::lock_guard<std::mutex> lock(g_runtime_spin_mutex);
        pending = g_runtime_spin;
    }
    if (!pending.active) return RuntimeSpinPromisePollResult::kNoPending;
    if (pending.terminal) return RuntimeSpinPromisePollResult::kCompleted;
    const uint64_t now = static_cast<uint64_t>(now_elapsed_nanos());
    if (pending.requested_at_ns != 0U && now >= pending.requested_at_ns &&
        now - pending.requested_at_ns > kRuntimeSpinPromiseTimeoutNs) {
        mark_runtime_spin_terminal(
            context,
            0,
            "spin_promise_timeout",
            "FortSearch Promise did not complete before the native deadline"
        );
        return RuntimeSpinPromisePollResult::kTimedOut;
    }
    const Il2CppApi &api = context.il2cpp_api;
    if (pending.promise_gc_handle == 0U || api.gchandle_get_target == nullptr) {
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "native spin Promise waiting for retained handle fort=%s",
            pending.fort_id.c_str()
        );
        return RuntimeSpinPromisePollResult::kWaiting;
    }
    void *promise = api.gchandle_get_target(pending.promise_gc_handle);
    if (promise == nullptr) {
        mark_runtime_spin_terminal(
            context,
            0,
            "spin_promise_target_unavailable",
            "FortSearch Promise GC target is no longer available"
        );
        return RuntimeSpinPromisePollResult::kFailed;
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
        std::lock_guard<std::mutex> lock(g_runtime_spin_mutex);
        if (g_runtime_spin.active &&
            g_runtime_spin.promise_gc_handle == pending.promise_gc_handle) {
            poll_count = ++g_runtime_spin.poll_count;
        }
    }
    __android_log_print(
        fields_read ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "native spin Promise state fort=%s promise=%p complete=%d error=%d value=%p error_value=%p poll=%u offsets=0x%x/0x%x/0x%x",
        pending.fort_id.c_str(),
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
        mark_runtime_spin_terminal(
            context,
            0,
            "spin_promise_layout_unavailable",
            "FortSearch Promise completion fields could not be read"
        );
        return RuntimeSpinPromisePollResult::kFailed;
    }
    if (error_called) {
        mark_runtime_spin_terminal(
            context,
            0,
            "spin_promise_failed",
            "FortSearch Promise completed with an error"
        );
        return RuntimeSpinPromisePollResult::kFailed;
    }
    if (!complete_called) return RuntimeSpinPromisePollResult::kWaiting;
    if (completed_value == nullptr || !runtime_object_is_exact_class(
            api, completed_value, "Holoholo.Rpc", "FortSearchOutProto")) {
        mark_runtime_spin_terminal(
            context,
            0,
            "spin_result_type_mismatch",
            "FortSearch Promise returned an unexpected result type"
        );
        return RuntimeSpinPromisePollResult::kFailed;
    }
    int32_t server_result = 0;
    int64_t cooldown_complete_ms = 0;
    if (!read_runtime_field(api, completed_value, "Result", &server_result)) {
        mark_runtime_spin_terminal(
            context,
            0,
            "spin_result_layout_unavailable",
            "FortSearchOutProto.Result could not be read"
        );
        return RuntimeSpinPromisePollResult::kFailed;
    }
    const bool cooldown_read = read_runtime_field(
        api, completed_value, "CooldownComplete", &cooldown_complete_ms
    );
    if (!cooldown_read) cooldown_complete_ms = 0;
    std::string response_fort_id;
    void *managed_response_fort_id = nullptr;
    if (read_runtime_field(api, completed_value, "FortId", &managed_response_fort_id)) {
        (void)read_runtime_managed_ascii_string(managed_response_fort_id, &response_fort_id);
    }
    __android_log_print(
        server_result == 1 ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "native spin FortSearch Promise result fort=%s response_fort=%s result=%d(%s) "
        "cooldown_complete_ms=%lld cooldown_read=%d",
        pending.fort_id.c_str(),
        response_fort_id.empty() ? "<empty>" : response_fort_id.c_str(),
        server_result,
        runtime_spin_result_name(server_result),
        static_cast<long long>(cooldown_complete_ms),
        cooldown_read ? 1 : 0
    );

    {
        std::lock_guard<std::mutex> lock(g_runtime_spin_mutex);
        if (g_runtime_spin.active &&
            g_runtime_spin.promise_gc_handle == pending.promise_gc_handle) {
            g_runtime_spin.cooldown_complete_ms = cooldown_complete_ms;
        }
    }

    const RuntimeBinding binding = runtime_binding_snapshot(context);
    if (server_result == 1 && binding.poi_item_spinner_result_callback != nullptr) {
        void *spinner = api.gchandle_get_target == nullptr
            ? nullptr : api.gchandle_get_target(pending.spinner_gc_handle);
        if (spinner != nullptr && runtime_object_is_exact_class(
                api, spinner, "Niantic.Holoholo.Map", "PoiItemSpinner")) {
            void *callback_arguments[] = {completed_value};
            void *callback_exception = nullptr;
            api.runtime_invoke(
                binding.poi_item_spinner_result_callback,
                spinner,
                callback_arguments,
                &callback_exception
            );
            __android_log_print(
                callback_exception == nullptr ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
                kLogTag,
                "native spin result callback invoked fort=%s method=%p exception=%p",
                pending.fort_id.c_str(),
                binding.poi_item_spinner_result_callback,
                callback_exception
            );
        } else {
            __android_log_print(
                ANDROID_LOG_WARN,
                kLogTag,
                "native spin result callback skipped fort=%s spinner=%p",
                pending.fort_id.c_str(),
                spinner
            );
        }
    }
    if ((server_result == 1 || server_result == 3) && cooldown_complete_ms > 0) {
        refresh_runtime_pokestop_cooldown(
            context, pending.fort_id, cooldown_complete_ms
        );
    }
    mark_runtime_spin_terminal(
        context,
        server_result,
        server_result == 1 ? "" : "spin_server_rejected",
        server_result == 1
            ? "FortSearch completed with SUCCESS"
            : "FortSearch completed without SUCCESS"
    );
    return RuntimeSpinPromisePollResult::kCompleted;
}

bool consume_terminal_runtime_spin(
    ProbeContext &context,
    std::string *fort_id,
    int32_t *server_result,
    int64_t *cooldown_complete_ms
) {
    if (fort_id == nullptr || server_result == nullptr || cooldown_complete_ms == nullptr) {
        return false;
    }
    Il2CppGcHandle promise_handle = 0U;
    Il2CppGcHandle spinner_handle = 0U;
    {
        std::lock_guard<std::mutex> lock(g_runtime_spin_mutex);
        if (!g_runtime_spin.active || !g_runtime_spin.terminal) return false;
        *fort_id = g_runtime_spin.fort_id;
        *server_result = g_runtime_spin.server_result;
        *cooldown_complete_ms = g_runtime_spin.cooldown_complete_ms;
        promise_handle = g_runtime_spin.promise_gc_handle;
        spinner_handle = g_runtime_spin.spinner_gc_handle;
        g_runtime_spin = PendingRuntimeSpin{};
    }
    free_runtime_spin_handles(context, promise_handle, spinner_handle);
    return true;
}

void reset_runtime_spin_state(ProbeContext &context) {
    Il2CppGcHandle promise_handle = 0U;
    Il2CppGcHandle spinner_handle = 0U;
    std::string fort_id;
    {
        std::lock_guard<std::mutex> lock(g_runtime_spin_mutex);
        if (!g_runtime_spin.active) return;
        promise_handle = g_runtime_spin.promise_gc_handle;
        spinner_handle = g_runtime_spin.spinner_gc_handle;
        fort_id = g_runtime_spin.fort_id;
        g_runtime_spin = PendingRuntimeSpin{};
    }
    free_runtime_spin_handles(context, promise_handle, spinner_handle);
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "native spin Promise state reset fort=%s",
        fort_id.c_str()
    );
}
