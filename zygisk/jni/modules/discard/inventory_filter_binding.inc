// Binds the static inventory filter delegate used by ListSortedPlayerInventory.

using Il2CppClassGetDeclaringType = Il2CppClass *(*)(Il2CppClass *);

Il2CppClassGetDeclaringType discard_lookup_class_get_declaring_type(const Il2CppApi &api) {
    if (api.handle == nullptr) return nullptr;
    return reinterpret_cast<Il2CppClassGetDeclaringType>(
        dlsym(api.handle, "il2cpp_class_get_declaring_type")
    );
}

bool discard_field_name_looks_like_inventory_filter(const char *field_name) {
    return field_name != nullptr &&
        (strcmp(field_name, "<>9__14_1") == 0 || strstr(field_name, "9__14_1") != nullptr);
}

bool discard_class_is_item_inventory_gui_service(const Il2CppApi &api, Il2CppClass *klass) {
    if (klass == nullptr || api.class_get_name == nullptr) return false;
    const char *class_name = api.class_get_name(klass);
    return class_name != nullptr && strcmp(class_name, "ItemInventoryGuiService") == 0;
}

bool discard_class_has_inventory_usable_filter_method(const Il2CppApi &api, Il2CppClass *klass) {
    return klass != nullptr && api.class_get_method_from_name != nullptr &&
        api.class_get_method_from_name(klass, "chvt", 1) != nullptr;
}

bool discard_nested_class_belongs_to_item_inventory_gui_service(
    const Il2CppApi &api,
    Il2CppClass *klass,
    Il2CppClass *gui_service_class
) {
    if (klass == nullptr || gui_service_class == nullptr) return false;
    Il2CppClassGetDeclaringType get_declaring = discard_lookup_class_get_declaring_type(api);
    if (get_declaring != nullptr && get_declaring(klass) == gui_service_class) {
        return true;
    }
    if (api.class_get_parent == nullptr) return false;
    for (uint32_t depth = 0U; depth < 4U; ++depth) {
        klass = api.class_get_parent(klass);
        if (klass == nullptr) break;
        if (klass == gui_service_class || discard_class_is_item_inventory_gui_service(api, klass)) {
            return true;
        }
    }
    return false;
}

const void *find_discard_inventory_filter_field(
    const Il2CppApi &api,
    Il2CppClass *klass
) {
    if (klass == nullptr || api.class_get_field_from_name == nullptr) return nullptr;
    const void *filter_field = api.class_get_field_from_name(klass, "<>9__14_1");
    if (filter_field != nullptr) return filter_field;
    if (api.class_get_fields == nullptr || api.field_get_name == nullptr ||
        api.field_get_flags == nullptr) {
        return nullptr;
    }
    void *iterator = nullptr;
    for (size_t field_index = 0U; field_index < 64U; ++field_index) {
        const void *field = api.class_get_fields(klass, &iterator);
        if (field == nullptr) break;
        if ((api.field_get_flags(field) & 0x0010U) == 0U) continue;
        const char *field_name = api.field_get_name(field);
        if (!discard_field_name_looks_like_inventory_filter(field_name)) continue;
        return field;
    }
    return nullptr;
}

Il2CppClass *find_item_inventory_gui_filter_host_class(
    const Il2CppApi &api,
    const Il2CppAssembly **assemblies,
    size_t assembly_count,
    Il2CppClass *gui_service_class
) {
    if (gui_service_class == nullptr || assemblies == nullptr ||
        api.assembly_get_image == nullptr || api.image_get_class_count == nullptr ||
        api.image_get_class == nullptr || api.class_get_name == nullptr) {
        return nullptr;
    }
    Il2CppClassGetDeclaringType get_declaring = discard_lookup_class_get_declaring_type(api);
    for (size_t assembly_index = 0U; assembly_index < assembly_count; ++assembly_index) {
        const Il2CppAssembly *assembly = assemblies[assembly_index];
        if (assembly == nullptr) continue;
        const Il2CppImage *image = api.assembly_get_image(assembly);
        if (image == nullptr) continue;
        const size_t class_count = api.image_get_class_count(image);
        for (size_t class_index = 0U; class_index < class_count; ++class_index) {
            Il2CppClass *klass = api.image_get_class(image, class_index);
            if (klass == nullptr) continue;
            const char *class_name = api.class_get_name(klass);
            if (class_name == nullptr || strcmp(class_name, "<>c") != 0) continue;
            if (!discard_class_has_inventory_usable_filter_method(api, klass)) continue;
            if (get_declaring != nullptr) {
                if (get_declaring(klass) != gui_service_class) continue;
            } else if (!discard_nested_class_belongs_to_item_inventory_gui_service(
                    api,
                    klass,
                    gui_service_class
                )) {
                continue;
            }
            if (find_discard_inventory_filter_field(api, klass) == nullptr) continue;
            return klass;
        }
    }
    return nullptr;
}

