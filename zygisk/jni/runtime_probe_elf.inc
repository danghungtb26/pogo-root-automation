bool find_mapping_path(const char *needle, char *output, size_t output_size) {
    if (needle == nullptr || output == nullptr || output_size == 0U) return false;
    FILE *maps = fopen("/proc/self/maps", "re");
    if (maps == nullptr) return false;

    bool found = false;
    char line[1536]{};
    char path[1024]{};
    while (fgets(line, sizeof(line), maps) != nullptr) {
        path[0] = '\0';
        const int parsed = sscanf(line, "%*s %*s %*s %*s %*s %1023s", path);
        if (parsed == 1 && strstr(path, needle) != nullptr) {
            copy_string(output, output_size, path);
            found = true;
            break;
        }
    }
    fclose(maps);
    return found;
}

bool mapping_contains(const char *needle) {
    char ignored[1024]{};
    return find_mapping_path(needle, ignored, sizeof(ignored));
}

struct LoadedElfExports {
    uintptr_t load_bias = 0U;
    const Elf64_Sym *symbols = nullptr;
    const char *strings = nullptr;
    size_t symbol_count = 0U;
};

uintptr_t dynamic_pointer(uintptr_t load_bias, Elf64_Addr pointer) {
    const uintptr_t value = static_cast<uintptr_t>(pointer);
    return value < load_bias ? load_bias + value : value;
}

bool find_loaded_elf_exports(const char *library_path, LoadedElfExports *exports) {
    if (library_path == nullptr || library_path[0] == '\0' || exports == nullptr) return false;

    FILE *maps = fopen("/proc/self/maps", "re");
    if (maps == nullptr) return false;

    uintptr_t mapping_start = 0U;
    char line[1536]{};
    char mapped_path[1024]{};
    while (fgets(line, sizeof(line), maps) != nullptr) {
        unsigned long start = 0UL;
        unsigned long offset = 0UL;
        mapped_path[0] = '\0';
        const int parsed = sscanf(
            line,
            "%lx-%*lx %*s %lx %*s %*s %1023s",
            &start,
            &offset,
            mapped_path
        );
        if (parsed == 3 && offset == 0UL && strcmp(mapped_path, library_path) == 0) {
            mapping_start = static_cast<uintptr_t>(start);
            break;
        }
    }
    fclose(maps);
    if (mapping_start == 0U) return false;

    const auto *header = reinterpret_cast<const Elf64_Ehdr *>(mapping_start);
    if (memcmp(header->e_ident, ELFMAG, SELFMAG) != 0 ||
        header->e_ident[EI_CLASS] != ELFCLASS64 ||
        header->e_phentsize != sizeof(Elf64_Phdr)) return false;

    uintptr_t load_bias = mapping_start;
    const auto *program_headers = reinterpret_cast<const Elf64_Phdr *>(
        mapping_start + static_cast<uintptr_t>(header->e_phoff)
    );
    const Elf64_Phdr *dynamic_header = nullptr;
    for (uint16_t index = 0U; index < header->e_phnum; ++index) {
        const Elf64_Phdr &program_header = program_headers[index];
        if (program_header.p_type == PT_LOAD && program_header.p_offset == 0U) {
            load_bias = mapping_start - static_cast<uintptr_t>(program_header.p_vaddr);
        } else if (program_header.p_type == PT_DYNAMIC) {
            dynamic_header = &program_header;
        }
    }
    if (dynamic_header == nullptr) return false;

    const auto *dynamic = reinterpret_cast<const Elf64_Dyn *>(
        load_bias + static_cast<uintptr_t>(dynamic_header->p_vaddr)
    );
    const Elf64_Sym *symbols = nullptr;
    const char *strings = nullptr;
    const uint32_t *sysv_hash = nullptr;
    const uint32_t *gnu_hash = nullptr;
    size_t entry_size = sizeof(Elf64_Sym);
    for (const Elf64_Dyn *entry = dynamic; entry->d_tag != DT_NULL; ++entry) {
        switch (entry->d_tag) {
            case DT_SYMTAB:
                symbols = reinterpret_cast<const Elf64_Sym *>(
                    dynamic_pointer(load_bias, entry->d_un.d_ptr)
                );
                break;
            case DT_STRTAB:
                strings = reinterpret_cast<const char *>(
                    dynamic_pointer(load_bias, entry->d_un.d_ptr)
                );
                break;
            case DT_SYMENT:
                entry_size = static_cast<size_t>(entry->d_un.d_val);
                break;
            case DT_HASH:
                sysv_hash = reinterpret_cast<const uint32_t *>(
                    dynamic_pointer(load_bias, entry->d_un.d_ptr)
                );
                break;
            case DT_GNU_HASH:
                gnu_hash = reinterpret_cast<const uint32_t *>(
                    dynamic_pointer(load_bias, entry->d_un.d_ptr)
                );
                break;
            default:
                break;
        }
    }
    if (symbols == nullptr || strings == nullptr || entry_size != sizeof(Elf64_Sym)) return false;

    size_t symbol_count = 0U;
    if (sysv_hash != nullptr) {
        symbol_count = static_cast<size_t>(sysv_hash[1]);
    } else if (gnu_hash != nullptr) {
        const uint32_t bucket_count = gnu_hash[0];
        const uint32_t symbol_offset = gnu_hash[1];
        const uint32_t bloom_size = gnu_hash[2];
        const size_t word_count = sizeof(Elf64_Addr) / sizeof(uint32_t);
        const uint32_t *buckets = gnu_hash + 4U +
            static_cast<size_t>(bloom_size) * word_count;
        const uint32_t *chains = buckets + bucket_count;
        uint32_t largest_symbol = symbol_offset;
        for (uint32_t bucket = 0U; bucket < bucket_count; ++bucket) {
            uint32_t symbol_index = buckets[bucket];
            if (symbol_index < symbol_offset) continue;
            while ((chains[symbol_index - symbol_offset] & 1U) == 0U) ++symbol_index;
            if (symbol_index >= largest_symbol) largest_symbol = symbol_index + 1U;
        }
        symbol_count = largest_symbol;
    }
    if (symbol_count == 0U) return false;

    exports->load_bias = load_bias;
    exports->symbols = symbols;
    exports->strings = strings;
    exports->symbol_count = symbol_count;
    return true;
}

