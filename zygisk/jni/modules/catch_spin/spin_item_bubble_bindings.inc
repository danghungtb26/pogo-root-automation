// Version-scoped bindings for native PokéStop item bubble VFX via the game's
// IAwardItemBubbleService (AwardItemBubbleService in build 0.427.0).

using SpinClassInflate = Il2CppClass *(*)(Il2CppClass *, const Il2CppType **, uint32_t);

SpinClassInflate spin_lookup_class_inflate(const Il2CppApi &api) {
    if (api.handle == nullptr) return nullptr;
    return reinterpret_cast<SpinClassInflate>(dlsym(api.handle, "il2cpp_class_inflate"));
}

const void *find_spin_instance_method_with_param_substring(
    const Il2CppApi &api,
    Il2CppClass *klass,
    const char *method_name,
    uint32_t parameter_count,
    uint32_t parameter_index,
    const char *parameter_substring
) {
    if (!has_runtime_inspection_api(api) || klass == nullptr || method_name == nullptr ||
        parameter_substring == nullptr || parameter_index >= parameter_count) return nullptr;
    void *iterator = nullptr;
    for (uint32_t inspected = 0U; inspected < 512U; ++inspected) {
        const void *method = api.class_get_methods(klass, &iterator);
        if (method == nullptr) break;
        const char *candidate_name = api.method_get_name(method);
        if (candidate_name == nullptr || strcmp(candidate_name, method_name) != 0 ||
            api.method_get_param_count(method) != parameter_count ||
            !api.method_is_instance(method)) continue;
        const Il2CppType *parameter_type = api.method_get_param(method, parameter_index);
        const char *parameter_name = parameter_type == nullptr || api.type_get_name == nullptr
            ? nullptr
            : api.type_get_name(parameter_type);
        if (parameter_name != nullptr && strstr(parameter_name, parameter_substring) != nullptr) {
            return method;
        }
    }
    return nullptr;
}

bool award_bubble_list_class_references_data(const Il2CppApi &api, Il2CppClass *klass) {
    if (klass == nullptr || api.class_get_type == nullptr || api.type_get_name == nullptr) {
        return false;
    }
    const Il2CppType *type = api.class_get_type(klass);
    const char *type_name = type == nullptr ? nullptr : api.type_get_name(type);
    if (type_name == nullptr) return false;
    return strstr(type_name, "AwardBubbleData") != nullptr;
}

Il2CppClass *resolve_award_bubble_list_class_from_method(
    const Il2CppApi &api,
    const void *gather_method
) {
    if (gather_method == nullptr || api.method_get_param == nullptr ||
        api.type_get_class_or_element_class == nullptr) {
        return nullptr;
    }
    const Il2CppType *parameter_type = api.method_get_param(gather_method, 0U);
    if (parameter_type == nullptr) return nullptr;
    Il2CppClass *list_class = api.type_get_class_or_element_class(parameter_type);
    if (list_class == nullptr || api.class_get_method_from_name == nullptr) return nullptr;
    const void *ctor = api.class_get_method_from_name(list_class, ".ctor", 0);
    return ctor == nullptr ? nullptr : list_class;
}

Il2CppClass *scan_award_bubble_data_list_class(
    const Il2CppApi &api,
    const Il2CppAssembly **assemblies,
    size_t assembly_count
) {
    if (assemblies == nullptr || api.assembly_get_image == nullptr ||
        api.image_get_class_count == nullptr || api.image_get_class == nullptr) {
        return nullptr;
    }
    constexpr size_t kMaxClasses = 100000U;
    size_t scanned_classes = 0U;
    for (size_t assembly_index = 0U; assembly_index < assembly_count &&
         scanned_classes < kMaxClasses; ++assembly_index) {
        const Il2CppAssembly *assembly = assemblies[assembly_index];
        if (assembly == nullptr) continue;
        const Il2CppImage *image = api.assembly_get_image(assembly);
        if (image == nullptr) continue;
        const size_t class_count = api.image_get_class_count(image);
        for (size_t class_index = 0U; class_index < class_count &&
             scanned_classes < kMaxClasses; ++class_index) {
            Il2CppClass *klass = api.image_get_class(image, class_index);
            ++scanned_classes;
            if (!award_bubble_list_class_references_data(api, klass)) continue;
            if (api.class_get_method_from_name == nullptr) continue;
            const void *ctor = api.class_get_method_from_name(klass, ".ctor", 0);
            if (ctor != nullptr) return klass;
        }
    }
    return nullptr;
}

