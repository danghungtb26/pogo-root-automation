// Captures IPromise<T> completion values without reading private Promise fields.
// A synchronized managed Queue is the callback target: Delegate.CreateDelegate
// binds the exact Action<T> expected by Then to Queue.Enqueue(object), whose
// parameter is compatible with every reference-type RPC result used here.

enum class RuntimePromiseSinkPollResult : uint32_t {
    kWaiting = 0U,
    kResult = 1U,
    kError = 2U,
    kFailed = 3U,
};

bool runtime_promise_sink_has_managed_pointer(const void *value) {
    const uintptr_t address = reinterpret_cast<uintptr_t>(value);
    return address >= 0x0000001000000000ULL && address < 0x0000800000000000ULL;
}

void free_runtime_promise_result_sink(
    const Il2CppApi &api,
    RuntimePromiseResultSink *sink
) {
    if (sink == nullptr) return;
    if (api.gchandle_free != nullptr) {
        const Il2CppGcHandle handles[] = {
            sink->result_queue_gc_handle,
            sink->result_delegate_gc_handle,
            sink->error_queue_gc_handle,
            sink->error_delegate_gc_handle,
        };
        for (Il2CppGcHandle handle : handles) {
            if (handle != 0U) api.gchandle_free(handle);
        }
    }
    *sink = RuntimePromiseResultSink{};
}

const void *find_runtime_promise_callback_method(
    const Il2CppApi &api,
    Il2CppClass *klass,
    const char *method_name,
    const char *parameter_type_name,
    const Il2CppType **parameter_type
) {
    if (parameter_type != nullptr) *parameter_type = nullptr;
    if (klass == nullptr || method_name == nullptr || parameter_type_name == nullptr ||
        api.class_get_methods == nullptr || api.method_get_name == nullptr ||
        api.method_get_param_count == nullptr || api.method_get_param == nullptr ||
        api.method_is_instance == nullptr || api.type_get_name == nullptr) return nullptr;
    const void *match = nullptr;
    for (uint32_t depth = 0U; klass != nullptr && depth < 8U; ++depth) {
        void *iterator = nullptr;
        for (uint32_t inspected = 0U; inspected < 128U; ++inspected) {
            const void *method = api.class_get_methods(klass, &iterator);
            if (method == nullptr) break;
            const char *name = api.method_get_name(method);
            if (!runtime_method_name_matches(name, method_name) ||
                !api.method_is_instance(method) ||
                api.method_get_param_count(method) != 1U) continue;
            const Il2CppType *candidate_type = api.method_get_param(method, 0U);
            OwnedIl2CppTypeName candidate_name = owned_il2cpp_type_name(api, candidate_type);
            if (candidate_name.get() == nullptr ||
                strcmp(candidate_name.get(), parameter_type_name) != 0) continue;
            if (match != nullptr) return nullptr;
            match = method;
            if (parameter_type != nullptr) *parameter_type = candidate_type;
        }
        if (match != nullptr) return match;
        klass = api.class_get_parent == nullptr ? nullptr : api.class_get_parent(klass);
    }
    return nullptr;
}

const void *find_runtime_delegate_factory(const Il2CppApi &api, Il2CppClass *delegate_class) {
    if (delegate_class == nullptr || api.class_get_methods == nullptr ||
        api.method_get_name == nullptr || api.method_get_param_count == nullptr ||
        api.method_get_param == nullptr || api.type_get_name == nullptr) return nullptr;
    const char *expected[] = {
        "System.Type", "System.Object", "System.String", "System.Boolean", "System.Boolean"
    };
    void *iterator = nullptr;
    for (uint32_t inspected = 0U; inspected < 128U; ++inspected) {
        const void *method = api.class_get_methods(delegate_class, &iterator);
        if (method == nullptr) break;
        const char *name = api.method_get_name(method);
        if (name == nullptr || strcmp(name, "CreateDelegate") != 0 ||
            api.method_get_param_count(method) != 5U) continue;
        bool matches = true;
        for (uint32_t index = 0U; index < 5U; ++index) {
            OwnedIl2CppTypeName type_name = owned_il2cpp_type_name(
                api, api.method_get_param(method, index)
            );
            matches = matches && type_name.get() != nullptr &&
                strcmp(type_name.get(), expected[index]) == 0;
        }
        if (matches) return method;
    }
    return nullptr;
}

const void *find_runtime_queue_synchronized_factory(
    const Il2CppApi &api,
    Il2CppClass *queue_class
) {
    if (queue_class == nullptr || api.class_get_methods == nullptr ||
        api.method_get_name == nullptr || api.method_get_param_count == nullptr ||
        api.method_get_param == nullptr || api.method_is_instance == nullptr) return nullptr;
    const void *match = nullptr;
    void *iterator = nullptr;
    for (uint32_t inspected = 0U; inspected < 128U; ++inspected) {
        const void *method = api.class_get_methods(queue_class, &iterator);
        if (method == nullptr) break;
        if (!runtime_method_name_matches(api.method_get_name(method), "Synchronized") ||
            api.method_is_instance(method) || api.method_get_param_count(method) != 1U) continue;
        OwnedIl2CppTypeName parameter_name = owned_il2cpp_type_name(
            api, api.method_get_param(method, 0U)
        );
        if (parameter_name.get() == nullptr ||
            strcmp(parameter_name.get(), "System.Collections.Queue") != 0) continue;
        if (match != nullptr) return nullptr;
        match = method;
    }
    return match;
}

