// Spawns the game's native award-item bubbles after a successful FortSearch spin.

struct RuntimeVector2 {
    float x;
    float y;
};

bool spawn_award_bubbles_at_canvas(
    const Il2CppApi &api,
    const RuntimeBinding &binding,
    void *service,
    void *bubble_list,
    const RuntimeVector2 &canvas_position,
    const char *success_log_prefix,
    const char *context_label,
    const RuntimeVector3 &world_position
);

struct RuntimeAwardBubbleConfig {
    void *scope = nullptr;
    RuntimeVector2 position{};
    float delay_sec = 0.15f;
    uint8_t lifetime_has_value = 0U;
    uint8_t lifetime_padding[3]{};
    float lifetime_value = 0.0f;
    void *last_bubble_spawned = nullptr;
    void *last_bubble_popped = nullptr;
    uint8_t is_additive = 0U;
    uint8_t trailing_padding[7]{};
};

void *resolve_award_item_bubble_service_on_main_thread(
    ProbeContext &context,
    RuntimeBinding *binding
) {
    if (binding == nullptr) return nullptr;
    ensure_award_item_bubble_bindings(context, binding);
    return binding->award_item_bubble_service;
}

bool read_runtime_boxed_vector3(
    const Il2CppApi &api,
    void *boxed,
    RuntimeVector3 *value
) {
    if (value == nullptr || !is_probable_managed_pointer(boxed) || api.object_unbox == nullptr) {
        return false;
    }
    void *unboxed = api.object_unbox(boxed);
    if (!is_probable_managed_pointer(unboxed)) return false;
    memcpy(value, unboxed, sizeof(*value));
    return std::isfinite(value->x) && std::isfinite(value->y) && std::isfinite(value->z);
}

bool read_runtime_boxed_vector2(
    const Il2CppApi &api,
    void *boxed,
    RuntimeVector2 *value
) {
    if (value == nullptr || !is_probable_managed_pointer(boxed) || api.object_unbox == nullptr) {
        return false;
    }
    void *unboxed = api.object_unbox(boxed);
    if (!is_probable_managed_pointer(unboxed)) return false;
    memcpy(value, unboxed, sizeof(*value));
    return std::isfinite(value->x) && std::isfinite(value->y);
}

void *create_award_bubble_data_list(
    const Il2CppApi &api,
    Il2CppClass *list_class
) {
    if (list_class == nullptr || api.object_new == nullptr || api.runtime_object_init == nullptr ||
        api.class_get_method_from_name == nullptr) return nullptr;
    void *list = api.object_new(list_class);
    if (!is_probable_managed_pointer(list)) return nullptr;
    api.runtime_object_init(list);
    const void *ctor = api.class_get_method_from_name(list_class, ".ctor", 0);
    if (ctor == nullptr) return list;
    void *exception = nullptr;
    api.runtime_invoke(ctor, list, nullptr, &exception);
    return exception == nullptr ? list : nullptr;
}

