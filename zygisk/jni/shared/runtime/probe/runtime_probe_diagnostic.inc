void log_runtime_binding_readiness(
    ProbeContext &context,
    const RuntimeBinding &previous,
    const RuntimeBinding &binding
) {
    bool main_thread_ready = false;
    bool main_thread_pending = false;
    {
        std::lock_guard<std::mutex> lock(context.main_thread_bridge.mutex);
        main_thread_ready = context.main_thread_bridge.ready;
        main_thread_pending = context.main_thread_bridge.pending_token != 0U;
    }
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "runtime binding summary previous_generation=%llu generation=%llu exact_build=%d main_thread_ready=%d main_thread_pending=%d stale_descriptors_invalidated=%d",
        static_cast<unsigned long long>(previous.owner_generation),
        static_cast<unsigned long long>(binding.owner_generation),
        binding.exact_build_verified ? 1 : 0,
        main_thread_ready ? 1 : 0,
        main_thread_pending ? 1 : 0,
        binding.owner_generation != previous.owner_generation ? 1 : 0
    );
    const auto log_method = [](const char *name,
                               const pogo_runtime_method::VerifiedRuntimeMethod &descriptor,
                               bool owner,
                               bool invocation,
                               bool outcome) {
        __android_log_print(
            descriptor.verified() ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
            kLogTag,
            "runtime binding method=%s reason=%s token=0x%08x inspected=%u signatures=%u matches=%u owner_session=%d invocation=%d outcome=%d",
            name,
            pogo_runtime_method::verification_reason_name(descriptor.reason),
            descriptor.metadata_token,
            descriptor.inspected_count,
            descriptor.signature_match_count,
            descriptor.verified_match_count,
            owner ? 1 : 0,
            invocation ? 1 : 0,
            outcome ? 1 : 0
        );
    };
    log_method(
        "MapPokemon.TryCapture",
        binding.map_pokemon_try_capture_descriptor,
        binding.direct_catch_owner_session_verified,
        binding.direct_catch_invocation_available,
        binding.direct_catch_outcome_verified
    );
    log_method(
        "ItemBagImpl.GetItemCount",
        binding.item_bag_get_item_count_descriptor,
        binding.discard_owner_session_verified,
        binding.inventory_read_verified,
        true
    );
    log_method(
        "ItemBagImpl.RecycleItem",
        binding.item_bag_recycle_item_descriptor,
        binding.discard_owner_session_verified,
        binding.discard_invocation_available,
        binding.discard_outcome_verified
    );
    log_method(
        "PokemonBagImpl.GetPokemon",
        binding.pokemon_inventory_get_pokemon_descriptor,
        binding.transfer_owner_session_verified,
        binding.transfer_invocation_available,
        true
    );
    log_method(
        "PokemonBagImpl.ReleasePokemon",
        binding.pokemon_inventory_release_descriptor,
        binding.transfer_owner_session_verified,
        binding.transfer_invocation_available,
        binding.transfer_outcome_verified
    );
    log_runtime_object(context.il2cpp_api, binding.item_bag, "ItemBag owner", "ItemBagImpl");
    log_runtime_object(
        context.il2cpp_api,
        binding.pokemon_inventory,
        "PokemonBag owner",
        "PokemonBagImpl"
    );
}

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
    const pogo_runtime_profile::IdentityObservation identity =
        runtime_identity_snapshot(context);
    discover_runtime_owners(
        api,
        assemblies,
        assembly_count,
        context.process_name,
        context.il2cpp_path,
        identity,
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
    log_runtime_binding_readiness(context, previous, binding);
    pogo_runtime_world_state::reset(
        binding.owner_generation,
        static_cast<uint64_t>(now_elapsed_nanos())
    );
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
