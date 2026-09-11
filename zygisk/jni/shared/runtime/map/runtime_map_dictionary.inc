constexpr size_t kIl2CppArrayDataOffset = sizeof(void *) * 4U;

struct RuntimeDictionaryLayout {
    void *entries_array = nullptr;
    size_t array_length = 0U;
    size_t array_byte_length = 0U;
    size_t element_size = 0U;
    int32_t count = 0;
    int32_t key_offset = -1;
    int32_t value_offset = -1;
};

int32_t normalize_entry_field_offset(int32_t field_offset, size_t element_size) {
    constexpr int32_t kIl2CppObjectHeaderBytes = static_cast<int32_t>(sizeof(void *) * 2U);
    if (field_offset >= static_cast<int32_t>(element_size) &&
        field_offset >= kIl2CppObjectHeaderBytes) {
        return field_offset - kIl2CppObjectHeaderBytes;
    }
    return field_offset;
}

bool read_dictionary_layout(
    const Il2CppApi &api,
    void *dictionary,
    RuntimeDictionaryLayout *layout
) {
    if (layout == nullptr || !is_probable_managed_pointer(dictionary) ||
        api.class_get_field_from_name == nullptr || api.field_get_value == nullptr ||
        api.field_get_type == nullptr || api.type_get_class_or_element_class == nullptr ||
        api.field_get_offset == nullptr || api.array_length == nullptr ||
        api.array_get_byte_length == nullptr) return false;
    Il2CppClass *dictionary_class = api.object_get_class(dictionary);
    const void *entries_field = api.class_get_field_from_name(dictionary_class, "_entries");
    const void *count_field = api.class_get_field_from_name(dictionary_class, "_count");
    if (entries_field == nullptr || count_field == nullptr) return false;
    api.field_get_value(dictionary, const_cast<void *>(entries_field), &layout->entries_array);
    api.field_get_value(dictionary, const_cast<void *>(count_field), &layout->count);
    if (layout->count < 0) return false;
    if (!is_probable_managed_pointer(layout->entries_array)) return layout->count == 0;
    layout->array_length = api.array_length(layout->entries_array);
    layout->array_byte_length = api.array_get_byte_length(layout->entries_array);
    if (layout->array_length == 0U || layout->array_byte_length == 0U) {
        return layout->count == 0;
    }
    if (layout->array_byte_length % layout->array_length != 0U) return false;
    layout->element_size = layout->array_byte_length / layout->array_length;
    const Il2CppType *entries_type = api.field_get_type(entries_field);
    Il2CppClass *entry_class = api.type_get_class_or_element_class(entries_type);
    const void *key_field = entry_class == nullptr
        ? nullptr
        : api.class_get_field_from_name(entry_class, "key");
    const void *value_field = entry_class == nullptr
        ? nullptr
        : api.class_get_field_from_name(entry_class, "value");
    if (key_field == nullptr || value_field == nullptr) return false;
    const int32_t raw_key_offset = api.field_get_offset(key_field);
    const int32_t raw_value_offset = api.field_get_offset(value_field);
    layout->key_offset = normalize_entry_field_offset(raw_key_offset, layout->element_size);
    layout->value_offset = normalize_entry_field_offset(raw_value_offset, layout->element_size);
    if (layout->element_size < sizeof(uint64_t) || layout->key_offset < 0 ||
        layout->value_offset < 0 || static_cast<size_t>(layout->key_offset) + sizeof(uint64_t) >
            layout->element_size || static_cast<size_t>(layout->value_offset) + sizeof(void *) >
            layout->element_size || layout->count > static_cast<int32_t>(layout->array_length)) return false;
    return true;
}

bool enumerate_map_dictionary(
    const Il2CppApi &api,
    void *dictionary,
    std::vector<MapDictionaryEntry> *entries
) {
    if (entries == nullptr) return false;
    entries->clear();
    RuntimeDictionaryLayout layout;
    if (!read_dictionary_layout(api, dictionary, &layout)) return false;
    auto *data = static_cast<uint8_t *>(layout.entries_array) + kIl2CppArrayDataOffset;
    for (int32_t index = 0; index < layout.count; ++index) {
        auto *entry = data + static_cast<size_t>(index) * layout.element_size;
        MapDictionaryEntry value;
        memcpy(&value.key, entry + layout.key_offset, sizeof(value.key));
        memcpy(&value.key_object, entry + layout.key_offset, sizeof(value.key_object));
        memcpy(&value.value, entry + layout.value_offset, sizeof(value.value));
        if (!is_probable_managed_pointer(value.value)) continue;
        entries->push_back(value);
    }
    return true;
}
