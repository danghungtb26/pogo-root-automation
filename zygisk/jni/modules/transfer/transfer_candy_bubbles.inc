// Spawns native candy/XL-candy award bubbles after a successful ReleasePokemon.

template <typename T>
bool write_runtime_field_value(
    const Il2CppApi &api,
    void *object,
    const char *field_name,
    const T &value
) {
    if (object == nullptr || field_name == nullptr || api.object_get_class == nullptr ||
        api.class_get_field_from_name == nullptr || api.field_get_offset == nullptr) {
        return false;
    }
    Il2CppClass *klass = api.object_get_class(object);
    const void *field = klass == nullptr
        ? nullptr
        : api.class_get_field_from_name(klass, field_name);
    if (field == nullptr) return false;
    const size_t offset = static_cast<size_t>(api.field_get_offset(field));
    T *destination = reinterpret_cast<T *>(static_cast<uint8_t *>(object) + offset);
    *destination = value;
    return true;
}

void *create_runtime_managed_object(
    const Il2CppApi &api,
    Il2CppClass *klass
) {
    if (klass == nullptr || api.object_new == nullptr || api.runtime_object_init == nullptr ||
        api.class_get_method_from_name == nullptr) return nullptr;
    void *object = api.object_new(klass);
    if (!is_probable_managed_pointer(object)) return nullptr;
    api.runtime_object_init(object);
    const void *ctor = api.class_get_method_from_name(klass, ".ctor", 0);
    if (ctor == nullptr) return object;
    void *exception = nullptr;
    api.runtime_invoke(ctor, object, nullptr, &exception);
    return exception == nullptr ? object : nullptr;
}

bool append_loot_item_to_proto(
    const Il2CppApi &api,
    void *loot_proto,
    void *loot_item
) {
    if (!is_probable_managed_pointer(loot_proto) || !is_probable_managed_pointer(loot_item)) {
        return false;
    }
    void *loot_items = nullptr;
    if (!read_runtime_field(api, loot_proto, "LootItem", &loot_items) ||
        !is_probable_managed_pointer(loot_items) ||
        api.object_get_class == nullptr || api.class_get_method_from_name == nullptr ||
        api.runtime_invoke == nullptr) {
        return false;
    }
    Il2CppClass *items_class = api.object_get_class(loot_items);
    const void *add_method = items_class == nullptr
        ? nullptr
        : api.class_get_method_from_name(items_class, "Add", 1);
    if (add_method == nullptr) return false;
    void *add_args[] = {loot_item};
    void *add_exception = nullptr;
    api.runtime_invoke(add_method, loot_items, add_args, &add_exception);
    return add_exception == nullptr;
}

void *create_runtime_candy_loot_item(
    const Il2CppApi &api,
    Il2CppClass *loot_item_class,
    int32_t species_id,
    int32_t count,
    bool xl_candy
) {
    if (loot_item_class == nullptr || count <= 0 || species_id <= 0) return nullptr;
    void *loot_item = create_runtime_managed_object(api, loot_item_class);
    if (!is_probable_managed_pointer(loot_item)) return nullptr;
    if (!write_runtime_field_value(api, loot_item, "Count", count)) return nullptr;
    const void *set_candy = find_runtime_instance_method_in_hierarchy(
        api,
        loot_item_class,
        xl_candy ? "set_XlCandy" : "set_PokemonCandy",
        1U
    );
    if (set_candy == nullptr || api.runtime_invoke == nullptr) return nullptr;
    void *set_args[] = {&species_id};
    void *set_exception = nullptr;
    api.runtime_invoke(set_candy, loot_item, set_args, &set_exception);
    return set_exception == nullptr ? loot_item : nullptr;
}

void *build_transfer_candy_loot_proto(
    const Il2CppApi &api,
    Il2CppClass *loot_proto_class,
    Il2CppClass *loot_item_class,
    int32_t species_id,
    int32_t candy_awarded,
    int32_t xl_candy_awarded
) {
    if (loot_proto_class == nullptr || loot_item_class == nullptr || species_id <= 0 ||
        (candy_awarded <= 0 && xl_candy_awarded <= 0)) {
        return nullptr;
    }
    void *loot_proto = create_runtime_managed_object(api, loot_proto_class);
    if (!is_probable_managed_pointer(loot_proto)) return nullptr;
    if (candy_awarded > 0) {
        void *candy_item = create_runtime_candy_loot_item(
            api, loot_item_class, species_id, candy_awarded, false
        );
        if (!append_loot_item_to_proto(api, loot_proto, candy_item)) return nullptr;
    }
    if (xl_candy_awarded > 0) {
        void *xl_item = create_runtime_candy_loot_item(
            api, loot_item_class, species_id, xl_candy_awarded, true
        );
        if (!append_loot_item_to_proto(api, loot_proto, xl_item)) return nullptr;
    }
    return loot_proto;
}

