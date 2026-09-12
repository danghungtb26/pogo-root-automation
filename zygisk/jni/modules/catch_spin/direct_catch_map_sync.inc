// Map-side postcondition for a successful/fled direct catch. Resolve the live
// owners again at invocation time because MapContentHandler can be recreated
// across map scene transitions.

namespace {
void *resolve_map_scene_view_service_on_main_thread(
    ProbeContext &context,
    const RuntimeBinding &binding
) {
    const Il2CppApi &api = context.il2cpp_api;
    if (runtime_object_is_exact_class(
            api,
            binding.map_scene_view_service,
            "Niantic.Holoholo.Map",
            "MapSceneViewService"
        )) return binding.map_scene_view_service;

    void *map_view = find_unity_object_on_main_thread(
        context, "Niantic.Holoholo.Map", "MapSceneViewService"
    );
    if (map_view != nullptr) return map_view;

    void *map_scene = find_unity_object_on_main_thread(
        context, "Niantic.Holoholo.Map", "MapScene"
    );
    if (map_scene == nullptr) return nullptr;
    void *scene_view = nullptr;
    if (!read_runtime_field(api, map_scene, "ehis", &scene_view) ||
        !runtime_object_is_exact_class(
            api,
            scene_view,
            "Niantic.Holoholo.Map",
            "MapSceneViewService"
        )) return nullptr;
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "runtime direct catch map sync resolved MapScene.ehis service=%p",
        scene_view
    );
    return scene_view;
}

void *current_map_content_handler(
    ProbeContext &context,
    const RuntimeBinding &binding
) {
    const Il2CppApi &api = context.il2cpp_api;
    if (runtime_object_is_exact_class(
            api,
            binding.map_content_handler,
            "Niantic.Holoholo.Map",
            "MapContentHandler"
        )) return binding.map_content_handler;
    void *map_view = resolve_map_scene_view_service_on_main_thread(context, binding);
    if (map_view == nullptr) return nullptr;
    void *handler = nullptr;
    if (read_runtime_field(
            api,
            map_view,
            "ehkb",
            &handler
        ) && runtime_object_is_exact_class(
            api, handler, "Niantic.Holoholo.Map", "MapContentHandler"
        )) return handler;
    return nullptr;
}

bool invoke_map_refresh_method(
    const Il2CppApi &api,
    const void *method,
    void *owner,
    uint64_t encounter_id,
    const char *label
) {
    if (method == nullptr || owner == nullptr || api.runtime_invoke == nullptr) return false;
    void *exception = nullptr;
    void *arguments[] = {&encounter_id};
    api.runtime_invoke(method, owner, arguments, &exception);
    __android_log_print(
        exception == nullptr ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "runtime direct catch map sync method=%s owner=%p method=%p id=%llu exception=%p",
        label == nullptr ? "<unknown>" : label,
        owner,
        method,
        static_cast<unsigned long long>(encounter_id),
        exception
    );
    return exception == nullptr;
}

bool invoke_map_refresh_no_args(
    const Il2CppApi &api,
    const void *method,
    void *owner,
    const char *label
) {
    if (method == nullptr || owner == nullptr || api.runtime_invoke == nullptr) return false;
    void *exception = nullptr;
    api.runtime_invoke(method, owner, nullptr, &exception);
    __android_log_print(
        exception == nullptr ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "runtime direct catch map sync method=%s owner=%p method=%p exception=%p",
        label == nullptr ? "<unknown>" : label,
        owner,
        method,
        exception
    );
    return exception == nullptr;
}

void refresh_map_after_direct_catch(
    ProbeContext &context,
    uint64_t encounter_id,
    uint32_t status
) {
    if (!direct_catch_status_removes_map_entity(status)) return;
    const Il2CppApi &api = context.il2cpp_api;
    RuntimeBinding binding = runtime_binding_snapshot(context);
    void *handler = current_map_content_handler(context, binding);
    bool entity_removed = false;
    if (handler != nullptr) {
        Il2CppClass *handler_class = api.object_get_class == nullptr
            ? nullptr : api.object_get_class(handler);
        const void *register_method = binding.map_content_handler_register_caught_or_fled;
        if (register_method == nullptr) {
            register_method = find_direct_map_instance_method(
                api, handler_class, "RegisterPokemonCaughtOrFled", 1U
            );
        }
        entity_removed = invoke_map_refresh_method(
            api,
            register_method,
            handler,
            encounter_id,
            "MapContentHandler.RegisterPokemonCaughtOrFled"
        );
        if (register_method == nullptr) {
            __android_log_print(
                ANDROID_LOG_WARN,
                kLogTag,
                "runtime direct catch map sync register method unavailable handler=%p",
                handler
            );
        }
    }
    if (!entity_removed && runtime_object_is_exact_class(
            api,
            binding.map_entity_service,
            "Niantic.Holoholo.Map",
            "MapEntityService"
        )) {
        Il2CppClass *service_class = api.object_get_class(binding.map_entity_service);
        const void *remove_method = find_direct_map_instance_method(
            api, service_class, "RemoveWildPokemon", 1U
        );
        entity_removed = invoke_map_refresh_method(
            api,
            remove_method,
            binding.map_entity_service,
            encounter_id,
            "MapEntityService.RemoveWildPokemon"
        );
    }
    if (!entity_removed) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "runtime direct catch map sync could not remove encounter=%llu",
            static_cast<unsigned long long>(encounter_id)
        );
    }
    if (handler == nullptr) return;
    Il2CppClass *handler_class = api.object_get_class == nullptr
        ? nullptr : api.object_get_class(handler);
    const void *force_refresh = find_direct_map_instance_method(
        api, handler_class, "ForceRefreshVisibleCells", 0U
    );
    if (!invoke_map_refresh_no_args(
            api,
            force_refresh,
            handler,
            "MapContentHandler.ForceRefreshVisibleCells"
        )) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "runtime direct catch map sync force refresh unavailable handler=%p",
            handler
        );
    }
}
}  // namespace
