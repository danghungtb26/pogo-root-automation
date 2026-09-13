// 0.427.0: ItemInventoryGuiService.<>c.chvt is the inventory's usable filter.
// Its compiler-generated delegate cache is lazy: class initialization alone
// never populates <>9__14_1. Bind the method without opening inventory UI.
Il2CppGcHandle g_discard_owned_filter_handle = 0U;
bool is_probable_managed_pointer(const void *value);

void *create_discard_inventory_filter(
    const Il2CppApi &api, Il2CppClass *host_class, const void *filter_field
) {
    if (api.domain_get == nullptr || api.domain_get_assemblies == nullptr ||
        api.field_get_type == nullptr || api.type_get_object == nullptr ||
        api.class_get_methods == nullptr || api.method_get_name == nullptr ||
        api.method_get_param_count == nullptr || api.method_get_param == nullptr ||
        api.type_get_name == nullptr || api.runtime_invoke == nullptr ||
        api.string_new == nullptr || api.gchandle_new == nullptr ||
        api.gchandle_get_target == nullptr) return nullptr;
    if (g_discard_owned_filter_handle != 0U) {
        return api.gchandle_get_target(g_discard_owned_filter_handle);
    }
    const void *singleton_field = api.class_get_field_from_name(host_class, "<>9");
    void *singleton = nullptr;
    if (singleton_field == nullptr || api.field_static_get_value == nullptr) return nullptr;
    api.field_static_get_value(const_cast<void *>(singleton_field), &singleton);
    if (!is_probable_managed_pointer(singleton)) return nullptr;
    const Il2CppType *filter_type = api.field_get_type(filter_field);
    if (filter_type == nullptr) return nullptr;
    void *delegate_type = api.type_get_object(filter_type);
    if (!is_probable_managed_pointer(delegate_type)) return nullptr;
    size_t count = 0U;
    Il2CppDomain *domain = api.domain_get();
    if (domain == nullptr) return nullptr;
    const Il2CppAssembly **assemblies = api.domain_get_assemblies(domain, &count);
    if (assemblies == nullptr) return nullptr;
    Il2CppClass *delegate_class = find_runtime_class(
        api, assemblies, count, "System", "Delegate", nullptr);
    if (delegate_class == nullptr) return nullptr;
    const char *expected[] = {
        "System.Type", "System.Object", "System.String", "System.Boolean", "System.Boolean"
    };
    void *iterator = nullptr;
    const void *factory = nullptr;
    for (const void *method = api.class_get_methods(delegate_class, &iterator);
         method != nullptr; method = api.class_get_methods(delegate_class, &iterator)) {
        const char *name = api.method_get_name(method);
        if (name == nullptr || strcmp(name, "CreateDelegate") != 0 ||
            api.method_get_param_count(method) != 5U) continue;
        bool matches = true;
        for (uint32_t index = 0; index < 5U; ++index) {
            const auto *parameter = api.method_get_param(method, index);
            const char *type = parameter == nullptr ? nullptr : api.type_get_name(parameter);
            matches = matches && type != nullptr && strcmp(type, expected[index]) == 0;
        }
        if (matches) { factory = method; break; }
    }
    if (factory == nullptr) return nullptr;
    void *method_name = api.string_new("chvt");
    uint8_t ignore_case = 0U;
    uint8_t throw_on_failure = 1U;
    void *arguments[] = {delegate_type, singleton, method_name, &ignore_case, &throw_on_failure};
    void *exception = nullptr;
    void *delegate = api.runtime_invoke(factory, nullptr, arguments, &exception);
    if (exception != nullptr || !is_probable_managed_pointer(delegate)) return nullptr;
    // Process-scoped root; the delegate is stateless and survives host STOP/START.
    g_discard_owned_filter_handle = api.gchandle_new(delegate, false);
    return g_discard_owned_filter_handle == 0U ? nullptr : delegate;
}
