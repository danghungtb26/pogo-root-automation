// Full inventory refresh used as the postcondition for a successful discard.
// A completed RecycleItem response is not enough to reopen the mutation gate:
// the cache must finish its own UpdateInventory Promise and expose a valid
// main-thread snapshot first.

bool clear_runtime_discard_reconcile(
    ProbeContext &context,
    uint64_t action_id,
    Il2CppGcHandle expected_handle,
    bool clear_blocked
) {
    Il2CppGcHandle handle = 0U;
    bool cleared = false;
    {
        std::lock_guard<std::mutex> lock(g_runtime_discard_mutex);
        if (g_runtime_discard.action_id != action_id ||
            g_runtime_discard.reconcile_promise_gc_handle != expected_handle) return false;
        handle = g_runtime_discard.reconcile_promise_gc_handle;
        g_runtime_discard.reconcile_promise_gc_handle = 0U;
        g_runtime_discard.awaiting_reconcile = false;
        if (clear_blocked) g_runtime_discard.blocked = false;
        cleared = true;
    }
    if (cleared) free_runtime_discard_handle(context, handle);
    return cleared;
}

bool begin_runtime_discard_reconcile(ProbeContext &context) {
    if (!kDiscardExecutionEnabled) return false;
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    const Il2CppApi &api = context.il2cpp_api;
    if (!binding.inventory_cache_read_verified || binding.inventory_cache == nullptr ||
        binding.inventory_cache_update_inventory == nullptr || api.runtime_invoke == nullptr ||
        api.gchandle_new == nullptr || api.gchandle_free == nullptr ||
        api.gchandle_get_target == nullptr) return false;

    PendingRuntimeDiscard pending;
    {
        std::lock_guard<std::mutex> lock(g_runtime_discard_mutex);
        pending = g_runtime_discard;
    }
    if (pending.active || !pending.blocked || pending.awaiting_reconcile) return false;

    void *exception = nullptr;
    void *promise = api.runtime_invoke(
        binding.inventory_cache_update_inventory,
        binding.inventory_cache,
        nullptr,
        &exception
    );
    if (exception != nullptr || !is_probable_managed_pointer(promise)) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native discard reconcile refresh unavailable action=%llu promise=%p exception=%p",
            static_cast<unsigned long long>(pending.action_id),
            promise,
            exception
        );
        return false;
    }
    const Il2CppGcHandle handle = api.gchandle_new(promise, false);
    if (handle == 0U) return false;
    {
        std::lock_guard<std::mutex> lock(g_runtime_discard_mutex);
        if (g_runtime_discard.action_id != pending.action_id ||
            g_runtime_discard.active || !g_runtime_discard.blocked ||
            g_runtime_discard.awaiting_reconcile) {
            free_runtime_discard_handle(context, handle);
            return false;
        }
        g_runtime_discard.awaiting_reconcile = true;
        g_runtime_discard.reconcile_started_at_ns =
            static_cast<uint64_t>(now_elapsed_nanos());
        g_runtime_discard.reconcile_promise_gc_handle = handle;
    }
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "native discard reconcile refresh started action=%llu promise=%p gc_handle=0x%llx",
        static_cast<unsigned long long>(pending.action_id),
        promise,
        static_cast<unsigned long long>(handle)
    );
    return true;
}

RuntimeDiscardPromisePollResult poll_runtime_discard_reconcile(ProbeContext &context) {
    PendingRuntimeDiscard pending;
    {
        std::lock_guard<std::mutex> lock(g_runtime_discard_mutex);
        pending = g_runtime_discard;
    }
    if (!pending.awaiting_reconcile) return RuntimeDiscardPromisePollResult::kNoPending;
    const uint64_t now = static_cast<uint64_t>(now_elapsed_nanos());
    if (pending.reconcile_started_at_ns != 0U &&
        now >= pending.reconcile_started_at_ns &&
        now - pending.reconcile_started_at_ns > kRuntimeDiscardPromiseTimeoutNs) {
        (void)clear_runtime_discard_reconcile(
            context, pending.action_id, pending.reconcile_promise_gc_handle, false
        );
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native discard reconcile timeout action=%llu",
            static_cast<unsigned long long>(pending.action_id)
        );
        return RuntimeDiscardPromisePollResult::kTimedOut;
    }
    const Il2CppApi &api = context.il2cpp_api;
    void *promise = api.gchandle_get_target == nullptr
        ? nullptr : api.gchandle_get_target(pending.reconcile_promise_gc_handle);
    if (promise == nullptr) {
        (void)clear_runtime_discard_reconcile(
            context, pending.action_id, pending.reconcile_promise_gc_handle, false
        );
        return RuntimeDiscardPromisePollResult::kFailed;
    }
    bool complete_called = false;
    bool error_called = false;
    void *completed_value = nullptr;
    void *error_value = nullptr;
    const bool fields_read =
        read_runtime_promise_field(api, promise, "completeCalled", &complete_called) &&
        read_runtime_promise_field(api, promise, "errorCalled", &error_called) &&
        read_runtime_promise_field(api, promise, "completedValue", &completed_value) &&
        read_runtime_promise_field(api, promise, "errorValue", &error_value);
    if (!fields_read || error_called) {
        (void)clear_runtime_discard_reconcile(
            context, pending.action_id, pending.reconcile_promise_gc_handle, false
        );
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native discard reconcile unknown action=%llu fields=%d error=%d error_value=%p",
            static_cast<unsigned long long>(pending.action_id),
            fields_read ? 1 : 0,
            error_called ? 1 : 0,
            error_value
        );
        return RuntimeDiscardPromisePollResult::kFailed;
    }
    if (!complete_called) return RuntimeDiscardPromisePollResult::kWaiting;
    if (completed_value == nullptr ||
        !runtime_object_is_exact_class(
            api, completed_value, "Holoholo.Rpc", "GetHoloholoInventoryOutProto"
        )) {
        (void)clear_runtime_discard_reconcile(
            context, pending.action_id, pending.reconcile_promise_gc_handle, false
        );
        return RuntimeDiscardPromisePollResult::kFailed;
    }
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    pogo_runtime::RuntimeInventoryObservation inventory;
    const bool snapshot_valid = binding.owner_generation == pending.owner_generation &&
        read_runtime_inventory(api, binding, &inventory);
    if (!snapshot_valid) {
        (void)clear_runtime_discard_reconcile(
            context, pending.action_id, pending.reconcile_promise_gc_handle, false
        );
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native discard reconcile snapshot unavailable action=%llu owner=%llu/%llu",
            static_cast<unsigned long long>(pending.action_id),
            static_cast<unsigned long long>(pending.owner_generation),
            static_cast<unsigned long long>(binding.owner_generation)
        );
        return RuntimeDiscardPromisePollResult::kFailed;
    }
    const bool cleared = clear_runtime_discard_reconcile(
        context, pending.action_id, pending.reconcile_promise_gc_handle, true
    );
    __android_log_print(
        cleared ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "native discard reconcile complete action=%llu cleared=%d stacks=%zu",
        static_cast<unsigned long long>(pending.action_id),
        cleared ? 1 : 0,
        inventory.items.size()
    );
    return cleared
        ? RuntimeDiscardPromisePollResult::kCompleted
        : RuntimeDiscardPromisePollResult::kFailed;
}
