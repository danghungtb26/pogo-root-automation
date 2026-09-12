// Resolves ItemInventoryService on demand and finds verified ItemData rows from the
// same inventory list the UI uses for discard.

constexpr size_t kDiscardInventoryListArrayDataOffset = sizeof(void *) * 4U;

bool api_has_assemblies(const Il2CppApi &api) {
    return api.domain_get != nullptr && api.domain_get_assemblies != nullptr;
}

bool runtime_object_is_item_inventory_service(const Il2CppApi &api, void *object) {
    if (object == nullptr || api.object_get_class == nullptr || api.class_get_name == nullptr) {
        return false;
    }
    Il2CppClass *klass = api.object_get_class(object);
    const char *class_name = klass == nullptr ? nullptr : api.class_get_name(klass);
    return class_name != nullptr && strcmp(class_name, "ItemInventoryService") == 0;
}

void *resolve_runtime_di_container(
    ProbeContext &context,
    const RuntimeBinding &binding,
    void **exception
) {
    if (exception != nullptr) *exception = nullptr;
    const Il2CppApi &api = context.il2cpp_api;
    if (binding.project_context_class == nullptr || binding.di_container_class == nullptr ||
        api.class_get_method_from_name == nullptr || api.runtime_invoke == nullptr) {
        return nullptr;
    }
    const void *project_instance = api.class_get_method_from_name(
        binding.project_context_class,
        "get_Instance",
        0
    );
    if (project_instance == nullptr) return nullptr;
    void *project = api.runtime_invoke(project_instance, nullptr, nullptr, exception);
    if (exception != nullptr && *exception != nullptr) return nullptr;
    if (!is_probable_managed_pointer(project)) return nullptr;
    const void *container_method = api.class_get_method_from_name(
        binding.project_context_class,
        "get_Container",
        0
    );
    if (container_method == nullptr) return nullptr;
    void *container = api.runtime_invoke(container_method, project, nullptr, exception);
    if (exception != nullptr && *exception != nullptr) return nullptr;
    if (!is_probable_managed_pointer(container)) return nullptr;
    return container;
}

void *try_resolve_inventory_service_from_container(
    const Il2CppApi &api,
    const RuntimeBinding &binding,
    void *container,
    Il2CppClass *service_class,
    void **exception
) {
    if (exception != nullptr) *exception = nullptr;
    if (!is_probable_managed_pointer(container) || service_class == nullptr ||
        binding.di_try_resolve == nullptr || api.runtime_invoke == nullptr ||
        api.class_get_type == nullptr || api.type_get_object == nullptr) {
        return nullptr;
    }
    const Il2CppType *service_type = api.class_get_type(service_class);
    void *type_object = service_type == nullptr ? nullptr : api.type_get_object(service_type);
    if (!is_probable_managed_pointer(type_object)) return nullptr;
    void *arguments[] = {type_object};
    return api.runtime_invoke(binding.di_try_resolve, container, arguments, exception);
}

