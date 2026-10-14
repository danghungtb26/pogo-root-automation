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
    const void *get_enumerator = find_runtime_instance_method_in_hierarchy(
        api, collection_class, "GetEnumerator", 0U
    );
    if (get_enumerator == nullptr) return false;
    *enumerator = invoke_runtime_method(api, get_enumerator, collection, nullptr, label);
    if (!is_probable_managed_pointer(*enumerator)) return false;
    Il2CppClass *enumerator_class = api.object_get_class(*enumerator);
    *move_next = find_runtime_instance_method_in_hierarchy(
        api, enumerator_class, "MoveNext", 0U
    );
    *get_current = find_runtime_instance_method_in_hierarchy(
        api, enumerator_class, "get_Current", 0U
    );
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
        if (!read_boxed_bool(api, moved, &has_value)) return 0U;
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
    const void *get_keys = find_runtime_instance_method_in_hierarchy(
        api, dictionary_class, "get_Keys", 0U
    );
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