Il2CppClass *resolve_award_bubble_data_list_class(
    const Il2CppApi &api,
    const Il2CppAssembly **assemblies,
    size_t assembly_count
) {
    Il2CppClass *element_class = find_runtime_class(
        api, assemblies, assembly_count, "Niantic.Holoholo.UI", "AwardBubbleData", nullptr
    );
    Il2CppClass *list_generic = find_runtime_class(
        api, assemblies, assembly_count, "System.Collections.Generic", "List`1", nullptr
    );
    SpinClassInflate class_inflate = spin_lookup_class_inflate(api);
    if (element_class != nullptr && list_generic != nullptr && class_inflate != nullptr &&
        api.class_get_type != nullptr) {
        const Il2CppType *element_type = api.class_get_type(element_class);
        if (element_type != nullptr) {
            const Il2CppType *type_args[] = {element_type};
            Il2CppClass *inflated = class_inflate(list_generic, type_args, 1U);
            if (inflated != nullptr && api.class_get_method_from_name != nullptr &&
                api.class_get_method_from_name(inflated, ".ctor", 0) != nullptr) {
                return inflated;
            }
        }
    }
    return scan_award_bubble_data_list_class(api, assemblies, assembly_count);
}

void bind_spin_item_bubble_service_methods(
    const Il2CppApi &api,
    RuntimeBinding *binding,
    void *service
) {
    if (binding == nullptr || service == nullptr || api.object_get_class == nullptr) return;
    Il2CppClass *service_class = api.object_get_class(service);
    if (service_class == nullptr) return;
    binding->award_item_bubble_service = service;
    binding->award_bubble_gather_fort_search = find_spin_instance_method_with_param_substring(
        api, service_class, "GatherBubbleData", 2U, 1U, "FortSearchOutProto"
    );
    binding->award_bubble_spawn_bubbles = find_spin_instance_method_with_param_substring(
        api, service_class, "SpawnBubbles", 2U, 1U, "AwardBubbleConfig"
    );
    binding->award_bubble_world_to_canvas = find_runtime_method_with_parameter(
        api, service_class, "WorldToCanvasPosition", "UnityEngine.Vector3"
    );
    binding->award_bubble_add_child = find_runtime_method_with_parameter(
        api, service_class, "AddChild", "UnityEngine.Transform"
    );
    binding->award_bubble_set_visibility = find_runtime_method_with_parameter(
        api, service_class, "SetBubbleVisibility", "System.Boolean"
    );
    binding->award_bubble_gather_candy = find_spin_instance_method_with_param_substring(
        api, service_class, "GatherCandyData", 3U, 1U, "LootProto"
    );
    binding->award_bubble_gather_xl_candy = find_spin_instance_method_with_param_substring(
        api, service_class, "GatherXlCandyData", 2U, 1U, "LootProto"
    );
    if (binding->award_bubble_data_list_class == nullptr) {
        binding->award_bubble_data_list_class = resolve_award_bubble_list_class_from_method(
            api, binding->award_bubble_gather_fort_search
        );
    }
    if (binding->award_bubble_data_list_class == nullptr) {
        binding->award_bubble_data_list_class = resolve_award_bubble_list_class_from_method(
            api, binding->award_bubble_gather_candy
        );
    }
}

void update_spin_item_bubble_capabilities(RuntimeBinding *binding) {
    if (binding == nullptr) return;
    binding->spin_item_bubbles_verified = binding->award_item_bubble_service != nullptr &&
        binding->award_bubble_data_list_class != nullptr &&
        binding->award_bubble_gather_fort_search != nullptr &&
        binding->award_bubble_spawn_bubbles != nullptr &&
        binding->award_bubble_world_to_canvas != nullptr &&
        binding->map_place_get_transform != nullptr &&
        binding->unity_transform_get_position != nullptr;
    binding->transfer_candy_bubbles_verified = binding->award_item_bubble_service != nullptr &&
        binding->award_bubble_data_list_class != nullptr &&
        binding->award_bubble_gather_candy != nullptr &&
        binding->award_bubble_spawn_bubbles != nullptr &&
        binding->award_bubble_world_to_canvas != nullptr &&
        binding->map_avatar_get_position != nullptr &&
        binding->loot_proto_class != nullptr &&
        binding->loot_item_proto_class != nullptr;
}

