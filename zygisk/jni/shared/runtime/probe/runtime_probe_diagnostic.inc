bool run_managed_runtime_diagnostic(ProbeContext &context) {
    if (!context.il2cpp_api_ready) {
        __android_log_print(ANDROID_LOG_WARN, kLogTag, "runtime bootstrap: IL2CPP API unavailable");
        return false;
    }

    const Il2CppApi &api = context.il2cpp_api;
    if (!has_runtime_inspection_api(api) || api.domain_get_assemblies == nullptr) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "runtime bootstrap: inspection API incomplete resolved=%u",
            api.symbol_count
        );
        return false;
    }

    Il2CppDomain *domain = api.domain_get();
    if (domain == nullptr) {
        __android_log_print(ANDROID_LOG_WARN, kLogTag, "runtime bootstrap: domain unavailable");
        return false;
    }
    Il2CppThread *thread = api.thread_attach(domain);
    if (thread == nullptr) {
        __android_log_print(ANDROID_LOG_WARN, kLogTag, "runtime bootstrap: thread attach failed");
        return false;
    }

    size_t assembly_count = 0U;
    const Il2CppAssembly **assemblies = api.domain_get_assemblies(domain, &assembly_count);
    if (assemblies == nullptr || assembly_count == 0U) {
        api.thread_detach(thread);
        __android_log_print(ANDROID_LOG_WARN, kLogTag, "runtime bootstrap: assemblies unavailable");
        return false;
    }

    // Preserve the physical hook state across a re-bootstrap. The owner
    // discovery function intentionally rebuilds RuntimeBinding from scratch,
    // while inline hooks remain installed in the process until it exits.
    const RuntimeBinding previous = runtime_binding_snapshot(context);
    RuntimeBinding binding = previous;
    discover_runtime_owners(
        api,
        assemblies,
        assembly_count,
        context.il2cpp_path,
        &binding
    );
    binding.throw_hooks_installed = previous.throw_hooks_installed;
    const bool throw_methods_bound = bind_runtime_throw_methods(
        api,
        assemblies,
        assembly_count,
        &binding
    );

    {
        std::lock_guard<std::mutex> binding_lock(context.binding_mutex);
        context.binding = binding;
    }
    api.thread_detach(thread);

    // Scene services are often created after the Zenject owners inspected
    // above. Resolve the map owner through the main-thread bridge while no
    // feature observer is active, so the first module start can read nearby.
    if (!binding.map_entity_read_verified) {
        discover_scene_runtime_owners(context);
        binding = runtime_binding_snapshot(context);
    }

    const bool ready = binding.exact_build_verified && binding.encounter_read_verified;
    __android_log_print(
        ready ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "runtime bootstrap: ready=%d exact_build=%d encounter_read=%d map_read=%d throw_bound=%d hooks=%d assemblies=%zu",
        ready ? 1 : 0,
        binding.exact_build_verified ? 1 : 0,
        binding.encounter_read_verified ? 1 : 0,
        binding.map_entity_read_verified ? 1 : 0,
        throw_methods_bound ? 1 : 0,
        binding.throw_hooks_installed ? 1 : 0,
        assembly_count
    );
    return ready;
}