void *create_runtime_synchronized_queue(
    const Il2CppApi &api,
    Il2CppClass *queue_class,
    const void *synchronized_factory
) {
    if (queue_class == nullptr || synchronized_factory == nullptr ||
        api.object_new == nullptr || api.runtime_object_init == nullptr ||
        api.runtime_invoke == nullptr) return nullptr;
    void *queue = api.object_new(queue_class);
    if (!runtime_promise_sink_has_managed_pointer(queue)) return nullptr;
    api.runtime_object_init(queue);
    void *arguments[] = {queue};
    void *exception = nullptr;
    void *synchronized_queue = api.runtime_invoke(
        synchronized_factory, nullptr, arguments, &exception
    );
    return exception == nullptr && runtime_promise_sink_has_managed_pointer(synchronized_queue)
        ? synchronized_queue : nullptr;
}

void *create_runtime_promise_queue_delegate(
    const Il2CppApi &api,
    const void *delegate_factory,
    const Il2CppType *delegate_type,
    void *queue,
    void **exception
) {
    if (exception != nullptr) *exception = nullptr;
    if (delegate_factory == nullptr || delegate_type == nullptr || queue == nullptr ||
        api.type_get_object == nullptr || api.string_new == nullptr ||
        api.runtime_invoke == nullptr) return nullptr;
    void *type_object = api.type_get_object(delegate_type);
    void *method_name = api.string_new("Enqueue");
    if (!runtime_promise_sink_has_managed_pointer(type_object) ||
        !runtime_promise_sink_has_managed_pointer(method_name)) return nullptr;
    uint8_t ignore_case = 0U;
    uint8_t throw_on_failure = 1U;
    void *arguments[] = {
        type_object, queue, method_name, &ignore_case, &throw_on_failure
    };
    return api.runtime_invoke(delegate_factory, nullptr, arguments, exception);
}

bool retain_runtime_promise_sink_object(
    const Il2CppApi &api,
    void *object,
    Il2CppGcHandle *handle
) {
    if (!runtime_promise_sink_has_managed_pointer(object) || handle == nullptr ||
        api.gchandle_new == nullptr) return false;
    *handle = api.gchandle_new(object, false);
    return *handle != 0U;
}

