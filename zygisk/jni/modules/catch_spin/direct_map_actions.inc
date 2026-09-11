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

// G1 Step 1 (read-only device diagnostic). TryCapture returns
// RpcPromise<CatchPokemonOutProto> : Promise<CatchPokemonOutProto>. The dump has
// no reliable field offsets for the generic promise, so log the runtime class +
// parent chain field name/offset/type once to capture the completion/value layout
// on the pinned build. This does not read game state and never mutates anything.
void log_direct_catch_promise_layout(const Il2CppApi &api, void *promise) {
    if (promise == nullptr || api.object_get_class == nullptr ||
        api.class_get_fields == nullptr || api.field_get_name == nullptr ||
        api.field_get_offset == nullptr) {
        return;
    }
    Il2CppClass *klass = api.object_get_class(promise);
    for (uint32_t depth = 0U; klass != nullptr && depth < 6U; ++depth) {
        const char *class_name = api.class_get_name == nullptr ? nullptr : api.class_get_name(klass);
        void *iterator = nullptr;
        for (uint32_t field_count = 0U; field_count < 64U; ++field_count) {
            const void *field = api.class_get_fields(klass, &iterator);
            if (field == nullptr) break;
            const char *field_name = api.field_get_name(field);
            const int32_t offset = api.field_get_offset(field);
            const Il2CppType *field_type = api.field_get_type == nullptr
                ? nullptr : api.field_get_type(field);
            const char *type_name = (field_type == nullptr || api.type_get_name == nullptr)
                ? "<unknown>" : api.type_get_name(field_type);
            __android_log_print(
                ANDROID_LOG_INFO,
                kLogTag,
                "direct catch promise layout depth=%u class=%s field=%s offset=0x%x type=%s",
                depth,
                class_name == nullptr ? "<unknown>" : class_name,
                field_name == nullptr ? "<unknown>" : field_name,
                offset,
                type_name
            );
        }
        klass = api.class_get_parent == nullptr ? nullptr : api.class_get_parent(klass);
    }
}

// Fire WildMapPokemon.SendEncounterRequest (RPC Encounter=102) before a direct
// catch when the target has no server-side encounter yet. Disable to fall back
// to a bare CatchPokemon (103) attempt for A/B verification on device.
constexpr bool kBackgroundEncounterEnabled = true;

// WildMapPokemon field offsets (reverse-verified, build 0.427.0 arm64-v8a).
// Only used after the object is exact-class verified as WildMapPokemon.
constexpr size_t kWildMapPokemonLocationProviderOffset = 0x228U;  // egwm ILocationProvider
constexpr size_t kWildMapPokemonEncounterRpcHandlerOffset = 0x230U;  // egwn IRpcHandler
constexpr size_t kWildMapPokemonEncounterOutProtoOffset = 0x258U;  // egws EncounterOutProto

