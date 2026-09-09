bool refresh_runtime_encounter_owners(ProbeContext &context) {
    const Il2CppApi &api = context.il2cpp_api;
    if (!context.il2cpp_api_ready || api.domain_get == nullptr || api.thread_attach == nullptr ||
        api.thread_detach == nullptr || api.domain_get_assemblies == nullptr ||
        api.class_get_type == nullptr || api.type_get_object == nullptr ||
        api.runtime_invoke == nullptr) return false;

    Il2CppDomain *domain = api.domain_get();
    if (domain == nullptr) return false;
    Il2CppThread *thread = api.thread_attach(domain);
    if (thread == nullptr) return false;
    size_t assembly_count = 0U;
    const Il2CppAssembly **assemblies = api.domain_get_assemblies(domain, &assembly_count);
    if (assemblies == nullptr) {
        api.thread_detach(thread);
        return false;
    }

    const Il2CppImage *ignored_image = nullptr;
    Il2CppClass *project_class = find_runtime_class(
        api, assemblies, assembly_count, "Zenject", "ProjectContext", &ignored_image
    );
    Il2CppClass *container_class = find_runtime_class(
        api, assemblies, assembly_count, "Zenject", "DiContainer", &ignored_image
    );
    void *project = project_class == nullptr ? nullptr : invoke_runtime_method(
        api,
        api.class_get_method_from_name(project_class, "get_Instance", 0),
        nullptr,
        nullptr,
        "refresh ProjectContext.get_Instance"
    );
    void *container = project == nullptr || project_class == nullptr
        ? nullptr
        : invoke_runtime_method(
            api,
            api.class_get_method_from_name(project_class, "get_Container", 0),
            project,
            nullptr,
            "refresh ProjectContext.get_Container"
        );
    const void *try_resolve = container_class == nullptr ? nullptr :
        find_runtime_method_with_parameter(api, container_class, "TryResolve", "System.Type");
    if (container == nullptr || try_resolve == nullptr) {
        api.thread_detach(thread);
        return false;
    }

    RuntimeBinding refreshed = runtime_binding_snapshot(context);
    refreshed.encounter_pokemon = nullptr;
    refreshed.encounter_state = nullptr;
    refreshed.encounter_interaction = nullptr;
    refreshed.encounter_gui = nullptr;
    refreshed.pokeball_service = nullptr;
    refreshed.pokeball_service_throw = nullptr;
    refreshed.catch_throw_verified = false;
    bool found = false;
    struct EncounterService {
        const char *name_space;
        const char *name;
    };
    static const EncounterService services[] = {
        {"Niantic.Holoholo.Encounter", "IEncounterPokemon"},
        {"Niantic.Holoholo.Encounter", "IEncounterState"},
        {"Niantic.Holoholo.Encounter", "IEncounterInteractionState"},
        {"Niantic.Holoholo.Encounter", "IEncounterGuiController"},
        {"Niantic.Holoholo.Services", "IPokeballService"},
    };
    for (const EncounterService &service : services) {
        Il2CppClass *service_class = find_runtime_class(
            api, assemblies, assembly_count, service.name_space, service.name, nullptr
        );
        const Il2CppType *service_type = service_class == nullptr
            ? nullptr : api.class_get_type(service_class);
        void *type_object = service_type == nullptr ? nullptr : api.type_get_object(service_type);
        if (type_object == nullptr) continue;
        void *arguments[] = {type_object};
        void *owner = invoke_runtime_method(
            api, try_resolve, container, arguments, "refresh encounter TryResolve"
        );
        if (strcmp(service.name, "IEncounterPokemon") == 0 && runtime_object_is_exact_class(
                api, owner, "Niantic.Holoholo.Encounter", "EncounterPokemon")) {
            refreshed.encounter_pokemon = owner;
            found = true;
        } else if (strcmp(service.name, "IEncounterState") == 0 && runtime_object_is_exact_class(
                api, owner, "Niantic.Holoholo.Encounter", "EncounterState")) {
            refreshed.encounter_state = owner;
            found = true;
        } else if (strcmp(service.name, "IEncounterInteractionState") == 0 &&
            runtime_object_is_exact_class(
                api, owner, "Niantic.Holoholo.Encounter", "EncounterInteractionState"
            )) {
            refreshed.encounter_interaction = owner;
            found = true;
        } else if (strcmp(service.name, "IEncounterGuiController") == 0 &&
            runtime_object_is_exact_class(
                api, owner, "Niantic.Holoholo.Encounter", "EncounterGuiController"
            )) {
            refreshed.encounter_gui = owner;
            found = true;
        } else if (strcmp(service.name, "IPokeballService") == 0 && runtime_object_is_exact_class(
                api, owner, "Niantic.Holoholo.Services", "PokeballService")) {
            refreshed.pokeball_service = owner;
            refreshed.pokeball_service_throw = find_runtime_method_with_two_parameters(
                api,
                api.object_get_class(owner),
                "Throw",
                "UnityEngine.GameObject",
                "UnityEngine.Vector3"
            );
            refreshed.catch_throw_verified = refreshed.pokeball_service_throw != nullptr;
            found = true;
        }
    }
    api.thread_detach(thread);
    if (found) {
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "runtime encounter owners refreshed pokemon=%p state=%p interaction=%p ball_service=%p throw=%p",
            refreshed.encounter_pokemon,
            refreshed.encounter_state,
            refreshed.encounter_interaction,
            refreshed.pokeball_service,
            refreshed.pokeball_service_throw
        );
    }
    {
        std::lock_guard<std::mutex> lock(context.binding_mutex);
        context.binding.encounter_pokemon = refreshed.encounter_pokemon;
        context.binding.encounter_state = refreshed.encounter_state;
        context.binding.encounter_interaction = refreshed.encounter_interaction;
        context.binding.encounter_gui = refreshed.encounter_gui;
        context.binding.pokeball_service = refreshed.pokeball_service;
        context.binding.pokeball_service_throw = refreshed.pokeball_service_throw;
        context.binding.catch_throw_verified = refreshed.catch_throw_verified;
    }
    return found;
}
