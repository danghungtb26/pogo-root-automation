bool run_managed_runtime_diagnostic(ProbeContext &context) {
    if (!context.il2cpp_api_ready) {
        __android_log_print(ANDROID_LOG_WARN, kLogTag, "managed diagnostic: IL2CPP API unavailable");
        return false;
    }

    const Il2CppApi &api = context.il2cpp_api;
    if (!has_runtime_inspection_api(api)) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "managed diagnostic: inspection API incomplete resolved=%u",
            api.symbol_count
        );
        return false;
    }

    // This function is intentionally reachable only through an explicit
    // controller command after the game has reached a stable UI state. It is
    // not called by the process-start probe, because managed APIs are unsafe
    // while Unity is still inside il2cpp_init.
    Il2CppDomain *domain = api.domain_get();
    if (domain == nullptr) {
        __android_log_print(ANDROID_LOG_WARN, kLogTag, "managed diagnostic: domain unavailable");
        return false;
    }
    Il2CppThread *thread = api.thread_attach(domain);
    if (thread == nullptr) {
        __android_log_print(ANDROID_LOG_WARN, kLogTag, "managed diagnostic: thread attach failed");
        return false;
    }

    size_t assembly_count = 0U;
    const Il2CppAssembly **assemblies = api.domain_get_assemblies(domain, &assembly_count);
    const Il2CppImage *csharp_image = nullptr;
    if (assemblies != nullptr) {
        const size_t inspect_count = assembly_count < kMaxAssembliesToInspect
            ? assembly_count
            : kMaxAssembliesToInspect;
        for (size_t index = 0U; index < inspect_count; ++index) {
            const Il2CppAssembly *assembly = assemblies[index];
            if (assembly == nullptr) continue;
            const Il2CppImage *image = api.assembly_get_image(assembly);
            if (image == nullptr) continue;
            const char *image_name = api.image_get_name(image);
            if (image_name == nullptr) continue;
            if (index < 32U || contains_ignore_case(image_name, "csharp") ||
                contains_ignore_case(image_name, "holoholo") ||
                contains_ignore_case(image_name, "niantic")) {
                __android_log_print(
                    ANDROID_LOG_INFO,
                    kLogTag,
                    "managed diagnostic: image index=%zu name=%s",
                    index,
                    image_name
                );
            }
            if (strcmp(image_name, "Assembly-CSharp.dll") == 0 ||
                strcmp(image_name, "Assembly-CSharp") == 0) {
                csharp_image = image;
                break;
            }
        }
    }
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "managed diagnostic: domain ready assemblies=%zu csharp=%s",
        assembly_count,
        csharp_image == nullptr ? "missing" : "found"
    );
    static const char *const item_methods[] = {
        "GetTotalItems",
        "GetItemCount",
        "UseItemOnPokemon",
        "UseItemsOnPokemon",
        "UseItemForEncounter",
    }; static const char *const pokemon_methods[] = {
        "get_Count",
        "get_AllPokemon",
        "GetPokemon",
        "ReleasePokemon",
        "FavoritePokemon",
    }; static const char *const location_methods[] = {
        "get_Location",
        "get_HasValidLocation",
        "Start",
        "Update",
    }; static const char *const snapshot_methods[] = {
        "Initialize",
        "Show",
        "Hide",
        "ClickSave",
        "ClickDiscard",
    }; static const char *const encounter_pokemon_methods[] = {
        "get_EncounterId",
        "get_IsEncounter",
        "get_Pokemon",
        "get_PokemonProto",
        "get_MapPokemon",
        "Initialize",
    }; static const char *const map_pokemon_methods[] = {
        "get_EncounterId",
        "get_Pokemon",
        "get_PokemonProto",
        "get_WildPokemon",
        "get_MapPokemonProto",
        "get_Latitude",
        "get_Longitude",
        "get_Position",
        "get_Transform",
        "Initialize",
    };
    static const char *const wild_map_pokemon_methods[] = {
        "get_EncounterId",
        "get_Pokemon",
        "get_PokemonProto",
        "get_WildPokemon",
        "get_MapPokemonProto",
        "get_Latitude",
        "get_Longitude",
        "Initialize",
    };
    static const char *const encounter_state_methods[] = {
        "get_EncounterId",
        "get_CurrentPokemon",
        "get_Pokemon",
        "get_State",
        "IsEncounterActive",
    };
    static const char *const snapshot_state_methods[] = {
        "get_EncounterId",
        "get_Pokemon",
        "get_Snapshot",
        "TakeSnapshot",
    };
    static const char *const nearby_service_methods[] = {
        "get_NearbyPokemon",
        "get_CurrentPokemon",
        "GetNearbyPokemon",
        "Refresh",
    };
    static const char *const encounter_proto_methods[] = {
        "ToByteArray",
        "WriteTo",
        "CalculateSize",
        "MergeFrom",
        "get_Pokemon",
        "get_EncounterId",
        "get_Latitude",
        "get_Longitude",
    };
    static const char *const wild_pokemon_proto_methods[] = {
        "ToByteArray",
        "WriteTo",
        "CalculateSize",
        "MergeFrom",
        "get_Pokemon",
        "get_EncounterId",
        "get_Latitude",
        "get_Longitude",
    };
    static const char *const pokemon_proto_methods[] = {
        "ToByteArray",
        "WriteTo",
        "CalculateSize",
        "MergeFrom",
        "get_PokemonId",
        "get_PokemonDisplay",
        "get_IndividualAttack",
        "get_IndividualDefense",
        "get_IndividualStamina",
    };
    static const char *const pokemon_display_proto_methods[] = {
        "ToByteArray",
        "WriteTo",
        "CalculateSize",
        "MergeFrom",
        "get_Shiny",
        "get_Shadow",
        "get_Costume",
    };
    static const char *const message_extensions_methods[] = {
        "ToByteArray",
        "WriteTo",
        "CalculateSize",
    };
    static const char *const unity_object_methods[] = {
        "FindObjectOfType",
        "FindObjectsOfType",
    };
    static const char *const project_context_methods[] = {
        "get_Instance",
        "get_Container",
    };
    static const char *const scene_context_methods[] = {
        "get_Container",
    };
    static const char *const di_container_methods[] = {
        "Resolve",
        "TryResolve",
    };
    static const char *const binding_id_methods[] = {
        ".ctor",
        "get_ContractType",
        "get_Identifier",
        "ToString",
    };
    static const char *const pokeball_throw_methods[] = {
        "Initialize",
        "Throw",
        "ThrowToPosition",
        "SpinThrow",
        "MissedThrow",
        "Start",
        "OnEnable",
        "Update",
    };
    static const char *const pokeball_methods[] = {
        "Initialize",
        "Throw",
        "ThrowToPosition",
        "SpinThrow",
        "MissedThrow",
        "Start",
        "Update",
    };
    static const char *const reticle_methods[] = {
        "Initialize",
        "Show",
        "Hide",
        "SetSize",
        "SetRadius",
        "Update",
        "get_Radius",
        "get_Size",
    };
    static const char *const encounter_gui_methods[] = {
        "Initialize",
        "Show",
        "Hide",
        "ClickRunAway",
        "NotifyPokeballThrown",
        "Start",
        "Update",
    };
    static const char *const encounter_state_action_methods[] = {
        "Initialize",
        "RunAway",
        "EncounterStateComplete",
        "NotifyPokeballThrown",
        "Start",
        "Update",
        "get_State",
        "get_EncounterId",
    };
    static const char *const encounter_gesture_methods[] = {
        "Initialize",
        "ThrowBall",
        "ThrowToPosition",
        "OnTouch",
        "OnPointerDown",
        "OnPointerUp",
        "Start",
        "Update",
    };
    static const char *const pokeball_service_methods[] = {
        "Initialize",
        "Throw",
        "ThrowBall",
        "GetPokeball",
        "GetCurrentPokeball",
        "Start",
        "Update",
    };
    static const char *const map_gesture_methods[] = {
        "Initialize",
        "OnTouch",
        "OnPointerDown",
        "OnPointerUp",
        "Select",
        "Click",
        "Start",
        "Update",
    };
    static const char *const poi_spinner_methods[] = {
        "Initialize",
        "Spin",
        "StartSpin",
        "OnSpin",
        "Interact",
        "Click",
        "Start",
        "Update",
    };
    static const char *const map_pokestop_methods[] = {
        "Initialize",
        "Open",
        "Close",
        "Interact",
        "Spin",
        "Click",
        "Start",
        "Update",
    };
    static const ManagedDiagnosticClass candidates[] = {
        {"Niantic.Holoholo.Internal", "ItemBagImpl", item_methods, sizeof(item_methods) / sizeof(item_methods[0])},
        {"Niantic.Holoholo.Internal", "PokemonBagImpl", pokemon_methods, sizeof(pokemon_methods) / sizeof(pokemon_methods[0])},
        {"Niantic.Holoholo", "LocationProviderAdapter", location_methods, sizeof(location_methods) / sizeof(location_methods[0])},
        {"Niantic.Holoholo.Snapshot", "SnapshotPreview", snapshot_methods, sizeof(snapshot_methods) / sizeof(snapshot_methods[0])},
        {"Niantic.Holoholo.Encounter", "EncounterPokemon", encounter_pokemon_methods, sizeof(encounter_pokemon_methods) / sizeof(encounter_pokemon_methods[0])},
        {"Niantic.Holoholo.Map", "MapPokemon", map_pokemon_methods, sizeof(map_pokemon_methods) / sizeof(map_pokemon_methods[0])},
        {"Niantic.Holoholo.Map", "WildMapPokemon", wild_map_pokemon_methods, sizeof(wild_map_pokemon_methods) / sizeof(wild_map_pokemon_methods[0])},
        {"Niantic.Holoholo.Map", "DailyEncounterMapPokemon", map_pokemon_methods, sizeof(map_pokemon_methods) / sizeof(map_pokemon_methods[0])},
        {"Niantic.Holoholo.Map", "PokemonEncounterResponse", encounter_proto_methods, sizeof(encounter_proto_methods) / sizeof(encounter_proto_methods[0])},
        {"Niantic.Holoholo.Encounter", "EncounterInteractionState", encounter_state_methods, sizeof(encounter_state_methods) / sizeof(encounter_state_methods[0])},
        {"Niantic.Holoholo.Encounter", "EncounterSnapshotGuiController", snapshot_state_methods, sizeof(snapshot_state_methods) / sizeof(snapshot_state_methods[0])},
        {"Niantic.Holoholo.NearbyPokemon", "NearbyPokemonService", nearby_service_methods, sizeof(nearby_service_methods) / sizeof(nearby_service_methods[0])},
        {"Holoholo.Rpc", "EncounterOutProto", encounter_proto_methods, sizeof(encounter_proto_methods) / sizeof(encounter_proto_methods[0])},
        {"Holoholo.Rpc", "WildPokemonProto", wild_pokemon_proto_methods, sizeof(wild_pokemon_proto_methods) / sizeof(wild_pokemon_proto_methods[0])},
        {"Holoholo.Rpc", "PokemonProto", pokemon_proto_methods, sizeof(pokemon_proto_methods) / sizeof(pokemon_proto_methods[0])},
        {"Holoholo.Rpc", "PokemonDisplayProto", pokemon_display_proto_methods, sizeof(pokemon_display_proto_methods) / sizeof(pokemon_display_proto_methods[0])},
        {"Google.Protobuf", "MessageExtensions", message_extensions_methods, sizeof(message_extensions_methods) / sizeof(message_extensions_methods[0])},
        {"UnityEngine", "Object", unity_object_methods, sizeof(unity_object_methods) / sizeof(unity_object_methods[0])},
        {"Zenject", "ProjectContext", project_context_methods, sizeof(project_context_methods) / sizeof(project_context_methods[0])},
        {"Zenject", "SceneContext", scene_context_methods, sizeof(scene_context_methods) / sizeof(scene_context_methods[0])},
        {"Zenject", "DiContainer", di_container_methods, sizeof(di_container_methods) / sizeof(di_container_methods[0])},
        {"Zenject", "BindingId", binding_id_methods, sizeof(binding_id_methods) / sizeof(binding_id_methods[0])},
        {"Niantic.Holoholo.Encounter", "PokeballThrow", pokeball_throw_methods, sizeof(pokeball_throw_methods) / sizeof(pokeball_throw_methods[0])},
        {"Niantic.Holoholo.Encounter", "Pokeball", pokeball_methods, sizeof(pokeball_methods) / sizeof(pokeball_methods[0])},
        {"Niantic.Holoholo.Encounter", "Reticle", reticle_methods, sizeof(reticle_methods) / sizeof(reticle_methods[0])},
        {"Niantic.Holoholo.Encounter", "EncounterGuiController", encounter_gui_methods, sizeof(encounter_gui_methods) / sizeof(encounter_gui_methods[0])},
        {"Niantic.Holoholo.Encounter", "EncounterState", encounter_state_action_methods, sizeof(encounter_state_action_methods) / sizeof(encounter_state_action_methods[0])},
        {"Niantic.Holoholo.Encounter", "EncounterGestureHandler", encounter_gesture_methods, sizeof(encounter_gesture_methods) / sizeof(encounter_gesture_methods[0])},
        {"Niantic.Holoholo.Encounter.Gestures", "EncounterGestureHandler", encounter_gesture_methods, sizeof(encounter_gesture_methods) / sizeof(encounter_gesture_methods[0])},
        {"Niantic.Holoholo.Services", "PokeballService", pokeball_service_methods, sizeof(pokeball_service_methods) / sizeof(pokeball_service_methods[0])},
        {"Niantic.Holoholo.Encounter", "Pokeball", pokeball_methods, sizeof(pokeball_methods) / sizeof(pokeball_methods[0])},
        {"Niantic.Holoholo.Map", "MapGestureHandler", map_gesture_methods, sizeof(map_gesture_methods) / sizeof(map_gesture_methods[0])},
        {"Niantic.Holoholo.Map", "BasePoiSpinner", poi_spinner_methods, sizeof(poi_spinner_methods) / sizeof(poi_spinner_methods[0])},
        {"Niantic.Holoholo.Map", "PoiItemSpinner", poi_spinner_methods, sizeof(poi_spinner_methods) / sizeof(poi_spinner_methods[0])},
        {"Niantic.Holoholo.Pokestop", "MapPokestop", map_pokestop_methods, sizeof(map_pokestop_methods) / sizeof(map_pokestop_methods[0])},
        {"Niantic.Holoholo.Pokestop", "MapPokestopInteractive", map_pokestop_methods, sizeof(map_pokestop_methods) / sizeof(map_pokestop_methods[0])},
        {"Niantic.Holoholo.Pokestop", "MapPokestopInteractiveTouchGuiController", map_pokestop_methods, sizeof(map_pokestop_methods) / sizeof(map_pokestop_methods[0])},
    };

    // Some Unity exports in this build do not use the conventional
    // Assembly-CSharp image name. Resolve the image by exact candidate class
    // identity before giving up; this remains read-only and bounded.
    if (csharp_image == nullptr && assemblies != nullptr && api.class_from_name != nullptr) {
        for (size_t image_index = 0U; image_index < assembly_count && csharp_image == nullptr; ++image_index) {
            const Il2CppAssembly *assembly = assemblies[image_index];
            if (assembly == nullptr) continue;
            const Il2CppImage *image = api.assembly_get_image(assembly);
            if (image == nullptr) continue;
            for (const ManagedDiagnosticClass &candidate : candidates) {
                Il2CppClass *klass = api.class_from_name(image, candidate.name_space, candidate.name);
                if (klass == nullptr) continue;
                csharp_image = image;
                const char *image_name = api.image_get_name(image);
                __android_log_print(
                    ANDROID_LOG_INFO,
                    kLogTag,
                    "managed diagnostic: candidate image found class=%s.%s image=%s",
                    candidate.name_space,
                    candidate.name,
                    image_name == nullptr ? "<unknown>" : image_name
                );
                break;
            }
        }
    }

    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "managed diagnostic: candidate image selected=%s",
        csharp_image == nullptr ? "missing" : "found"
    );

    scan_runtime_static_owner_fields(api, assemblies, assembly_count);
    scan_runtime_encounter_classes(api, assemblies, assembly_count);
    {
        std::lock_guard<std::mutex> binding_lock(context.binding_mutex);
        discover_runtime_owners(api, assemblies, assembly_count, context.il2cpp_path, &context.binding);
    }
    discover_scene_runtime_owners(context);
    diagnose_map_pokemon_instance(context);
    diagnose_tap_gesture_class(api, assemblies, assembly_count);
    const RuntimeBinding diagnostic_binding = runtime_binding_snapshot(context);
    log_runtime_field_object(
        api,
        diagnostic_binding.encounter_pokemon,
        "<MapPokemon>k__BackingField",
        "EncounterPokemon.MapPokemon",
        "Niantic.Holoholo.Map",
        "WildMapPokemon"
    );
    log_runtime_field_object(
        api,
        diagnostic_binding.encounter_state,
        "<MapPokemon>k__BackingField",
        "EncounterState.MapPokemon",
        "Niantic.Holoholo.Map",
        "MapPokemon"
    );
    log_runtime_field_object(
        api,
        diagnostic_binding.encounter_state,
        "<EncounterResponse>k__BackingField",
        "EncounterState.EncounterResponse"
    );
    log_runtime_field_object(
        api,
        diagnostic_binding.encounter_state,
        "<EncounterDefaultBall>k__BackingField",
        "EncounterState.EncounterDefaultBall",
        "Holoholo.Rpc",
        "Item"
    );
    log_runtime_field_object(
        api,
        diagnostic_binding.encounter_interaction,
        "<EncounterPokemon>k__BackingField",
        "EncounterInteractionState.EncounterPokemon",
        "Niantic.Holoholo.Encounter",
        "EncounterPokemon"
    );
    log_runtime_field_object(
        api,
        diagnostic_binding.encounter_interaction,
        "<Pokeball>k__BackingField",
        "EncounterInteractionState.Pokeball"
    );
    log_runtime_field_object(
        api,
        diagnostic_binding.encounter_interaction,
        "<EncounterUI>k__BackingField",
        "EncounterInteractionState.EncounterUI"
    );
    log_runtime_field_object(
        api,
        diagnostic_binding.encounter_interaction,
        "<CurrentItem>k__BackingField",
        "EncounterInteractionState.CurrentItem",
        "Holoholo.Rpc",
        "Item"
    );
    log_runtime_field_object(
        api,
        diagnostic_binding.encounter_interaction,
        "dorm",
        "EncounterInteractionState.GestureHandler",
        "Niantic.Holoholo.Encounter.Gestures",
        "EncounterGestureHandler"
    );

    diagnose_managed_runtime_candidates(
        api,
        assemblies,
        assembly_count,
        candidates,
        sizeof(candidates) / sizeof(candidates[0])
    );
    diagnose_map_runtime_classes(api, assemblies, assembly_count);
    scan_runtime_map_class_names(api, assemblies, assembly_count);
    api.thread_detach(thread); return csharp_image != nullptr;
}