void persist_runtime_binding_snapshot(ProbeContext &context, const RuntimeBinding &binding) {
    std::lock_guard<std::mutex> lock(context.binding_mutex);
    context.binding = binding;
}

void ensure_award_item_bubble_bindings(
    ProbeContext &context,
    RuntimeBinding *binding
) {
    if (binding == nullptr) return;
    const Il2CppApi &api = context.il2cpp_api;
    if (binding->award_bubble_data_list_class == nullptr && api.domain_get_assemblies != nullptr) {
        Il2CppDomain *domain = api.domain_get == nullptr ? nullptr : api.domain_get();
        size_t assembly_count = 0U;
        const Il2CppAssembly **assemblies = domain == nullptr
            ? nullptr
            : api.domain_get_assemblies(domain, &assembly_count);
        binding->award_bubble_data_list_class = resolve_award_bubble_data_list_class(
            api, assemblies, assembly_count
        );
    }
    if (binding->award_item_bubble_service == nullptr) {
        void *service = find_unity_object_on_main_thread(
            context, "Niantic.Holoholo.UI", "AwardItemBubbleService"
        );
        if (service != nullptr) {
            bind_spin_item_bubble_service_methods(api, binding, service);
        }
    } else {
        bind_spin_item_bubble_service_methods(api, binding, binding->award_item_bubble_service);
    }
    update_spin_item_bubble_capabilities(binding);
    persist_runtime_binding_snapshot(context, *binding);
}

void discover_spin_item_bubble_bindings(
    const Il2CppApi &api,
    const Il2CppAssembly **assemblies,
    size_t assembly_count,
    RuntimeBinding *binding
) {
    if (binding == nullptr || assemblies == nullptr) return;
    binding->award_bubble_data_list_class = resolve_award_bubble_data_list_class(
        api, assemblies, assembly_count
    );
    Il2CppClass *map_place_class = find_runtime_class(
        api, assemblies, assembly_count, "Niantic.Holoholo.Map", "AbstractMapPlace", nullptr
    );
    if (map_place_class != nullptr) {
        binding->map_place_get_transform = find_runtime_instance_method_in_hierarchy(
            api, map_place_class, "get_MapPlaceTransform", 0U
        );
    }
    Il2CppClass *transform_class = find_runtime_class(
        api, assemblies, assembly_count, "UnityEngine", "Transform", nullptr
    );
    if (transform_class != nullptr) {
        binding->unity_transform_get_position = find_runtime_instance_method_in_hierarchy(
            api, transform_class, "get_position", 0U
        );
    }
    Il2CppClass *map_avatar_class = find_runtime_class(
        api, assemblies, assembly_count, "Niantic.Holoholo.Avatar", "MapAvatar", nullptr
    );
    if (map_avatar_class != nullptr) {
        binding->map_avatar_get_position = find_runtime_instance_method_in_hierarchy(
            api, map_avatar_class, "get_Position", 0U
        );
    }
    binding->loot_proto_class = find_runtime_class(
        api, assemblies, assembly_count, "Holoholo.Rpc", "LootProto", nullptr
    );
    binding->loot_item_proto_class = find_runtime_class(
        api, assemblies, assembly_count, "Holoholo.Rpc", "LootItemProto", nullptr
    );
    if (binding->award_item_bubble_service != nullptr) {
        bind_spin_item_bubble_service_methods(api, binding, binding->award_item_bubble_service);
    }
    update_spin_item_bubble_capabilities(binding);
    __android_log_print(
        binding->spin_item_bubbles_verified ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "managed diagnostic: spin item bubble binding verified=%d service=%p list=%p gather=%p "
        "spawn=%p world_to_canvas=%p map_transform=%p transform_pos=%p",
        binding->spin_item_bubbles_verified ? 1 : 0,
        binding->award_item_bubble_service,
        binding->award_bubble_data_list_class,
        binding->award_bubble_gather_fort_search,
        binding->award_bubble_spawn_bubbles,
        binding->award_bubble_world_to_canvas,
        binding->map_place_get_transform,
        binding->unity_transform_get_position
    );
    __android_log_print(
        binding->transfer_candy_bubbles_verified ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "managed diagnostic: transfer candy bubble binding verified=%d gather_candy=%p "
        "map_avatar_pos=%p loot=%p loot_item=%p",
        binding->transfer_candy_bubbles_verified ? 1 : 0,
        binding->award_bubble_gather_candy,
        binding->map_avatar_get_position,
        binding->loot_proto_class,
        binding->loot_item_proto_class
    );
}
