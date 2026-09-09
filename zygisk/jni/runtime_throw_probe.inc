struct RuntimeThrowProbeState {
    void *pokeball = nullptr;
    bool interaction_in_play = false;
    bool ball_in_play = false;
    bool dpjx = false;
    bool dpki = false;
    bool dpks = false;
    bool dplt = false;
    bool dplu = false;
    int32_t ball_type = 0;
    void *pokemon_hit_collision = nullptr;
    void *capture_promise = nullptr;
    bool initialized = false;
};

bool invoke_runtime_bool(
    const Il2CppApi &api,
    const void *method,
    void *instance,
    bool *value
) {
    if (method == nullptr || instance == nullptr || value == nullptr ||
        api.runtime_invoke == nullptr || api.object_unbox == nullptr) return false;
    void *exception = nullptr;
    void *boxed = api.runtime_invoke(method, instance, nullptr, &exception);
    if (exception != nullptr || boxed == nullptr) return false;
    void *unboxed = api.object_unbox(boxed);
    if (unboxed == nullptr) return false;
    *value = *static_cast<bool *>(unboxed);
    return true;
}

bool read_runtime_throw_probe(
    const Il2CppApi &api,
    const RuntimeBinding &binding,
    RuntimeThrowProbeState *state
) {
    if (state == nullptr || binding.encounter_interaction == nullptr ||
        !has_core_il2cpp_api(api)) return false;
    RuntimeThrowProbeState current{};
    if (!read_runtime_field(
            api,
            binding.encounter_interaction,
            "<Pokeball>k__BackingField",
            &current.pokeball
        ) || !runtime_object_is_exact_class(
            api,
            current.pokeball,
            "Niantic.Holoholo.Encounter",
            "Pokeball"
        )) return false;

    const bool interaction_in_play = invoke_runtime_bool(
        api,
        binding.encounter_interaction_pokeball_in_play,
        binding.encounter_interaction,
        &current.interaction_in_play
    );
    const Il2CppClass *pokeball_class = api.object_get_class(current.pokeball);
    const void *ball_in_play_method = pokeball_class == nullptr
        ? nullptr
        : api.class_get_method_from_name(
            const_cast<Il2CppClass *>(pokeball_class), "InPlay", 0
        );
    const bool ball_in_play = invoke_runtime_bool(
        api,
        ball_in_play_method,
        current.pokeball,
        &current.ball_in_play
    );
    const bool fields_read =
        read_runtime_field(api, current.pokeball, "dpjx", &current.dpjx) &&
        read_runtime_field(api, current.pokeball, "dpki", &current.dpki) &&
        read_runtime_field(api, current.pokeball, "dpks", &current.dpks) &&
        read_runtime_field(api, current.pokeball, "dplt", &current.dplt) &&
        read_runtime_field(api, current.pokeball, "dplu", &current.dplu) &&
        read_runtime_field(api, current.pokeball, "dpkh", &current.capture_promise) &&
        read_runtime_field(api, current.pokeball, "<BallType>k__BackingField", &current.ball_type) &&
        read_runtime_field(
            api,
            current.pokeball,
            "<PokemonHitCollision>k__BackingField",
            &current.pokemon_hit_collision
        );
    if (!interaction_in_play && !ball_in_play && !fields_read) return false;
    current.initialized = true;
    *state = current;
    return true;
}

bool throw_probe_changed(
    const RuntimeThrowProbeState &before,
    const RuntimeThrowProbeState &after
) {
    return !before.initialized || before.pokeball != after.pokeball ||
        before.interaction_in_play != after.interaction_in_play ||
        before.ball_in_play != after.ball_in_play || before.dpjx != after.dpjx ||
        before.dpki != after.dpki || before.dpks != after.dpks ||
        before.dplt != after.dplt || before.dplu != after.dplu ||
        before.ball_type != after.ball_type ||
        before.pokemon_hit_collision != after.pokemon_hit_collision ||
        before.capture_promise != after.capture_promise;
}

pogo_runtime::RuntimeThrowDiagnosticObservation make_throw_diagnostic(
    uint64_t encounter_id,
    const RuntimeThrowProbeState &current,
    uint32_t stage
) {
    pogo_runtime::RuntimeThrowDiagnosticObservation diagnostic;
    diagnostic.encounter_id = encounter_id;
    diagnostic.stage = stage;
    diagnostic.ball_type = current.ball_type;
    if (current.interaction_in_play) {
        diagnostic.flags |= pogo_runtime::kThrowInteractionInPlay;
    }
    if (current.ball_in_play) diagnostic.flags |= pogo_runtime::kThrowBallInPlay;
    if (current.dpjx) diagnostic.flags |= pogo_runtime::kThrowDpjx;
    if (current.dpki) diagnostic.flags |= pogo_runtime::kThrowDpki;
    if (current.dpks) diagnostic.flags |= pogo_runtime::kThrowDpks;
    if (current.dplt) diagnostic.flags |= pogo_runtime::kThrowDplt;
    if (current.dplu) diagnostic.flags |= pogo_runtime::kThrowDplu;
    if (current.pokemon_hit_collision != nullptr) {
        diagnostic.flags |= pogo_runtime::kThrowHitCollision;
    }
    if (current.capture_promise != nullptr) {
        diagnostic.flags |= pogo_runtime::kThrowCapturePromise;
    }
    return diagnostic;
}

bool log_runtime_throw_probe(
    const Il2CppApi &api,
    const RuntimeBinding &binding,
    uint64_t encounter_id,
    RuntimeThrowProbeState *previous,
    pogo_runtime::RuntimeThrowDiagnosticObservation *diagnostic
) {
    if (previous == nullptr || diagnostic == nullptr) return false;
    RuntimeThrowProbeState current{};
    if (!read_runtime_throw_probe(api, binding, &current)) return false;
    if (!throw_probe_changed(*previous, current)) return false;
    const uint32_t stage = current.capture_promise != nullptr &&
            current.capture_promise != previous->capture_promise
        ? pogo_runtime::kThrowCapturePromiseCreated
        : previous->ball_in_play && !current.ball_in_play
            ? pogo_runtime::kThrowBallStopped
            : pogo_runtime::kThrowStateChanged;
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "runtime throw probe encounter=%llu ball=%p interaction_in_play=%d "
        "ball_in_play=%d flags=(%d,%d,%d,%d,%d) ball_type=%d hit_collision=%p",
        static_cast<unsigned long long>(encounter_id),
        current.pokeball,
        current.interaction_in_play ? 1 : 0,
        current.ball_in_play ? 1 : 0,
        current.dpjx ? 1 : 0,
        current.dpki ? 1 : 0,
        current.dpks ? 1 : 0,
        current.dplt ? 1 : 0,
        current.dplu ? 1 : 0,
        current.ball_type,
        current.pokemon_hit_collision
    );
    *diagnostic = make_throw_diagnostic(encounter_id, current, stage);
    *previous = current;
    return true;
}
