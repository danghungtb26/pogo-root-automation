// Owns the native transfer state machine. Both auto transfer and the legacy
// command path enter here; GetPokemon, ReleasePokemon and Promise polling all
// run on Unity's main thread.

constexpr bool kTransferExecutionEnabled = true;
constexpr uint64_t kRuntimeTransferMetadataTimeoutNs = 5000000000ULL;
constexpr uint64_t kRuntimeTransferPromiseTimeoutNs = 15000000000ULL;

enum class RuntimeTransferPendingPhase : uint32_t {
    kWaitingPokemon = 1U,
    kWaitingRelease = 2U,
};

struct PendingRuntimeTransfer {
    bool active = false;
    bool blocked = false;
    bool evaluate_keep = false;
    RuntimeTransferPendingPhase phase = RuntimeTransferPendingPhase::kWaitingPokemon;
    uint64_t pokemon_id = 0U;
    uint64_t started_at_ns = 0U;
    uint32_t poll_count = 0U;
    Il2CppGcHandle promise_gc_handle = 0U;
    std::string command_id;
};

std::mutex g_runtime_transfer_mutex;
PendingRuntimeTransfer g_runtime_transfer;

void free_runtime_transfer_handle(ProbeContext &context, Il2CppGcHandle handle) {
    if (handle != 0U && context.il2cpp_api.gchandle_free != nullptr) {
        context.il2cpp_api.gchandle_free(handle);
    }
}

bool runtime_transfer_blocks_catch_spin() {
    std::lock_guard<std::mutex> lock(g_runtime_transfer_mutex);
    return g_runtime_transfer.active || g_runtime_transfer.blocked;
}

bool runtime_transfer_has_pending() {
    std::lock_guard<std::mutex> lock(g_runtime_transfer_mutex);
    return g_runtime_transfer.active;
}

void finish_runtime_transfer(
    ProbeContext &context,
    uint32_t phase,
    const char *error_code,
    const char *message,
    bool blocked
) {
    Il2CppGcHandle handle = 0U;
    uint64_t pokemon_id = 0U;
    std::string command_id;
    {
        std::lock_guard<std::mutex> lock(g_runtime_transfer_mutex);
        if (!g_runtime_transfer.active) return;
        handle = g_runtime_transfer.promise_gc_handle;
        pokemon_id = g_runtime_transfer.pokemon_id;
        command_id = g_runtime_transfer.command_id;
        g_runtime_transfer.active = false;
        g_runtime_transfer.promise_gc_handle = 0U;
        g_runtime_transfer.command_id.clear();
        g_runtime_transfer.blocked = blocked;
    }
    free_runtime_transfer_handle(context, handle);
    const bool success = phase == kCommandPhaseCompleted &&
        (error_code == nullptr || error_code[0] == '\0');
    __android_log_print(
        success ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "native auto transfer terminal id=%llu phase=%u success=%d blocked=%d error=%s message=%s",
        static_cast<unsigned long long>(pokemon_id),
        phase,
        success ? 1 : 0,
        blocked ? 1 : 0,
        error_code == nullptr ? "" : error_code,
        message == nullptr ? "" : message
    );
    if (success) {
        (void)send_runtime_automation_event(
            context,
            pogo_runtime::RuntimeAutomationEventType::kPokemonTransferred,
            pokemon_id
        );
    }
    if (!command_id.empty()) {
        (void)send_runtime_result(
            context.fd,
            command_id.c_str(),
            phase,
            error_code == nullptr ? "" : error_code,
            message == nullptr ? "runtime transfer completed" : message
        );
    }
}

void reset_runtime_transfer_state(ProbeContext &context) {
    Il2CppGcHandle handle = 0U;
    {
        std::lock_guard<std::mutex> lock(g_runtime_transfer_mutex);
        handle = g_runtime_transfer.promise_gc_handle;
        g_runtime_transfer = PendingRuntimeTransfer{};
    }
    free_runtime_transfer_handle(context, handle);
}

void *get_runtime_pokemon_on_main_thread(
    ProbeContext &context,
    uint64_t pokemon_id,
    void **exception
) {
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    if (exception != nullptr) *exception = nullptr;
    if (!binding.transfer_verified || binding.pokemon_inventory == nullptr ||
        binding.pokemon_inventory_get_pokemon == nullptr ||
        context.il2cpp_api.runtime_invoke == nullptr) return nullptr;
    void *arguments[] = {&pokemon_id};
    return context.il2cpp_api.runtime_invoke(
        binding.pokemon_inventory_get_pokemon,
        binding.pokemon_inventory,
        arguments,
        exception
    );
}

