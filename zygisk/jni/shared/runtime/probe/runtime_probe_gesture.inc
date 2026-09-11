[[maybe_unused]] void diagnose_tap_gesture_class(
    const Il2CppApi &api,
    const Il2CppAssembly **assemblies,
    size_t assembly_count
) {
    if (assemblies == nullptr || assembly_count == 0U) return;
    Il2CppClass *klass = find_runtime_class(
        api,
        assemblies,
        assembly_count,
        "TouchScript.Gestures",
        "TapGesture",
        nullptr
    );
    if (klass == nullptr) {
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "managed diagnostic: class missing TouchScript.Gestures.TapGesture"
        );
        return;
    }

    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "managed diagnostic: gesture class found TouchScript.Gestures.TapGesture ptr=%p",
        klass
    );
    log_runtime_class_fields(api, klass, "TapGesture");
    static const char *const method_names[] = {
        "Tap",
        "Commit",
        "Reset",
        "OnPointerDown",
        "OnPointerUp",
        "OnPointerClick",
        "OnEnable",
        "OnDisable",
        "Start",
        "Update",
        "get_TapTime",
        "set_TapTime",
        "get_NumberOfTaps",
        "set_NumberOfTaps",
    };
    for (const char *method_name : method_names) {
        log_runtime_method_signature(api, klass, "TapGesture", method_name);
    }
}

bool verify_tap_event_contract(
    const Il2CppApi &api,
    const Il2CppAssembly **assemblies,
    size_t assembly_count,
    RuntimeBinding *binding
) {
    if (binding == nullptr || assemblies == nullptr || assembly_count == 0U ||
        api.class_get_field_from_name == nullptr || api.class_get_method_from_name == nullptr ||
        api.method_get_param_count == nullptr || api.method_is_instance == nullptr) return false;
    Il2CppClass *tap_class = find_runtime_class(
        api, assemblies, assembly_count, "TouchScript.Gestures", "TapGesture", nullptr
    );
    Il2CppClass *event_args_class = find_runtime_class(
        api, assemblies, assembly_count, "System", "EventArgs", nullptr
    );
    const void *tapped_field = tap_class == nullptr
        ? nullptr : api.class_get_field_from_name(tap_class, "tappedInvoker");
    binding->map_pokemon_tap_event_verified = tap_class != nullptr && event_args_class != nullptr &&
        tapped_field != nullptr;
    __android_log_print(
        binding->map_pokemon_tap_event_verified ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "managed diagnostic: MapPokemon TapGesture event contract verified=%d gesture=%p field=%p event_args=%p",
        binding->map_pokemon_tap_event_verified ? 1 : 0,
        tap_class,
        tapped_field,
        event_args_class
    );
    return binding->map_pokemon_tap_event_verified;
}
