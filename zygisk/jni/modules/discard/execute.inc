// Native auto/manual discard state machine. RecycleItem and its Promise are
// always created and inspected on Unity's main thread.

constexpr bool kDiscardExecutionEnabled = true;
constexpr uint64_t kRuntimeDiscardPromiseTimeoutNs = 15000000000ULL;

struct PendingRuntimeDiscard {
    bool active = false;
    bool blocked = false;
    int32_t item_id = 0;
    int32_t amount = 0;
    uint64_t started_at_ns = 0U;
    uint32_t poll_count = 0U;
    Il2CppGcHandle promise_gc_handle = 0U;
    std::string command_id;
};

std::mutex g_runtime_discard_mutex;
PendingRuntimeDiscard g_runtime_discard;

void free_runtime_discard_handle(ProbeContext &context, Il2CppGcHandle handle) {
    if (handle != 0U && context.il2cpp_api.gchandle_free != nullptr) {
        context.il2cpp_api.gchandle_free(handle);
    }
}

bool runtime_discard_blocks_catch_spin() {
    std::lock_guard<std::mutex> lock(g_runtime_discard_mutex);
    return g_runtime_discard.active || g_runtime_discard.blocked;
}

bool runtime_discard_has_pending() {
    std::lock_guard<std::mutex> lock(g_runtime_discard_mutex);
    return g_runtime_discard.active;
}

void finish_runtime_discard(
    ProbeContext &context,
    uint32_t phase,
    const char *error_code,
    const char *message,
    bool blocked
) {
    Il2CppGcHandle handle = 0U;
    int32_t item_id = 0;
    int32_t amount = 0;
    std::string command_id;
    {
        std::lock_guard<std::mutex> lock(g_runtime_discard_mutex);
        if (!g_runtime_discard.active) return;
        handle = g_runtime_discard.promise_gc_handle;
        item_id = g_runtime_discard.item_id;
        amount = g_runtime_discard.amount;
        command_id = g_runtime_discard.command_id;
        g_runtime_discard.active = false;
        g_runtime_discard.promise_gc_handle = 0U;
        g_runtime_discard.command_id.clear();
        g_runtime_discard.blocked = blocked;
    }
    free_runtime_discard_handle(context, handle);
    const bool success = phase == kCommandPhaseCompleted &&
        (error_code == nullptr || error_code[0] == '\0');
    __android_log_print(
        success ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "native discard result item_id=%d amount=%d mode=%s phase=%u success=%d blocked=%d "
        "error=%s message=%s",
        item_id,
        amount,
        command_id.empty() ? "auto" : "manual",
        phase,
        success ? 1 : 0,
        blocked ? 1 : 0,
        error_code == nullptr ? "" : error_code,
        message == nullptr ? "" : message
    );
    if (success) {
        (void)send_runtime_automation_event(
            context,
            pogo_runtime::RuntimeAutomationEventType::kItemDiscarded,
            static_cast<uint64_t>(item_id),
            static_cast<uint64_t>(amount)
        );
    } else {
        (void)send_runtime_automation_event(
            context,
            pogo_runtime::RuntimeAutomationEventType::kItemDiscardFailed,
            static_cast<uint64_t>(item_id),
            static_cast<uint64_t>(amount),
            message
        );
    }
    if (!command_id.empty()) {
        (void)send_runtime_result(
            context.fd,
            command_id.c_str(),
            phase,
            error_code == nullptr ? "" : error_code,
            message == nullptr ? "runtime discard completed" : message
        );
    }
}

void reset_runtime_discard_state(ProbeContext &context) {
    Il2CppGcHandle handle = 0U;
    {
        std::lock_guard<std::mutex> lock(g_runtime_discard_mutex);
        handle = g_runtime_discard.promise_gc_handle;
        g_runtime_discard = PendingRuntimeDiscard{};
    }
    free_runtime_discard_handle(context, handle);
}

#include "recycle_item_action.inc"

