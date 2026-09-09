bool read_dynamic_tappable_encounter_id(
    const Il2CppApi &api,
    void *tappable,
    uint64_t *encounter_id
) {
    if (tappable == nullptr || encounter_id == nullptr || api.object_get_class == nullptr) {
        return false;
    }
    const void *get_encounter_id = find_map_method(
        api,
        api.object_get_class(tappable),
        "get_EncounterId",
        0U
    );
    if (get_encounter_id == nullptr) return false;
    void *boxed = invoke_runtime_method(
        api,
        get_encounter_id,
        tappable,
        nullptr,
        "DynamicTappable.get_EncounterId"
    );
    return read_boxed_u64(api, boxed, encounter_id);
}

void *find_active_map_tappable_by_id(ProbeContext &context, uint64_t encounter_id) {
    if (encounter_id == 0U) return nullptr;
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    if (binding.map_entity_cells == nullptr || !has_core_il2cpp_api(context.il2cpp_api)) {
        return nullptr;
    }
    std::vector<MapDictionaryEntry> cell_entries;
    if (!enumerate_map_dictionary(
            context.il2cpp_api, binding.map_entity_cells, &cell_entries)) return nullptr;
    size_t wild_count = 0U;
    size_t tappable_count = 0U;
    size_t readable_id_count = 0U;
    for (const MapDictionaryEntry &cell_entry : cell_entries) {
        void *cell = cell_entry.value;
        if (cell != nullptr) {
            const Il2CppApi &api = context.il2cpp_api;
            void *tappable_dictionary = nullptr;
            if (read_runtime_field(api, cell, "eghx", &tappable_dictionary)) {
                std::vector<MapDictionaryEntry> tappable_entries;
                if (enumerate_map_dictionary(api, tappable_dictionary, &tappable_entries)) {
                    for (const MapDictionaryEntry &tappable_entry : tappable_entries) {
                        ++tappable_count;
                        uint64_t candidate_id = 0U;
                        if (!read_dynamic_tappable_encounter_id(
                                api, tappable_entry.value, &candidate_id) ||
                            candidate_id != encounter_id) continue;
                        __android_log_print(
                            ANDROID_LOG_INFO,
                            kLogTag,
                            "runtime action target matched via eghx id=%llu cell_key=%llu "
                            "tappable_key=%llu tappable=%p",
                            static_cast<unsigned long long>(encounter_id),
                            static_cast<unsigned long long>(cell_entry.key),
                            static_cast<unsigned long long>(tappable_entry.key),
                            tappable_entry.value
                        );
                        return tappable_entry.value;
                    }
                }
            }
            const void *get_map_pokemon = find_runtime_method_with_parameter(
                api,
                api.object_get_class(cell),
                "GetMapPokemon",
                "System.UInt64"
            );
            if (get_map_pokemon != nullptr && api.method_is_instance != nullptr &&
                api.method_is_instance(get_map_pokemon)) {
                void *arguments[] = {&encounter_id};
                void *map_pokemon = invoke_runtime_method(
                    api,
                    get_map_pokemon,
                    cell,
                    arguments,
                    "MapEntityCell.GetMapPokemon"
                );
                void *tappable = nullptr;
                if (is_probable_managed_pointer(map_pokemon) &&
                    read_runtime_field(api, map_pokemon, "egwu", &tappable) &&
                    is_probable_managed_pointer(tappable)) {
                    __android_log_print(
                        ANDROID_LOG_INFO,
                        kLogTag,
                        "runtime action target matched via GetMapPokemon id=%llu cell_key=%llu map=%p tappable=%p",
                        static_cast<unsigned long long>(encounter_id),
                        static_cast<unsigned long long>(cell_entry.key),
                        map_pokemon,
                        tappable
                    );
                    return tappable;
                }
            }
            const void *get_map_tappable = find_runtime_method_with_parameter(
                api,
                api.object_get_class(cell),
                "GetMapTappable",
                "System.UInt64"
            );
            if (get_map_tappable != nullptr && api.method_is_instance != nullptr &&
                api.method_is_instance(get_map_tappable)) {
                void *arguments[] = {&encounter_id};
                void *tappable = invoke_runtime_method(
                    api,
                    get_map_tappable,
                    cell,
                    arguments,
                    "MapEntityCell.GetMapTappable"
                );
                if (is_probable_managed_pointer(tappable)) {
                    __android_log_print(
                        ANDROID_LOG_INFO,
                        kLogTag,
                        "runtime action target matched via GetMapTappable id=%llu cell_key=%llu tappable=%p",
                        static_cast<unsigned long long>(encounter_id),
                        static_cast<unsigned long long>(cell_entry.key),
                        tappable
                    );
                    return tappable;
                }
            }
        }
        void *wild_dictionary = nullptr;
        if (!read_runtime_field(
                context.il2cpp_api, cell_entry.value, "eghw", &wild_dictionary)) continue;
        std::vector<MapDictionaryEntry> wild_entries;
        if (!enumerate_map_dictionary(context.il2cpp_api, wild_dictionary, &wild_entries)) continue;
        for (const MapDictionaryEntry &wild_entry : wild_entries) {
            ++wild_count;
            uint64_t candidate_id = 0U;
            if (!read_map_pokemon_id(context.il2cpp_api, wild_entry.value, &candidate_id)) continue;
            ++readable_id_count;
            if (candidate_id != encounter_id) continue;
            void *tappable = nullptr;
            if (read_runtime_field(
                    context.il2cpp_api, wild_entry.value, "egwu", &tappable) &&
                is_probable_managed_pointer(tappable)) {
                __android_log_print(
                    ANDROID_LOG_INFO,
                    kLogTag,
                    "runtime action target matched id=%llu cell_key=%llu wild_key=%llu tappable=%p",
                    static_cast<unsigned long long>(encounter_id),
                    static_cast<unsigned long long>(cell_entry.key),
                    static_cast<unsigned long long>(wild_entry.key),
                    tappable
                );
                return tappable;
            }
            __android_log_print(
                ANDROID_LOG_WARN,
                kLogTag,
                "runtime action target matched id=%llu but tappable field was unavailable wild=%p",
                static_cast<unsigned long long>(encounter_id),
                wild_entry.value
            );
        }
    }
    __android_log_print(
        ANDROID_LOG_WARN,
        kLogTag,
        "runtime action target miss id=%llu cells=%zu wild=%zu tappables=%zu readable_ids=%zu",
        static_cast<unsigned long long>(encounter_id),
        cell_entries.size(),
        wild_count,
        tappable_count,
        readable_id_count
    );
    return nullptr;
}

