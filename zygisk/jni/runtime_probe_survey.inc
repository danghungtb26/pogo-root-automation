[[maybe_unused]] bool survey_il2cpp_assemblies(
    Il2CppApi *api,
    RuntimeEvent *event,
    const Il2CppImage **csharp_image
) {
    if (api == nullptr || event == nullptr || csharp_image == nullptr ||
        api->domain_get == nullptr || api->thread_attach == nullptr ||
        api->thread_detach == nullptr || api->domain_get_assemblies == nullptr ||
        api->assembly_get_image == nullptr || api->image_get_name == nullptr) return false;

    Il2CppDomain *domain = api->domain_get();
    if (domain == nullptr) return false;
    Il2CppThread *thread = api->thread_attach(domain);
    if (thread == nullptr) return false;

    size_t assembly_count = 0U;
    const Il2CppAssembly **assemblies = api->domain_get_assemblies(domain, &assembly_count);
    if (assemblies == nullptr || assembly_count == 0U) {
        api->thread_detach(thread);
        return false;
    }

    event->assembly_count = assembly_count > 0xFFFFFFFFULL
        ? 0xFFFFFFFFU
        : static_cast<uint32_t>(assembly_count);
    const size_t inspect_count = assembly_count < kMaxAssembliesToInspect
        ? assembly_count
        : kMaxAssembliesToInspect;
    for (size_t index = 0U; index < inspect_count; ++index) {
        const Il2CppAssembly *assembly = assemblies[index];
        if (assembly == nullptr) continue;
        const Il2CppImage *image = api->assembly_get_image(assembly);
        if (image == nullptr) continue;
        const char *name = api->image_get_name(image);
        if (name == nullptr) continue;
        if (strcmp(name, "Assembly-CSharp.dll") == 0 || strcmp(name, "Assembly-CSharp") == 0) {
            event->probe_flags |= kAssemblyCSharpFound;
            copy_string(event->assembly_csharp_name, sizeof(event->assembly_csharp_name), name);
            *csharp_image = image;
            break;
        }
    }

    api->thread_detach(thread);
    event->probe_flags |= kAssemblySurveyComplete;
    return true;
}

[[maybe_unused]] void survey_candidate_classes(
    Il2CppApi *api,
    const Il2CppImage *image,
    RuntimeEvent *event
) {
    if (api == nullptr || image == nullptr || event == nullptr || !has_class_survey_api(*api)) return;

    const size_t class_count = api->image_get_class_count(image);
    event->class_count = class_count > 0xFFFFFFFFULL
        ? 0xFFFFFFFFU
        : static_cast<uint32_t>(class_count);
    const size_t inspect_count = class_count < kMaxClassesToInspect
        ? class_count
        : kMaxClassesToInspect;

    for (size_t index = 0U; index < inspect_count &&
         event->candidate_class_count < kMaxCandidateClasses; ++index) {
        Il2CppClass *klass = api->image_get_class(image, index);
        if (klass == nullptr) continue;
        const char *name = api->class_get_name(klass);
        const char *name_space = api->class_get_namespace(klass);
        if (name != nullptr && is_candidate_class(name, name_space)) {
            append_candidate(event, name_space, name);
            survey_candidate_methods(api, klass, name_space, name, event);
        }
    }
    event->probe_flags |= kClassSurveyComplete;
}
