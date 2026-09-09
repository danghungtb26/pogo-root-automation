MainThreadActionOutcome direct_catch_runtime_on_main_thread(
    ProbeContext &context,
    uint64_t encounter_id
) {
    return soft_sfida_capture_runtime_on_main_thread(context, encounter_id);
}

void *find_runtime_pokestop_by_id(
    ProbeContext &context,
    const std::string &fort_id
) {
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    const Il2CppApi &api = context.il2cpp_api;
    if (fort_id.empty() || !binding.spin_verified ||
        binding.map_place_directory_pokestops == nullptr) return nullptr;
    std::vector<MapDictionaryEntry> entries;
    if (!enumerate_map_dictionary(
            api, binding.map_place_directory_pokestops, &entries)) return nullptr;
    for (const MapDictionaryEntry &entry : entries) {
        std::string candidate_id;
        if (!read_runtime_managed_ascii_string(entry.key_object, &candidate_id) ||
            candidate_id != fort_id) continue;
        if (runtime_object_is_exact_class(
                api, entry.value, "Niantic.Holoholo.Pokestop", "MapPokestop")) {
            return entry.value;
        }
    }
    return nullptr;
}

MainThreadActionOutcome spin_runtime_pokestop_on_main_thread(
    ProbeContext &context,
    const std::string &fort_id
) {
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    const Il2CppApi &api = context.il2cpp_api;
    if (fort_id.empty() || !binding.spin_verified || api.runtime_invoke == nullptr) {
        return MainThreadActionOutcome::kRejected;
    }
    void *pokestop = find_runtime_pokestop_by_id(context, fort_id);
    if (pokestop == nullptr) return MainThreadActionOutcome::kRejected;
    void *cooldown_boxed = invoke_runtime_method(
        api,
        binding.map_pokestop_get_is_cooling_down,
        pokestop,
        nullptr,
        "MapPokestop.get_IsCoolingDown"
    );
    bool cooling_down = false;
    if (!read_boxed_bool(api, cooldown_boxed, &cooling_down)) {
        return MainThreadActionOutcome::kRejected;
    }
    if (cooling_down) return MainThreadActionOutcome::kRejected;

    void *interactive = invoke_runtime_method(
        api,
        binding.map_pokestop_start_interactive,
        pokestop,
        nullptr,
        "MapPokestop.StartInteractiveMode"
    );
    if (!runtime_object_is_exact_class(
            api,
            interactive,
            "Niantic.Holoholo.Pokestop",
            "MapPokestopInteractive")) return MainThreadActionOutcome::kRejected;
    void *spinner = nullptr;
    if (!read_runtime_field(
            api,
            interactive,
            "ItemSpinner",
            &spinner
        ) || !runtime_object_is_exact_class(
            api, spinner, "Niantic.Holoholo.Map", "PoiItemSpinner")) {
        return MainThreadActionOutcome::kRejected;
    }
    float speed = 1.0F;
    void *arguments[] = {&speed};
    void *exception = nullptr;
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "runtime action invoking action=SPIN fort=%s pokestop=%p spinner=%p method=%p",
        fort_id.c_str(),
        pokestop,
        spinner,
        binding.poi_item_spinner_spin
    );
    api.runtime_invoke(binding.poi_item_spinner_spin, spinner, arguments, &exception);
    if (exception != nullptr) return MainThreadActionOutcome::kIndeterminate;
    return MainThreadActionOutcome::kInvoked;
}