void *release_runtime_pokemon_on_main_thread(
    ProbeContext &context,
    void *pokemon,
    void **exception
) {
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    if (exception != nullptr) *exception = nullptr;
    if (!binding.transfer_verified || binding.pokemon_inventory == nullptr ||
        binding.pokemon_inventory_release == nullptr || pokemon == nullptr ||
        context.il2cpp_api.runtime_invoke == nullptr) return nullptr;
    void *arguments[] = {pokemon};
    return context.il2cpp_api.runtime_invoke(
        binding.pokemon_inventory_release,
        binding.pokemon_inventory,
        arguments,
        exception
    );
}

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

RuntimeTransferPromisePollResult poll_pending_runtime_transfer_on_main_thread(
    ProbeContext &context
) {
    PendingRuntimeTransfer pending;
    {
        std::lock_guard<std::mutex> lock(g_runtime_transfer_mutex);
        pending = g_runtime_transfer;
    }
    if (!pending.active) return RuntimeTransferPromisePollResult::kNoPending;
    const uint64_t now = static_cast<uint64_t>(now_elapsed_nanos());
    const uint64_t timeout = pending.phase == RuntimeTransferPendingPhase::kWaitingRelease
        ? kRuntimeTransferPromiseTimeoutNs : kRuntimeTransferMetadataTimeoutNs;
    if (pending.started_at_ns != 0U && now >= pending.started_at_ns &&
        now - pending.started_at_ns > timeout) {
        if (pending.phase == RuntimeTransferPendingPhase::kWaitingRelease) {
            finish_runtime_transfer(
                context,
                kCommandPhaseIndeterminate,
                "transfer_promise_timeout",
                "ReleasePokemon Promise did not complete before the native deadline",
                true
            );
        } else {
            finish_runtime_transfer(
                context,
                kCommandPhaseCompleted,
                "",
                "Pokemon metadata stayed unavailable; kept the Pokémon",
                false
            );
        }
        return RuntimeTransferPromisePollResult::kTimedOut;
    }
    if (pending.phase == RuntimeTransferPendingPhase::kWaitingRelease) {
        return poll_release_promise_on_main_thread(context, pending);
    }

    const RuntimeTransferConfig config = runtime_transfer_config_snapshot();
    if (pending.evaluate_keep && (!config.configured || !config.auto_transfer)) {
        finish_runtime_transfer(
            context,
            kCommandPhaseCompleted,
            "",
            "auto transfer disabled before metadata resolution; kept the Pokémon",
            false
        );
        return RuntimeTransferPromisePollResult::kCompleted;
    }
    void *exception = nullptr;
    void *pokemon = get_runtime_pokemon_on_main_thread(context, pending.pokemon_id, &exception);
    if (exception != nullptr || !is_probable_managed_pointer(pokemon)) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native auto transfer metadata pending id=%llu exception=%p pokemon=%p",
            static_cast<unsigned long long>(pending.pokemon_id),
            exception,
            pokemon
        );
        return RuntimeTransferPromisePollResult::kWaiting;
    }

    if (pending.evaluate_keep) {
        RuntimePokemonMetadata metadata;
        if (!read_runtime_pokemon_metadata(context.il2cpp_api, runtime_binding_snapshot(context), pokemon, &metadata)) {
            __android_log_print(
                ANDROID_LOG_WARN,
                kLogTag,
                "native auto transfer metadata pending id=%llu reason=layout_unavailable",
                static_cast<unsigned long long>(pending.pokemon_id)
            );
            return RuntimeTransferPromisePollResult::kWaiting;
        }
        std::string reason;
        const RuntimeTransferDecision decision = evaluate_runtime_transfer_decision(
            config, metadata, &reason
        );
        if (decision == RuntimeTransferDecision::kKeep) {
            __android_log_print(
                ANDROID_LOG_INFO,
                kLogTag,
                "native auto transfer decision id=%llu action=KEEP reason=%s species=%d iv=%d/%d/%d",
                static_cast<unsigned long long>(pending.pokemon_id),
                reason.c_str(),
                metadata.species_id,
                metadata.individual_attack,
                metadata.individual_defense,
                metadata.individual_stamina
            );
            finish_runtime_transfer(
                context,
                kCommandPhaseCompleted,
                "",
                reason.c_str(),
                false
            );
            return RuntimeTransferPromisePollResult::kCompleted;
        }
        if (decision == RuntimeTransferDecision::kMetadataUnavailable) {
            return RuntimeTransferPromisePollResult::kWaiting;
        }
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "native auto transfer decision id=%llu action=TRANSFER reason=%s species=%d iv=%d/%d/%d",
            static_cast<unsigned long long>(pending.pokemon_id),
            reason.c_str(),
            metadata.species_id,
            metadata.individual_attack,
            metadata.individual_defense,
            metadata.individual_stamina
        );
    }

    void *release_exception = nullptr;
    void *promise = release_runtime_pokemon_on_main_thread(
        context, pokemon, &release_exception
    );
    if (release_exception != nullptr || !is_probable_managed_pointer(promise)) {
        finish_runtime_transfer(
            context,
            kCommandPhaseIndeterminate,
            "transfer_invoke_indeterminate",
            "ReleasePokemon may have run but no Promise was retained",
            true
        );
        return RuntimeTransferPromisePollResult::kFailed;
    }
    const Il2CppApi &api = context.il2cpp_api;
    if (api.gchandle_new == nullptr || api.gchandle_get_target == nullptr ||
        api.gchandle_free == nullptr) {
        finish_runtime_transfer(
            context,
            kCommandPhaseIndeterminate,
            "transfer_promise_retention_unavailable",
            "IL2CPP GC handle API is unavailable for ReleasePokemon Promise",
            true
        );
        return RuntimeTransferPromisePollResult::kFailed;
    }
    const Il2CppGcHandle handle = api.gchandle_new(promise, false);
    if (handle == 0U) {
        finish_runtime_transfer(
            context,
            kCommandPhaseIndeterminate,
            "transfer_promise_retention_failed",
            "ReleasePokemon Promise could not be retained",
            true
        );
        return RuntimeTransferPromisePollResult::kFailed;
    }
    {
        std::lock_guard<std::mutex> lock(g_runtime_transfer_mutex);
        if (!g_runtime_transfer.active || g_runtime_transfer.pokemon_id != pending.pokemon_id) {
            free_runtime_transfer_handle(context, handle);
            return RuntimeTransferPromisePollResult::kFailed;
        }
        g_runtime_transfer.phase = RuntimeTransferPendingPhase::kWaitingRelease;
        g_runtime_transfer.promise_gc_handle = handle;
        g_runtime_transfer.started_at_ns = static_cast<uint64_t>(now_elapsed_nanos());
        g_runtime_transfer.poll_count = 0U;
    }
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "native auto transfer release invoked id=%llu promise=%p gc_handle=0x%llx",
        static_cast<unsigned long long>(pending.pokemon_id),
        promise,
        static_cast<unsigned long long>(handle)
    );
    return RuntimeTransferPromisePollResult::kWaiting;
}