MainThreadActionOutcome start_runtime_discard_on_main_thread(
    ProbeContext &context,
    int32_t item_id,
    int32_t amount,
    const char *command_id
) {
    if (!kDiscardExecutionEnabled || item_id <= 0 || amount <= 0) {
        return MainThreadActionOutcome::kRejected;
    }
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    const Il2CppApi &api = context.il2cpp_api;
    if (!binding.discard_verified || binding.item_bag == nullptr ||
        binding.item_bag_recycle_item == nullptr || binding.item_data_class == nullptr ||
        api.gchandle_new == nullptr || api.gchandle_get_target == nullptr ||
        api.gchandle_free == nullptr) return MainThreadActionOutcome::kRejected;
    {
        std::lock_guard<std::mutex> lock(g_runtime_discard_mutex);
        if (g_runtime_discard.active || g_runtime_discard.blocked) {
            return MainThreadActionOutcome::kIndeterminate;
        }
        g_runtime_discard = PendingRuntimeDiscard{};
        g_runtime_discard.active = true;
        g_runtime_discard.item_id = item_id;
        g_runtime_discard.amount = amount;
        g_runtime_discard.started_at_ns = static_cast<uint64_t>(now_elapsed_nanos());
        g_runtime_discard.command_id = command_id == nullptr ? "" : command_id;
    }
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "native discard trigger item_id=%d amount=%d mode=%s action=RecycleItem",
        item_id,
        amount,
        command_id == nullptr || command_id[0] == '\0' ? "auto" : "manual"
    );
    (void)send_runtime_automation_event(
        context,
        pogo_runtime::RuntimeAutomationEventType::kItemDiscardTriggered,
        static_cast<uint64_t>(item_id),
        static_cast<uint64_t>(amount)
    );

    void *exception = nullptr;
    bool invoked = false;
    const char *failure = "Inventory unavailable";
    void *promise = recycle_runtime_item_on_main_thread(
        context, item_id, amount, &exception, &invoked, &failure);
    if (exception != nullptr || !is_probable_managed_pointer(promise)) {
        finish_runtime_discard(
            context,
            invoked ? kCommandPhaseIndeterminate : kCommandPhaseRejected,
            invoked ? "discard_invoke_indeterminate" : "discard_preparation_failed",
            failure,
            invoked
        );
        return invoked ? MainThreadActionOutcome::kIndeterminate : MainThreadActionOutcome::kRejected;
    }
    const Il2CppGcHandle handle = api.gchandle_new(promise, false);
    if (handle == 0U) {
        finish_runtime_discard(
            context,
            kCommandPhaseIndeterminate,
            "discard_promise_retention_failed",
            "RecycleItem Promise could not be retained",
            true
        );
        return MainThreadActionOutcome::kIndeterminate;
    }
    {
        std::lock_guard<std::mutex> lock(g_runtime_discard_mutex);
        if (!g_runtime_discard.active || g_runtime_discard.item_id != item_id ||
            g_runtime_discard.amount != amount) {
            free_runtime_discard_handle(context, handle);
            return MainThreadActionOutcome::kIndeterminate;
        }
        g_runtime_discard.promise_gc_handle = handle;
    }
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "native discard invoke retained item_id=%d amount=%d promise=%p gc_handle=0x%llx",
        item_id,
        amount,
        promise,
        static_cast<unsigned long long>(handle)
    );
    (void)poll_pending_runtime_discard_on_main_thread(context);
    return MainThreadActionOutcome::kInvoked;
}

#include "promise_observer.inc"

RuntimeActionResult execute_runtime_discard(
    ProbeContext &context,
    const RuntimeActionCommand &command
) {
    RuntimeActionResult result{};
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    if (!kDiscardExecutionEnabled || !binding.exact_build_verified || !binding.discard_verified) {
        result.error_code = "binding_unavailable";
        result.message = "direct item discard is unavailable for this verified build";
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
    if (command.expires_at_elapsed_ns <= static_cast<uint64_t>(now_elapsed_nanos())) {
        result.error_code = "command_expired";
        result.message = "discard command expired before invocation";
        return result;
    }
    if (command.discard_item_id <= 0 || command.discard_amount <= 0) {
        result.error_code = "invalid_discard_command";
        result.message = "discard item and amount must be positive";
        return result;
    }
    if (command.command_id == context.last_action_command_id) {
        result.error_code = "duplicate_command";
        result.message = "command was already attempted in this runtime session";
        return result;
    }
    context.last_action_command_id = command.command_id;
    const MainThreadActionOutcome outcome = request_main_thread_start_runtime_discard(
        context,
        command.discard_item_id,
        command.discard_amount,
        command.command_id.c_str()
    );
    if (outcome == MainThreadActionOutcome::kInvoked) {
        result.phase = kCommandPhaseIndeterminate;
        result.error_code = "discard_promise_pending";
        result.message = "RecycleItem was invoked; native is waiting for its Promise";
    } else if (outcome == MainThreadActionOutcome::kIndeterminate) {
        result.phase = kCommandPhaseIndeterminate;
        result.error_code = "invoke_outcome_indeterminate";
        result.message = "RecycleItem may have run but the result was lost";
    } else {
        result.error_code = "discard_target_unavailable";
        result.message = "the requested item could not be recycled";
    }
    return result;
}
