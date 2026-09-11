bool is_runtime_owner_candidate(const char *field_name, const char *type_name) {
    static const char *const needles[] = {
        "ItemBag",
        "PokemonBag",
        "LocationProvider",
        "IPlayerService",
        "SnapshotPreview",
        "IEncounter",
        "IMapPokemon",
    };
    for (const char *needle : needles) {
        if (contains_ignore_case(field_name, needle) || contains_ignore_case(type_name, needle)) {
            return true;
        }
    }
    return false;
}

[[maybe_unused]] void scan_runtime_static_owner_fields(
    const Il2CppApi &api,
    const Il2CppAssembly **assemblies,
    size_t assembly_count
) {
    if (!has_field_inspection_api(api) || api.image_get_class_count == nullptr ||
        api.image_get_class == nullptr || api.class_get_name == nullptr ||
        api.class_get_namespace == nullptr || assemblies == nullptr) return;

    size_t scanned_classes = 0U;
    size_t matched_fields = 0U;
    constexpr size_t kMaxClasses = 100000U;
    constexpr size_t kMaxMatches = 256U;
    for (size_t assembly_index = 0U; assembly_index < assembly_count &&
         scanned_classes < kMaxClasses && matched_fields < kMaxMatches; ++assembly_index) {
        const Il2CppAssembly *assembly = assemblies[assembly_index];
        if (assembly == nullptr) continue;
        const Il2CppImage *image = api.assembly_get_image(assembly);
        if (image == nullptr) continue;
        const size_t class_count = api.image_get_class_count(image);
        for (size_t class_index = 0U; class_index < class_count &&
             scanned_classes < kMaxClasses && matched_fields < kMaxMatches; ++class_index) {
            Il2CppClass *klass = api.image_get_class(image, class_index);
            ++scanned_classes;
            if (klass == nullptr) continue;
            const char *class_name = api.class_get_name(klass);
            const char *name_space = api.class_get_namespace(klass);
            void *iterator = nullptr;
            for (size_t field_index = 0U; field_index < 128U; ++field_index) {
                const void *field = api.class_get_fields(klass, &iterator);
                if (field == nullptr) break;
                const uint32_t flags = api.field_get_flags(field);
                if ((flags & 0x0010U) == 0U || (flags & 0x1000U) != 0U) continue;
                const char *field_name = api.field_get_name(field);
                const Il2CppType *field_type = api.field_get_type == nullptr
                    ? nullptr
                    : api.field_get_type(field);
                const char *type_name = (field_type == nullptr || api.type_get_name == nullptr)
                    ? "<unknown>"
                    : api.type_get_name(field_type);
                if (!is_runtime_owner_candidate(field_name, type_name)) continue;
                ++matched_fields;
                const char *image_name = api.image_get_name == nullptr
                    ? "<unknown>"
                    : api.image_get_name(image);
                __android_log_print(
                    ANDROID_LOG_INFO,
                    kLogTag,
                    "managed diagnostic: static owner image=%s class=%s.%s field=%s flags=0x%x type=%s",
                    image_name == nullptr ? "<unknown>" : image_name,
                    name_space == nullptr ? "" : name_space,
                    class_name == nullptr ? "<unknown>" : class_name,
                    field_name == nullptr ? "<unknown>" : field_name,
                    flags,
                    type_name == nullptr ? "<unknown>" : type_name
                );
            }
        }
    }
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "managed diagnostic: static owner scan classes=%zu matches=%zu truncated=%d",
        scanned_classes,
        matched_fields,
        scanned_classes >= kMaxClasses || matched_fields >= kMaxMatches ? 1 : 0
    );
}

