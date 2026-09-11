[[maybe_unused]] void diagnose_managed_runtime_candidates(
    const Il2CppApi &api,
    const Il2CppAssembly **assemblies,
    size_t assembly_count,
    const ManagedDiagnosticClass *candidates,
    size_t candidate_count
) {
    if (candidates == nullptr) return;
    for (size_t candidate_index = 0U; candidate_index < candidate_count; ++candidate_index) {
        const ManagedDiagnosticClass &candidate = candidates[candidate_index];
        const Il2CppImage *candidate_image = nullptr;
        Il2CppClass *klass = find_runtime_class(
            api, assemblies, assembly_count, candidate.name_space, candidate.name, &candidate_image
        );
        if (klass == nullptr) {
            __android_log_print(
                ANDROID_LOG_INFO,
                kLogTag,
                "managed diagnostic: class missing %s.%s",
                candidate.name_space,
                candidate.name
            );
            continue;
        }
        const char *image_name = candidate_image == nullptr || api.image_get_name == nullptr
            ? "<unknown>" : api.image_get_name(candidate_image);
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "managed diagnostic: class found %s.%s image=%s ptr=%p",
            candidate.name_space,
            candidate.name,
            image_name == nullptr ? "<unknown>" : image_name,
            klass
        );
        log_runtime_class_fields(api, klass, candidate.name);
        for (size_t method_index = 0U; method_index < candidate.method_count; ++method_index) {
            log_runtime_method_signature(
                api, klass, candidate.name, candidate.methods[method_index]
            );
        }
        if (strcmp(candidate.name, "Object") == 0 || strcmp(candidate.name, "DiContainer") == 0) {
            log_runtime_method_overloads(
                api, klass, candidate.name, candidate.methods, candidate.method_count
            );
        }
    }
}