void *find_map_pokemon_from_cells_by_id(ProbeContext &context, uint64_t encounter_id) {
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    const Il2CppApi &api = context.il2cpp_api;
    if (encounter_id == 0U || binding.map_entity_cells == nullptr ||
        !has_core_il2cpp_api(api)) return nullptr;
    std::vector<MapDictionaryEntry> cell_entries;
    if (!enumerate_map_dictionary(api, binding.map_entity_cells, &cell_entries)) return nullptr;
    for (const MapDictionaryEntry &cell_entry : cell_entries) {
        void *wild_dictionary = nullptr;
        if (!read_runtime_field(api, cell_entry.value, "eghw", &wild_dictionary) ||
            !is_probable_managed_pointer(wild_dictionary)) continue;
        std::vector<MapDictionaryEntry> wild_entries;
        if (!enumerate_map_dictionary(api, wild_dictionary, &wild_entries)) continue;
        for (const MapDictionaryEntry &wild_entry : wild_entries) {
            void *wild = wild_entry.value;
            if (!runtime_object_is_exact_class(
                    api, wild, "Niantic.Holoholo.Map", "WildMapPokemon")) continue;
            uint64_t candidate_id = 0U;
            if (read_map_pokemon_id(api, wild, &candidate_id) && candidate_id == encounter_id) {
                __android_log_print(
                    ANDROID_LOG_INFO,
                    kLogTag,
                    "runtime action target matched via map cells id=%llu cell_key=%llu wild_key=%llu object=%p",
                    static_cast<unsigned long long>(encounter_id),
                    static_cast<unsigned long long>(cell_entry.key),
                    static_cast<unsigned long long>(wild_entry.key),
                    wild
                );
                return wild;
            }
        }
    }
    return nullptr;
}

