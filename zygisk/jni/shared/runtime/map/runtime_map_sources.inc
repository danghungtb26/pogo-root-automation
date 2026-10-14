struct RuntimeMapSourceReadStats {
    size_t visited = 0U;
    size_t accepted = 0U;
    size_t duplicate = 0U;
    size_t unknown = 0U;
    size_t unreadable = 0U;
    size_t cell_failures = 0U;
    size_t accepted_by_source[11] = {};
    bool capped = false;
};

const void *find_runtime_map_getter(
    const Il2CppApi &api,
    Il2CppClass *klass,
    const char *method_name,
    const char *return_type
) {
    const void *method = find_runtime_instance_method_in_hierarchy(
        api, klass, method_name, 0U
    );
    if (method == nullptr || return_type == nullptr ||
        api.method_get_return_type == nullptr) return nullptr;
    bool name_unavailable = false;
    return runtime_method_type_matches(
        api, api.method_get_return_type(method), return_type, &name_unavailable
    ) ? method : nullptr;
}

pogo_runtime::RuntimeNearbySourceType classify_runtime_map_pokemon_source(
    const Il2CppApi &api,
    void *object
) {
    if (!is_probable_managed_pointer(object) || api.object_get_class == nullptr ||
        api.class_get_name == nullptr || api.class_get_namespace == nullptr) {
        return pogo_runtime::RuntimeNearbySourceType::kUnknown;
    }
    Il2CppClass *klass = api.object_get_class(object);
    const char *name = klass == nullptr ? nullptr : api.class_get_name(klass);
    const char *name_space = klass == nullptr ? nullptr : api.class_get_namespace(klass);
    if (name == nullptr || name_space == nullptr) {
        return pogo_runtime::RuntimeNearbySourceType::kUnknown;
    }
    const bool map_namespace = strcmp(name_space, "Niantic.Holoholo.Map") == 0;
    if (map_namespace && strcmp(name, "WildMapPokemon") == 0) {
        return pogo_runtime::RuntimeNearbySourceType::kWild;
    }
    if (map_namespace && strcmp(name, "IncenseMapPokemon") == 0) {
        return pogo_runtime::RuntimeNearbySourceType::kIncense;
    }
    if (map_namespace && strcmp(name, "TroyDiskMapPlacePokemon") == 0) {
        return pogo_runtime::RuntimeNearbySourceType::kTroyDisk;
    }
    if (map_namespace && strcmp(name, "DailyEncounterMapPokemon") == 0) {
        return pogo_runtime::RuntimeNearbySourceType::kDailyEncounter;
    }
    if (map_namespace && strcmp(name, "PhotobombingMapPokemon") == 0) {
        return pogo_runtime::RuntimeNearbySourceType::kPhotobomb;
    }
    if (map_namespace && strcmp(name, "IncidentMapPokemon") == 0) {
        return pogo_runtime::RuntimeNearbySourceType::kIncident;
    }
    if (strcmp(name_space, "Niantic.Holoholo.Pokestop") == 0 &&
        strcmp(name, "PokestopSpawnPokemon") == 0) {
        return pogo_runtime::RuntimeNearbySourceType::kPokestopSpawn;
    }
    if (map_namespace && strcmp(name, "RouteMapPokemon") == 0) {
        return pogo_runtime::RuntimeNearbySourceType::kRoute;
    }
    if (map_namespace && strcmp(name, "RaidMapPokemon") == 0) {
        return pogo_runtime::RuntimeNearbySourceType::kRaid;
    }
    if (map_namespace && strcmp(name, "StationMapPokemon") == 0) {
        return pogo_runtime::RuntimeNearbySourceType::kStation;
    }
    return pogo_runtime::RuntimeNearbySourceType::kUnknown;
}

