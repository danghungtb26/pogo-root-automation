// Resolves MapSceneContext and its Zenject scene DiContainer for discard lookups.

constexpr size_t kDiscardSceneObjectArrayDataOffset = sizeof(void *) * 4U;

bool discard_scene_api_has_assemblies(const Il2CppApi &api) {
    return api.domain_get != nullptr && api.domain_get_assemblies != nullptr;
}

bool discard_runtime_class_is_map_scene_context(const Il2CppApi &api, Il2CppClass *klass) {
    if (klass == nullptr || api.class_get_name == nullptr) return false;
    const char *class_name = api.class_get_name(klass);
    return class_name != nullptr && strcmp(class_name, "MapSceneContext") == 0;
}

bool discard_runtime_object_is_map_scene_context(const Il2CppApi &api, void *object) {
    return runtime_object_is_exact_class(api, object, "Niantic.Holoholo", "MapSceneContext");
}

void cache_runtime_map_scene_context(ProbeContext &context, void *scene) {
    if (!discard_runtime_object_is_map_scene_context(context.il2cpp_api, scene)) return;
    std::lock_guard<std::mutex> lock(context.binding_mutex);
    context.binding.map_scene_context = scene;
}

void *find_first_discard_scene_context_object(
    ProbeContext &context,
    const char *name_space,
    const char *class_name
) {
    const Il2CppApi &api = context.il2cpp_api;
    if (!discard_scene_api_has_assemblies(api) || name_space == nullptr || class_name == nullptr ||
        api.class_get_type == nullptr || api.type_get_object == nullptr ||
        api.array_length == nullptr || api.array_get_byte_length == nullptr) {
        return nullptr;
    }
    Il2CppDomain *domain = api.domain_get();
    size_t assembly_count = 0U;
    const Il2CppAssembly **assemblies = domain == nullptr
        ? nullptr
        : api.domain_get_assemblies(domain, &assembly_count);
    if (assemblies == nullptr) return nullptr;
    Il2CppClass *object_class = find_runtime_class(
        api, assemblies, assembly_count, "UnityEngine", "Object", nullptr
    );
    Il2CppClass *target_class = find_runtime_class(
        api, assemblies, assembly_count, name_space, class_name, nullptr
    );
    const void *find_method = object_class == nullptr || api.method_is_instance == nullptr
        ? nullptr
        : find_runtime_method_with_parameter(api, object_class, "FindObjectsOfType", "System.Type");
    if (target_class == nullptr || find_method == nullptr || api.method_is_instance(find_method)) {
        return nullptr;
    }
    const Il2CppType *target_type = api.class_get_type(target_class);
    void *type_object = target_type == nullptr ? nullptr : api.type_get_object(target_type);
    if (type_object == nullptr || api.runtime_invoke == nullptr) return nullptr;
    void *arguments[] = {type_object};
    void *objects = api.runtime_invoke(find_method, nullptr, arguments, nullptr);
    if (!is_probable_managed_pointer(objects)) return nullptr;
    const size_t length = api.array_length(objects);
    const size_t byte_length = api.array_get_byte_length(objects);
    if (length == 0U || length > 16U || byte_length < length * sizeof(void *)) return nullptr;
    const auto *data = static_cast<const uint8_t *>(objects) + kDiscardSceneObjectArrayDataOffset;
    for (size_t index = 0U; index < length; ++index) {
        void *object = nullptr;
        memcpy(&object, data + index * sizeof(void *), sizeof(object));
        if (is_probable_managed_pointer(object)) return object;
    }
    return nullptr;
}

void *resolve_runtime_map_scene_context_on_main_thread(
    ProbeContext &context,
    RuntimeBinding &binding
) {
    if (discard_runtime_object_is_map_scene_context(context.il2cpp_api, binding.map_scene_context)) {
        return binding.map_scene_context;
    }
    void *scene = find_unity_object_on_main_thread(context, "Niantic.Holoholo", "MapSceneContext");
    if (!is_probable_managed_pointer(scene)) {
        scene = find_first_discard_scene_context_object(context, "Niantic.Holoholo", "MapSceneContext");
    }
    if (!is_probable_managed_pointer(scene)) {
        scene = find_unity_object_on_main_thread(context, "Zenject", "SceneContext");
    }
    if (discard_runtime_object_is_map_scene_context(context.il2cpp_api, scene)) {
        binding.map_scene_context = scene;
        cache_runtime_map_scene_context(context, scene);
        return scene;
    }
    return nullptr;
}