void *find_active_map_pokemon_by_id(ProbeContext &context, uint64_t encounter_id) {
    const Il2CppApi &api = context.il2cpp_api;
    if (encounter_id == 0U || !ensure_main_thread_bridge(context) ||
        api.domain_get_assemblies == nullptr || api.array_length == nullptr ||
        api.array_get_byte_length == nullptr || api.class_get_type == nullptr ||
        api.type_get_object == nullptr) return nullptr;
    if (void *map_pokemon = find_map_pokemon_from_cells_by_id(context, encounter_id)) {
        return map_pokemon;
    }
    Il2CppDomain *domain = api.domain_get == nullptr ? nullptr : api.domain_get();
    size_t assembly_count = 0U;
    const Il2CppAssembly **assemblies = domain == nullptr
        ? nullptr : api.domain_get_assemblies(domain, &assembly_count);
    if (assemblies == nullptr) return nullptr;
    Il2CppClass *object_class = find_runtime_class(
        api, assemblies, assembly_count, "UnityEngine", "Object", nullptr
    );
    Il2CppClass *map_pokemon_class = find_runtime_class(
        api, assemblies, assembly_count, "Niantic.Holoholo.Map", "MapPokemon", nullptr
    );
    const void *find_method = object_class == nullptr ? nullptr :
        find_runtime_method_with_parameter(api, object_class, "FindObjectsOfType", "System.Type");
    if (map_pokemon_class == nullptr || find_method == nullptr) return nullptr;
    const Il2CppType *type = api.class_get_type(map_pokemon_class);
    void *type_object = type == nullptr ? nullptr : api.type_get_object(type);
    if (type_object == nullptr) return nullptr;
    void *arguments[] = {type_object};
    void *objects = invoke_runtime_method(api, find_method, nullptr, arguments, "FindObjectsOfType(MapPokemon)");
    if (!is_probable_managed_pointer(objects)) return nullptr;
    const size_t length = api.array_length(objects);
    const size_t byte_length = api.array_get_byte_length(objects);
    if (length > 256U || byte_length < length * sizeof(void *)) return nullptr;
    const auto *data = static_cast<const uint8_t *>(objects) + kRuntimeObjectArrayDataOffset;
    for (size_t index = 0U; index < length; ++index) {
        void *object = nullptr;
        memcpy(&object, data + index * sizeof(object), sizeof(object));
        uint64_t object_id = 0U;
        if (read_map_pokemon_id(api, object, &object_id) && object_id == encounter_id) return object;
    }
    return nullptr;
}

void *new_runtime_event_args(
    const Il2CppApi &api,
    const Il2CppAssembly **assemblies,
    size_t assembly_count
) {
    if (assemblies == nullptr || api.object_new == nullptr || api.runtime_object_init == nullptr) return nullptr;
    Il2CppClass *event_args_class = find_runtime_class(
        api, assemblies, assembly_count, "System", "EventArgs", nullptr
    );
    if (event_args_class == nullptr) return nullptr;
    void *event_args = api.object_new(event_args_class);
    if (!is_probable_managed_pointer(event_args)) return nullptr;
    api.runtime_object_init(event_args);
    return event_args;
}

