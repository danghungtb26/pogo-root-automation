// Read the retained RecycleItem Promise on Unity main thread.
RuntimeDiscardPromisePollResult poll_pending_runtime_discard_on_main_thread(
    ProbeContext &context
) {
    PendingRuntimeDiscard pending;
    {
        std::lock_guard<std::mutex> lock(g_runtime_discard_mutex);
        pending = g_runtime_discard;
    }
    if (pending.awaiting_reconcile) {
        return poll_runtime_discard_reconcile(context);
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
    void *completed_value = nullptr;
    const RuntimePromiseSinkPollResult sink_result =
        poll_runtime_promise_result_sink(api, pending.result_sink, &completed_value);
    uint32_t poll_count = 0U;
    {
        std::lock_guard<std::mutex> lock(g_runtime_discard_mutex);
        if (g_runtime_discard.active &&
            g_runtime_discard.promise_gc_handle == pending.promise_gc_handle) {
            poll_count = ++g_runtime_discard.poll_count;
        }
    }
    if (poll_count == 1U) {
        log_runtime_object(api, promise, "RecycleItem Promise", nullptr);
    }
    __android_log_print(
        sink_result == RuntimePromiseSinkPollResult::kFailed
            ? ANDROID_LOG_WARN : ANDROID_LOG_INFO,
        kLogTag,
        "native discard promise sink item_id=%d amount=%d promise=%p state=%u "
        "value=%p poll=%u",
        pending.item_id,
        pending.amount,
        promise,
        static_cast<uint32_t>(sink_result),
        completed_value,
        poll_count
    );
    if (sink_result == RuntimePromiseSinkPollResult::kFailed) {
        finish_runtime_discard(
            context,
            kCommandPhaseIndeterminate,
            "discard_promise_sink_failed",
            "RecycleItem Promise callback sink could not be read",
            true
        );
        return RuntimeDiscardPromisePollResult::kFailed;
    }
    if (sink_result == RuntimePromiseSinkPollResult::kError) {
        finish_runtime_discard(
            context,
            kCommandPhaseIndeterminate,
            "discard_transport_unknown",
            "RecycleItem Promise completed with an error",
            true
        );
        return RuntimeDiscardPromisePollResult::kFailed;
    }
    if (sink_result == RuntimePromiseSinkPollResult::kWaiting) {
        return RuntimeDiscardPromisePollResult::kWaiting;
    }
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
            kCommandPhaseIndeterminate,
            "discard_postcondition_unverified",
            "RecycleItem returned SUCCESS but authoritative inventory refresh is unverified",
            true
        );
    } else if (result == 2) {
        finish_runtime_discard(
            context,
            kCommandPhaseCompleted,
            "discard_not_enough_copies",
            "RecycleItem rejected: not enough copies",
            false
        );
    } else if (result == 3) {
        finish_runtime_discard(
            context,
            kCommandPhaseCompleted,
            "discard_cannot_recycle_incubator",
            "RecycleItem rejected: incubator cannot be recycled",
            false
        );
    } else {
        finish_runtime_discard(
            context,
            kCommandPhaseIndeterminate,
            "discard_result_unknown",
            "RecycleItem completed with an unknown result",
            true
        );
    }
    return RuntimeDiscardPromisePollResult::kCompleted;
}
