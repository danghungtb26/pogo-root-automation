const void *find_direct_map_instance_method(
    const Il2CppApi &api,
    Il2CppClass *klass,
    const char *name,
    uint32_t parameter_count
) {
    if (!has_runtime_inspection_api(api) || klass == nullptr || name == nullptr) return nullptr;
    const void *method = api.class_get_method_from_name(
        klass, name, static_cast<int>(parameter_count)
    );
    if (method == nullptr || !api.method_is_instance(method) ||
        api.method_get_param_count(method) != parameter_count) return nullptr;
    return method;
}

const void *find_direct_map_instance_method_with_parameter(
    const Il2CppApi &api,
    Il2CppClass *klass,
    const char *name,
    const char *parameter_type_name
) {
    const void *method = find_runtime_method_with_parameter(
        api, klass, name, parameter_type_name
    );
    return method != nullptr && api.method_is_instance != nullptr &&
        api.method_is_instance(method) ? method : nullptr;
}

const void *find_direct_map_instance_method_with_two_parameters(
    const Il2CppApi &api,
    Il2CppClass *klass,
    const char *name,
    const char *first_parameter_type_name,
    const char *second_parameter_type_name
) {
    const void *method = find_runtime_method_with_two_parameters(
        api,
        klass,
        name,
        first_parameter_type_name,
        second_parameter_type_name
    );
    return method != nullptr && api.method_is_instance != nullptr &&
        api.method_is_instance(method) ? method : nullptr;
}

void discover_direct_map_bindings(
    const Il2CppApi &api,
    const Il2CppAssembly **assemblies,
    size_t assembly_count,
    RuntimeBinding *binding
) {
    if (binding == nullptr || assemblies == nullptr || !has_runtime_inspection_api(api)) return;

    Il2CppClass *map_pokemon_class = find_runtime_class(
        api, assemblies, assembly_count, "Niantic.Holoholo.Map", "MapPokemon", nullptr
    );
    binding->map_pokemon_try_capture = find_direct_map_instance_method_with_two_parameters(
        api,
        map_pokemon_class,
        "TryCapture",
        "Niantic.Holoholo.Encounter.PokeballThrow",
        "Holoholo.Rpc.ARPlusEncounterValuesProto"
    );
    binding->ar_plus_encounter_values_class = find_runtime_class(
        api,
        assemblies,
        assembly_count,
        "Holoholo.Rpc",
        "ARPlusEncounterValuesProto",
        nullptr
    );
    binding->catch_pokemon_proto_class = find_runtime_class(
        api,
        assemblies,
        assembly_count,
        "Holoholo.Rpc",
        "CatchPokemonProto",
        nullptr
    );
    Il2CppClass *wild_map_pokemon_class = find_runtime_class(
        api, assemblies, assembly_count, "Niantic.Holoholo.Map", "WildMapPokemon", nullptr
    );
    binding->map_pokemon_get_spawn_point_id = find_direct_map_instance_method(
        api, wild_map_pokemon_class, "get_SpawnPointId", 0U
    );
    binding->direct_catch_verified = binding->map_pokemon_try_capture != nullptr &&
        binding->ar_plus_encounter_values_class != nullptr &&
        binding->catch_pokemon_proto_class != nullptr &&
        binding->map_pokemon_get_spawn_point_id != nullptr;

    void *directory = nullptr;
    if (binding->map_entity_service != nullptr) {
        read_runtime_field(api, binding->map_entity_service, "egiq", &directory);
    }
    if (runtime_object_is_exact_class(
            api, directory, "Niantic.Holoholo.Map", "MapPlaceDirectoryService")) {
        binding->map_place_directory_service = directory;
        read_runtime_field(
            api,
            directory,
            "ehft",
            &binding->map_place_directory_pokestops
        );
        Il2CppClass *directory_class = api.object_get_class(directory);
        binding->map_place_directory_get_active_pokestops = find_direct_map_instance_method(
            api, directory_class, "get_ActivePokestops", 0U
        );
        binding->map_place_directory_get_pokestop = find_direct_map_instance_method_with_parameter(
            api, directory_class, "GetPokestop", "System.String"
        );
    }

    Il2CppClass *pokestop_class = find_runtime_class(
        api, assemblies, assembly_count, "Niantic.Holoholo.Pokestop", "MapPokestop", nullptr
    );
    if (pokestop_class != nullptr) {
        binding->map_pokestop_get_id = find_direct_map_instance_method(
            api, pokestop_class, "get_Id", 0U
        );
        binding->map_pokestop_get_location = find_direct_map_instance_method(
            api, pokestop_class, "get_Location", 0U
        );
        binding->map_pokestop_get_is_cooling_down = find_direct_map_instance_method(
            api, pokestop_class, "get_IsCoolingDown", 0U
        );
        binding->map_pokestop_start_interactive = find_direct_map_instance_method(
            api, pokestop_class, "StartInteractiveMode", 0U
        );
    }

    Il2CppClass *interactive_class = find_runtime_class(
        api,
        assemblies,
        assembly_count,
        "Niantic.Holoholo.Pokestop",
        "MapPokestopInteractive",
        nullptr
    );
    if (interactive_class != nullptr && api.class_get_field_from_name != nullptr) {
        binding->map_pokestop_interactive_item_spinner = api.class_get_field_from_name(
            interactive_class, "ItemSpinner"
        );
    }
    Il2CppClass *spinner_class = find_runtime_class(
        api, assemblies, assembly_count, "Niantic.Holoholo.Map", "PoiItemSpinner", nullptr
    );
    binding->poi_item_spinner_spin = find_direct_map_instance_method_with_parameter(
        api, spinner_class, "Spin", "System.Single"
    );

    binding->forts_read_verified = binding->map_place_directory_service != nullptr &&
        binding->map_place_directory_pokestops != nullptr &&
        binding->map_pokestop_get_id != nullptr &&
        binding->map_pokestop_get_location != nullptr &&
        binding->map_pokestop_get_is_cooling_down != nullptr;
    binding->spin_verified = binding->forts_read_verified &&
        binding->map_pokestop_start_interactive != nullptr &&
        binding->map_pokestop_interactive_item_spinner != nullptr &&
        binding->poi_item_spinner_spin != nullptr;

    __android_log_print(
        binding->direct_catch_verified ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "managed diagnostic: direct map catch binding verified=%d method=%p",
        binding->direct_catch_verified ? 1 : 0,
        binding->map_pokemon_try_capture
    );
    __android_log_print(
        binding->forts_read_verified ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "managed diagnostic: fort reader verified=%d directory=%p pokestops=%p active=%p id=%p location=%p cooldown=%p",
        binding->forts_read_verified ? 1 : 0,
        binding->map_place_directory_service,
        binding->map_place_directory_pokestops,
        binding->map_place_directory_get_active_pokestops,
        binding->map_pokestop_get_id,
        binding->map_pokestop_get_location,
        binding->map_pokestop_get_is_cooling_down
    );
    __android_log_print(
        binding->spin_verified ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "managed diagnostic: direct spin binding verified=%d get_pokestop=%p start=%p spinner_field=%p spin=%p",
        binding->spin_verified ? 1 : 0,
        binding->map_place_directory_get_pokestop,
        binding->map_pokestop_start_interactive,
        binding->map_pokestop_interactive_item_spinner,
        binding->poi_item_spinner_spin
    );
}