bool resolve_map_avatar_canvas_position(
    ProbeContext &context,
    RuntimeBinding *binding,
    void *service,
    RuntimeVector2 *canvas_position,
    RuntimeVector3 *world_position
) {
    if (binding == nullptr || canvas_position == nullptr || world_position == nullptr ||
        binding->map_avatar_get_position == nullptr ||
        binding->award_bubble_world_to_canvas == nullptr) {
        return false;
    }
    const Il2CppApi &api = context.il2cpp_api;
    void *avatar = find_unity_object_on_main_thread(
        context, "Niantic.Holoholo.Avatar", "MapAvatar"
    );
    if (!is_probable_managed_pointer(avatar)) return false;
    void *position_boxed = invoke_runtime_method(
        api,
        binding->map_avatar_get_position,
        avatar,
        nullptr,
        "MapAvatar.get_Position"
    );
    if (!read_runtime_boxed_vector3(api, position_boxed, world_position)) return false;
    void *world_args[] = {world_position};
    void *canvas_exception = nullptr;
    void *canvas_boxed = api.runtime_invoke(
        binding->award_bubble_world_to_canvas,
        service,
        world_args,
        &canvas_exception
    );
    return canvas_exception == nullptr &&
        read_runtime_boxed_vector2(api, canvas_boxed, canvas_position);
}

bool spawn_runtime_transfer_candy_bubbles_on_main_thread(
    ProbeContext &context,
    uint64_t pokemon_id,
    int32_t species_id,
    void *release_out_proto
) {
    RuntimeBinding binding = runtime_binding_snapshot(context);
    if (pokemon_id == 0U || species_id <= 0 || !is_probable_managed_pointer(release_out_proto)) {
        return false;
    }
    const Il2CppApi &api = context.il2cpp_api;
    int32_t candy_awarded = 0;
    int32_t xl_candy_awarded = 0;
    if (!read_runtime_field(api, release_out_proto, "CandyAwarded", &candy_awarded) ||
        !read_runtime_field(api, release_out_proto, "XlCandyAwarded", &xl_candy_awarded)) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native transfer candy bubbles skipped id=%llu reason=release_layout_unavailable",
            static_cast<unsigned long long>(pokemon_id)
        );
        return false;
    }
    if (candy_awarded <= 0 && xl_candy_awarded <= 0) {
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "native transfer candy bubbles skipped id=%llu reason=no_candy_awarded",
            static_cast<unsigned long long>(pokemon_id)
        );
        return false;
    }
    void *service = resolve_award_item_bubble_service_on_main_thread(context, &binding);
    if (!binding.transfer_candy_bubbles_verified || service == nullptr) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native transfer candy bubbles skipped id=%llu verified=%d service=%p",
            static_cast<unsigned long long>(pokemon_id),
            binding.transfer_candy_bubbles_verified ? 1 : 0,
            service
        );
        return false;
    }
    RuntimeVector2 canvas_position{};
    RuntimeVector3 world_position{};
    if (!resolve_map_avatar_canvas_position(
            context, &binding, service, &canvas_position, &world_position)) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native transfer candy bubbles skipped id=%llu reason=canvas_position_unavailable",
            static_cast<unsigned long long>(pokemon_id)
        );
        return false;
    }
    void *loot_proto = build_transfer_candy_loot_proto(
        api,
        binding.loot_proto_class,
        binding.loot_item_proto_class,
        species_id,
        candy_awarded,
        xl_candy_awarded
    );
    if (!is_probable_managed_pointer(loot_proto)) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native transfer candy bubbles skipped id=%llu reason=loot_proto_unavailable "
            "species=%d candy=%d xl=%d",
            static_cast<unsigned long long>(pokemon_id),
            species_id,
            candy_awarded,
            xl_candy_awarded
        );
        return false;
    }
    void *bubble_list = create_award_bubble_data_list(api, binding.award_bubble_data_list_class);
    if (!is_probable_managed_pointer(bubble_list)) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native transfer candy bubbles skipped id=%llu reason=bubble_list_unavailable",
            static_cast<unsigned long long>(pokemon_id)
        );
        return false;
    }
    uint8_t combine_duplicates = 0U;
    void *gather_candy_args[] = {bubble_list, loot_proto, &combine_duplicates};
    void *gather_candy_exception = nullptr;
    api.runtime_invoke(
        binding.award_bubble_gather_candy,
        service,
        gather_candy_args,
        &gather_candy_exception
    );
    if (gather_candy_exception != nullptr) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native transfer candy bubbles GatherCandyData failed id=%llu exception=%p",
            static_cast<unsigned long long>(pokemon_id),
            gather_candy_exception
        );
        return false;
    }
    if (xl_candy_awarded > 0 && binding.award_bubble_gather_xl_candy != nullptr) {
        void *gather_xl_args[] = {bubble_list, loot_proto};
        void *gather_xl_exception = nullptr;
        api.runtime_invoke(
            binding.award_bubble_gather_xl_candy,
            service,
            gather_xl_args,
            &gather_xl_exception
        );
        if (gather_xl_exception != nullptr) {
            __android_log_print(
                ANDROID_LOG_WARN,
                kLogTag,
                "native transfer candy bubbles GatherXlCandyData failed id=%llu exception=%p",
                static_cast<unsigned long long>(pokemon_id),
                gather_xl_exception
            );
            return false;
        }
    }
    char context_label[96];
    snprintf(
        context_label,
        sizeof(context_label),
        "id=%llu species=%d candy=%d xl=%d",
        static_cast<unsigned long long>(pokemon_id),
        species_id,
        candy_awarded,
        xl_candy_awarded
    );
    return spawn_award_bubbles_at_canvas(
        api,
        binding,
        service,
        bubble_list,
        canvas_position,
        "native transfer candy bubbles spawned",
        context_label,
        world_position
    );
}
