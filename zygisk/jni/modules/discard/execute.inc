// Native auto/manual discard state machine. RecycleItem and its Promise are
// always created and inspected on Unity's main thread.

// Static reverse resolution is not runtime calibration. Keep mutation closed
// until the supported build/ABI has completed the device contract in the issue
// plan; READ_INVENTORY may still be advertised independently.
constexpr bool kDiscardExecutionEnabled = false;
constexpr uint64_t kRuntimeDiscardPromiseTimeoutNs = 15000000000ULL;

bool begin_runtime_discard_reconcile(ProbeContext &context);
RuntimeDiscardPromisePollResult poll_runtime_discard_reconcile(ProbeContext &context);

struct PendingRuntimeDiscard {
    bool active = false;
    bool blocked = false;
    int32_t item_id = 0;
    int32_t amount = 0;
    uint64_t started_at_ns = 0U;
    uint32_t poll_count = 0U;
    bool automatic = false;
    uint64_t config_revision = 0U;
    uint64_t action_id = 0U;
    uint64_t owner_generation = 0U;
    uint64_t inventory_generation_before = 0U;
    bool awaiting_reconcile = false;
    uint64_t reconcile_started_at_ns = 0U;
    Il2CppGcHandle reconcile_promise_gc_handle = 0U;
    Il2CppGcHandle promise_gc_handle = 0U;
    std::string command_id;
};

struct RuntimeDiscardSuppression {
    int32_t item_id = 0;
    uint64_t config_revision = 0U;
    uint64_t owner_generation = 0U;
    int32_t count_at_failure = 0;
    const char *reason = nullptr;
};

std::mutex g_runtime_discard_mutex;
PendingRuntimeDiscard g_runtime_discard;
std::vector<RuntimeDiscardSuppression> g_runtime_discard_suppressions;
std::atomic<uint64_t> g_runtime_discard_next_action_id{1U};

bool runtime_discard_item_suppressed(
    int32_t item_id,
    uint64_t /*config_revision*/,
    int32_t current_count,
    uint64_t owner_generation
) {
    std::lock_guard<std::mutex> lock(g_runtime_discard_mutex);
    for (auto iterator = g_runtime_discard_suppressions.begin();
         iterator != g_runtime_discard_suppressions.end();) {
        if (iterator->item_id != item_id) {
            ++iterator;
            continue;
        }
        if (iterator->owner_generation != owner_generation ||
            iterator->count_at_failure != current_count) {
            iterator = g_runtime_discard_suppressions.erase(iterator);
            continue;
        }
        return true;
    }
    return false;
}

void suppress_runtime_discard_item(
    int32_t item_id,
    uint64_t config_revision,
    uint64_t owner_generation,
    int32_t count_at_failure,
    const char *reason
) {
    if (item_id <= 0) return;
    std::lock_guard<std::mutex> lock(g_runtime_discard_mutex);
    for (RuntimeDiscardSuppression &suppression : g_runtime_discard_suppressions) {
        if (suppression.item_id != item_id || suppression.config_revision != config_revision) continue;
        suppression.reason = reason;
        return;
    }
    g_runtime_discard_suppressions.push_back({
        item_id,
        config_revision,
        owner_generation,
        count_at_failure,
        reason,
    });
}

void clear_runtime_discard_suppressions() {
    std::lock_guard<std::mutex> lock(g_runtime_discard_mutex);
    g_runtime_discard_suppressions.clear();
}

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

bool runtime_discard_needs_maintenance() {
    std::lock_guard<std::mutex> lock(g_runtime_discard_mutex);
    return g_runtime_discard.active || g_runtime_discard.awaiting_reconcile;
}