MainThreadActionOutcome start_runtime_transfer_on_main_thread(
    ProbeContext &context,
    uint64_t pokemon_id,
    const char *command_id
) {
    if (!kTransferExecutionEnabled || pokemon_id == 0U) return MainThreadActionOutcome::kRejected;
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    if (!binding.transfer_verified || binding.pokemon_inventory == nullptr ||
        binding.pokemon_inventory_get_pokemon == nullptr ||
        binding.pokemon_inventory_release == nullptr) return MainThreadActionOutcome::kRejected;
    {
        std::lock_guard<std::mutex> lock(g_runtime_transfer_mutex);
        if (g_runtime_transfer.active || g_runtime_transfer.blocked) {
            return MainThreadActionOutcome::kIndeterminate;
        }
        g_runtime_transfer = PendingRuntimeTransfer{};
        g_runtime_transfer.active = true;
        g_runtime_transfer.evaluate_keep = command_id == nullptr || command_id[0] == '\0';
        g_runtime_transfer.pokemon_id = pokemon_id;
        g_runtime_transfer.command_id = command_id == nullptr ? "" : command_id;
        g_runtime_transfer.started_at_ns = static_cast<uint64_t>(now_elapsed_nanos());
    }
    const RuntimeTransferPromisePollResult first_poll =
        poll_pending_runtime_transfer_on_main_thread(context);
    if (first_poll == RuntimeTransferPromisePollResult::kFailed ||
        first_poll == RuntimeTransferPromisePollResult::kTimedOut) {
        return MainThreadActionOutcome::kIndeterminate;
    }
    return MainThreadActionOutcome::kInvoked;
}
