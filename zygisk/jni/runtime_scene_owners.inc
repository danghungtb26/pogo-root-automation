[[maybe_unused]] void discover_scene_runtime_owners(ProbeContext &context) {
    std::lock_guard<std::mutex> binding_lock(context.binding_mutex);
    RuntimeBinding &binding = context.binding;
    if (binding.map_entity_read_verified || !ensure_main_thread_bridge(context)) return;
    void *map_view = request_main_thread_object(
        context, "Niantic.Holoholo.Map", "MapSceneViewService"
    );
    if (map_view == nullptr) {
        void *map_scene = request_main_thread_object(
            context, "Niantic.Holoholo.Map", "MapScene"
        );
        if (map_scene != nullptr) {
            void *scene_view = nullptr;
            if (read_runtime_field(context.il2cpp_api, map_scene, "ehis", &scene_view) &&
                runtime_object_is_exact_class(
                    context.il2cpp_api,
                    scene_view,
                    "Niantic.Holoholo.Map",
                    "MapSceneViewService"
                )) {
                map_view = scene_view;
                __android_log_print(
                    ANDROID_LOG_INFO,
                    kLogTag,
                    "MapScene.ehis yielded MapSceneViewService object=%p",
                    map_view
                );
            }
        }
    }
    if (map_view == nullptr) {
        __android_log_print(ANDROID_LOG_INFO, kLogTag, "scene map service unavailable");
        return;
    }
    binding.map_scene_view_service = map_view;
    void *content_handler = nullptr;
    if (!read_runtime_field(context.il2cpp_api, map_view, "ehkb", &content_handler) ||
        !runtime_object_is_exact_class(
            context.il2cpp_api, content_handler, "Niantic.Holoholo.Map", "MapContentHandler"
        )) {
        __android_log_print(ANDROID_LOG_INFO, kLogTag, "MapSceneViewService content handler unavailable");
        return;
    }
    binding.map_content_handler = content_handler;
    void *map_service = nullptr;
    if (read_runtime_field(context.il2cpp_api, content_handler, "egft", &map_service)) {
        bind_map_entity_service(context.il2cpp_api, map_service, &binding, "MapSceneViewService.MapEntityService");
    } else {
        __android_log_print(ANDROID_LOG_INFO, kLogTag, "MapContentHandler egft field unavailable");
    }
    if (!binding.map_entity_read_verified) {
        __android_log_print(ANDROID_LOG_INFO, kLogTag, "MapSceneViewService map entity service unavailable");
    }
}
