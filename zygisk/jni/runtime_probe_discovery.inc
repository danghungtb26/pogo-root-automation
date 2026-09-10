void log_runtime_object(
    const Il2CppApi &api,
    void *object,
    const char *label,
    const char *expected_class
) {
    if (object == nullptr || api.object_get_class == nullptr || label == nullptr) {
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "managed diagnostic: owner label=%s object=<null> expected=%s",
            label == nullptr ? "<unknown>" : label,
            expected_class == nullptr ? "<unknown>" : expected_class
        );
        return;
    }
    Il2CppClass *klass = api.object_get_class(object);
    const char *class_name = klass == nullptr || api.class_get_name == nullptr
        ? "<unknown>"
        : api.class_get_name(klass);
    const char *name_space = klass == nullptr || api.class_get_namespace == nullptr
        ? ""
        : api.class_get_namespace(klass);
    const bool expected = expected_class != nullptr && class_name != nullptr &&
        strcmp(class_name, expected_class) == 0;
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "managed diagnostic: owner label=%s object=%p class=%s.%s expected=%s exact=%d",
        label,
        object,
        name_space == nullptr ? "" : name_space,
        class_name == nullptr ? "<unknown>" : class_name,
        expected_class == nullptr ? "<unknown>" : expected_class,
        expected ? 1 : 0
    );
}

void log_runtime_field_object(
    const Il2CppApi &api,
    void *owner,
    const char *field_name,
    const char *label,
    const char *expected_namespace = nullptr,
    const char *expected_class = nullptr
) {
    void *value = nullptr;
    bool read = false;
    if (owner != nullptr && field_name != nullptr && api.object_get_class != nullptr &&
        api.class_get_field_from_name != nullptr && api.field_get_value != nullptr) {
        Il2CppClass *owner_class = api.object_get_class(owner);
        const void *field = owner_class == nullptr
            ? nullptr
            : api.class_get_field_from_name(owner_class, field_name);
        if (field != nullptr) {
            api.field_get_value(owner, const_cast<void *>(field), &value);
            read = true;
        }
    }
    __android_log_print(
        read ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "managed diagnostic: field object label=%s field=%s read=%d value=%p",
        label == nullptr ? "<unknown>" : label,
        field_name == nullptr ? "<unknown>" : field_name,
        read ? 1 : 0,
        value
    );
    if (expected_namespace != nullptr && expected_class != nullptr) {
        log_runtime_object(api, value, label, expected_class);
    } else {
        log_runtime_object(api, value, label, nullptr);
    }
}

void *invoke_runtime_method(
    const Il2CppApi &api,
    const void *method,
    void *object,
    void **arguments,
    const char *label
) {
    if (api.runtime_invoke == nullptr || method == nullptr) return nullptr;
    void *exception = nullptr;
    void *result = api.runtime_invoke(method, object, arguments, &exception);
    if (exception != nullptr) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "managed diagnostic: invoke exception label=%s exception=%p",
            label == nullptr ? "<unknown>" : label,
            exception
        );
        return nullptr;
    }
    return result;
}