bool subscribe_runtime_promise_result(
    const Il2CppApi &api,
    void *promise,
    const char *result_type_name,
    RuntimePromiseResultSink *sink
) {
    if (!runtime_promise_sink_has_managed_pointer(promise) || result_type_name == nullptr ||
        sink == nullptr || sink->subscribed || api.object_get_class == nullptr ||
        api.domain_get == nullptr || api.domain_get_assemblies == nullptr ||
        api.object_new == nullptr || api.runtime_object_init == nullptr ||
        api.class_get_method_from_name == nullptr || api.runtime_invoke == nullptr ||
        api.gchandle_new == nullptr || api.gchandle_get_target == nullptr ||
        api.gchandle_free == nullptr) return false;

    const std::string result_action_type =
        std::string("System.Action<") + result_type_name + ">";
    const Il2CppType *result_delegate_type = nullptr;
    const Il2CppType *error_delegate_type = nullptr;
    Il2CppClass *promise_class = api.object_get_class(promise);
    const void *then_method = find_runtime_promise_callback_method(
        api, promise_class, "Then", result_action_type.c_str(), &result_delegate_type
    );
    const void *error_method = find_runtime_promise_callback_method(
        api,
        promise_class,
        "Error",
        "System.Action<System.String>",
        &error_delegate_type
    );
    if (then_method == nullptr || error_method == nullptr) return false;

    Il2CppDomain *domain = api.domain_get();
    size_t assembly_count = 0U;
    const Il2CppAssembly **assemblies = domain == nullptr
        ? nullptr : api.domain_get_assemblies(domain, &assembly_count);
    if (assemblies == nullptr) return false;
    Il2CppClass *queue_class = find_runtime_class(
        api, assemblies, assembly_count, "System.Collections", "Queue", nullptr
    );
    Il2CppClass *delegate_class = find_runtime_class(
        api, assemblies, assembly_count, "System", "Delegate", nullptr
    );
    const void *delegate_factory = find_runtime_delegate_factory(api, delegate_class);
    const void *queue_synchronized_factory = find_runtime_queue_synchronized_factory(
        api, queue_class
    );
    if (queue_class == nullptr || delegate_factory == nullptr ||
        queue_synchronized_factory == nullptr) return false;

    RuntimePromiseResultSink candidate;
    void *result_queue = create_runtime_synchronized_queue(
        api, queue_class, queue_synchronized_factory
    );
    if (!retain_runtime_promise_sink_object(
            api, result_queue, &candidate.result_queue_gc_handle
        )) return false;
    void *error_queue = create_runtime_synchronized_queue(
        api, queue_class, queue_synchronized_factory
    );
    if (!retain_runtime_promise_sink_object(
            api, error_queue, &candidate.error_queue_gc_handle
        )) {
        free_runtime_promise_result_sink(api, &candidate);
        return false;
    }
    Il2CppClass *synchronized_queue_class = api.object_get_class(result_queue);
    candidate.queue_get_count = find_runtime_instance_method_in_hierarchy(
        api, synchronized_queue_class, "get_Count", 0U
    );
    candidate.queue_peek = find_runtime_instance_method_in_hierarchy(
        api, synchronized_queue_class, "Peek", 0U
    );
    if (candidate.queue_get_count == nullptr || candidate.queue_peek == nullptr) {
        free_runtime_promise_result_sink(api, &candidate);
        return false;
    }

    void *exception = nullptr;
    void *result_delegate = create_runtime_promise_queue_delegate(
        api, delegate_factory, result_delegate_type, result_queue, &exception
    );
    if (exception != nullptr || !retain_runtime_promise_sink_object(
            api, result_delegate, &candidate.result_delegate_gc_handle
        )) {
        free_runtime_promise_result_sink(api, &candidate);
        return false;
    }
    exception = nullptr;
    void *error_delegate = create_runtime_promise_queue_delegate(
        api, delegate_factory, error_delegate_type, error_queue, &exception
    );
    if (exception != nullptr || !retain_runtime_promise_sink_object(
            api, error_delegate, &candidate.error_delegate_gc_handle
        )) {
        free_runtime_promise_result_sink(api, &candidate);
        return false;
    }

    void *error_arguments[] = {error_delegate};
    exception = nullptr;
    (void)api.runtime_invoke(error_method, promise, error_arguments, &exception);
    if (exception != nullptr) {
        free_runtime_promise_result_sink(api, &candidate);
        return false;
    }
    void *result_arguments[] = {result_delegate};
    exception = nullptr;
    (void)api.runtime_invoke(then_method, promise, result_arguments, &exception);
    if (exception != nullptr) {
        free_runtime_promise_result_sink(api, &candidate);
        return false;
    }
    candidate.subscribed = true;
    *sink = candidate;
    return true;
}

bool runtime_promise_queue_peek(
    const Il2CppApi &api,
    Il2CppGcHandle queue_handle,
    const RuntimePromiseResultSink &sink,
    void **value,
    bool *has_value
) {
    if (value != nullptr) *value = nullptr;
    if (has_value != nullptr) *has_value = false;
    if (queue_handle == 0U || value == nullptr || has_value == nullptr ||
        api.gchandle_get_target == nullptr || api.runtime_invoke == nullptr ||
        api.object_unbox == nullptr || sink.queue_get_count == nullptr ||
        sink.queue_peek == nullptr) return false;
    void *queue = api.gchandle_get_target(queue_handle);
    if (!runtime_promise_sink_has_managed_pointer(queue)) return false;
    void *exception = nullptr;
    void *boxed_count = api.runtime_invoke(
        sink.queue_get_count, queue, nullptr, &exception
    );
    if (exception != nullptr || boxed_count == nullptr) return false;
    void *count_value = api.object_unbox(boxed_count);
    if (count_value == nullptr) return false;
    int32_t count = 0;
    memcpy(&count, count_value, sizeof(count));
    if (count <= 0) return true;
    exception = nullptr;
    *value = api.runtime_invoke(sink.queue_peek, queue, nullptr, &exception);
    if (exception != nullptr || !runtime_promise_sink_has_managed_pointer(*value)) return false;
    *has_value = true;
    return true;
}

RuntimePromiseSinkPollResult poll_runtime_promise_result_sink(
    const Il2CppApi &api,
    const RuntimePromiseResultSink &sink,
    void **result
) {
    if (result != nullptr) *result = nullptr;
    if (!sink.subscribed || result == nullptr) return RuntimePromiseSinkPollResult::kFailed;
    void *error = nullptr;
    bool has_error = false;
    if (!runtime_promise_queue_peek(
            api, sink.error_queue_gc_handle, sink, &error, &has_error
        )) return RuntimePromiseSinkPollResult::kFailed;
    if (has_error) return RuntimePromiseSinkPollResult::kError;
    bool has_result = false;
    if (!runtime_promise_queue_peek(
            api, sink.result_queue_gc_handle, sink, result, &has_result
        )) return RuntimePromiseSinkPollResult::kFailed;
    return has_result
        ? RuntimePromiseSinkPollResult::kResult
        : RuntimePromiseSinkPollResult::kWaiting;
}