bool append_runtime_map_pokemon_source(
    const Il2CppApi &api,
    void *map_pokemon,
    std::vector<pogo_runtime::RuntimeNearbySpawnObservation> *spawns,
    RuntimeMapSourceReadStats *stats
) {
    if (spawns == nullptr || stats == nullptr) return false;
    ++stats->visited;
    const auto source = classify_runtime_map_pokemon_source(api, map_pokemon);
    if (!pogo_runtime::runtime_nearby_source_known(source)) {
        ++stats->unknown;
        Il2CppClass *klass = is_probable_managed_pointer(map_pokemon) &&
                api.object_get_class != nullptr
            ? api.object_get_class(map_pokemon)
            : nullptr;
        const char *name = klass == nullptr || api.class_get_name == nullptr
            ? "<unknown>" : api.class_get_name(klass);
        const char *name_space = klass == nullptr || api.class_get_namespace == nullptr
            ? "" : api.class_get_namespace(klass);
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "runtime map source rejected reason=unknown_class class=%s.%s object=%p",
            name_space == nullptr ? "" : name_space,
            name == nullptr ? "<unknown>" : name,
            map_pokemon
        );
        return true;
    }

    pogo_runtime::RuntimeEncounterObservation encounter{};
    if (!read_encounter_from_map_pokemon(api, map_pokemon, &encounter)) {
        ++stats->unreadable;
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "runtime map source rejected reason=common_contract_unreadable source=%s object=%p",
            pogo_runtime::runtime_nearby_source_name(source),
            map_pokemon
        );
        return true;
    }
    const bool duplicate = std::any_of(
        spawns->begin(), spawns->end(),
        [&encounter](const auto &spawn) { return spawn.spawn_id == encounter.encounter_id; }
    );
    if (duplicate) {
        ++stats->duplicate;
        return true;
    }
    pogo_runtime::RuntimeNearbySpawnObservation spawn{
        encounter.encounter_id,
        encounter.species_id,
        encounter.latitude,
        encounter.longitude,
    };
    spawn.source_type = source;
    spawns->push_back(spawn);
    ++stats->accepted;
    ++stats->accepted_by_source[static_cast<uint32_t>(source)];
    return true;
}

bool read_runtime_map_cell_sources(
    const Il2CppApi &api,
    const void *get_all_cell_pokemon,
    void *cell,
    std::vector<pogo_runtime::RuntimeNearbySpawnObservation> *spawns,
    RuntimeMapSourceReadStats *stats
) {
    if (get_all_cell_pokemon == nullptr || cell == nullptr || spawns == nullptr ||
        stats == nullptr || !runtime_object_is_exact_class(
            api, cell, "Niantic.Holoholo.Map", "MapEntityCell")) return false;
    void *collection = invoke_runtime_method(
        api,
        get_all_cell_pokemon,
        cell,
        nullptr,
        "MapEntityCell.get_AllCellWildPokemon"
    );
    if (!is_probable_managed_pointer(collection)) return false;

    void *enumerator = nullptr;
    const void *move_next = nullptr;
    const void *get_current = nullptr;
    if (!get_map_enumerator(
            api,
            collection,
            &enumerator,
            &move_next,
            &get_current,
            "MapEntityCell.AllCellWildPokemon.GetEnumerator"
        )) {
        return false;
    }

    bool terminated = false;
    for (size_t index = 0U; index < 512U; ++index) {
        if (spawns->size() >= 512U) {
            stats->capped = true;
            break;
        }
        void *moved = invoke_runtime_method(
            api, move_next, enumerator, nullptr, "MapEntityCell.AllCellWildPokemon.MoveNext"
        );
        bool has_value = false;
        if (!read_boxed_bool(api, moved, &has_value)) return false;
        if (!has_value) {
            terminated = true;
            break;
        }
        void *map_pokemon = invoke_runtime_method(
            api, get_current, enumerator, nullptr, "MapEntityCell.AllCellWildPokemon.Current"
        );
        if (!append_runtime_map_pokemon_source(api, map_pokemon, spawns, stats)) return false;
    }
    if (!terminated) stats->capped = true;
    return true;
}