bool bind_map_entity_service(
    const Il2CppApi &api,
    void *map_service,
    RuntimeBinding *binding,
    const char *label
) {
    if (binding == nullptr || !runtime_object_is_exact_class(
            api, map_service, "Niantic.Holoholo.Map", "MapEntityService")) return false;
    Il2CppClass *map_class = api.object_get_class(map_service);
    const void *get_cells = map_class == nullptr || api.class_get_method_from_name == nullptr
        ? nullptr
        : api.class_get_method_from_name(map_class, "get_Cells", 0);
    const bool method_verified = get_cells != nullptr && api.method_get_param_count != nullptr &&
        api.method_get_param_count(get_cells) == 0U && api.method_is_instance != nullptr &&
        api.method_is_instance(get_cells);
    void *cells = nullptr;
    const bool field_read = read_runtime_field(api, map_service, "egix", &cells);
    binding->map_entity_service = map_service;
    binding->map_entity_cells = cells;
    binding->map_entity_get_cells = method_verified ? get_cells : nullptr;
    binding->map_entity_read_verified = method_verified && field_read;
    void *dynamic_service = nullptr;
    const bool dynamic_field_read = read_runtime_field(api, map_service, "egiv", &dynamic_service);
    log_runtime_object(
        api,
        dynamic_service,
        "MapEntityService.DynamicTappablesService",
        "DynamicTappablesService"
    );
    if (field_read && dynamic_field_read &&
        runtime_object_is_exact_class(
            api,
            dynamic_service,
            "Niantic.Holoholo.Services.DynamicTappables",
            "DynamicTappablesService"
        )) {
        binding->dynamic_tappables_service = dynamic_service;
        Il2CppClass *dynamic_class = api.object_get_class(dynamic_service);
        binding->dynamic_tappables_tap = find_runtime_method_with_parameter(
            api,
            dynamic_class,
            "TapTappable",
            "Niantic.Holoholo.Services.DynamicTappables.IDynamicTappable"
        );
        __android_log_print(
            binding->dynamic_tappables_tap == nullptr ? ANDROID_LOG_WARN : ANDROID_LOG_INFO,
            kLogTag,
            "managed diagnostic: MapEntityService DynamicTappables binding verified=%d service=%p method=%p",
            binding->dynamic_tappables_tap == nullptr ? 0 : 1,
            dynamic_service,
            binding->dynamic_tappables_tap
        );
    }
    log_runtime_object(api, map_service, label, "MapEntityService");
    log_runtime_object(api, cells, "MapEntityService.Cells", nullptr);
    log_runtime_method_signature(api, map_class, "MapEntityService", "get_Cells");
    __android_log_print(
        binding->map_entity_read_verified ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "managed diagnostic: map entity field binding verified=%d cells=%p dynamic=%p dynamic_read=%d tap=%p",
        binding->map_entity_read_verified ? 1 : 0,
        cells,
        dynamic_service,
        dynamic_field_read ? 1 : 0,
        binding->dynamic_tappables_tap
    );
    return binding->map_entity_read_verified;
}