void *resolve_item_inventory_service_on_main_thread(
    ProbeContext &context,
    RuntimeBinding &binding,
    void **exception
) {
    if (exception != nullptr) *exception = nullptr;
    if (is_probable_managed_pointer(binding.item_inventory_service)) {
        return binding.item_inventory_service;
    }
    const Il2CppApi &api = context.il2cpp_api;
    if (binding.di_try_resolve == nullptr ||
        (binding.item_inventory_service_class == nullptr &&
         binding.item_inventory_service_impl_class == nullptr)) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native discard item inventory service unavailable try_resolve=%p service_class=%p impl=%p",
            binding.di_try_resolve,
            binding.item_inventory_service_class,
            binding.item_inventory_service_impl_class
        );
        return nullptr;
    }

    void *project_container = resolve_runtime_di_container(context, binding, exception);
    if (exception != nullptr && *exception != nullptr) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native discard project container exception exception=%p",
            *exception
        );
        if (exception != nullptr) *exception = nullptr;
    }
    void *scene_container = resolve_runtime_scene_di_container(
        context,
        binding,
        project_container,
        exception
    );
    if (exception != nullptr && *exception != nullptr) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native discard scene container exception exception=%p",
            *exception
        );
        if (exception != nullptr) *exception = nullptr;
    }
    if (discard_runtime_object_is_map_scene_context(context.il2cpp_api, binding.map_scene_context)) {
        cache_runtime_map_scene_context(context, binding.map_scene_context);
    }

    Il2CppClass *service_classes[2] = {
        binding.item_inventory_service_class,
        binding.item_inventory_service_impl_class,
    };
    void *containers[2] = {scene_container, project_container};
    const char *container_labels[2] = {"scene", "project"};
    for (size_t class_index = 0U; class_index < 2U; ++class_index) {
        Il2CppClass *service_class = service_classes[class_index];
        if (service_class == nullptr) continue;
        for (size_t container_index = 0U; container_index < 2U; ++container_index) {
            void *service = try_resolve_inventory_service_from_container(
                api,
                binding,
                containers[container_index],
                service_class,
                exception
            );
            if (exception != nullptr && *exception != nullptr) {
                __android_log_print(
                    ANDROID_LOG_WARN,
                    kLogTag,
                    "native discard %s TryResolve exception class=%zu exception=%p",
                    container_labels[container_index],
                    class_index,
                    *exception
                );
                if (exception != nullptr) *exception = nullptr;
                service = nullptr;
            }
            if (!is_probable_managed_pointer(service)) continue;
            if (!runtime_object_is_item_inventory_service(api, service)) {
                Il2CppClass *klass = api.object_get_class(service);
                const char *class_name = klass == nullptr || api.class_get_name == nullptr
                    ? "<unknown>"
                    : api.class_get_name(klass);
                __android_log_print(
                    ANDROID_LOG_WARN,
                    kLogTag,
                    "native discard item inventory service class mismatch service=%p class=%s",
                    service,
                    class_name
                );
                continue;
            }
            binding.item_inventory_service = service;
            if (binding.item_inventory_get_expiring_items == nullptr &&
                api.class_get_method_from_name != nullptr && api.object_get_class != nullptr) {
                Il2CppClass *resolved_class = api.object_get_class(service);
                binding.item_inventory_get_expiring_items = resolved_class == nullptr
                    ? nullptr
                    : api.class_get_method_from_name(resolved_class, "get_ExpiringItemsCopy", 0);
            }
            __android_log_print(
                ANDROID_LOG_INFO,
                kLogTag,
                "native discard item inventory service resolved service=%p expiring=%p project=%p scene=%p class=%zu",
                service,
                binding.item_inventory_get_expiring_items,
                project_container,
                scene_container,
                class_index
            );
            return service;
        }
    }
    __android_log_print(
        ANDROID_LOG_WARN,
        kLogTag,
        "native discard item inventory service TryResolve failed project=%p scene=%p map_scene=%p",
        project_container,
        scene_container,
        binding.map_scene_context
    );
    return nullptr;
}

bool read_managed_list_count(
    const Il2CppApi &api,
    void *list,
    void **exception,
    int32_t *count
) {
    if (count == nullptr || list == nullptr) return false;
    if (exception != nullptr) *exception = nullptr;
    if (read_runtime_field(api, list, "_size", count) && *count > 0) return true;
    if (exception != nullptr) *exception = nullptr;
    Il2CppClass *list_class = api.object_get_class == nullptr ? nullptr : api.object_get_class(list);
    const void *get_count = list_class == nullptr || api.class_get_method_from_name == nullptr
        ? nullptr
        : api.class_get_method_from_name(list_class, "get_Count", 0);
    if (get_count == nullptr || api.runtime_invoke == nullptr) return false;
    void *result = api.runtime_invoke(get_count, list, nullptr, exception);
    if (exception != nullptr && *exception != nullptr) return false;
    if (result == nullptr || api.object_unbox == nullptr) return false;
    void *unboxed = api.object_unbox(result);
    if (!is_probable_managed_pointer(unboxed)) return false;
    memcpy(count, unboxed, sizeof(*count));
    return *count > 0;
}

