const void *find_map_method(
    const Il2CppApi &api,
    Il2CppClass *klass,
    const char *name,
    uint32_t parameter_count
) {
    if (!has_runtime_inspection_api(api) || klass == nullptr || name == nullptr) return nullptr;
    const void *direct = api.class_get_method_from_name(
        klass, name, static_cast<int>(parameter_count)
    );
    if (direct != nullptr && api.method_is_instance(direct) &&
        api.method_get_param_count(direct) == parameter_count) return direct;
    if (api.class_get_methods == nullptr || api.method_get_name == nullptr) return nullptr;
    void *iterator = nullptr;
    for (uint32_t inspected = 0U; inspected < 512U; ++inspected) {
        const void *method = api.class_get_methods(klass, &iterator);
        if (method == nullptr) break;
        const char *method_name = api.method_get_name(method);
        if (method_name == nullptr || api.method_is_instance(method) == false ||
            api.method_get_param_count(method) != parameter_count) continue;
        const size_t name_length = strlen(method_name);
        const size_t wanted_length = strlen(name);
        const bool exact = strcmp(method_name, name) == 0;
        const bool explicit_interface = name_length > wanted_length + 1U &&
            method_name[name_length - wanted_length - 1U] == '.' &&
            strcmp(method_name + name_length - wanted_length, name) == 0;
        if (exact || explicit_interface) return method;
    }
    return nullptr;
}

bool is_probable_managed_pointer(const void *value) {
    const uintptr_t address = reinterpret_cast<uintptr_t>(value);
    return address >= 0x0000001000000000ULL && address < 0x0000800000000000ULL;
}

bool read_boxed_bool(const Il2CppApi &api, void *boxed, bool *value) {
    if (!is_probable_managed_pointer(boxed) || value == nullptr || api.object_unbox == nullptr) {
        return false;
    }
    void *unboxed = api.object_unbox(boxed);
    if (!is_probable_managed_pointer(unboxed)) return false;
    *value = *static_cast<uint8_t *>(unboxed) != 0U;
    return true;
}

bool read_boxed_u64(const Il2CppApi &api, void *boxed, uint64_t *value) {
    if (!is_probable_managed_pointer(boxed) || value == nullptr || api.object_unbox == nullptr) {
        return false;
    }
    void *unboxed = api.object_unbox(boxed);
    if (!is_probable_managed_pointer(unboxed)) return false;
    memcpy(value, unboxed, sizeof(*value));
    return *value != 0U;
}

bool get_map_enumerator(
    const Il2CppApi &api,
    void *collection,
    void **enumerator,
    const void **move_next,
    const void **get_current,
    const char *label
) {
    if (!is_probable_managed_pointer(collection) || enumerator == nullptr ||
        move_next == nullptr || get_current == nullptr) return false;
    Il2CppClass *collection_class = api.object_get_class(collection);
    const void *get_enumerator = find_map_method(api, collection_class, "GetEnumerator", 0U);
    if (get_enumerator == nullptr) return false;
    *enumerator = invoke_runtime_method(api, get_enumerator, collection, nullptr, label);
    if (!is_probable_managed_pointer(*enumerator)) return false;
    Il2CppClass *enumerator_class = api.object_get_class(*enumerator);
    *move_next = find_map_method(api, enumerator_class, "MoveNext", 0U);
    *get_current = find_map_method(api, enumerator_class, "get_Current", 0U);
    return *move_next != nullptr && *get_current != nullptr && api.object_unbox != nullptr;
}

size_t count_map_collection(const Il2CppApi &api, void *collection) {
    void *enumerator = nullptr;
    const void *move_next = nullptr;
    const void *get_current = nullptr;
    if (!get_map_enumerator(
            api, collection, &enumerator, &move_next, &get_current, "map.GetEnumerator")) {
        return 0U;
    }
    size_t count = 0U;
    for (size_t index = 0U; index < 512U; ++index) {
        void *moved = invoke_runtime_method(api, move_next, enumerator, nullptr, "map.MoveNext");
        bool has_value = false;
        if (!read_boxed_bool(api, moved, &has_value)) return false;
        if (!has_value) break;
        ++count;
    }
    return count;
}

bool enumerate_map_keys(
    const Il2CppApi &api,
    void *dictionary,
    std::vector<uint64_t> *keys
) {
    if (keys == nullptr || !is_probable_managed_pointer(dictionary)) return false;
    keys->clear();
    Il2CppClass *dictionary_class = api.object_get_class(dictionary);
    const void *get_keys = find_map_method(api, dictionary_class, "get_Keys", 0U);
    if (get_keys == nullptr) return false;
    void *key_collection = invoke_runtime_method(
        api, get_keys, dictionary, nullptr, "map.Dictionary.Keys"
    );
    void *enumerator = nullptr;
    const void *move_next = nullptr;
    const void *get_current = nullptr;
    if (!get_map_enumerator(
            api, key_collection, &enumerator, &move_next, &get_current, "map.Keys.GetEnumerator")) {
        return false;
    }
    for (size_t index = 0U; index < 512U; ++index) {
        void *moved = invoke_runtime_method(api, move_next, enumerator, nullptr, "map.Keys.MoveNext");
        bool has_value = false;
        if (!read_boxed_bool(api, moved, &has_value)) return false;
        if (!has_value) break;
        void *boxed_key = invoke_runtime_method(
            api, get_current, enumerator, nullptr, "map.Keys.Current"
        );
        uint64_t key = 0U;
        if (!read_boxed_u64(api, boxed_key, &key)) {
            __android_log_print(
                ANDROID_LOG_WARN,
                kLogTag,
                "runtime map reader rejected dictionary key pointer=%p",
                boxed_key
            );
            return false;
        }
        keys->push_back(key);
    }
    return true;
}

