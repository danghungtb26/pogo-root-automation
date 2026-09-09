void *find_unity_object_on_main_thread(
    ProbeContext &context,
    const char *name_space,
    const char *class_name
) {
    const Il2CppApi &api = context.il2cpp_api;
    if (!has_runtime_inspection_api(api) || api.domain_get_assemblies == nullptr ||
        name_space == nullptr || class_name == nullptr) return nullptr;
    Il2CppDomain *domain = api.domain_get();
    size_t assembly_count = 0U;
    const Il2CppAssembly **assemblies = domain == nullptr
        ? nullptr
        : api.domain_get_assemblies(domain, &assembly_count);
    if (assemblies == nullptr) return nullptr;
    Il2CppClass *object_class = find_runtime_class(
        api, assemblies, assembly_count, "UnityEngine", "Object", nullptr
    );
    Il2CppClass *target_class = find_runtime_class(
        api, assemblies, assembly_count, name_space, class_name, nullptr
    );
    if (object_class == nullptr || target_class == nullptr || api.class_get_type == nullptr ||
        api.type_get_object == nullptr) return nullptr;
    const Il2CppType *target_type = api.class_get_type(target_class);
    void *type_object = target_type == nullptr ? nullptr : api.type_get_object(target_type);
    const void *find_method = find_runtime_method_with_parameter(
        api, object_class, "FindObjectOfType", "System.Type"
    );
    if (type_object == nullptr || find_method == nullptr) return nullptr;
    void *arguments[] = {type_object};
    return invoke_runtime_method(api, find_method, nullptr, arguments, "FindObjectOfType.main");
}