void *read_managed_list_element(
    const Il2CppApi &api,
    void *list,
    int32_t index,
    void **exception
) {
    if (list == nullptr || index < 0 || api.object_get_class == nullptr ||
        api.class_get_method_from_name == nullptr || api.runtime_invoke == nullptr) {
        return nullptr;
    }
    void *items_array = nullptr;
    if (read_runtime_field(api, list, "_items", &items_array) &&
        is_probable_managed_pointer(items_array) && api.array_length != nullptr &&
        api.array_get_byte_length != nullptr) {
        const size_t length = api.array_length(items_array);
        const size_t byte_length = api.array_get_byte_length(items_array);
        if (index < static_cast<int32_t>(length) &&
            byte_length >= static_cast<size_t>(index + 1) * sizeof(void *)) {
            void *element = nullptr;
            const auto *data = static_cast<const uint8_t *>(items_array) +
                kDiscardInventoryListArrayDataOffset;
            memcpy(&element, data + static_cast<size_t>(index) * sizeof(void *), sizeof(element));
            if (is_probable_managed_pointer(element)) return element;
        }
    }
    if (exception != nullptr) *exception = nullptr;
    Il2CppClass *list_class = api.object_get_class(list);
    const void *get_item = list_class == nullptr
        ? nullptr
        : api.class_get_method_from_name(list_class, "get_Item", 1);
    if (get_item == nullptr) return nullptr;
    void *index_args[] = {&index};
    return api.runtime_invoke(get_item, list, index_args, exception);
}

void log_managed_list_class(
    const Il2CppApi &api,
    void *list,
    const char *label
) {
    if (!is_probable_managed_pointer(list) || api.object_get_class == nullptr ||
        api.class_get_name == nullptr || api.class_get_namespace == nullptr) {
        return;
    }
    Il2CppClass *klass = api.object_get_class(list);
    const char *class_name = klass == nullptr ? "<unknown>" : api.class_get_name(klass);
    const char *name_space = klass == nullptr ? "" : api.class_get_namespace(klass);
    int32_t size = 0;
    (void)read_runtime_field(api, list, "_size", &size);
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "native discard inventory %s list=%p class=%s.%s _size=%d",
        label == nullptr ? "lookup" : label,
        list,
        name_space == nullptr ? "" : name_space,
        class_name == nullptr ? "<unknown>" : class_name,
        size
    );
}

void *find_item_data_in_managed_list(
    const Il2CppApi &api,
    void *list,
    int32_t item_id,
    void **exception
) {
    if (list == nullptr || item_id <= 0 || api.object_get_class == nullptr ||
        api.class_get_method_from_name == nullptr) return nullptr;
    Il2CppClass *list_class = api.object_get_class(list);
    const char *list_class_name = list_class == nullptr || api.class_get_name == nullptr
        ? "<unknown>"
        : api.class_get_name(list_class);
    int32_t count = 0;
    if (!read_managed_list_count(api, list, exception, &count) || count <= 0) {
        int32_t raw_size = 0;
        (void)read_runtime_field(api, list, "_size", &raw_size);
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native discard inventory list empty list=%p class=%s count=%d _size=%d",
            list,
            list_class_name,
            count,
            raw_size
        );
        return nullptr;
    }
    for (int32_t index = 0; index < count; ++index) {
        if (exception != nullptr) *exception = nullptr;
        void *item_data = read_managed_list_element(api, list, index, exception);
        if (exception != nullptr && *exception != nullptr) continue;
        if (!is_probable_managed_pointer(item_data)) continue;
        int32_t row_item_id = 0;
        if (!read_runtime_field(api, item_data, "item", &row_item_id)) continue;
        if (row_item_id != item_id) continue;
        int32_t row_count = 0;
        (void)read_runtime_field(api, item_data, "count", &row_count);
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "native discard inventory row matched item_id=%d count=%d item_data=%p index=%d",
            row_item_id,
            row_count,
            item_data,
            index
        );
        return item_data;
    }
    __android_log_print(
        ANDROID_LOG_WARN,
        kLogTag,
        "native discard inventory row missing item_id=%d scanned=%d",
        item_id,
        count
    );
    return nullptr;
}

