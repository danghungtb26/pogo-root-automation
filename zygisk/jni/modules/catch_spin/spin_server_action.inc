// Invokes the server-side PokéStop search for the exact 0.427.0 binding.
// PoiItemSpinner.Spin(float) is intentionally not used here: it only applies
// Unity physics and waits for a gesture threshold before the game calls ckko().

constexpr size_t kPoiItemSpinnerMapPlaceOffset = 0x88U;
constexpr size_t kPoiItemSpinnerRpcHandlerOffset = 0xC0U;

MainThreadActionOutcome spin_runtime_pokestop_on_main_thread(
    ProbeContext &context,
    const std::string &fort_id
) {
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    const Il2CppApi &api = context.il2cpp_api;
    if (fort_id.empty() || !binding.exact_build_verified || !binding.spin_verified ||
        binding.poi_item_spinner_request_items == nullptr ||
        api.runtime_invoke == nullptr) {
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
    if (cooling_down) {
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "native spin rejected fort=%s reason=game_cooldown",
            fort_id.c_str()
        );
        return MainThreadActionOutcome::kRejected;
    }

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
            "MapPokestopInteractive")) {
        return MainThreadActionOutcome::kRejected;
    }
    RuntimeSpinSetupGuard interaction{context, pokestop, interactive};
    void *spinner = nullptr;
    if (!read_runtime_field(api, interactive, "ItemSpinner", &spinner) ||
        !runtime_object_is_exact_class(
            api, spinner, "Niantic.Holoholo.Map", "PoiItemSpinner")) {
        return MainThreadActionOutcome::kRejected;
    }

    auto read_spinner_dependency = [spinner](size_t offset) {
        void *value = nullptr;
        memcpy(&value, static_cast<const uint8_t *>(spinner) + offset, sizeof(value));
        return value;
    };
    void *map_place = read_spinner_dependency(kPoiItemSpinnerMapPlaceOffset);
    void *rpc_handler = read_spinner_dependency(kPoiItemSpinnerRpcHandlerOffset);
    if (!is_probable_managed_pointer(map_place) ||
        !is_probable_managed_pointer(rpc_handler)) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native spin rejected fort=%s reason=spinner_dependencies_unavailable spinner=%p map_place=%p rpc=%p",
            fort_id.c_str(),
            spinner,
            map_place,
            rpc_handler
        );
        return MainThreadActionOutcome::kRejected;
    }
    if (!begin_pending_runtime_spin(context, fort_id, nullptr)) {
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "native spin rejected fort=%s reason=another_spin_pending",
            fort_id.c_str()
        );
        return MainThreadActionOutcome::kRejected;
    }

    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "runtime action invoking action=FORT_SEARCH fort=%s pokestop=%p spinner=%p ckko=%p rpc=%p",
        fort_id.c_str(),
        pokestop,
        spinner,
        binding.poi_item_spinner_request_items,
        rpc_handler
    );
    void *exception = nullptr;
    void *promise = api.runtime_invoke(
        binding.poi_item_spinner_request_items,
        spinner,
        nullptr,
        &exception
    );
    if (exception != nullptr || !is_probable_managed_pointer(promise)) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native spin FortSearch invoke failed fort=%s promise=%p exception=%p",
            fort_id.c_str(),
            promise,
            exception
        );
        abandon_pending_runtime_spin(context, "fort_search_invoke_failed");
        return exception == nullptr
            ? MainThreadActionOutcome::kRejected
            : MainThreadActionOutcome::kIndeterminate;
    }
    if (!retain_pending_runtime_spin(context, fort_id, spinner, promise)) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "native spin FortSearch Promise retention failed fort=%s promise=%p",
            fort_id.c_str(),
            promise
        );
        abandon_pending_runtime_spin(context, "fort_search_promise_retention_failed");
        return MainThreadActionOutcome::kIndeterminate;
    }
    interaction.retained = true;
    return MainThreadActionOutcome::kInvoked;
}
