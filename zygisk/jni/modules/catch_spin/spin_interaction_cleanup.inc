// Match the spinner before completing the interactive mode. A player's newly
// opened mode must never be closed by an older automation result.
namespace {
bool complete_runtime_spin_interaction(ProbeContext &context, void *pokestop, void *interactive) {
    const Il2CppApi &api = context.il2cpp_api;
    if (!runtime_binding_snapshot(context).exact_build_verified ||
        !runtime_object_is_exact_class(api, pokestop, "Niantic.Holoholo.Pokestop", "MapPokestop") ||
        !runtime_object_is_exact_class(api, interactive, "Niantic.Holoholo.Pokestop", "MapPokestopInteractive") ||
        api.object_get_class == nullptr) return false;
    void *active = nullptr;
    if (!read_runtime_field(api, pokestop, "<ActiveMode>k__BackingField", &active) ||
        active != interactive) return false;
    const void *complete = find_direct_map_instance_method(api, api.object_get_class(pokestop),
        "CompleteInteractiveMode", 0U);
    return invoke_map_refresh_no_args(api, complete, pokestop, "MapPokestop.CompleteInteractiveMode");
}

void complete_runtime_spin_for_spinner(ProbeContext &context, const std::string &fort_id,
                                     Il2CppGcHandle spinner_handle) {
    const Il2CppApi &api = context.il2cpp_api;
    if (spinner_handle == 0U || api.gchandle_get_target == nullptr) return;
    void *spinner = api.gchandle_get_target(spinner_handle);
    if (!runtime_object_is_exact_class(api, spinner, "Niantic.Holoholo.Map", "PoiItemSpinner")) return;
    void *pokestop = find_runtime_pokestop_by_id(context, fort_id);
    void *active = nullptr;
    void *active_spinner = nullptr;
    if (pokestop == nullptr ||
        !read_runtime_field(api, pokestop, "<ActiveMode>k__BackingField", &active) ||
        !runtime_object_is_exact_class(api, active, "Niantic.Holoholo.Pokestop", "MapPokestopInteractive") ||
        !read_runtime_field(api, active, "ItemSpinner", &active_spinner) || active_spinner != spinner) return;
    (void)complete_runtime_spin_interaction(context, pokestop, active);
}

struct RuntimeSpinSetupGuard {
    ProbeContext &context;
    void *pokestop;
    void *interactive;
    bool retained = false;
    ~RuntimeSpinSetupGuard() {
        if (!retained) (void)complete_runtime_spin_interaction(context, pokestop, interactive);
    }
};
} // namespace