void *invoke_inventory_list_sorted(
    const Il2CppApi &api,
    void *service,
    const void *list_sorted,
    void *filter_delegate,
    void **exception
) {
    if (exception != nullptr) *exception = nullptr;
    if (!is_probable_managed_pointer(service) || api.runtime_invoke == nullptr) return nullptr;
    const void *resolved_list_sorted = list_sorted;
    if (resolved_list_sorted == nullptr && api.object_get_class != nullptr &&
        api.class_get_method_from_name != nullptr) {
        Il2CppClass *service_class = api.object_get_class(service);
        resolved_list_sorted = service_class == nullptr
            ? nullptr
            : api.class_get_method_from_name(service_class, "ListSortedPlayerInventory", 5);
    }
    if (resolved_list_sorted == nullptr) return nullptr;
    uint8_t cannot_delete_items = 0U;
    void *arguments[] = {
        filter_delegate,
        &cannot_delete_items,
        nullptr,
        nullptr,
        nullptr,
    };
    return api.runtime_invoke(resolved_list_sorted, service, arguments, exception);
}

void *lookup_recycle_item_data_from_inventory_service(
    ProbeContext &context,
    RuntimeBinding &binding,
    int32_t item_id,
    void **exception
) {
    if (exception != nullptr) *exception = nullptr;
    if (binding.item_inventory_list_sorted == nullptr || item_id <= 0) return nullptr;

    const Il2CppApi &api = context.il2cpp_api;
    void *service = resolve_item_inventory_service_on_main_thread(context, binding, exception);
    if (exception != nullptr && *exception != nullptr) return nullptr;
    if (!is_probable_managed_pointer(service)) return nullptr;

    if (binding.item_inventory_filter_delegate == nullptr && api_has_assemblies(api)) {
        Il2CppDomain *domain = api.domain_get();
        size_t assembly_count = 0U;
        const Il2CppAssembly **assemblies = domain == nullptr
            ? nullptr
            : api.domain_get_assemblies(domain, &assembly_count);
        if (assemblies != nullptr) {
            resolve_discard_inventory_filter_delegate(
                api,
                assemblies,
                assembly_count,
                &binding
            );
            if (binding.item_inventory_filter_delegate != nullptr) {
                std::lock_guard<std::mutex> lock(context.binding_mutex);
                context.binding.item_inventory_filter_delegate =
                    binding.item_inventory_filter_delegate;
            }
        }
    }

    void *filter_delegate = binding.item_inventory_filter_delegate;
    void *list = invoke_inventory_list_sorted(
        api,
        service,
        binding.item_inventory_list_sorted,
        filter_delegate,
        exception
    );
    if ((exception != nullptr && *exception != nullptr) || !is_probable_managed_pointer(list)) {
        if (exception != nullptr) *exception = nullptr;
        if (filter_delegate == nullptr) {
            __android_log_print(
                ANDROID_LOG_WARN,
                kLogTag,
                "native discard inventory list unavailable item_id=%d filter=0x0",
                item_id
            );
            return nullptr;
        }
        list = invoke_inventory_list_sorted(
            api,
            service,
            binding.item_inventory_list_sorted,
            nullptr,
            exception
        );
    }
    if (exception != nullptr && *exception != nullptr) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native discard inventory list exception item_id=%d exception=%p filter=%p",
            item_id,
            *exception,
            filter_delegate
        );
        return nullptr;
    }
    if (!is_probable_managed_pointer(list)) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native discard inventory list unavailable item_id=%d filter=%p",
            item_id,
            filter_delegate
        );
        return nullptr;
    }
    log_managed_list_class(api, list, "lookup");
    return find_item_data_in_managed_list(api, list, item_id, exception);
}