bool read_map_wild_pokemon(
    const Il2CppApi &api,
    void *cell,
    std::vector<pogo_runtime::RuntimeNearbySpawnObservation> *spawns
) {
    if (cell == nullptr || spawns == nullptr || !runtime_object_is_exact_class(
            api, cell, "Niantic.Holoholo.Map", "MapEntityCell")) return false;
    void *wild_dictionary = nullptr;
    if (!read_runtime_field(api, cell, "eghw", &wild_dictionary)) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "runtime map reader cell eghw field unavailable cell=%p",
            cell
        );
        return false;
    }
    if (!is_probable_managed_pointer(wild_dictionary)) return true;
    std::vector<MapDictionaryEntry> wild_entries;
    if (!enumerate_map_dictionary(api, wild_dictionary, &wild_entries)) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "runtime map reader cell wild dictionary rejected cell=%p dictionary=%p",
            cell,
            wild_dictionary
        );
        return false;
    }
    for (const MapDictionaryEntry &wild_entry : wild_entries) {
        void *wild = wild_entry.value;
        if (wild == nullptr) continue;
        const bool wild_exact = runtime_object_is_exact_class(
            api, wild, "Niantic.Holoholo.Map", "WildMapPokemon"
        );
        if (!wild_exact) {
            __android_log_print(
                ANDROID_LOG_WARN,
                kLogTag,
                "runtime map reader rejected wild object key=%llu value=%p",
                static_cast<unsigned long long>(wild_entry.key),
                wild
            );
            continue;
        }
        pogo_runtime::RuntimeEncounterObservation encounter{};
        if (!read_encounter_from_wild_map_pokemon(api, wild, &encounter)) {
            __android_log_print(
                ANDROID_LOG_WARN,
                kLogTag,
                "runtime map reader wild payload unavailable key=%llu value=%p",
                static_cast<unsigned long long>(wild_entry.key),
                wild
            );
            continue;
        }
        const bool duplicate = std::any_of(
            spawns->begin(), spawns->end(),
            [&encounter](const auto &spawn) { return spawn.spawn_id == encounter.encounter_id; }
        );
        if (duplicate) continue;
        spawns->push_back({
            encounter.encounter_id,
            encounter.species_id,
            encounter.latitude,
            encounter.longitude,
        });
    }
    return true;
}

bool read_runtime_map_snapshot_on_main_thread(
    ProbeContext &context,
    pogo_runtime::RuntimeNearbyObservation *observation,
    pogo_runtime::RuntimeFortsObservation *forts
) {
    // Keep the collection walk staged until each IL2CPP call has passed the
    // live build smoke test. Stage 3 uses dictionary keys and never treats the
    // broken value-collection Current return as a managed object.
    constexpr int kMapReaderStage = 5;
    if (kMapReaderStage == 0) return false;
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    if (observation == nullptr || forts == nullptr || !binding.map_entity_read_verified ||
        binding.map_entity_service == nullptr ||
        binding.map_entity_get_cells == nullptr ||
        !has_core_il2cpp_api(context.il2cpp_api)) return false;
    void *cells = invoke_runtime_method(
        context.il2cpp_api,
        binding.map_entity_get_cells,
        binding.map_entity_service,
        nullptr,
        "map.GetCells"
    );
    if (cells == nullptr) return false;
    if (kMapReaderStage == 1) return true;
    observation->spawns.clear();
    forts->forts.clear();
    if (kMapReaderStage == 2) {
        count_map_collection(context.il2cpp_api, cells);
        return true;
    }
    if (kMapReaderStage == 3) {
        std::vector<uint64_t> cell_keys;
        if (!enumerate_map_keys(context.il2cpp_api, binding.map_entity_cells, &cell_keys)) {
            return false;
        }
        return true;
    }
    std::vector<MapDictionaryEntry> cell_entries;
    if (!enumerate_map_dictionary(
            context.il2cpp_api, binding.map_entity_cells, &cell_entries)) {
        return false;
    }
    for (const MapDictionaryEntry &cell_entry : cell_entries) {
        if (observation->spawns.size() >= 512U) break;
        void *cell = cell_entry.value;
        if (cell != nullptr && !read_map_wild_pokemon(
                context.il2cpp_api, cell, &observation->spawns)) return false;
    }
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "runtime map reader cells=%zu wild=%zu",
        cell_entries.size(),
        observation->spawns.size()
    );
    if (binding.forts_read_verified && !read_runtime_map_forts(
            context.il2cpp_api, binding, forts)) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "runtime map reader could not enumerate active PokéStops"
        );
    }
    return pogo_runtime::valid_runtime_nearby_observation(*observation);
}
