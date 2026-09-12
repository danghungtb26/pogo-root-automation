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
        std::lock_guard<std::mutex> lock(g_runtime_transfer_mutex);
        if (g_runtime_transfer.active &&
            g_runtime_transfer.promise_gc_handle == pending.promise_gc_handle) {
            poll_count = ++g_runtime_transfer.poll_count;
        }
    }
    __android_log_print(
        fields_read ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "native transfer promise state id=%llu promise=%p complete=%d error=%d "
        "value=%p error_value=%p poll=%u offsets=0x%x/0x%x/0x%x",
        static_cast<unsigned long long>(pending.pokemon_id),
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
        finish_runtime_transfer(
            context,
            kCommandPhaseIndeterminate,
            "transfer_promise_layout_unavailable",
            "ReleasePokemon Promise completion fields could not be read",
            true
        );
        return RuntimeTransferPromisePollResult::kFailed;
    }
    if (error_called) {
        finish_runtime_transfer(
            context,
            kCommandPhaseCompleted,
            "transfer_failed",
            "ReleasePokemon Promise completed with an error",
            false
        );
        return RuntimeTransferPromisePollResult::kFailed;
    }
    if (!complete_called) return RuntimeTransferPromisePollResult::kWaiting;
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
            kCommandPhaseCompleted,
            "",
            "ReleasePokemon completed with SUCCESS",
            false
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
