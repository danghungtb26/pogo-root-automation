constexpr size_t kRuntimeObjectArrayDataOffset = sizeof(void *) * 4U;

bool read_map_pokemon_id(const Il2CppApi &api, void *object, uint64_t *encounter_id) {
    if (object == nullptr || encounter_id == nullptr || api.object_get_class == nullptr) return false;
    const void *method = find_map_method(
        api, api.object_get_class(object), "get_EncounterId", 0U
    );
    if (method == nullptr || api.runtime_invoke == nullptr) return false;
    void *exception = nullptr;
    void *boxed = api.runtime_invoke(method, object, nullptr, &exception);
    return exception == nullptr && read_boxed_u64(api, boxed, encounter_id);
}

void log_map_pokemon_instance(const Il2CppApi &api, void *object, size_t index) {
    if (!is_probable_managed_pointer(object) || api.object_get_class == nullptr) return;
    Il2CppClass *klass = api.object_get_class(object);
    const char *name = klass == nullptr || api.class_get_name == nullptr
        ? "<unknown>" : api.class_get_name(klass);
    const char *name_space = klass == nullptr || api.class_get_namespace == nullptr
        ? "" : api.class_get_namespace(klass);
    uint64_t encounter_id = 0U;
    const bool id_read = read_map_pokemon_id(api, object, &encounter_id);
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "managed diagnostic: active MapPokemon index=%zu object=%p class=%s.%s id_read=%d id=%llu",
        index, object, name_space == nullptr ? "" : name_space,
        name == nullptr ? "<unknown>" : name, id_read ? 1 : 0,
        static_cast<unsigned long long>(encounter_id)
    );
    static const char *const fields[] = {
        "mapExploreState", "mapObjectRoot", "rpcHandler", "tapGesture",
        "itemBag", "egwm", "egwn", "egwo", "egwp", "egwq", "egwu", "egwv",
    };
    for (const char *field : fields) log_runtime_field_object(api, object, field, "ActiveMapPokemon");
}

void log_active_map_pokemon_objects(ProbeContext &context) {
    const Il2CppApi &api = context.il2cpp_api;
    if (!ensure_main_thread_bridge(context) || api.domain_get_assemblies == nullptr ||
        api.array_length == nullptr || api.array_get_byte_length == nullptr) return;
    Il2CppDomain *domain = api.domain_get();
    size_t assembly_count = 0U;
    const Il2CppAssembly **assemblies = domain == nullptr
        ? nullptr : api.domain_get_assemblies(domain, &assembly_count);
    if (assemblies == nullptr || api.class_get_type == nullptr || api.type_get_object == nullptr) return;
    Il2CppClass *object_class = find_runtime_class(
        api, assemblies, assembly_count, "UnityEngine", "Object", nullptr
    );
    Il2CppClass *map_pokemon_class = find_runtime_class(
        api, assemblies, assembly_count, "Niantic.Holoholo.Map", "MapPokemon", nullptr
    );
    const void *find_method = object_class == nullptr ? nullptr : find_runtime_method_with_parameter(
        api, object_class, "FindObjectsOfType", "System.Type"
    );
    if (map_pokemon_class == nullptr || find_method == nullptr || api.method_is_instance == nullptr ||
        api.method_is_instance(find_method)) return;
    const Il2CppType *type = api.class_get_type(map_pokemon_class);
    void *type_object = type == nullptr ? nullptr : api.type_get_object(type);
    if (type_object == nullptr) return;
    void *arguments[] = {type_object};
    void *objects = invoke_runtime_method(
        api, find_method, nullptr, arguments, "FindObjectsOfType(MapPokemon).diagnostic"
    );
    if (!is_probable_managed_pointer(objects)) {
        __android_log_print(ANDROID_LOG_INFO, kLogTag, "managed diagnostic: active MapPokemon array is null");
        return;
    }
    const size_t length = api.array_length(objects);
    const size_t byte_length = api.array_get_byte_length(objects);
    if (length > 256U || byte_length < length * sizeof(void *)) return;
    __android_log_print(ANDROID_LOG_INFO, kLogTag, "managed diagnostic: active MapPokemon array length=%zu", length);
    const auto *data = static_cast<const uint8_t *>(objects) + kRuntimeObjectArrayDataOffset;
    for (size_t index = 0U; index < length; ++index) {
        void *object = nullptr;
        memcpy(&object, data + index * sizeof(void *), sizeof(object));
        log_map_pokemon_instance(api, object, index);
    }
}

void diagnose_map_pokemon_instance(ProbeContext &context) {
    log_active_map_pokemon_objects(context);
    if (!ensure_main_thread_bridge(context)) return;
    void *map_pokemon = request_main_thread_object(
        context, "Niantic.Holoholo.Map", "MapPokemon"
    );
    const Il2CppApi &api = context.il2cpp_api;
    log_runtime_object(api, map_pokemon, "Find.MapPokemon", "MapPokemon");
}
