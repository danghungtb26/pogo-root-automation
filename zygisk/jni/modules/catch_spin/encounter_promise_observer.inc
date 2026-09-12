// Native observer for WildMapPokemon.SendEncounterRequest(). The method returns
// IPromise<PokemonEncounterResponse>; it must be retained and read on Unity's
// main thread before TryCapture is allowed to run.

namespace {
constexpr uint64_t kEncounterPromiseTimeoutNs = 15000000000ULL;

enum class RuntimeEncounterPromisePollResult : uint32_t {
    kNoPending = 0U,
    kWaiting = 1U,
    kCompleted = 2U,
    kFailed = 3U,
    kTimedOut = 4U,
};

const void *find_runtime_promise_field(
    const Il2CppApi &api,
    Il2CppClass *klass,
    const char *field_name
) {
    if (klass == nullptr || field_name == nullptr || api.class_get_field_from_name == nullptr) {
        return nullptr;
    }
    for (uint32_t depth = 0U; klass != nullptr && depth < 8U; ++depth) {
        const void *field = api.class_get_field_from_name(klass, field_name);
        if (field != nullptr) return field;
        klass = api.class_get_parent == nullptr ? nullptr : api.class_get_parent(klass);
    }
    return nullptr;
}

template <typename T>
bool read_runtime_promise_field(
    const Il2CppApi &api,
    void *promise,
    const char *field_name,
    T *value,
    int32_t *field_offset = nullptr
) {
    if (promise == nullptr || value == nullptr || api.object_get_class == nullptr ||
        api.field_get_value == nullptr) return false;
    const void *field = find_runtime_promise_field(
        api, api.object_get_class(promise), field_name
    );
    if (field == nullptr) return false;
    if (field_offset != nullptr && api.field_get_offset != nullptr) {
        *field_offset = api.field_get_offset(field);
    }
    api.field_get_value(promise, const_cast<void *>(field), value);
    return true;
}

void clear_pending_encounter_request(ProbeContext &context, const char *reason) {
    Il2CppGcHandle promise_gc_handle = 0U;
    uint64_t encounter_id = 0U;
    {
        std::lock_guard<std::mutex> lock(context.pending_encounter_request_mutex);
        if (!context.pending_encounter_request.active) return;
        promise_gc_handle = context.pending_encounter_request.promise_gc_handle;
        encounter_id = context.pending_encounter_request.encounter_id;
        context.pending_encounter_request = PendingEncounterRequest{};
    }
    if (promise_gc_handle != 0U && context.il2cpp_api.gchandle_free != nullptr) {
        context.il2cpp_api.gchandle_free(promise_gc_handle);
    }
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "runtime encounter promise cleared id=%llu reason=%s",
        static_cast<unsigned long long>(encounter_id),
        reason == nullptr ? "unspecified" : reason
    );
}

bool retain_pending_encounter_request(
    ProbeContext &context,
    uint64_t encounter_id,
    void *promise
) {
    const Il2CppApi &api = context.il2cpp_api;
    if (encounter_id == 0U || promise == nullptr || api.gchandle_new == nullptr ||
        api.gchandle_get_target == nullptr || api.gchandle_free == nullptr) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "runtime encounter promise not retained id=%llu promise=%p gc_api=%d",
            static_cast<unsigned long long>(encounter_id),
            promise,
            api.gchandle_new != nullptr && api.gchandle_get_target != nullptr &&
                api.gchandle_free != nullptr ? 1 : 0
        );
        return false;
    }
    const Il2CppGcHandle promise_gc_handle = api.gchandle_new(promise, false);
    if (promise_gc_handle == 0U) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "runtime encounter promise GC handle failed id=%llu promise=%p",
            static_cast<unsigned long long>(encounter_id),
            promise
        );
        return false;
    }
    Il2CppGcHandle previous_handle = 0U;
    {
        std::lock_guard<std::mutex> lock(context.pending_encounter_request_mutex);
        previous_handle = context.pending_encounter_request.promise_gc_handle;
        context.pending_encounter_request = PendingEncounterRequest{};
        context.pending_encounter_request.active = true;
        context.pending_encounter_request.encounter_id = encounter_id;
        context.pending_encounter_request.promise_gc_handle = promise_gc_handle;
        context.pending_encounter_request.requested_at_ns =
            static_cast<uint64_t>(now_elapsed_nanos());
    }
    if (previous_handle != 0U) api.gchandle_free(previous_handle);
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "runtime encounter promise retained id=%llu promise=%p gc_handle=0x%llx",
        static_cast<unsigned long long>(encounter_id),
        promise,
        static_cast<unsigned long long>(promise_gc_handle)
    );
    return true;
}

