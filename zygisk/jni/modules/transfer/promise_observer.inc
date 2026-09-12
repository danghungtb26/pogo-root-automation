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
    bool release_attempted = false;
    bool release_invoked = false;
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
    bool release_attempted = false;
    bool release_invoked = false;
    {
        std::lock_guard<std::mutex> lock(g_runtime_transfer_mutex);
        if (!g_runtime_transfer.active) return;
        handle = g_runtime_transfer.promise_gc_handle;
        pokemon_id = g_runtime_transfer.pokemon_id;
        command_id = g_runtime_transfer.command_id;
        release_attempted = g_runtime_transfer.release_attempted;
        release_invoked = g_runtime_transfer.release_invoked;
        g_runtime_transfer.active = false;
        g_runtime_transfer.promise_gc_handle = 0U;
        g_runtime_transfer.command_id.clear();
        g_runtime_transfer.blocked = blocked;
    }
    free_runtime_transfer_handle(context, handle);
    const bool terminal_success = phase == kCommandPhaseCompleted &&
        (error_code == nullptr || error_code[0] == '\0');
    const bool transfer_success = terminal_success && release_invoked;
    const char *result = !release_attempted
        ? "KEEP"
        : transfer_success ? "SUCCESS" : "FAILED";
    __android_log_print(
        transfer_success || !release_attempted ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "native transfer result id=%llu mode=%s phase=%u result=%s release_attempted=%d "
        "release_invoked=%d blocked=%d error=%s message=%s",
        static_cast<unsigned long long>(pokemon_id),
        command_id.empty() ? "auto" : "manual",
        phase,
        result,
        release_attempted ? 1 : 0,
        release_invoked ? 1 : 0,
        blocked ? 1 : 0,
        error_code == nullptr ? "" : error_code,
        message == nullptr ? "" : message
    );
    if (release_attempted) {
        (void)send_runtime_automation_event(
            context,
            transfer_success
                ? pogo_runtime::RuntimeAutomationEventType::kPokemonTransferred
                : pogo_runtime::RuntimeAutomationEventType::kPokemonTransferFailed,
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
);

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

    {
        std::lock_guard<std::mutex> lock(g_runtime_transfer_mutex);
        if (g_runtime_transfer.active && g_runtime_transfer.pokemon_id == pending.pokemon_id) {
            g_runtime_transfer.release_attempted = true;
        }
    }
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "native transfer trigger id=%llu mode=%s action=ReleasePokemon",
        static_cast<unsigned long long>(pending.pokemon_id),
        pending.command_id.empty() ? "auto" : "manual"
    );
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
        g_runtime_transfer.release_invoked = true;
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
    (void)send_runtime_automation_event(
        context,
        pogo_runtime::RuntimeAutomationEventType::kPokemonTransferTriggered,
        pending.pokemon_id
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
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "native transfer queued id=%llu mode=%s phase=WAITING_POKEMON",
        static_cast<unsigned long long>(pokemon_id),
        command_id == nullptr || command_id[0] == '\0' ? "auto" : "manual"
    );
    const RuntimeTransferPromisePollResult first_poll =
        poll_pending_runtime_transfer_on_main_thread(context);
    if (first_poll == RuntimeTransferPromisePollResult::kFailed ||
        first_poll == RuntimeTransferPromisePollResult::kTimedOut) {
        return MainThreadActionOutcome::kIndeterminate;
    }
    return MainThreadActionOutcome::kInvoked;
}
