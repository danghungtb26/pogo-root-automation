struct DirectMapPokeballThrow {
    int32_t ball_type = 1;
    float reticle_size = 0.0F;
    uint8_t hit_bullseye = 0U;
    uint8_t spinning = 0U;
    uint8_t missed = 0U;
    uint8_t reserved = 0U;
};

static_assert(offsetof(DirectMapPokeballThrow, ball_type) == 0U);
static_assert(offsetof(DirectMapPokeballThrow, reticle_size) == 4U);
static_assert(offsetof(DirectMapPokeballThrow, hit_bullseye) == 8U);
static_assert(offsetof(DirectMapPokeballThrow, spinning) == 9U);
static_assert(offsetof(DirectMapPokeballThrow, missed) == 10U);
static_assert(sizeof(DirectMapPokeballThrow) == 12U);

// This is the stable prefix of IL2CPP's MethodInfo layout from the verified
// reverse dump. Keep this local to the direct-catch diagnostic path so the
// shared native prelude stays below the repository's 500-line limit.
struct DirectRuntimeMethodInfoView {
    const void *method_pointer = nullptr;
    const void *virtual_method_pointer = nullptr;
    const void *invoker_method = nullptr;
    const char *name = nullptr;
    Il2CppClass *klass = nullptr;
    const void *return_type = nullptr;
    const void *parameters = nullptr;
    const void *rgctx_data = nullptr;
    const void *generic_method = nullptr;
    uint32_t token = 0U;
    uint16_t flags = 0U;
    uint16_t iflags = 0U;
    uint16_t slot = 0U;
    uint8_t parameters_count = 0U;
    uint8_t bitflags = 0U;
};

static_assert(offsetof(DirectRuntimeMethodInfoView, method_pointer) == 0U);
static_assert(offsetof(DirectRuntimeMethodInfoView, rgctx_data) == 56U);
static_assert(offsetof(DirectRuntimeMethodInfoView, generic_method) == 64U);
static_assert(offsetof(DirectRuntimeMethodInfoView, token) == 72U);

void log_direct_capture_method_metadata(const void *method) {
    if (method == nullptr) return;
    const auto *view = reinterpret_cast<const DirectRuntimeMethodInfoView *>(method);
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "direct map catch method metadata method=%p code=%p virtual=%p invoker=%p name=%p klass=%p return=%p params=%p rgctx=%p generic=%p token=0x%08x flags=0x%04x iflags=0x%04x slot=%u params_count=%u bits=0x%02x",
        method,
        view->method_pointer,
        view->virtual_method_pointer,
        view->invoker_method,
        view->name,
        view->klass,
        view->return_type,
        view->parameters,
        view->rgctx_data,
        view->generic_method,
        view->token,
        view->flags,
        view->iflags,
        view->slot,
        view->parameters_count,
        view->bitflags
    );
}

MainThreadActionOutcome direct_catch_runtime_on_main_thread(
    ProbeContext &context,
    uint64_t encounter_id
) {
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    const Il2CppApi &api = context.il2cpp_api;
    if (encounter_id == 0U || !binding.direct_catch_verified ||
        binding.map_pokemon_try_capture == nullptr ||
        binding.ar_plus_encounter_values_class == nullptr ||
        binding.catch_pokemon_proto_class == nullptr ||
        api.runtime_invoke == nullptr || api.object_new == nullptr ||
        api.runtime_object_init == nullptr) {
        return MainThreadActionOutcome::kRejected;
    }
    void *map_pokemon = find_active_map_pokemon_by_id(context, encounter_id);
    if (map_pokemon == nullptr || !runtime_object_is_exact_class(
            api, map_pokemon, "Niantic.Holoholo.Map", "WildMapPokemon")) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "direct map catch target unavailable id=%llu",
            static_cast<unsigned long long>(encounter_id)
        );
        return MainThreadActionOutcome::kRejected;
    }

    // These are readonly injected fields from the reverse-verified
    // MapPokemon_Fields layout. Do not call managed field helpers here: the
    // object is already exact-class verified and this path must not re-enter
    // IL2CPP metadata code before the capture invocation.
    auto read_dependency = [map_pokemon](size_t offset) {
        void *value = nullptr;
        memcpy(&value, static_cast<const uint8_t *>(map_pokemon) + offset, sizeof(value));
        return value;
    };
    void *rpc_handler = read_dependency(0x38U);
    void *map_place_directory = read_dependency(0x40U);
    void *telemetry_service = read_dependency(0x68U);
    void *item_bag = read_dependency(0xE8U);
    const bool dependencies_read = true;
    __android_log_print(
        dependencies_read ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "direct map catch dependencies read=%d rpc=%p directory=%p telemetry=%p item_bag=%p",
        dependencies_read ? 1 : 0,
        rpc_handler,
        map_place_directory,
        telemetry_service,
        item_bag
    );
    if (!dependencies_read || !is_probable_managed_pointer(rpc_handler) ||
        !is_probable_managed_pointer(map_place_directory) ||
        !is_probable_managed_pointer(telemetry_service) ||
        !is_probable_managed_pointer(item_bag)) {
        return MainThreadActionOutcome::kRejected;
    }

    void *spawn_point_id_object = invoke_runtime_method(
        api,
        binding.map_pokemon_get_spawn_point_id,
        map_pokemon,
        nullptr,
        "WildMapPokemon.get_SpawnPointId"
    );
    std::string spawn_point_id;
    if (!read_runtime_managed_ascii_string(spawn_point_id_object, &spawn_point_id)) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "direct map catch rejected missing spawn point id object=%p value=%p",
            map_pokemon,
            spawn_point_id_object
        );
        return MainThreadActionOutcome::kRejected;
    }
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "direct map catch target spawn point id=%s",
        spawn_point_id.c_str()
    );

    // This is an ordinary Poke Ball intent. No hit, curve, excellent, or
    // guaranteed-catch flag is synthesized by the native bridge.
    DirectMapPokeballThrow throw_data;
    void *ar_plus_values = api.object_new(binding.ar_plus_encounter_values_class);
    if (!is_probable_managed_pointer(ar_plus_values)) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "direct map catch AR+ values allocation failed class=%p",
            binding.ar_plus_encounter_values_class
        );
        return MainThreadActionOutcome::kRejected;
    }
    api.runtime_object_init(ar_plus_values);
    using DirectMapTryCapture = void *(*)(
        void *, DirectMapPokeballThrow, void *, const void *
    );
    const auto *method_view = reinterpret_cast<const DirectRuntimeMethodInfoView *>(
        binding.map_pokemon_try_capture
    );
    const auto try_capture = reinterpret_cast<DirectMapTryCapture>(
        const_cast<void *>(method_view->method_pointer)
    );
    if (try_capture == nullptr) return MainThreadActionOutcome::kRejected;
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "runtime action invoking action=DIRECT_MAP_CATCH_DIRECT id=%llu object=%p ar_plus=%p method=%p code=%p",
        static_cast<unsigned long long>(encounter_id),
        map_pokemon,
        ar_plus_values,
        binding.map_pokemon_try_capture,
        method_view->method_pointer
    );
    log_direct_capture_method_metadata(binding.map_pokemon_try_capture);
    void *promise = try_capture(
        map_pokemon, throw_data, ar_plus_values, binding.map_pokemon_try_capture
    );
    return promise == nullptr
        ? MainThreadActionOutcome::kIndeterminate
        : MainThreadActionOutcome::kInvoked;
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