MainThreadActionOutcome open_runtime_encounter_on_main_thread(
    ProbeContext &context,
    uint64_t encounter_id
) {
    const Il2CppApi &api = context.il2cpp_api;
    if (encounter_id == 0U || api.domain_get == nullptr || api.domain_get_assemblies == nullptr ||
        api.runtime_invoke == nullptr) return MainThreadActionOutcome::kRejected;
    Il2CppDomain *domain = api.domain_get();
    size_t assembly_count = 0U;
    const Il2CppAssembly **assemblies = domain == nullptr
        ? nullptr : api.domain_get_assemblies(domain, &assembly_count);
    if (assemblies == nullptr) return MainThreadActionOutcome::kRejected;
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    if (binding.dynamic_tappables_service != nullptr && binding.dynamic_tappables_tap != nullptr) {
        void *tappable = find_active_map_tappable_by_id(context, encounter_id);
        if (tappable != nullptr) {
            void *arguments[] = {tappable};
            void *exception = nullptr;
            __android_log_print(
                ANDROID_LOG_INFO,
                kLogTag,
                "runtime action invoking action=OPEN_ENCOUNTER route=DynamicTappables id=%llu service=%p method=%p tappable=%p",
                static_cast<unsigned long long>(encounter_id),
                binding.dynamic_tappables_service,
                binding.dynamic_tappables_tap,
                tappable
            );
            api.runtime_invoke(
                binding.dynamic_tappables_tap,
                binding.dynamic_tappables_service,
                arguments,
                &exception
            );
            if (exception != nullptr) {
                __android_log_print(
                    ANDROID_LOG_WARN,
                    kLogTag,
                    "runtime action invoke exception action=OPEN_ENCOUNTER route=DynamicTappables exception=%p",
                    exception
                );
                return MainThreadActionOutcome::kIndeterminate;
            }
            return MainThreadActionOutcome::kInvoked;
        }
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "runtime action DynamicTappables target unavailable id=%llu; trying MapPokemon.OnTap",
            static_cast<unsigned long long>(encounter_id)
        );
    }
    void *map_pokemon = find_active_map_pokemon_by_id(context, encounter_id);
    if (map_pokemon == nullptr || api.object_get_class == nullptr) return MainThreadActionOutcome::kRejected;
    if (binding.map_pokemon_on_tapped != nullptr) {
        void *exception = nullptr;
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "runtime action invoking action=OPEN_ENCOUNTER route=MapPokemon.OnTapped "
            "id=%llu object=%p method=%p",
            static_cast<unsigned long long>(encounter_id),
            map_pokemon,
            binding.map_pokemon_on_tapped
        );
        api.runtime_invoke(binding.map_pokemon_on_tapped, map_pokemon, nullptr, &exception);
        if (exception != nullptr) {
            __android_log_print(
                ANDROID_LOG_WARN,
                kLogTag,
                "runtime action invoke exception action=OPEN_ENCOUNTER "
                "route=MapPokemon.OnTapped exception=%p",
                exception
            );
            return MainThreadActionOutcome::kIndeterminate;
        }
        return MainThreadActionOutcome::kInvoked;
    }
    Il2CppClass *map_class = find_runtime_class(
        api, assemblies, assembly_count, "Niantic.Holoholo.Map", "MapPokemon", nullptr
    );
    const void *on_tap = find_runtime_method_with_two_parameters(
        api, map_class, "OnTap", "System.Object", "System.EventArgs"
    );
    if (on_tap == nullptr) return MainThreadActionOutcome::kRejected;
    void *gesture = nullptr;
    (void)read_runtime_field(api, map_pokemon, "tapGesture", &gesture);
    void *event_args = new_runtime_event_args(api, assemblies, assembly_count);
    if (event_args == nullptr) return MainThreadActionOutcome::kRejected;
    void *arguments[] = {gesture, event_args};
    void *exception = nullptr;
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "runtime action invoking action=OPEN_ENCOUNTER id=%llu object=%p method=%p gesture=%p",
        static_cast<unsigned long long>(encounter_id), map_pokemon, on_tap, gesture
    );
    api.runtime_invoke(on_tap, map_pokemon, arguments, &exception);
    if (exception != nullptr) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "runtime action invoke exception action=OPEN_ENCOUNTER exception=%p",
            exception
        );
        return MainThreadActionOutcome::kIndeterminate;
    }
    return MainThreadActionOutcome::kInvoked;
}