void *resolve_loaded_elf_symbol(const LoadedElfExports &exports, const char *name) {
    if (name == nullptr || exports.symbols == nullptr || exports.strings == nullptr) return nullptr;
    for (size_t index = 0U; index < exports.symbol_count; ++index) {
        const Elf64_Sym &symbol = exports.symbols[index];
        if (symbol.st_name == 0U || symbol.st_value == 0U) continue;
        const char *symbol_name = exports.strings + symbol.st_name;
        if (strcmp(symbol_name, name) == 0) {
            return reinterpret_cast<void *>(exports.load_bias + static_cast<uintptr_t>(symbol.st_value));
        }
    }
    return nullptr;
}

template <typename T>
T resolve_symbol_with_fallback(
    void *handle,
    const LoadedElfExports *exports,
    const char *name,
    uint32_t *resolved_count
) {
    void *symbol = dlsym(handle, name);
    if (symbol == nullptr && exports != nullptr) symbol = resolve_loaded_elf_symbol(*exports, name);
    if (symbol != nullptr && resolved_count != nullptr) *resolved_count += 1U;
    return reinterpret_cast<T>(symbol);
}

bool has_core_il2cpp_api(const Il2CppApi &api) {
    return api.domain_get != nullptr && api.thread_attach != nullptr &&
        api.thread_detach != nullptr && api.domain_get_assemblies != nullptr &&
        api.assembly_get_image != nullptr && api.image_get_name != nullptr &&
        api.class_from_name != nullptr && api.class_get_field_from_name != nullptr &&
        api.field_get_value != nullptr && api.object_get_class != nullptr;
}

bool has_class_survey_api(const Il2CppApi &api) {
    return api.image_get_class_count != nullptr && api.image_get_class != nullptr &&
        api.class_get_name != nullptr && api.class_get_namespace != nullptr;
}

bool has_method_survey_api(const Il2CppApi &api) {
    return api.class_get_methods != nullptr && api.method_get_name != nullptr;
}

bool has_runtime_inspection_api(const Il2CppApi &api) {
    return has_core_il2cpp_api(api) && api.class_get_method_from_name != nullptr &&
        api.method_get_param_count != nullptr && api.method_get_param != nullptr &&
        api.method_get_return_type != nullptr && api.method_is_instance != nullptr;
}

bool has_field_inspection_api(const Il2CppApi &api) {
    return api.class_get_fields != nullptr && api.field_get_flags != nullptr &&
        api.field_get_name != nullptr;
}

