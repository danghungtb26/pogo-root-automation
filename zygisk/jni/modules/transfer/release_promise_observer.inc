// Polls the ReleasePokemon Promise after the transfer executor has retained it.

RuntimeTransferPromisePollResult poll_release_promise_on_main_thread(
    ProbeContext &context,
    const PendingRuntimeTransfer &pending
) {
    const Il2CppApi &api = context.il2cpp_api;
    void *promise = api.gchandle_get_target == nullptr
        ? nullptr : api.gchandle_get_target(pending.promise_gc_handle);
    if (promise == nullptr) {
        finish_runtime_transfer(
            context,
            kCommandPhaseIndeterminate,
            "transfer_promise_target_unavailable",
            "ReleasePokemon Promise GC target is no longer available",
            true
        );
        return RuntimeTransferPromisePollResult::kFailed;
    }
    void *completed_value = nullptr;
    const RuntimePromiseSinkPollResult sink_result =
        poll_runtime_promise_result_sink(api, pending.result_sink, &completed_value);
    uint32_t poll_count = 0U;
    {
        std::lock_guard<std::mutex> lock(g_runtime_transfer_mutex);
        if (g_runtime_transfer.active &&
            g_runtime_transfer.promise_gc_handle == pending.promise_gc_handle) {
            poll_count = ++g_runtime_transfer.poll_count;
        }
    }
    if (poll_count == 1U) {
        log_runtime_object(api, promise, "ReleasePokemon Promise", nullptr);
    }
    __android_log_print(
        sink_result == RuntimePromiseSinkPollResult::kFailed
            ? ANDROID_LOG_WARN : ANDROID_LOG_INFO,
        kLogTag,
        "native transfer promise sink id=%llu promise=%p state=%u value=%p poll=%u",
        static_cast<unsigned long long>(pending.pokemon_id),
        promise,
        static_cast<uint32_t>(sink_result),
        completed_value,
        poll_count
    );
    if (sink_result == RuntimePromiseSinkPollResult::kFailed) {
        finish_runtime_transfer(
            context,
            kCommandPhaseIndeterminate,
            "transfer_promise_sink_failed",
            "ReleasePokemon Promise callback sink could not be read",
            true
        );
        return RuntimeTransferPromisePollResult::kFailed;
    }
    if (sink_result == RuntimePromiseSinkPollResult::kError) {
        finish_runtime_transfer(
            context,
            kCommandPhaseCompleted,
            "transfer_failed",
            "ReleasePokemon Promise completed with an error",
            false
        );
        return RuntimeTransferPromisePollResult::kFailed;
    }
    if (sink_result == RuntimePromiseSinkPollResult::kWaiting) {
        return RuntimeTransferPromisePollResult::kWaiting;
    }
    if (completed_value == nullptr || api.object_get_class == nullptr ||
        !runtime_object_is_exact_class(
            api, completed_value, "Holoholo.Rpc", "ReleasePokemonOutProto"
        )) {
        finish_runtime_transfer(
            context,
            kCommandPhaseIndeterminate,
            "transfer_result_type_mismatch",
            "ReleasePokemon Promise returned an unexpected result type",
            true
        );
        return RuntimeTransferPromisePollResult::kFailed;
    }
    int32_t status = 0;
    if (!read_runtime_field(api, completed_value, "Status", &status)) {
        finish_runtime_transfer(
            context,
            kCommandPhaseIndeterminate,
            "transfer_result_layout_unavailable",
            "ReleasePokemonOutProto.Status could not be read",
            true
        );
        return RuntimeTransferPromisePollResult::kFailed;
    }
    __android_log_print(
        status == 1 ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "native transfer promise result id=%llu status=%d",
        static_cast<unsigned long long>(pending.pokemon_id),
        status
    );
    if (status == 1) {
        finish_runtime_transfer(
            context,
            kCommandPhaseIndeterminate,
            "transfer_postcondition_unverified",
            "ReleasePokemon returned SUCCESS but authoritative storage refresh is unverified",
            true
        );
    } else {
        finish_runtime_transfer(
            context,
            kCommandPhaseCompleted,
            "transfer_rejected",
            "ReleasePokemon completed without SUCCESS",
            false
        );
    }
    return RuntimeTransferPromisePollResult::kCompleted;
}