[[maybe_unused]] void scan_runtime_encounter_classes(
    const Il2CppApi &api,
    const Il2CppAssembly **assemblies,
    size_t assembly_count
) {
    if (api.image_get_class_count == nullptr || api.image_get_class == nullptr ||
        api.assembly_get_image == nullptr || api.class_get_name == nullptr ||
        api.class_get_namespace == nullptr || api.image_get_name == nullptr ||
        assemblies == nullptr) return;
    static const char *const keywords[] = {
        "EncounterService", "IEncounterService", "EncounterData", "EncounterPokemon",
        "EncounterState", "EncounterInteraction", "MapPokemon", "WildPokemon",
        "Catch",
    };
    constexpr size_t kMaxClasses = 100000U;
    constexpr size_t kMaxMatches = 192U;
    size_t scanned_classes = 0U;
    size_t matched_classes = 0U;
    for (size_t assembly_index = 0U; assembly_index < assembly_count &&
         scanned_classes < kMaxClasses && matched_classes < kMaxMatches; ++assembly_index) {
        const Il2CppAssembly *assembly = assemblies[assembly_index];
        if (assembly == nullptr) continue;
        const Il2CppImage *image = api.assembly_get_image(assembly);
        if (image == nullptr) continue;
        const size_t class_count = api.image_get_class_count(image);
        for (size_t class_index = 0U; class_index < class_count &&
             scanned_classes < kMaxClasses && matched_classes < kMaxMatches; ++class_index) {
            Il2CppClass *klass = api.image_get_class(image, class_index);
            ++scanned_classes;
            if (klass == nullptr) continue;
            const char *class_name = api.class_get_name(klass);
            const char *name_space = api.class_get_namespace(klass);
            bool matched = false;
            for (const char *keyword : keywords) {
                if (contains_ignore_case(class_name, keyword) ||
                    contains_ignore_case(name_space, keyword)) {
                    matched = true;
                    break;
                }
            }
            if (!matched) continue;
            ++matched_classes;
            __android_log_print(
                ANDROID_LOG_INFO,
                kLogTag,
                "managed diagnostic: encounter class image=%s namespace=%s name=%s ptr=%p",
                api.image_get_name(image) == nullptr ? "<unknown>" : api.image_get_name(image),
                name_space == nullptr ? "" : name_space,
                class_name == nullptr ? "<unknown>" : class_name,
                klass
            );
        }
    }
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "managed diagnostic: encounter class scan classes=%zu matches=%zu truncated=%d",
        scanned_classes,
        matched_classes,
        scanned_classes >= kMaxClasses || matched_classes >= kMaxMatches ? 1 : 0
    );
}

Il2CppClass *find_runtime_class(
    const Il2CppApi &api,
    const Il2CppAssembly **assemblies,
    size_t assembly_count,
    const char *name_space,
    const char *class_name,
    const Il2CppImage **image_out
) {
    if (assemblies == nullptr || api.class_from_name == nullptr ||
        api.assembly_get_image == nullptr || name_space == nullptr || class_name == nullptr) {
        return nullptr;
    }
    const size_t inspect_count = assembly_count < kMaxAssembliesToInspect
        ? assembly_count
        : kMaxAssembliesToInspect;
    for (size_t index = 0U; index < inspect_count; ++index) {
        const Il2CppAssembly *assembly = assemblies[index];
        if (assembly == nullptr) continue;
        const Il2CppImage *image = api.assembly_get_image(assembly);
        if (image == nullptr) continue;
        Il2CppClass *klass = api.class_from_name(image, name_space, class_name);
        if (klass == nullptr) continue;
        if (image_out != nullptr) *image_out = image;
        return klass;
    }
    return nullptr;
}

const void *find_runtime_method_with_parameter(
    const Il2CppApi &api,
    Il2CppClass *klass,
    const char *method_name,
    const char *parameter_type_name
) {
    if (!has_runtime_inspection_api(api) || api.class_get_methods == nullptr ||
        api.method_get_name == nullptr || klass == nullptr || method_name == nullptr ||
        parameter_type_name == nullptr) return nullptr;

    void *iterator = nullptr;
    uint32_t inspected = 0U;
    while (inspected < 512U) {
        const void *method = api.class_get_methods(klass, &iterator);
        if (method == nullptr) break;
        ++inspected;
        const char *candidate_name = api.method_get_name(method);
        if (candidate_name == nullptr || strcmp(candidate_name, method_name) != 0 ||
            api.method_get_param_count(method) != 1U) continue;
        const Il2CppType *parameter_type = api.method_get_param(method, 0U);
        const char *candidate_type = parameter_type == nullptr || api.type_get_name == nullptr
            ? nullptr
            : api.type_get_name(parameter_type);
        if (candidate_type != nullptr && strcmp(candidate_type, parameter_type_name) == 0) {
            return method;
        }
    }
    return nullptr;
}