void *resolve_runtime_scene_container_from_registry(
    const Il2CppApi &api,
    const RuntimeBinding &binding,
    void *project_container,
    void **exception
) {
    if (exception != nullptr) *exception = nullptr;
    if (!is_probable_managed_pointer(project_container) || binding.di_try_resolve == nullptr ||
        api.runtime_invoke == nullptr || !discard_scene_api_has_assemblies(api)) {
        return nullptr;
    }
    Il2CppDomain *domain = api.domain_get();
    size_t assembly_count = 0U;
    const Il2CppAssembly **assemblies = domain == nullptr
        ? nullptr
        : api.domain_get_assemblies(domain, &assembly_count);
    if (assemblies == nullptr) return nullptr;
    Il2CppClass *registry_class = find_runtime_class(
        api, assemblies, assembly_count, "Zenject", "SceneContextRegistry", nullptr
    );
    if (registry_class == nullptr) return nullptr;
    const Il2CppType *registry_type = api.class_get_type == nullptr
        ? nullptr
        : api.class_get_type(registry_class);
    void *type_object = registry_type == nullptr || api.type_get_object == nullptr
        ? nullptr
        : api.type_get_object(registry_type);
    if (!is_probable_managed_pointer(type_object)) return nullptr;
    void *arguments[] = {type_object};
    void *registry = api.runtime_invoke(binding.di_try_resolve, project_container, arguments, exception);
    if (exception != nullptr && *exception != nullptr) {
        if (exception != nullptr) *exception = nullptr;
        return nullptr;
    }
    if (!is_probable_managed_pointer(registry) || api.object_get_class == nullptr ||
        api.class_get_method_from_name == nullptr) {
        return nullptr;
    }
    Il2CppClass *registry_object_class = api.object_get_class(registry);
    const void *get_scene_contexts = registry_object_class == nullptr
        ? nullptr
        : api.class_get_method_from_name(registry_object_class, "get_SceneContexts", 0);
    if (get_scene_contexts == nullptr) return nullptr;
    void *enumerable = api.runtime_invoke(get_scene_contexts, registry, nullptr, exception);
    if (exception != nullptr && *exception != nullptr) {
        if (exception != nullptr) *exception = nullptr;
        return nullptr;
    }
    if (!is_probable_managed_pointer(enumerable)) return nullptr;
    Il2CppClass *enumerable_class = api.object_get_class(enumerable);
    const void *get_enumerator = enumerable_class == nullptr
        ? nullptr
        : api.class_get_method_from_name(enumerable_class, "GetEnumerator", 0);
    if (get_enumerator == nullptr) return nullptr;
    void *enumerator = api.runtime_invoke(get_enumerator, enumerable, nullptr, exception);
    if (exception != nullptr && *exception != nullptr) {
        if (exception != nullptr) *exception = nullptr;
        return nullptr;
    }
    if (!is_probable_managed_pointer(enumerator)) return nullptr;
    Il2CppClass *enumerator_class = api.object_get_class(enumerator);
    const void *move_next = enumerator_class == nullptr
        ? nullptr
        : api.class_get_method_from_name(enumerator_class, "MoveNext", 0);
    const void *get_current = enumerator_class == nullptr
        ? nullptr
        : api.class_get_method_from_name(enumerator_class, "get_Current", 0);
    if (move_next == nullptr || get_current == nullptr) return nullptr;
    for (int32_t step = 0; step < 8; ++step) {
        if (exception != nullptr) *exception = nullptr;
        void *moved = api.runtime_invoke(move_next, enumerator, nullptr, exception);
        if (exception != nullptr && *exception != nullptr) break;
        bool has_next = false;
        if (moved != nullptr) memcpy(&has_next, moved, sizeof(has_next));
        if (!has_next) break;
        if (exception != nullptr) *exception = nullptr;
        void *scene_context = api.runtime_invoke(get_current, enumerator, nullptr, exception);
        if (exception != nullptr && *exception != nullptr) continue;
        if (!is_probable_managed_pointer(scene_context)) continue;
        Il2CppClass *scene_class = api.object_get_class(scene_context);
        if (!discard_runtime_class_is_map_scene_context(api, scene_class)) continue;
        const void *container_method = scene_class == nullptr
            ? nullptr
            : api.class_get_method_from_name(scene_class, "get_Container", 0);
        if (container_method == nullptr) continue;
        if (exception != nullptr) *exception = nullptr;
        void *container = api.runtime_invoke(container_method, scene_context, nullptr, exception);
        if (exception != nullptr && *exception != nullptr) continue;
        if (is_probable_managed_pointer(container)) return container;
    }
    return nullptr;
}

void *resolve_runtime_scene_di_container(
    ProbeContext &context,
    RuntimeBinding &binding,
    void *project_container,
    void **exception
) {
    if (exception != nullptr) *exception = nullptr;
    const Il2CppApi &api = context.il2cpp_api;
    void *scene = resolve_runtime_map_scene_context_on_main_thread(context, binding);
    if (!is_probable_managed_pointer(scene)) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native discard scene context unavailable cached=%p",
            binding.map_scene_context
        );
        return resolve_runtime_scene_container_from_registry(api, binding, project_container, exception);
    }
    if (api.object_get_class == nullptr || api.class_get_method_from_name == nullptr ||
        api.runtime_invoke == nullptr) return nullptr;
    Il2CppClass *scene_class = api.object_get_class(scene);
    const void *container_method = scene_class == nullptr
        ? nullptr
        : api.class_get_method_from_name(scene_class, "get_Container", 0);
    if (container_method == nullptr) return nullptr;
    void *container = api.runtime_invoke(container_method, scene, nullptr, exception);
    if (exception != nullptr && *exception != nullptr) return nullptr;
    if (!is_probable_managed_pointer(container)) {
        return resolve_runtime_scene_container_from_registry(api, binding, project_container, exception);
    }
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "native discard scene container resolved scene=%p container=%p",
        scene,
        container
    );
    return container;
}