bool runtime_discard_intent_is_current(
    ProbeContext &context,
    const RuntimeBinding &binding,
    int32_t item_id,
    int32_t amount
) {
    PendingRuntimeDiscard pending;
    {
        std::lock_guard<std::mutex> lock(g_runtime_discard_mutex);
        pending = g_runtime_discard;
    }
    if (!pending.active || pending.item_id != item_id || pending.amount != amount ||
        pending.owner_generation != binding.owner_generation) return false;
    const RuntimeDiscardConfig config = runtime_discard_config_snapshot();
    if (config.revision != pending.config_revision) return false;
    if (pending.automatic) {
        int32_t max_count = 0;
        if (!config.configured || !config.auto_discard ||
            !runtime_discard_config_find_limit(config, item_id, &max_count) ||
            !runtime_inventory_baseline_ready()) return false;
    }
    if (!runtime_inventory_baseline_ready()) return false;
    return context.il2cpp_api.runtime_invoke != nullptr;
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
    uint64_t owner_generation = 0U;
    int32_t count_at_failure = 0;
    {
        std::lock_guard<std::mutex> lock(g_runtime_discard_mutex);
        if (!g_runtime_discard.active) return;
        handle = g_runtime_discard.promise_gc_handle;
        item_id = g_runtime_discard.item_id;
        amount = g_runtime_discard.amount;
        owner_generation = g_runtime_discard.owner_generation;
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
        // Result=SUCCESS is a server outcome, not proof that the local bag has
        // reconciled. Keep the mutation barrier until a future authoritative
        // inventory/reconcile path clears it.
        std::lock_guard<std::mutex> lock(g_runtime_discard_mutex);
        g_runtime_discard.blocked = true;
    } else if (error_code != nullptr &&
        (strcmp(error_code, "discard_not_enough_copies") == 0 ||
         strcmp(error_code, "discard_cannot_recycle_incubator") == 0)) {
        const RuntimeDiscardConfig config = runtime_discard_config_snapshot();
        const RuntimeBinding binding = runtime_binding_snapshot(context);
        count_at_failure = read_runtime_item_count(
            context.il2cpp_api,
            binding,
            item_id
        );
        suppress_runtime_discard_item(
            item_id,
            config.revision,
            owner_generation,
            count_at_failure,
            error_code
        );
    }
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
    if (success) (void)begin_runtime_discard_reconcile(context);
}

void reset_runtime_discard_state(ProbeContext &context) {
    Il2CppGcHandle handle = 0U;
    Il2CppGcHandle reconcile_handle = 0U;
    {
        std::lock_guard<std::mutex> lock(g_runtime_discard_mutex);
        if (g_runtime_discard.active || g_runtime_discard.blocked) {
            // Resetting a native mirror does not cancel a game RPC. Preserve
            // the pending/unknown barrier so a config toggle cannot open a
            // second mutation before the first one is reconciled.
            return;
        }
        handle = g_runtime_discard.promise_gc_handle;
        reconcile_handle = g_runtime_discard.reconcile_promise_gc_handle;
        g_runtime_discard = PendingRuntimeDiscard{};
    }
    free_runtime_discard_handle(context, handle);
    free_runtime_discard_handle(context, reconcile_handle);
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
        !binding.inventory_cache_read_verified ||
        api.gchandle_new == nullptr || api.gchandle_get_target == nullptr ||
        api.gchandle_free == nullptr || !runtime_inventory_baseline_ready()) {
        return MainThreadActionOutcome::kRejected;
    }
    const bool automatic = command_id == nullptr || command_id[0] == '\0';
    const RuntimeDiscardConfig config = runtime_discard_config_snapshot();
    uint64_t config_revision = config.revision;
    if (automatic) {
        int32_t max_count = 0;
        if (!config.configured || !config.auto_discard ||
            !runtime_discard_config_find_limit(config, item_id, &max_count)) {
            return MainThreadActionOutcome::kRejected;
        }
        const int32_t current_count = read_runtime_item_count(api, binding, item_id);
        const int64_t excess = static_cast<int64_t>(current_count) -
            static_cast<int64_t>(max_count);
        if (current_count <= 0 || excess <= 0 || excess > 2147483647LL) {
            return MainThreadActionOutcome::kRejected;
        }
        amount = static_cast<int32_t>(excess);
        if (!runtime_inventory_baseline_ready()) return MainThreadActionOutcome::kRejected;
    }
    const uint64_t action_id = g_runtime_discard_next_action_id.fetch_add(
        1U,
        std::memory_order_relaxed
    );
    {
        std::lock_guard<std::mutex> lock(g_runtime_discard_mutex);
        if (g_runtime_discard.active || g_runtime_discard.blocked) {
            return MainThreadActionOutcome::kIndeterminate;
        }
        g_runtime_discard = PendingRuntimeDiscard{};
        g_runtime_discard.active = true;
        g_runtime_discard.item_id = item_id;
        g_runtime_discard.amount = amount;
        g_runtime_discard.automatic = automatic;
        g_runtime_discard.config_revision = config_revision;
        g_runtime_discard.action_id = action_id;
        g_runtime_discard.owner_generation = binding.owner_generation;
        g_runtime_discard.inventory_generation_before = runtime_inventory_observation_generation();
        g_runtime_discard.started_at_ns = static_cast<uint64_t>(now_elapsed_nanos());
        g_runtime_discard.command_id = command_id == nullptr ? "" : command_id;
    }
    if (!runtime_discard_intent_is_current(context, binding, item_id, amount)) {
        finish_runtime_discard(
            context,
            kCommandPhaseRejected,
            "discard_intent_stale",
            "discard intent changed before the main-thread invoke",
            false
        );
        return MainThreadActionOutcome::kRejected;
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
    if (!kDiscardExecutionEnabled || !binding.exact_build_verified ||
        !binding.discard_verified || !binding.inventory_cache_read_verified) {
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