void discover_runtime_owners(
    const Il2CppApi &api,
    const Il2CppAssembly **assemblies,
    size_t assembly_count,
    const char *il2cpp_path,
    RuntimeBinding *binding
) {
    if (binding == nullptr) return;
    *binding = RuntimeBinding{};
    binding->exact_build_verified = verify_expected_il2cpp_build_id(il2cpp_path);
    __android_log_print(
        binding->exact_build_verified ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "managed diagnostic: expected libil2cpp build id match=%d",
        binding->exact_build_verified ? 1 : 0
    );
    if (api.runtime_invoke == nullptr || api.class_get_type == nullptr ||
        api.type_get_object == nullptr || assemblies == nullptr) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "managed diagnostic: owner discovery API incomplete"
        );
        return;
    }

    const Il2CppImage *ignored_image = nullptr;
    Il2CppClass *project_class = find_runtime_class(
        api, assemblies, assembly_count, "Zenject", "ProjectContext", &ignored_image
    );
    Il2CppClass *scene_class = find_runtime_class(
        api, assemblies, assembly_count, "Zenject", "SceneContext", &ignored_image
    );
    Il2CppClass *container_class = find_runtime_class(
        api, assemblies, assembly_count, "Zenject", "DiContainer", &ignored_image
    );
    if (project_class == nullptr || container_class == nullptr) {
        __android_log_print(ANDROID_LOG_WARN, kLogTag, "managed diagnostic: DI classes missing");
        return;
    }

    const void *project_instance_method = api.class_get_method_from_name(
        project_class, "get_Instance", 0
    );
    void *project = invoke_runtime_method(
        api, project_instance_method, nullptr, nullptr, "ProjectContext.get_Instance"
    );
    // The probe/diagnostic runs on a native worker thread. Unity object
    // enumeration is not thread-safe, so discovery must use DI owners only.
    if (project == nullptr) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "managed diagnostic: ProjectContext instance unavailable; skipping Unity fallback"
        );
    }
    log_runtime_object(api, project, "ProjectContext", "ProjectContext");

    void *container = nullptr;
    if (project != nullptr) {
        const void *container_method = api.class_get_method_from_name(
            project_class, "get_Container", 0
        );
        container = invoke_runtime_method(
            api, container_method, project, nullptr, "ProjectContext.get_Container"
        );
    }
    void *scene_container = nullptr;
    (void)scene_class;
    // MapSceneContext is not a Zenject.SceneContext instance in this build.
    // Do not call FindObjectOfType to recover it from a worker thread.
    log_runtime_object(api, container, "DiContainer", "DiContainer");
    log_runtime_object(api, scene_container, "SceneDiContainer", "DiContainer");
    if (container == nullptr) container = scene_container;
    if (container == nullptr) return;

    const void *try_resolve = find_runtime_method_with_parameter(
        api, container_class, "TryResolve", "System.Type"
    );
    if (try_resolve == nullptr) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "managed diagnostic: TryResolve(System.Type) missing"
        );
        return;
    }

    struct ServiceCandidate {
        const char *name_space;
        const char *name;
        const char *label;
        const char *expected_class;
    };
    static const ServiceCandidate services[] = {
        {"Niantic.Holoholo", "IItemBag", "IItemBag", "ItemBagImpl"},
        {"Niantic.Holoholo", "IPokemonBag", "IPokemonBag", "PokemonBagImpl"},
        {"Niantic.Holoholo", "ILocationProvider", "ILocationProvider", "LocationProviderAdapter"},
        {"Niantic.Holoholo.Encounter", "IEncounterPokemon", "IEncounterPokemon", "EncounterPokemon"},
        {"Niantic.Holoholo.Encounter", "IEncounterState", "IEncounterState", "EncounterState"},
        {"Niantic.Holoholo.Encounter", "IEncounterInteractionState", "IEncounterInteractionState", "EncounterInteractionState"},
        {"Niantic.Holoholo.Encounter", "IEncounterGuiController", "IEncounterGuiController", "EncounterGuiController"},
        {"Niantic.Holoholo.Encounter", "IEncounterSnapshotGuiController", "IEncounterSnapshotGuiController", "EncounterSnapshotGuiController"},
        {"Niantic.Holoholo.Services", "IPokeballService", "IPokeballService", "PokeballService"},
        {"Niantic.Holoholo.NearbyPokemon", "INearbyPokemonService", "INearbyPokemonService", "NearbyPokemonService"},
        {"Niantic.Holoholo.Map", "IMapSceneViewService", "IMapSceneViewService", "MapSceneViewService"},
        {"Niantic.Holoholo.Services.DynamicTappables", "IDynamicTappablesService", "IDynamicTappablesService", "DynamicTappablesService"},
    };
    for (const ServiceCandidate &service : services) {
        Il2CppClass *service_class = find_runtime_class(
            api,
            assemblies,
            assembly_count,
            service.name_space,
            service.name,
            nullptr
        );
        if (service_class == nullptr) {
            __android_log_print(
                ANDROID_LOG_INFO,
                kLogTag,
                "managed diagnostic: service class missing %s.%s",
                service.name_space,
                service.name
            );
            continue;
        }
        const Il2CppType *service_type = api.class_get_type(service_class);
        void *type_object = service_type == nullptr ? nullptr : api.type_get_object(service_type);
        if (type_object == nullptr) {
            log_runtime_object(api, nullptr, service.label, service.expected_class);
            continue;
        }
        void *arguments[] = {type_object};
        void *owner = invoke_runtime_method(api, try_resolve, container, arguments, service.label);
        if (owner == nullptr && scene_container != nullptr && scene_container != container) {
            owner = invoke_runtime_method(api, try_resolve, scene_container, arguments, service.label);
        }
        log_runtime_object(api, owner, service.label, service.expected_class);
        if (strcmp(service.label, "IItemBag") == 0 && owner != nullptr &&
            api.object_get_class != nullptr && api.class_get_name != nullptr) {
            Il2CppClass *owner_class = api.object_get_class(owner);
            const char *owner_class_name = owner_class == nullptr
                ? nullptr
                : api.class_get_name(owner_class);
            const void *use_item_method = find_runtime_method_with_two_parameters(
                api,
                owner_class,
                "UseItemOnPokemon",
                "Holoholo.Rpc.Item",
                "System.UInt64"
            );
            const bool is_item_bag_impl = owner_class_name != nullptr &&
                strcmp(owner_class_name, "ItemBagImpl") == 0;
            if (is_item_bag_impl && use_item_method != nullptr) {
                binding->item_bag = owner;
                binding->use_item_on_pokemon = use_item_method;
                binding->use_berry_verified = true;
            }
            if (is_item_bag_impl) {
                // Inventory read: GetItemCount(Holoholo.Rpc.Item) -> int. Ensure
                // item_bag is set even when the berry path is unavailable.
                const void *get_item_count = find_runtime_method_with_parameter(
                    api, owner_class, "GetItemCount", "Holoholo.Rpc.Item"
                );
                if (get_item_count != nullptr) {
                    binding->item_bag = owner;
                    binding->item_bag_get_item_count = get_item_count;
                    binding->inventory_read_verified = true;
                }
                __android_log_print(
                    binding->inventory_read_verified ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
                    kLogTag,
                    "managed diagnostic: inventory read verified=%d item_bag=%p get_item_count=%p",
                    binding->inventory_read_verified ? 1 : 0, owner, get_item_count
                );
            }
        }
        if (strcmp(service.label, "IEncounterPokemon") == 0 && owner != nullptr &&
            api.object_get_class != nullptr && api.class_get_name != nullptr &&
            api.class_get_namespace != nullptr) {
            Il2CppClass *owner_class = api.object_get_class(owner);
            const char *owner_name = owner_class == nullptr
                ? nullptr
                : api.class_get_name(owner_class);
            const char *owner_namespace = owner_class == nullptr
                ? nullptr
                : api.class_get_namespace(owner_class);
            if (owner_name != nullptr && owner_namespace != nullptr &&
                strcmp(owner_name, "EncounterPokemon") == 0 &&
                strcmp(owner_namespace, "Niantic.Holoholo.Encounter") == 0) {
                binding->encounter_pokemon = owner;
            }
        }
        if (strcmp(service.label, "IEncounterState") == 0 && owner != nullptr &&
            runtime_object_is_exact_class(api, owner, "Niantic.Holoholo.Encounter", "EncounterState")) {
            binding->encounter_state = owner;
        }
        if (strcmp(service.label, "IEncounterInteractionState") == 0 && owner != nullptr &&
            runtime_object_is_exact_class(
                api,
                owner,
                "Niantic.Holoholo.Encounter",
                "EncounterInteractionState"
            )) {
            binding->encounter_interaction = owner;
        }
        if (strcmp(service.label, "IEncounterGuiController") == 0 && owner != nullptr &&
            runtime_object_is_exact_class(api, owner, "Niantic.Holoholo.Encounter", "EncounterGuiController")) {
            binding->encounter_gui = owner;
        }
        if (strcmp(service.label, "ILocationProvider") == 0 && owner != nullptr &&
            api.object_get_class != nullptr && api.class_get_method_from_name != nullptr) {
            Il2CppClass *owner_class = api.object_get_class(owner);
            const void *get_current = owner_class == nullptr ? nullptr
                : api.class_get_method_from_name(owner_class, "get_CurrentLocation", 0);
            Il2CppClass *location_class = find_runtime_class(
                api, assemblies, assembly_count,
                "Niantic.Platform.Ditto.Geo", "Location", nullptr
            );
            const void *get_lat = location_class == nullptr ? nullptr
                : api.class_get_method_from_name(location_class, "get_LatitudeDeg", 0);
            const void *get_lng = location_class == nullptr ? nullptr
                : api.class_get_method_from_name(location_class, "get_LongitudeDeg", 0);
            if (get_current != nullptr && get_lat != nullptr && get_lng != nullptr) {
                binding->location_provider = owner;
                binding->location_get_current = get_current;
                binding->location_get_latitude = get_lat;
                binding->location_get_longitude = get_lng;
                binding->location_read_verified = true;
            }
            __android_log_print(
                binding->location_read_verified ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
                kLogTag,
                "managed diagnostic: location provider verified=%d owner=%p current=%p lat=%p lng=%p",
                binding->location_read_verified ? 1 : 0,
                owner, get_current, get_lat, get_lng
            );
        }
        if (strcmp(service.label, "IPokeballService") == 0 && owner != nullptr &&
            runtime_object_is_exact_class(api, owner, "Niantic.Holoholo.Services", "PokeballService")) {
            binding->pokeball_service = owner;
            Il2CppClass *owner_class = api.object_get_class(owner);
            binding->pokeball_service_throw = find_runtime_method_with_two_parameters(
                api,
                owner_class,
                "Throw",
                "UnityEngine.GameObject",
                "UnityEngine.Vector3"
            );
            binding->catch_throw_verified = binding->pokeball_service_throw != nullptr;
            __android_log_print(
                binding->catch_throw_verified ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
                kLogTag,
                "managed diagnostic: PokeballService.Throw binding verified=%d owner=%p method=%p",
                binding->catch_throw_verified ? 1 : 0,
                owner,
                binding->pokeball_service_throw
            );
        }
        if (strcmp(service.label, "INearbyPokemonService") == 0 && owner != nullptr &&
            runtime_object_is_exact_class(api, owner, "Niantic.Holoholo.NearbyPokemon", "NearbyPokemonService")) {
            void *map_service = nullptr;
            if (read_runtime_field(api, owner, "ddmr", &map_service)) {
                bind_map_entity_service(api, map_service, binding, "NearbyPokemonService.MapEntityService");
            }
        }
        if (strcmp(service.label, "IMapSceneViewService") == 0 && owner != nullptr &&
            runtime_object_is_exact_class(api, owner, "Niantic.Holoholo.Map", "MapSceneViewService")) {
            binding->map_scene_view_service = owner;
            void *map_content_handler = nullptr;
            if (read_runtime_field(api, owner, "ehkb", &map_content_handler) &&
                runtime_object_is_exact_class(api, map_content_handler, "Niantic.Holoholo.Map", "MapContentHandler")) {
                binding->map_content_handler = map_content_handler;
                void *map_service = nullptr;
                if (read_runtime_field(api, map_content_handler, "egft", &map_service)) {
                    bind_map_entity_service(api, map_service, binding, "MapContentHandler.MapEntityService");
                }
            }
        }
        if (strcmp(service.label, "IDynamicTappablesService") == 0 && owner != nullptr &&
            runtime_object_is_exact_class(
                api,
                owner,
                "Niantic.Holoholo.Services.DynamicTappables",
                "DynamicTappablesService"
            )) {
            binding->dynamic_tappables_service = owner;
            binding->dynamic_tappables_tap = find_runtime_method_with_parameter(
                api,
                api.object_get_class(owner),
                "TapTappable",
                "Niantic.Holoholo.Services.DynamicTappables.IDynamicTappable"
            );
            __android_log_print(
                binding->dynamic_tappables_tap == nullptr ? ANDROID_LOG_WARN : ANDROID_LOG_INFO,
                kLogTag,
                "managed diagnostic: DynamicTappables.TapTappable binding verified=%d service=%p method=%p",
                binding->dynamic_tappables_tap == nullptr ? 0 : 1,
                owner,
                binding->dynamic_tappables_tap
            );
        }
    }

    Il2CppClass *wild_map_class = find_runtime_class(
        api, assemblies, assembly_count, "Niantic.Holoholo.Map", "WildMapPokemon", nullptr
    );
    Il2CppClass *map_pokemon_class = find_runtime_class(
        api, assemblies, assembly_count, "Niantic.Holoholo.Map", "MapPokemon", nullptr
    );
    const void *map_pokemon_on_tapped = map_pokemon_class == nullptr ||
        api.class_get_method_from_name == nullptr
        ? nullptr
        : api.class_get_method_from_name(map_pokemon_class, "OnTapped", 0);
    if (map_pokemon_on_tapped != nullptr && api.method_get_param_count != nullptr &&
        api.method_is_instance != nullptr && api.method_get_param_count(map_pokemon_on_tapped) == 0U &&
        api.method_is_instance(map_pokemon_on_tapped)) {
        binding->map_pokemon_on_tapped = map_pokemon_on_tapped;
    }
    Il2CppClass *unity_object_class = find_runtime_class(
        api, assemblies, assembly_count, "UnityEngine", "Object", nullptr
    );
    if (unity_object_class != nullptr && api.method_is_instance != nullptr) {
        const void *find_objects = find_runtime_method_with_parameter(
            api, unity_object_class, "FindObjectsOfType", "System.Type"
        );
        if (find_objects != nullptr && !api.method_is_instance(find_objects)) {
            binding->unity_find_objects_of_type = find_objects;
        }
    }
    verify_tap_event_contract(api, assemblies, assembly_count, binding);
    Il2CppClass *encounter_proto_class = find_runtime_class(
        api, assemblies, assembly_count, "Holoholo.Rpc", "EncounterOutProto", nullptr
    );
    Il2CppClass *wild_pokemon_proto_class = find_runtime_class(
        api, assemblies, assembly_count, "Holoholo.Rpc", "WildPokemonProto", nullptr
    );
    Il2CppClass *pokemon_proto_class = find_runtime_class(
        api, assemblies, assembly_count, "Holoholo.Rpc", "PokemonProto", nullptr
    );
    Il2CppClass *pokemon_display_class = find_runtime_class(
        api, assemblies, assembly_count, "Holoholo.Rpc", "PokemonDisplayProto", nullptr
    );
    const bool encounter_fields_verified = wild_map_class != nullptr &&
        encounter_proto_class != nullptr && wild_pokemon_proto_class != nullptr &&
        pokemon_proto_class != nullptr && pokemon_display_class != nullptr &&
        api.class_get_field_from_name(wild_map_class, "egws") != nullptr &&
        api.class_get_field_from_name(encounter_proto_class, "Pokemon") != nullptr &&
        api.class_get_field_from_name(wild_pokemon_proto_class, "EncounterId") != nullptr &&
        api.class_get_field_from_name(wild_pokemon_proto_class, "Latitude") != nullptr &&
        api.class_get_field_from_name(wild_pokemon_proto_class, "Longitude") != nullptr &&
        api.class_get_field_from_name(wild_pokemon_proto_class, "Pokemon") != nullptr &&
        api.class_get_field_from_name(pokemon_proto_class, "pokemonId_") != nullptr &&
        api.class_get_field_from_name(pokemon_proto_class, "individualAttack_") != nullptr &&
        api.class_get_field_from_name(pokemon_proto_class, "individualDefense_") != nullptr &&
        api.class_get_field_from_name(pokemon_proto_class, "individualStamina_") != nullptr &&
        api.class_get_field_from_name(pokemon_proto_class, "pokemonDisplay_") != nullptr &&
        api.class_get_field_from_name(pokemon_display_class, "Shiny") != nullptr;
    binding->encounter_read_verified = encounter_fields_verified;
    // Mode B: resolve the map-query batch callback for inline hooking. The
    // 2-parameter overload (int rpcId, byte[] response) fires once per server
    // map response. Only the MethodInfo is captured here; installing the hook
    // (and capturing its prologue from live memory) happens in map_hooks.inc.
    Il2CppClass *map_query_manager_class = find_runtime_class(
        api, assemblies, assembly_count,
        "Niantic.Platform.GameMapObject.Map", "MapQueryManager", nullptr
    );
    if (map_query_manager_class != nullptr && api.class_get_method_from_name != nullptr) {
        binding->map_query_on_response = api.class_get_method_from_name(
            map_query_manager_class, "OnMapQueryResponse", 2
        );
    }
    binding->map_query_hook_binding_verified = binding->map_query_on_response != nullptr;
    __android_log_print(
        binding->map_query_hook_binding_verified ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "managed diagnostic: map query hook binding verified=%d method=%p",
        binding->map_query_hook_binding_verified ? 1 : 0,
        binding->map_query_on_response
    );
    __android_log_print(
        encounter_fields_verified ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "managed diagnostic: encounter field binding verified=%d",
        encounter_fields_verified ? 1 : 0
    );
    __android_log_print(
        binding->map_pokemon_on_tapped != nullptr && binding->unity_find_objects_of_type != nullptr
            ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "managed diagnostic: map pokemon tap binding verified=%d on_tapped=%p find_objects=%p",
        binding->map_pokemon_on_tapped != nullptr && binding->unity_find_objects_of_type != nullptr ? 1 : 0,
        binding->map_pokemon_on_tapped,
        binding->unity_find_objects_of_type
    );
    discover_direct_map_bindings(api, assemblies, assembly_count, binding);
}