MainThreadActionOutcome direct_catch_runtime_on_main_thread(
    ProbeContext &context,
    uint64_t encounter_id
) {
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    const Il2CppApi &api = context.il2cpp_api;
    if (encounter_id == 0U || !binding.direct_catch_verified ||
        binding.map_pokemon_try_capture == nullptr ||
        api.runtime_invoke == nullptr) {
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

    // Background encounter gate. CatchPokemon (103) needs an encounter that the
    // server has opened; WildMapPokemon caches the EncounterOutProto in field
    // `egws` once SendEncounterRequest (102) resolves. If it is still null, fire
    // the encounter request (a pure RPC, not the encounter UI) and ask the
    // controller to retry the same target on a later cycle. The Unity main thread
    // is never blocked on the async response: the game's own continuation fills
    // `egws`, and the next cycle re-reads it here before throwing a ball.
    auto read_wild_field = [map_pokemon](size_t offset) {
        void *value = nullptr;
        memcpy(&value, static_cast<const uint8_t *>(map_pokemon) + offset, sizeof(value));
        return value;
    };
    void *encounter_out_proto = read_wild_field(kWildMapPokemonEncounterOutProtoOffset);
    if (encounter_out_proto == nullptr) {
        void *location_provider = read_wild_field(kWildMapPokemonLocationProviderOffset);
        void *encounter_rpc_handler = read_wild_field(kWildMapPokemonEncounterRpcHandlerOffset);
        if (!kBackgroundEncounterEnabled || !binding.background_encounter_verified ||
            binding.map_pokemon_send_encounter_request == nullptr ||
            !is_probable_managed_pointer(location_provider) ||
            !is_probable_managed_pointer(encounter_rpc_handler)) {
            __android_log_print(
                ANDROID_LOG_WARN,
                kLogTag,
                "direct map catch rejected id=%llu: no open encounter and background request unavailable (enabled=%d verified=%d loc=%p rpc=%p)",
                static_cast<unsigned long long>(encounter_id),
                kBackgroundEncounterEnabled ? 1 : 0,
                binding.background_encounter_verified ? 1 : 0,
                location_provider,
                encounter_rpc_handler
            );
            return MainThreadActionOutcome::kRejected;
        }
        void *encounter_exception = nullptr;
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "runtime action invoking action=DIRECT_MAP_SEND_ENCOUNTER id=%llu object=%p method=%p",
            static_cast<unsigned long long>(encounter_id),
            map_pokemon,
            binding.map_pokemon_send_encounter_request
        );
        api.runtime_invoke(
            binding.map_pokemon_send_encounter_request,
            map_pokemon,
            nullptr,
            &encounter_exception
        );
        if (encounter_exception != nullptr) {
            __android_log_print(
                ANDROID_LOG_WARN,
                kLogTag,
                "runtime action background encounter exception id=%llu exception=%p",
                static_cast<unsigned long long>(encounter_id),
                encounter_exception
            );
            return MainThreadActionOutcome::kIndeterminate;
        }
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "runtime action background encounter requested id=%llu; retry after server response",
            static_cast<unsigned long long>(encounter_id)
        );
        return MainThreadActionOutcome::kEncounterRequested;
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

    // On-demand ball gate: DIRECT_MAP always throws a Poké Ball (ball_type = 1).
    // Check the count right before the throw instead of continuously observing
    // inventory. Zero -> do not waste the throw; report out-of-balls so the
    // service can spin to farm balls. A negative count means the inventory
    // binding is unavailable, so proceed rather than block.
    constexpr int32_t kPokeBallItemId = 1;
    const int32_t poke_ball_count = read_runtime_item_count(api, binding, kPokeBallItemId);
    if (poke_ball_count == 0) {
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "direct map catch skipped id=%llu: out of Poké Balls",
            static_cast<unsigned long long>(encounter_id)
        );
        return MainThreadActionOutcome::kOutOfBalls;
    }

    // Poke Ball intent forced to a guaranteed Excellent + curveball + bullseye throw.
    // Verified from libil2cpp.so (build 0.427.0, arm64-v8a):
    //   PokeballThrow.get_ServerReticleSize @0x89F42D4:  serverReticle = 2.0 - ReticleSize
    //   MapPokemon.TryCapture @0x7F91ECC builds CatchPokemonProto:
    //     NormalizedReticleSize = serverReticle          (higher = better)
    //     NormalizedHitPosition = HitBullseye ? 1.0 : 0.0
    //     SpinModifier          = Spinning    ? 1.0 : 0.0
    //     HitPokemon            = !Missed
    //   PokeballThrow.GetGrade @0x89F0DA0: Excellent when
    //     serverReticle >= EncounterSettingsProto.ExcellentThrowThreshold (~1.7).
    // reticle_size = 0.05 -> serverReticle = 1.95: solidly Excellent without sending the
    // physically-impossible perfect 2.0. Set 0.0 for the absolute maximum if desired.
    DirectMapPokeballThrow throw_data;
    throw_data.reticle_size = 0.05F;
    throw_data.hit_bullseye = 1U;
    throw_data.spinning = 1U;
    const auto *method_view = reinterpret_cast<const DirectRuntimeMethodInfoView *>(
        binding.map_pokemon_try_capture
    );
    void *arguments[] = {&throw_data, nullptr};
    void *exception = nullptr;
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "runtime action invoking action=DIRECT_MAP_CATCH_DIRECT id=%llu object=%p ar_plus=null ball=%d reticle=%.3f hit=%d spinning=%d missed=%d method=%p code=%p",
        static_cast<unsigned long long>(encounter_id),
        map_pokemon,
        throw_data.ball_type,
        throw_data.reticle_size,
        throw_data.hit_bullseye,
        throw_data.spinning,
        throw_data.missed,
        binding.map_pokemon_try_capture,
        method_view == nullptr ? nullptr : method_view->method_pointer
    );
    log_direct_capture_method_metadata(binding.map_pokemon_try_capture);
    void *promise = api.runtime_invoke(
        binding.map_pokemon_try_capture,
        map_pokemon,
        arguments,
        &exception
    );
    if (exception != nullptr) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "runtime action direct map catch exception id=%llu exception=%p promise=%p",
            static_cast<unsigned long long>(encounter_id),
            exception,
            promise
        );
        return MainThreadActionOutcome::kIndeterminate;
    }
    __android_log_print(
        promise == nullptr ? ANDROID_LOG_WARN : ANDROID_LOG_INFO,
        kLogTag,
        "runtime action direct map catch returned id=%llu promise=%p exception=%p",
        static_cast<unsigned long long>(encounter_id),
        promise,
        exception
    );
    // G1 Step 1: capture the RpcPromise<CatchPokemonOutProto> layout on the pinned
    // build so the completion/value field offsets can be read for the outcome
    // observer (Step 2). Read-only; safe to run every invocation.
    log_direct_catch_promise_layout(api, promise);
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
