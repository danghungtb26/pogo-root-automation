// Shared, version-scoped map refresh for catch and spin. Calls run on Unity main thread.

namespace {
bool g_runtime_map_refresh_pending = false;
uint64_t g_runtime_map_refresh_retry_ms = 0U;

void *resolve_map_scene_view_service_on_main_thread(
    ProbeContext &context,
    const RuntimeBinding &binding
) {
    const Il2CppApi &api = context.il2cpp_api;
    (void)binding;

    void *map_view = find_unity_object_on_main_thread(
        context, "Niantic.Holoholo.Map", "MapSceneViewService"
    );
    if (runtime_object_is_exact_class(api, map_view,
            "Niantic.Holoholo.Map", "MapSceneViewService")) return map_view;

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

bool refresh_runtime_map_after_action(ProbeContext &context, const char *reason) {
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    if (!binding.exact_build_verified) return false;
    g_runtime_map_refresh_pending = true;
    g_runtime_map_refresh_retry_ms = static_cast<uint64_t>(now_epoch_millis()) + 5000U;
    void *handler = current_map_content_handler(context, binding);
    if (handler == nullptr || context.il2cpp_api.object_get_class == nullptr) {
        __android_log_print(ANDROID_LOG_WARN, kLogTag,
            "runtime map refresh deferred reason=%s owner unavailable", reason);
        return false;
    }
    const void *refresh = find_direct_map_instance_method(context.il2cpp_api,
        context.il2cpp_api.object_get_class(handler), "ForceRefreshVisibleCells", 0U);
    const bool requested = invoke_map_refresh_no_args(context.il2cpp_api, refresh, handler,
        "MapContentHandler.ForceRefreshVisibleCells");
    __android_log_print(requested ? ANDROID_LOG_INFO : ANDROID_LOG_WARN, kLogTag,
        "runtime map refresh reason=%s requested=%d handler=%p", reason, requested ? 1 : 0, handler);
    g_runtime_map_refresh_pending = !requested;
    return requested;
}

void retry_runtime_map_refresh(ProbeContext &context, uint64_t now_ms) {
    if (g_runtime_map_refresh_pending && now_ms >= g_runtime_map_refresh_retry_ms) {
        (void)refresh_runtime_map_after_action(context, "post-action retry");
    }
}
} // namespace