bool resolve_il2cpp_api(const char *il2cpp_path, Il2CppApi *api) {
    if (il2cpp_path == nullptr || il2cpp_path[0] == '\0' || api == nullptr) return false;
    api->handle = dlopen(il2cpp_path, RTLD_NOW | RTLD_NOLOAD);
    if (api->handle == nullptr) api->handle = dlopen("libil2cpp.so", RTLD_NOW | RTLD_NOLOAD);
    if (api->handle != nullptr) {
        api->owns_handle = true;
    } else {
        // Android linker namespaces may hide an already-loaded library from
        // an absolute-path RTLD_NOLOAD lookup. The exported IL2CPP API is
        // still visible through the target process' default lookup scope.
        // RTLD_DEFAULT is a lookup handle, not an owned dlopen handle.
        api->handle = RTLD_DEFAULT;
        api->owns_handle = false;
    }
    LoadedElfExports loaded_exports{};
    const bool has_loaded_exports = find_loaded_elf_exports(il2cpp_path, &loaded_exports);

#define RESOLVE(field, type, name) api->field = resolve_symbol_with_fallback<type>( \
        api->handle, has_loaded_exports ? &loaded_exports : nullptr, name, &api->symbol_count \
    )
    RESOLVE(domain_get, Il2CppDomainGet, "il2cpp_domain_get");
    RESOLVE(thread_current, Il2CppThreadCurrent, "il2cpp_thread_current");
    RESOLVE(thread_attach, Il2CppThreadAttach, "il2cpp_thread_attach");
    RESOLVE(thread_detach, Il2CppThreadDetach, "il2cpp_thread_detach");
    RESOLVE(domain_get_assemblies, Il2CppDomainGetAssemblies, "il2cpp_domain_get_assemblies");
    RESOLVE(assembly_get_image, Il2CppAssemblyGetImage, "il2cpp_assembly_get_image");
    RESOLVE(image_get_name, Il2CppImageGetName, "il2cpp_image_get_name");
    RESOLVE(class_from_name, Il2CppClassFromName, "il2cpp_class_from_name");
    RESOLVE(class_get_field_from_name, Il2CppClassGetFieldFromName, "il2cpp_class_get_field_from_name");
    RESOLVE(field_get_value, Il2CppFieldGetValue, "il2cpp_field_get_value");
    RESOLVE(object_get_class, Il2CppObjectGetClass, "il2cpp_object_get_class");
    RESOLVE(object_unbox, Il2CppObjectUnbox, "il2cpp_object_unbox");
    RESOLVE(array_length, Il2CppArrayLength, "il2cpp_array_length");
    RESOLVE(array_get_byte_length, Il2CppArrayGetByteLength, "il2cpp_array_get_byte_length");
    RESOLVE(image_get_class_count, Il2CppImageGetClassCount, "il2cpp_image_get_class_count");
    RESOLVE(image_get_class, Il2CppImageGetClass, "il2cpp_image_get_class");
    RESOLVE(class_get_name, Il2CppClassGetName, "il2cpp_class_get_name");
    RESOLVE(class_get_namespace, Il2CppClassGetNamespace, "il2cpp_class_get_namespace");
    RESOLVE(class_get_methods, Il2CppClassGetMethods, "il2cpp_class_get_methods");
    RESOLVE(method_get_name, Il2CppMethodGetName, "il2cpp_method_get_name");
    RESOLVE(class_get_method_from_name, Il2CppClassGetMethodFromName, "il2cpp_class_get_method_from_name");
    RESOLVE(method_get_param_count, Il2CppMethodGetParamCount, "il2cpp_method_get_param_count");
    RESOLVE(method_get_param, Il2CppMethodGetParam, "il2cpp_method_get_param");
    RESOLVE(method_get_return_type, Il2CppMethodGetReturnType, "il2cpp_method_get_return_type");
    RESOLVE(method_is_instance, Il2CppMethodIsInstance, "il2cpp_method_is_instance");
    RESOLVE(type_get_name, Il2CppTypeGetName, "il2cpp_type_get_name");
    RESOLVE(class_get_fields, Il2CppClassGetFields, "il2cpp_class_get_fields");
    RESOLVE(field_get_flags, Il2CppFieldGetFlags, "il2cpp_field_get_flags");
    RESOLVE(field_get_name, Il2CppFieldGetName, "il2cpp_field_get_name");
    RESOLVE(field_get_type, Il2CppFieldGetType, "il2cpp_field_get_type");
    RESOLVE(field_get_offset, Il2CppFieldGetOffset, "il2cpp_field_get_offset");
    RESOLVE(field_static_get_value, Il2CppFieldStaticGetValue, "il2cpp_field_static_get_value");
    RESOLVE(type_get_class_or_element_class, Il2CppTypeGetClassOrElementClass, "il2cpp_type_get_class_or_element_class");
    RESOLVE(class_get_type, Il2CppClassGetType, "il2cpp_class_get_type");
    RESOLVE(type_get_object, Il2CppTypeGetObject, "il2cpp_type_get_object");
    RESOLVE(object_new, Il2CppObjectNew, "il2cpp_object_new");
    RESOLVE(runtime_object_init, Il2CppRuntimeObjectInit, "il2cpp_runtime_object_init");
    RESOLVE(runtime_invoke, Il2CppRuntimeInvoke, "il2cpp_runtime_invoke");
#undef RESOLVE

    const bool available = has_core_il2cpp_api(*api);
    if (!available) {
        const char *lookup_error = dlerror();
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "IL2CPP API lookup incomplete path=%s resolved=%u required=%u dlerror=%s",
            il2cpp_path,
            api->symbol_count,
            kRequiredIl2cppCoreSymbolCount,
            lookup_error == nullptr ? "none" : lookup_error
        );
    } else if (has_loaded_exports) {
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "IL2CPP API resolved through loaded ELF exports path=%s symbols=%u",
            il2cpp_path,
            api->symbol_count
        );
    }
    return available;
}

void close_il2cpp_api(Il2CppApi *api) {
    if (api != nullptr && api->owns_handle && api->handle != nullptr) {
        dlclose(api->handle);
    }
    if (api != nullptr) {
        api->handle = nullptr;
        api->owns_handle = false;
    }
}

char ascii_lower(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
}
