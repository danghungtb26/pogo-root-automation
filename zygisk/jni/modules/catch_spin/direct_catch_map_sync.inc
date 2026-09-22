// Register catch suppression, remove the live entity, then request fresh map cells.
namespace {
void refresh_map_after_direct_catch(
    ProbeContext &context,
    uint64_t encounter_id,
    uint32_t status
) {
    if (!direct_catch_status_removes_map_entity(status)) return;
    const Il2CppApi &api = context.il2cpp_api;
    RuntimeBinding binding = runtime_binding_snapshot(context);
    if (!binding.exact_build_verified) return;
    void *handler = current_map_content_handler(context, binding);
    bool entity_removed = false;
    if (handler != nullptr) {
        Il2CppClass *handler_class = api.object_get_class == nullptr
            ? nullptr : api.object_get_class(handler);
        const void *register_method = binding.map_content_handler_register_caught_or_fled;
        if (register_method == nullptr) {
            register_method = find_direct_map_instance_method(
                api, handler_class, "RegisterPokemonCaughtOrFled", 1U
            );
        }
        (void)invoke_map_refresh_method(
            api,
            register_method,
            handler,
            encounter_id,
            "MapContentHandler.RegisterPokemonCaughtOrFled"
        );
        if (register_method == nullptr) {
            __android_log_print(
                ANDROID_LOG_WARN,
                kLogTag,
                "runtime direct catch map sync register method unavailable handler=%p",
                handler
            );
        }
    }
    // RegisterPokemonCaughtOrFled only appends suppression history (0x7F54EBC).
    // It does not remove MapEntityCell's wild entry. Removal is a separate step.
    void *live_service = nullptr;
    if (handler != nullptr) (void)read_runtime_field(api, handler, "egft", &live_service);
    if (runtime_object_is_exact_class(api, live_service, "Niantic.Holoholo.Map", "MapEntityService")) {
        binding.map_entity_service = live_service;
    }
    if (runtime_object_is_exact_class(
            api,
            binding.map_entity_service,
            "Niantic.Holoholo.Map",
            "MapEntityService"
        )) {
        Il2CppClass *service_class = api.object_get_class(binding.map_entity_service);
        const void *remove_method = find_direct_map_instance_method(
            api, service_class, "RemoveWildPokemon", 1U
        );
        entity_removed = invoke_map_refresh_method(
            api,
            remove_method,
            binding.map_entity_service,
            encounter_id,
            "MapEntityService.RemoveWildPokemon"
        );
    }
    if (!entity_removed) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "runtime direct catch map sync could not remove encounter=%llu",
            static_cast<unsigned long long>(encounter_id)
        );
    }
    (void)refresh_runtime_map_after_action(context, "catch completed");
}
} // namespace