const void *find_runtime_method_with_two_parameters(
    const Il2CppApi &api,
    Il2CppClass *klass,
    const char *method_name,
    const char *first_parameter_type_name,
    const char *second_parameter_type_name
) {
    if (!has_runtime_inspection_api(api) || api.class_get_methods == nullptr ||
        api.method_get_name == nullptr || klass == nullptr || method_name == nullptr ||
        first_parameter_type_name == nullptr || second_parameter_type_name == nullptr) return nullptr;

    void *iterator = nullptr;
    uint32_t inspected = 0U;
    while (inspected < 512U) {
        const void *method = api.class_get_methods(klass, &iterator);
        if (method == nullptr) break;
        ++inspected;
        const char *candidate_name = api.method_get_name(method);
        if (candidate_name == nullptr || strcmp(candidate_name, method_name) != 0 ||
            api.method_get_param_count(method) != 2U) continue;
        const Il2CppType *first_type = api.method_get_param(method, 0U);
        const Il2CppType *second_type = api.method_get_param(method, 1U);
        const char *first_name = first_type == nullptr || api.type_get_name == nullptr
            ? nullptr
            : api.type_get_name(first_type);
        const char *second_name = second_type == nullptr || api.type_get_name == nullptr
            ? nullptr
            : api.type_get_name(second_type);
        if (first_name != nullptr && second_name != nullptr &&
            strcmp(first_name, first_parameter_type_name) == 0 &&
            strcmp(second_name, second_parameter_type_name) == 0) return method;
    }
    return nullptr;
}

bool verify_expected_il2cpp_build_id(const char *library_path) {
    if (library_path == nullptr || library_path[0] == '\0') return false;
    static constexpr uint8_t kExpectedBuildId[] = {
        0x84U, 0xedU, 0x21U, 0x00U, 0x98U, 0x99U, 0xc8U, 0x4aU,
        0xbcU, 0xa0U, 0x1eU, 0x0bU, 0x8cU, 0x6fU, 0x4bU, 0x0bU,
        0x05U, 0x4fU, 0x26U, 0x4eU,
    };
    const int fd = open(library_path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;
    Elf64_Ehdr header{};
    const bool header_read = pread(fd, &header, sizeof(header), 0) == static_cast<ssize_t>(sizeof(header));
    if (!header_read || memcmp(header.e_ident, ELFMAG, SELFMAG) != 0 ||
        header.e_ident[EI_CLASS] != ELFCLASS64 || header.e_phentsize != sizeof(Elf64_Phdr)) {
        close(fd);
        return false;
    }
    std::vector<Elf64_Phdr> program_headers(header.e_phnum);
    const size_t headers_size = program_headers.size() * sizeof(Elf64_Phdr);
    if (pread(fd, program_headers.data(), headers_size, static_cast<off_t>(header.e_phoff)) !=
        static_cast<ssize_t>(headers_size)) {
        close(fd);
        return false;
    }
    for (const Elf64_Phdr &program_header : program_headers) {
        if (program_header.p_type != PT_NOTE || program_header.p_filesz == 0U ||
            program_header.p_filesz > 1024U * 1024U) continue;
        std::vector<uint8_t> notes(static_cast<size_t>(program_header.p_filesz));
        if (pread(
                fd,
                notes.data(),
                notes.size(),
                static_cast<off_t>(program_header.p_offset)
            ) != static_cast<ssize_t>(notes.size())) continue;
        size_t offset = 0U;
        while (offset + 12U <= notes.size()) {
            uint32_t name_size = 0U;
            uint32_t description_size = 0U;
            uint32_t type = 0U;
            memcpy(&name_size, notes.data() + offset, sizeof(name_size));
            memcpy(&description_size, notes.data() + offset + 4U, sizeof(description_size));
            memcpy(&type, notes.data() + offset + 8U, sizeof(type));
            offset += 12U;
            const size_t name_padded = (static_cast<size_t>(name_size) + 3U) & ~3U;
            const size_t description_padded = (static_cast<size_t>(description_size) + 3U) & ~3U;
            if (name_padded > notes.size() - offset ||
                description_padded > notes.size() - offset - name_padded) break;
            const uint8_t *name = notes.data() + offset;
            const uint8_t *description = name + name_padded;
            if (type == 3U && name_size >= 3U && memcmp(name, "GNU", 3U) == 0 &&
                description_size == sizeof(kExpectedBuildId) &&
                memcmp(description, kExpectedBuildId, sizeof(kExpectedBuildId)) == 0) {
                close(fd);
                return true;
            }
            offset += name_padded + description_padded;
        }
    }
    close(fd);
    return false;
}