void discard_warmup_inventory_filter_host_class(
    const Il2CppApi &api,
    Il2CppClass *gui_service_class,
    Il2CppClass *host_class
) {
    if (api.runtime_class_init == nullptr || host_class == nullptr) return;
    if (gui_service_class != nullptr) {
        api.runtime_class_init(gui_service_class);
    }
    api.runtime_class_init(host_class);
    if (api.class_get_field_from_name == nullptr || api.field_static_get_value == nullptr) {
        return;
    }
    // The singleton constructor has already run; never initialize it twice.
}

#include "inventory_filter_factory.inc"

void *read_discard_inventory_filter_delegate(
    const Il2CppApi &api,
    Il2CppClass *host_class
) {
    if (host_class == nullptr || api.field_static_get_value == nullptr) return nullptr;
    const void *filter_field = find_discard_inventory_filter_field(api, host_class);
    if (filter_field == nullptr) return nullptr;
    void *delegate = nullptr;
    api.field_static_get_value(const_cast<void *>(filter_field), &delegate);
    if (delegate == nullptr) delegate = create_discard_inventory_filter(api, host_class, filter_field);
    return delegate;
}

bool bind_discard_inventory_filter_delegate_from_host_class(
    const Il2CppApi &api,
    Il2CppClass *gui_service_class,
    Il2CppClass *host_class,
    RuntimeBinding *binding
) {
    if (binding == nullptr || host_class == nullptr ||
        binding->item_inventory_filter_delegate != nullptr ||
        gui_service_class == nullptr ||
        !discard_nested_class_belongs_to_item_inventory_gui_service(
            api,
            host_class,
            gui_service_class
        ) ||
        find_discard_inventory_filter_field(api, host_class) == nullptr) {
        return false;
    }

    for (uint32_t attempt = 0U; attempt < 3U; ++attempt) {
        discard_warmup_inventory_filter_host_class(api, gui_service_class, host_class);
        void *delegate = read_discard_inventory_filter_delegate(api, host_class);
        if (delegate != nullptr) {
            binding->item_inventory_filter_delegate = delegate;
            __android_log_print(
                ANDROID_LOG_INFO,
                kLogTag,
                "managed diagnostic: discard inventory filter verified=1 class=%s delegate=%p attempt=%u",
                api.class_get_name(host_class),
                delegate,
                attempt + 1U
            );
            return true;
        }
    }

    __android_log_print(
        ANDROID_LOG_WARN,
        kLogTag,
        "managed diagnostic: discard inventory filter delegate null class=%s host=%p",
        api.class_get_name(host_class),
        host_class
    );
    return false;
}

void resolve_discard_inventory_filter_delegate(
    const Il2CppApi &api,
    const Il2CppAssembly **assemblies,
    size_t assembly_count,
    RuntimeBinding *binding
) {
    if (binding == nullptr || binding->item_inventory_filter_delegate != nullptr ||
        assemblies == nullptr) {
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
    if (gui_service_class == nullptr) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "managed diagnostic: discard inventory filter verified=0 gui_service=0x0"
        );
        return;
    }

    Il2CppClass *host_class = find_item_inventory_gui_filter_host_class(
        api,
        assemblies,
        assembly_count,
        gui_service_class
    );
    if (host_class == nullptr) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "managed diagnostic: discard inventory filter verified=0 host=0x0 gui_service=%p",
            gui_service_class
        );
        return;
    }

    if (bind_discard_inventory_filter_delegate_from_host_class(
            api,
            gui_service_class,
            host_class,
            binding
        )) {
        return;
    }

    __android_log_print(
        ANDROID_LOG_WARN,
        kLogTag,
        "managed diagnostic: discard inventory filter verified=0 gui_service=%p host=%p",
        gui_service_class,
        host_class
    );
}
