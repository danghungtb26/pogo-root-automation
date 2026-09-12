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

void update_direct_map_fort_capabilities(RuntimeBinding *binding) {
    if (binding == nullptr) return;
    binding->forts_read_verified = binding->map_place_directory_service != nullptr &&
        binding->map_place_directory_pokestops != nullptr &&
        binding->map_pokestop_get_id != nullptr &&
        binding->map_pokestop_get_location != nullptr &&
        binding->map_pokestop_get_is_cooling_down != nullptr;
    binding->spin_verified = binding->forts_read_verified &&
        binding->map_pokestop_start_interactive != nullptr &&
        binding->map_pokestop_interactive_item_spinner != nullptr &&
        binding->poi_item_spinner_request_items != nullptr;
}

void bind_direct_map_place_directory(
    const Il2CppApi &api,
    RuntimeBinding *binding
) {
    if (binding == nullptr || !has_runtime_inspection_api(api)) return;

    void *directory = nullptr;
    const char *directory_source = nullptr;
    const auto try_directory_field = [&api, &directory, &directory_source](
        void *owner,
        const char *field_name,
        const char *source
    ) {
        if (directory != nullptr || owner == nullptr) return;
        void *candidate = nullptr;
        const bool field_read = read_runtime_field(api, owner, field_name, &candidate);
        const bool exact = field_read && runtime_object_is_exact_class(
            api, candidate, "Niantic.Holoholo.Map", "MapPlaceDirectoryService"
        );
        __android_log_print(
            exact ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
            kLogTag,
            "managed diagnostic: map directory candidate source=%s owner=%p field=%s read=%d object=%p exact=%d",
            source,
            owner,
            field_name,
            field_read ? 1 : 0,
            candidate,
            exact ? 1 : 0
        );
        if (exact) {
            directory = candidate;
            directory_source = source;
        }
    };

    // Keep the existing MapEntityService owner as the first choice. Scene
    // discovery can run later, so also resolve the same injected directory
    // from the already verified MapSceneViewService/MapContentHandler owners.
    try_directory_field(
        binding->map_entity_service,
        "egiq",
        "MapEntityService.egiq"
    );
    try_directory_field(
        binding->map_scene_view_service,
        "ehjt",
        "MapSceneViewService.ehjt"
    );
    try_directory_field(
        binding->map_content_handler,
        "egga",
        "MapContentHandler.egga"
    );

    if (directory != nullptr) {
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
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "managed diagnostic: map directory bound source=%s directory=%p pokestops=%p",
            directory_source == nullptr ? "unknown" : directory_source,
            binding->map_place_directory_service,
            binding->map_place_directory_pokestops
        );
    }
    update_direct_map_fort_capabilities(binding);
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
    binding->soft_sfida_capture_out_proto_class = find_runtime_class(
        api,
        assemblies,
        assembly_count,
        "Holoholo.Rpc",
        "SoftSfidaCaptureOutProto",
        nullptr
    );
    binding->map_pokemon_get_spawn_point_id = find_direct_map_instance_method(
        api, wild_map_pokemon_class, "get_SpawnPointId", 0U
    );
    binding->direct_catch_verified = binding->map_pokemon_try_capture != nullptr &&
        binding->map_pokemon_get_spawn_point_id != nullptr;
    // Background encounter: SendEncounterRequest (RPC Encounter=102) on WildMapPokemon.
    // Overrides MapPokemon.SendEncounterRequest (Slot 41), zero managed parameters.
    binding->map_pokemon_send_encounter_request = find_direct_map_instance_method(
        api, wild_map_pokemon_class, "SendEncounterRequest", 0U
    );
    binding->background_encounter_verified = binding->direct_catch_verified &&
        binding->map_pokemon_send_encounter_request != nullptr;
    // Map sync after a terminal outcome. Resolved from the live MapContentHandler
    // owner (bound during scene/nearby discovery) so the exact instantiated class is used.
    if (binding->map_content_handler != nullptr && api.object_get_class != nullptr) {
        Il2CppClass *content_handler_class = api.object_get_class(binding->map_content_handler);
        binding->map_content_handler_register_caught_or_fled =
            find_direct_map_instance_method(
                api, content_handler_class, "RegisterPokemonCaughtOrFled", 1U
            );
    }
    // The current binding has no verified way to observe the returned
    // IPromise/CatchPokemon result. Keep invocation evidence separate from the
    // capability advertised to the controller; disappearance from the map is
    // not an authoritative caught/fled result.
    binding->direct_catch_outcome_verified = false;
    binding->soft_sfida_outcome_observer_verified = false;

    bind_direct_map_place_directory(api, binding);

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
    // In 0.427.0 Spin(float) only starts the physics coroutine. ckko() is the
    // private zero-argument method that constructs FortSearchOutProto and sends
    // RPC method 101 through IRpcHandler. ckkq(FortSearchOutProto) is the
    // result continuation used by the game to update spinner state.
    binding->poi_item_spinner_request_items = find_direct_map_instance_method(
        api, spinner_class, "ckko", 0U
    );
    binding->poi_item_spinner_result_callback = find_direct_map_instance_method_with_parameter(
        api, spinner_class, "ckkq", "Holoholo.Rpc.FortSearchOutProto"
    );

    update_direct_map_fort_capabilities(binding);

    __android_log_print(
        binding->direct_catch_verified ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "managed diagnostic: direct map catch binding verified=%d method=%p",
        binding->direct_catch_verified ? 1 : 0,
        binding->map_pokemon_try_capture
    );
    __android_log_print(
        binding->direct_catch_outcome_verified ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "managed diagnostic: direct map catch outcome observer verified=%d",
        binding->direct_catch_outcome_verified ? 1 : 0
    );
    __android_log_print(
        binding->background_encounter_verified ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "managed diagnostic: background encounter binding verified=%d send_encounter=%p register_caught_or_fled=%p",
        binding->background_encounter_verified ? 1 : 0,
        binding->map_pokemon_send_encounter_request,
        binding->map_content_handler_register_caught_or_fled
    );
    __android_log_print(
        binding->soft_sfida_capture_verified ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "managed diagnostic: SoftSfidaCaptureRpc binding verified=%d handler=%p method=%p result_proto=%p",
        binding->soft_sfida_capture_verified ? 1 : 0,
        binding->soft_sfida_rpc_handler,
        binding->soft_sfida_capture_rpc,
        binding->soft_sfida_capture_out_proto_class
    );
    __android_log_print(
        binding->soft_sfida_outcome_observer_verified ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "managed diagnostic: SoftSfida outcome observer verified=%d",
        binding->soft_sfida_outcome_observer_verified ? 1 : 0
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
        "managed diagnostic: direct spin binding verified=%d get_pokestop=%p start=%p spinner_field=%p spin=%p request_items=%p result_callback=%p",
        binding->spin_verified ? 1 : 0,
        binding->map_place_directory_get_pokestop,
        binding->map_pokestop_start_interactive,
        binding->map_pokestop_interactive_item_spinner,
        binding->poi_item_spinner_spin,
        binding->poi_item_spinner_request_items,
        binding->poi_item_spinner_result_callback
    );
}
