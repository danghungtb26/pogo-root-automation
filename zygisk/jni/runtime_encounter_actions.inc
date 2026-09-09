struct RuntimeVector3 {
    float x;
    float y;
    float z;
};

MainThreadActionOutcome throw_runtime_ball_on_main_thread(
    ProbeContext &context,
    uint64_t encounter_id
) {
    const Il2CppApi &api = context.il2cpp_api;
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    if (encounter_id == 0U || binding.encounter_interaction == nullptr ||
        binding.encounter_pokemon == nullptr || binding.pokeball_service == nullptr ||
        binding.pokeball_service_throw == nullptr || api.runtime_invoke == nullptr) {
        return MainThreadActionOutcome::kRejected;
    }

    void *map_pokemon = nullptr;
    if (!read_runtime_field(
            api,
            binding.encounter_pokemon,
            "<MapPokemon>k__BackingField",
            &map_pokemon
        )) return MainThreadActionOutcome::kRejected;
    uint64_t active_encounter_id = 0U;
    if (!read_map_pokemon_id(api, map_pokemon, &active_encounter_id) ||
        active_encounter_id != encounter_id) return MainThreadActionOutcome::kRejected;

    void *pokeball = nullptr;
    if (!read_runtime_field(
            api,
            binding.encounter_interaction,
            "<Pokeball>k__BackingField",
            &pokeball
        ) || !runtime_object_is_exact_class(
            api,
            pokeball,
            "Niantic.Holoholo.Encounter",
            "Pokeball"
        )) return MainThreadActionOutcome::kRejected;

    Il2CppClass *pokeball_class = api.object_get_class(pokeball);
    const void *get_game_object = find_map_method(api, pokeball_class, "get_gameObject", 0U);
    if (get_game_object == nullptr) return MainThreadActionOutcome::kRejected;
    void *exception = nullptr;
    void *game_object = api.runtime_invoke(get_game_object, pokeball, nullptr, &exception);
    if (exception != nullptr || !is_probable_managed_pointer(game_object)) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "runtime catch get_gameObject failed exception=%p object=%p",
            exception,
            game_object
        );
        return exception == nullptr
            ? MainThreadActionOutcome::kRejected
            : MainThreadActionOutcome::kIndeterminate;
    }

    RuntimeVector3 target{};
    if (!read_runtime_field(
            api,
            binding.encounter_pokemon,
            "<TargetPosition>k__BackingField",
            &target
        ) || !std::isfinite(target.x) || !std::isfinite(target.y) ||
        !std::isfinite(target.z)) return MainThreadActionOutcome::kRejected;

    void *arguments[] = {game_object, &target};
    exception = nullptr;
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "runtime catch invoking action=CATCH encounter=%llu service=%p method=%p ball=%p target=(%.3f,%.3f,%.3f)",
        static_cast<unsigned long long>(encounter_id),
        binding.pokeball_service,
        binding.pokeball_service_throw,
        game_object,
        target.x,
        target.y,
        target.z
    );
    void *promise = api.runtime_invoke(
        binding.pokeball_service_throw,
        binding.pokeball_service,
        arguments,
        &exception
    );
    if (exception != nullptr) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "runtime catch Throw returned exception=%p",
            exception
        );
        return MainThreadActionOutcome::kIndeterminate;
    }
    if (promise == nullptr) {
        __android_log_print(ANDROID_LOG_WARN, kLogTag, "runtime catch Throw returned null promise");
        return MainThreadActionOutcome::kIndeterminate;
    }
    return MainThreadActionOutcome::kInvoked;
}