RuntimeEncounterPromisePollResult poll_pending_encounter_request(
    ProbeContext &context,
    uint64_t encounter_id
) {
    PendingEncounterRequest pending;
    {
        std::lock_guard<std::mutex> lock(context.pending_encounter_request_mutex);
        pending = context.pending_encounter_request;
    }
    if (!pending.active) return RuntimeEncounterPromisePollResult::kNoPending;

    const uint64_t now = static_cast<uint64_t>(now_elapsed_nanos());
    if (now >= pending.requested_at_ns &&
        now - pending.requested_at_ns > kEncounterPromiseTimeoutNs) {
        clear_pending_encounter_request(context, "timeout");
        return RuntimeEncounterPromisePollResult::kTimedOut;
    }
    if (pending.encounter_id != encounter_id) {
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "runtime encounter promise busy requested_id=%llu target_id=%llu",
            static_cast<unsigned long long>(pending.encounter_id),
            static_cast<unsigned long long>(encounter_id)
        );
        return RuntimeEncounterPromisePollResult::kWaiting;
    }
    if (pending.promise_completed) return RuntimeEncounterPromisePollResult::kCompleted;

    const Il2CppApi &api = context.il2cpp_api;
    void *promise = api.gchandle_get_target == nullptr
        ? nullptr : api.gchandle_get_target(pending.promise_gc_handle);
    if (promise == nullptr) {
        clear_pending_encounter_request(context, "gc_target_unavailable");
        return RuntimeEncounterPromisePollResult::kFailed;
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
    if (!fields_read) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "runtime encounter promise fields unavailable id=%llu promise=%p",
            static_cast<unsigned long long>(encounter_id),
            promise
        );
        clear_pending_encounter_request(context, "promise_layout_unavailable");
        return RuntimeEncounterPromisePollResult::kFailed;
    }

    uint32_t poll_count = 0U;
    {
        std::lock_guard<std::mutex> lock(context.pending_encounter_request_mutex);
        if (context.pending_encounter_request.active &&
            context.pending_encounter_request.promise_gc_handle == pending.promise_gc_handle) {
            poll_count = ++context.pending_encounter_request.poll_count;
        }
    }
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "runtime encounter promise state id=%llu promise=%p complete=%d error=%d value=%p error_value=%p poll=%u offsets=0x%x/0x%x/0x%x",
        static_cast<unsigned long long>(encounter_id),
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

    if (error_called) {
        clear_pending_encounter_request(context, "promise_failed");
        return RuntimeEncounterPromisePollResult::kFailed;
    }
    if (!complete_called) return RuntimeEncounterPromisePollResult::kWaiting;
    if (completed_value == nullptr) {
        clear_pending_encounter_request(context, "completed_without_value");
        return RuntimeEncounterPromisePollResult::kFailed;
    }

    const Il2CppClass *value_class = api.object_get_class(completed_value);
    const char *value_name = value_class == nullptr || api.class_get_name == nullptr
        ? "<unknown>" : api.class_get_name(const_cast<Il2CppClass *>(value_class));
    const char *value_namespace = value_class == nullptr || api.class_get_namespace == nullptr
        ? "<unknown>" : api.class_get_namespace(const_cast<Il2CppClass *>(value_class));
    {
        std::lock_guard<std::mutex> lock(context.pending_encounter_request_mutex);
        if (context.pending_encounter_request.active &&
            context.pending_encounter_request.promise_gc_handle == pending.promise_gc_handle) {
            context.pending_encounter_request.promise_completed = true;
            context.pending_encounter_request.completed_value = completed_value;
        }
    }
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "runtime encounter promise completed id=%llu promise=%p response=%p class=%s.%s",
        static_cast<unsigned long long>(encounter_id),
        promise,
        completed_value,
        value_namespace == nullptr ? "<unknown>" : value_namespace,
        value_name == nullptr ? "<unknown>" : value_name
    );
    return RuntimeEncounterPromisePollResult::kCompleted;
}

bool runtime_encounter_request_is_pending(ProbeContext &context) {
    std::lock_guard<std::mutex> lock(context.pending_encounter_request_mutex);
    return context.pending_encounter_request.active &&
        !context.pending_encounter_request.promise_completed;
}

}  // namespace

uint64_t runtime_pending_encounter_id(ProbeContext &context) {
    std::lock_guard<std::mutex> lock(context.pending_encounter_request_mutex);
    return context.pending_encounter_request.active
        ? context.pending_encounter_request.encounter_id
        : 0U;
}

MainThreadActionOutcome poll_runtime_encounter_on_main_thread(
    ProbeContext &context,
    uint64_t encounter_id
) {
    const RuntimeEncounterPromisePollResult poll_result =
        poll_pending_encounter_request(context, encounter_id);
    switch (poll_result) {
        case RuntimeEncounterPromisePollResult::kWaiting:
        case RuntimeEncounterPromisePollResult::kCompleted:
            return MainThreadActionOutcome::kInvoked;
        case RuntimeEncounterPromisePollResult::kFailed:
        case RuntimeEncounterPromisePollResult::kTimedOut:
            return MainThreadActionOutcome::kIndeterminate;
        case RuntimeEncounterPromisePollResult::kNoPending:
        default:
            return MainThreadActionOutcome::kRejected;
    }
}
