// Resolves System.Collections.Generic.HashSet<Holoholo.Rpc.Item> for RecycleItem's
// expiringItemsCopy argument when IItemInventoryService is unavailable on the map.

using Il2CppClassInflate = Il2CppClass *(*)(Il2CppClass *, const Il2CppType **, uint32_t);

Il2CppClassInflate discard_lookup_class_inflate(const Il2CppApi &api) {
    if (api.handle == nullptr) return nullptr;
    return reinterpret_cast<Il2CppClassInflate>(dlsym(api.handle, "il2cpp_class_inflate"));
}

bool discard_type_name_references_item(const Il2CppApi &api, Il2CppClass *klass) {
    if (klass == nullptr || api.class_get_type == nullptr || api.type_get_name == nullptr) {
        return false;
    }
    const Il2CppType *type = api.class_get_type(klass);
    const char *type_name = type == nullptr ? nullptr : api.type_get_name(type);
    if (type_name == nullptr) return false;
    return strstr(type_name, "Holoholo.Rpc.Item") != nullptr ||
        strstr(type_name, "HashSet<Item>") != nullptr ||
        (strstr(type_name, "HashSet") != nullptr && strstr(type_name, "Item") != nullptr);
}

bool discard_class_looks_like_hash_set_item(const Il2CppApi &api, Il2CppClass *klass) {
    if (klass == nullptr || api.class_get_name == nullptr) return false;
    const char *class_name = api.class_get_name(klass);
    if (class_name == nullptr || strstr(class_name, "HashSet") == nullptr) return false;
    return discard_type_name_references_item(api, klass) || strstr(class_name, "Item") != nullptr;
}

bool bind_discard_hash_set_item_class(
    const Il2CppApi &api,
    Il2CppClass *klass,
    RuntimeBinding *binding
) {
    if (binding == nullptr || klass == nullptr || binding->hash_set_item_class != nullptr ||
        api.class_get_method_from_name == nullptr) return false;
    const void *ctor = api.class_get_method_from_name(klass, ".ctor", 0);
    if (ctor == nullptr) return false;
    binding->hash_set_item_class = klass;
    binding->hash_set_item_ctor = ctor;
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "managed diagnostic: discard hash set verified=1 class=%p ctor=%p name=%s",
        klass,
        ctor,
        api.class_get_name(klass)
    );
    return true;
}

void resolve_discard_hash_set_item_supplier(
    const Il2CppApi &api,
    const Il2CppAssembly **assemblies,
    size_t assembly_count,
    RuntimeBinding *binding
) {
    if (binding == nullptr || binding->hash_set_item_supplier != nullptr ||
        api.class_get_method_from_name == nullptr) {
        return;
    }
    Il2CppClass *reroll_class = find_runtime_class(
        api,
        assemblies,
        assembly_count,
        "Niantic.Holoholo.UI.Pokemon",
        "RequiredMoveRerollPromptService",
        nullptr
    );
    if (reroll_class == nullptr) return;
    const void *supplier = api.class_get_method_from_name(reroll_class, "cfrw", 1);
    if (supplier == nullptr) return;
    binding->hash_set_item_supplier = supplier;
    if (binding->hash_set_item_class == nullptr && api.method_get_return_type != nullptr &&
        api.type_get_class_or_element_class != nullptr) {
        const Il2CppType *return_type = api.method_get_return_type(supplier);
        Il2CppClass *return_class = return_type == nullptr
            ? nullptr
            : api.type_get_class_or_element_class(return_type);
        if (return_class != nullptr) {
            binding->hash_set_item_class = return_class;
            binding->hash_set_item_ctor = api.class_get_method_from_name(return_class, ".ctor", 0);
        }
    }
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "managed diagnostic: discard hash set supplier verified=1 supplier=%p class=%p ctor=%p",
        supplier,
        binding->hash_set_item_class,
        binding->hash_set_item_ctor
    );
}

void resolve_discard_hash_set_item_binding(
    const Il2CppApi &api,
    const Il2CppAssembly **assemblies,
    size_t assembly_count,
    RuntimeBinding *binding
) {
    if (binding == nullptr || binding->hash_set_item_class != nullptr ||
        api.image_get_class_count == nullptr || api.image_get_class == nullptr ||
        api.assembly_get_image == nullptr || assemblies == nullptr) {
        return;
    }

    resolve_discard_hash_set_item_supplier(api, assemblies, assembly_count, binding);
    if (binding->hash_set_item_class != nullptr && binding->hash_set_item_ctor != nullptr) return;

    Il2CppClass *item_class = find_runtime_class(
        api, assemblies, assembly_count, "Holoholo.Rpc", "Item", nullptr
    );
    Il2CppClassInflate class_inflate = discard_lookup_class_inflate(api);
    if (item_class != nullptr && class_inflate != nullptr && api.class_get_type != nullptr) {
        Il2CppClass *hash_set_generic = find_runtime_class(
            api, assemblies, assembly_count, "System.Collections.Generic", "HashSet`1", nullptr
        );
        if (hash_set_generic != nullptr) {
            const Il2CppType *item_type = api.class_get_type(item_class);
            const Il2CppType *type_args[] = {item_type};
            Il2CppClass *inflated = class_inflate(hash_set_generic, type_args, 1U);
            if (bind_discard_hash_set_item_class(api, inflated, binding)) return;
        }
    }

    constexpr size_t kMaxClasses = 100000U;
    size_t scanned_classes = 0U;
    for (size_t assembly_index = 0U; assembly_index < assembly_count &&
         scanned_classes < kMaxClasses; ++assembly_index) {
        const Il2CppAssembly *assembly = assemblies[assembly_index];
        if (assembly == nullptr) continue;
        const Il2CppImage *image = api.assembly_get_image(assembly);
        if (image == nullptr) continue;
        const size_t class_count = api.image_get_class_count(image);
        for (size_t class_index = 0U; class_index < class_count &&
             scanned_classes < kMaxClasses; ++class_index) {
            Il2CppClass *klass = api.image_get_class(image, class_index);
            ++scanned_classes;
            if (!discard_class_looks_like_hash_set_item(api, klass)) continue;
            if (bind_discard_hash_set_item_class(api, klass, binding)) return;
        }
    }
    __android_log_print(
        ANDROID_LOG_WARN,
        kLogTag,
        "managed diagnostic: discard hash set verified=0 scanned_classes=%zu inflate=%p item=%p "
        "supplier=%p",
        scanned_classes,
        class_inflate,
        item_class,
        binding->hash_set_item_supplier
    );
}
