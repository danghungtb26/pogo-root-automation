// Read the retained RecycleItem Promise on Unity main thread.
RuntimeDiscardPromisePollResult poll_pending_runtime_discard_on_main_thread(
    ProbeContext &context
) {
    PendingRuntimeDiscard pending;
    {
        std::lock_guard<std::mutex> lock(g_runtime_discard_mutex);
        pending = g_runtime_discard;
    }
    if (!pending.active) return RuntimeDiscardPromisePollResult::kNoPending;
    const uint64_t now = static_cast<uint64_t>(now_elapsed_nanos());
    if (pending.started_at_ns != 0U && now >= pending.started_at_ns &&
        now - pending.started_at_ns > kRuntimeDiscardPromiseTimeoutNs) {
        finish_runtime_discard(
            context,
            kCommandPhaseIndeterminate,
            "discard_promise_timeout",
            "RecycleItem Promise did not complete before the native deadline",
            true
        );
        return RuntimeDiscardPromisePollResult::kTimedOut;
    }

    const Il2CppApi &api = context.il2cpp_api;
    void *promise = api.gchandle_get_target == nullptr
        ? nullptr : api.gchandle_get_target(pending.promise_gc_handle);
    if (promise == nullptr) {
        finish_runtime_discard(
            context,
            kCommandPhaseIndeterminate,
            "discard_promise_target_unavailable",
            "RecycleItem Promise GC target is no longer available",
            true
        );
        return RuntimeDiscardPromisePollResult::kFailed;
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
        std::lock_guard<std::mutex> lock(g_runtime_discard_mutex);
        if (g_runtime_discard.active &&
            g_runtime_discard.promise_gc_handle == pending.promise_gc_handle) {
            poll_count = ++g_runtime_discard.poll_count;
        }
    }
    __android_log_print(
        fields_read ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "native discard promise state item_id=%d amount=%d promise=%p complete=%d error=%d "
        "value=%p error_value=%p poll=%u offsets=0x%x/0x%x/0x%x",
        pending.item_id,
        pending.amount,
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
        finish_runtime_discard(
            context,
            kCommandPhaseIndeterminate,
            "discard_promise_layout_unavailable",
            "RecycleItem Promise completion fields could not be read",
            true
        );
        return RuntimeDiscardPromisePollResult::kFailed;
    }
    if (error_called) {
        finish_runtime_discard(
            context,
            kCommandPhaseCompleted,
            "discard_failed",
            "RecycleItem Promise completed with an error",
            false
        );
        return RuntimeDiscardPromisePollResult::kFailed;
    }
    if (!complete_called) return RuntimeDiscardPromisePollResult::kWaiting;
    if (completed_value == nullptr ||
        !runtime_object_is_exact_class(api, completed_value, "Holoholo.Rpc", "RecycleItemOutProto")) {
        finish_runtime_discard(
            context,
            kCommandPhaseIndeterminate,
            "discard_result_type_mismatch",
            "RecycleItem Promise returned an unexpected result type",
            true
        );
        return RuntimeDiscardPromisePollResult::kFailed;
    }
    int32_t result = 0;
    int32_t new_count = -1;
    if (!read_runtime_field(api, completed_value, "Result", &result) ||
        !read_runtime_field(api, completed_value, "NewCount", &new_count)) {
        finish_runtime_discard(
            context,
            kCommandPhaseIndeterminate,
            "discard_result_layout_unavailable",
            "RecycleItemOutProto result fields could not be read",
            true
        );
        return RuntimeDiscardPromisePollResult::kFailed;
    }
    __android_log_print(
        result == 1 ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "native discard promise result item_id=%d amount=%d result=%d new_count=%d",
        pending.item_id,
        pending.amount,
        result,
        new_count
    );
    if (result == 1) {
        finish_runtime_discard(
            context,
            kCommandPhaseCompleted,
            "",
            "RecycleItem completed with SUCCESS",
            false
        );
    } else {
        finish_runtime_discard(
            context,
            kCommandPhaseCompleted,
            "discard_rejected",
            "RecycleItem completed without SUCCESS",
            false
        );
    }
    return RuntimeDiscardPromisePollResult::kCompleted;
}

