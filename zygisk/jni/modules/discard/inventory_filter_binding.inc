// Binds the static inventory filter delegate used by ListSortedPlayerInventory.

bool discard_field_name_looks_like_inventory_filter(const char *field_name) {
    return field_name != nullptr &&
        (strcmp(field_name, "<>9__14_1") == 0 || strstr(field_name, "9__14_1") != nullptr);
}

bool discard_class_is_item_inventory_gui_service(const Il2CppApi &api, Il2CppClass *klass) {
    if (klass == nullptr || api.class_get_name == nullptr) return false;
    const char *class_name = api.class_get_name(klass);
    return class_name != nullptr && strcmp(class_name, "ItemInventoryGuiService") == 0;
}

bool discard_class_is_item_inventory_gui_filter_host(const Il2CppApi &api, Il2CppClass *klass) {
    if (klass == nullptr || api.class_get_name == nullptr) return false;
    const char *class_name = api.class_get_name(klass);
    if (class_name != nullptr && strstr(class_name, "ItemInventoryGuiService") != nullptr) {
        return true;
    }
    if (class_name == nullptr || strcmp(class_name, "<>c") != 0 ||
        api.class_get_parent == nullptr) {
        return false;
    }
    for (uint32_t depth = 0U; depth < 4U; ++depth) {
        klass = api.class_get_parent(klass);
        if (klass == nullptr) break;
        if (discard_class_is_item_inventory_gui_service(api, klass)) return true;
    }
    return false;
}

bool discard_filter_field_type_looks_like_inventory_usable_filter(
    const Il2CppApi &api,
    const void *field
) {
    if (field == nullptr || api.field_get_type == nullptr || api.type_get_name == nullptr) {
        return false;
    }
    const Il2CppType *field_type = api.field_get_type(field);
    const char *type_name = field_type == nullptr ? nullptr : api.type_get_name(field_type);
    return type_name != nullptr &&
        strstr(type_name, "ValueTuple") != nullptr &&
        strstr(type_name, "Item") != nullptr &&
        strstr(type_name, "HoloItemType") != nullptr;
}

bool bind_discard_inventory_filter_delegate_from_class(
    const Il2CppApi &api,
    Il2CppClass *klass,
    RuntimeBinding *binding
) {
    if (binding == nullptr || klass == nullptr || binding->item_inventory_filter_delegate != nullptr ||
        !discard_class_is_item_inventory_gui_filter_host(api, klass) ||
        api.class_get_field_from_name == nullptr || api.field_static_get_value == nullptr) {
        return false;
    }
    const void *filter_field = api.class_get_field_from_name(klass, "<>9__14_1");
    if (filter_field == nullptr && api.class_get_fields != nullptr &&
        api.field_get_name != nullptr && api.field_get_flags != nullptr) {
        void *iterator = nullptr;
        for (size_t field_index = 0U; field_index < 64U; ++field_index) {
            const void *field = api.class_get_fields(klass, &iterator);
            if (field == nullptr) break;
            if ((api.field_get_flags(field) & 0x0010U) == 0U) continue;
            const char *field_name = api.field_get_name(field);
            if (!discard_field_name_looks_like_inventory_filter(field_name)) continue;
            filter_field = field;
            break;
        }
    }
    if (filter_field == nullptr || !discard_filter_field_type_looks_like_inventory_usable_filter(
            api,
            filter_field
        )) {
        return false;
    }
    if (api.runtime_class_init != nullptr) {
        api.runtime_class_init(klass);
    }
    void *delegate = nullptr;
    api.field_static_get_value(const_cast<void *>(filter_field), &delegate);
    if (delegate == nullptr) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "managed diagnostic: discard inventory filter delegate null class=%s",
            api.class_get_name(klass)
        );
        return false;
    }
    binding->item_inventory_filter_delegate = delegate;
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "managed diagnostic: discard inventory filter verified=1 class=%s delegate=%p",
        api.class_get_name(klass),
        delegate
    );
    return true;
}

void resolve_discard_inventory_filter_delegate(
    const Il2CppApi &api,
    const Il2CppAssembly **assemblies,
    size_t assembly_count,
    RuntimeBinding *binding
) {
    if (binding == nullptr || binding->item_inventory_filter_delegate != nullptr ||
        api.assembly_get_image == nullptr || api.image_get_class_count == nullptr ||
        api.image_get_class == nullptr || assemblies == nullptr) {
        return;
    }

    Il2CppClass *gui_service_class = find_runtime_class(
        api,
        assemblies,
        assembly_count,
        "Niantic.Holoholo.UI.Items",
        "ItemInventoryGuiService",
        nullptr
    );
    if (gui_service_class != nullptr && api.runtime_class_init != nullptr) {
        api.runtime_class_init(gui_service_class);
    }

    static const char *kFilterClassNames[] = {
        "ItemInventoryGuiService/<>c",
        "ItemInventoryGuiService___c",
        "<>c",
        "<ItemInventoryGuiService>c",
        nullptr,
    };
    static const char *kFilterNamespaces[] = {
        "",
        "Niantic.Holoholo.UI.Items",
        nullptr,
    };
    for (size_t ns_index = 0U; kFilterNamespaces[ns_index] != nullptr; ++ns_index) {
        for (size_t name_index = 0U; kFilterClassNames[name_index] != nullptr; ++name_index) {
            Il2CppClass *filter_class = find_runtime_class(
                api,
                assemblies,
                assembly_count,
                kFilterNamespaces[ns_index],
                kFilterClassNames[name_index],
                nullptr
            );
            if (bind_discard_inventory_filter_delegate_from_class(api, filter_class, binding)) return;
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
            if (klass == nullptr || !discard_class_is_item_inventory_gui_filter_host(api, klass)) {
                continue;
            }
            if (bind_discard_inventory_filter_delegate_from_class(api, klass, binding)) return;
        }
    }
    __android_log_print(
        ANDROID_LOG_WARN,
        kLogTag,
        "managed diagnostic: discard inventory filter verified=0 scanned_classes=%zu",
        scanned_classes
    );
}