bool spawn_runtime_spin_item_bubbles_on_main_thread(
    ProbeContext &context,
    const std::string &fort_id,
    void *fort_search_out_proto
) {
    RuntimeBinding binding = runtime_binding_snapshot(context);
    if (fort_id.empty() || !is_probable_managed_pointer(fort_search_out_proto)) return false;
    const Il2CppApi &api = context.il2cpp_api;
    void *service = resolve_award_item_bubble_service_on_main_thread(context, &binding);
    if (!binding.spin_item_bubbles_verified || service == nullptr) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native spin item bubbles skipped fort=%s verified=%d service=%p list=%p "
            "gather=%p spawn=%p world_to_canvas=%p map_transform=%p transform_pos=%p",
            fort_id.c_str(),
            binding.spin_item_bubbles_verified ? 1 : 0,
            service,
            binding.award_bubble_data_list_class,
            binding.award_bubble_gather_fort_search,
            binding.award_bubble_spawn_bubbles,
            binding.award_bubble_world_to_canvas,
            binding.map_place_get_transform,
            binding.unity_transform_get_position
        );
        return false;
    }
    void *pokestop = find_runtime_pokestop_by_id(context, fort_id);
    if (pokestop == nullptr) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native spin item bubbles skipped fort=%s reason=pokestop_not_found",
            fort_id.c_str()
        );
        return false;
    }
    void *transform = invoke_runtime_method(
        api,
        binding.map_place_get_transform,
        pokestop,
        nullptr,
        "AbstractMapPlace.get_MapPlaceTransform"
    );
    if (!is_probable_managed_pointer(transform)) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native spin item bubbles skipped fort=%s reason=map_transform_unavailable",
            fort_id.c_str()
        );
        return false;
    }
    if (binding.award_bubble_add_child != nullptr) {
        void *add_child_args[] = {transform};
        void *add_child_exception = nullptr;
        api.runtime_invoke(
            binding.award_bubble_add_child,
            service,
            add_child_args,
            &add_child_exception
        );
        if (add_child_exception != nullptr) {
            __android_log_print(
                ANDROID_LOG_WARN,
                kLogTag,
                "native spin item bubbles AddChild failed fort=%s exception=%p",
                fort_id.c_str(),
                add_child_exception
            );
        }
    }
    void *position_boxed = invoke_runtime_method(
        api,
        binding.unity_transform_get_position,
        transform,
        nullptr,
        "Transform.get_position"
    );
    RuntimeVector3 world_position{};
    if (!read_runtime_boxed_vector3(api, position_boxed, &world_position)) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native spin item bubbles skipped fort=%s reason=world_position_unavailable",
            fort_id.c_str()
        );
        return false;
    }
    void *world_args[] = {&world_position};
    void *canvas_exception = nullptr;
    void *canvas_boxed = api.runtime_invoke(
        binding.award_bubble_world_to_canvas,
        service,
        world_args,
        &canvas_exception
    );
    RuntimeVector2 canvas_position{};
    if (canvas_exception != nullptr ||
        !read_runtime_boxed_vector2(api, canvas_boxed, &canvas_position)) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native spin item bubbles skipped fort=%s reason=canvas_position_unavailable exception=%p",
            fort_id.c_str(),
            canvas_exception
        );
        return false;
    }
    void *bubble_list = create_award_bubble_data_list(api, binding.award_bubble_data_list_class);
    if (!is_probable_managed_pointer(bubble_list)) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native spin item bubbles skipped fort=%s reason=bubble_list_unavailable",
            fort_id.c_str()
        );
        return false;
    }
    void *gather_args[] = {bubble_list, fort_search_out_proto};
    void *gather_exception = nullptr;
    api.runtime_invoke(
        binding.award_bubble_gather_fort_search,
        service,
        gather_args,
        &gather_exception
    );
    if (gather_exception != nullptr) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native spin item bubbles GatherBubbleData failed fort=%s exception=%p",
            fort_id.c_str(),
            gather_exception
        );
        return false;
    }
    return spawn_award_bubbles_at_canvas(
        api,
        binding,
        service,
        bubble_list,
        canvas_position,
        "native spin item bubbles spawned",
        fort_id.c_str(),
        world_position
    );
}

bool spawn_award_bubbles_at_canvas(
    const Il2CppApi &api,
    const RuntimeBinding &binding,
    void *service,
    void *bubble_list,
    const RuntimeVector2 &canvas_position,
    const char *success_log_prefix,
    const char *context_label,
    const RuntimeVector3 &world_position
) {
    if (!is_probable_managed_pointer(service) || !is_probable_managed_pointer(bubble_list) ||
        binding.award_bubble_spawn_bubbles == nullptr) return false;
    if (binding.award_bubble_set_visibility != nullptr) {
        uint8_t visible = 1U;
        void *visibility_args[] = {&visible};
        void *visibility_exception = nullptr;
        api.runtime_invoke(
            binding.award_bubble_set_visibility,
            service,
            visibility_args,
            &visibility_exception
        );
    }
    RuntimeAwardBubbleConfig config{};
    config.position = canvas_position;
    config.delay_sec = 0.15f;
    void *spawn_args[] = {bubble_list, &config};
    void *spawn_exception = nullptr;
    api.runtime_invoke(
        binding.award_bubble_spawn_bubbles,
        service,
        spawn_args,
        &spawn_exception
    );
    __android_log_print(
        spawn_exception == nullptr ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "%s context=%s world=(%.2f,%.2f,%.2f) canvas=(%.1f,%.1f) service=%p exception=%p",
        success_log_prefix == nullptr ? "native award bubbles spawned" : success_log_prefix,
        context_label == nullptr ? "" : context_label,
        world_position.x,
        world_position.y,
        world_position.z,
        canvas_position.x,
        canvas_position.y,
        service,
        spawn_exception
    );
    return spawn_exception == nullptr;
}
