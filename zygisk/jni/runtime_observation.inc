bool runtime_object_is_exact_class(
    const Il2CppApi &api,
    void *object,
    const char *expected_namespace,
    const char *expected_name
) {
    if (object == nullptr || expected_namespace == nullptr || expected_name == nullptr ||
        api.object_get_class == nullptr || api.class_get_name == nullptr ||
        api.class_get_namespace == nullptr) return false;
    Il2CppClass *klass = api.object_get_class(object);
    const char *name = klass == nullptr ? nullptr : api.class_get_name(klass);
    const char *name_space = klass == nullptr ? nullptr : api.class_get_namespace(klass);
    return name != nullptr && name_space != nullptr && strcmp(name, expected_name) == 0 &&
        strcmp(name_space, expected_namespace) == 0;
}

bool read_encounter_from_wild_map_pokemon(
    const Il2CppApi &api,
    void *map_pokemon,
    pogo_runtime::RuntimeEncounterObservation *observation
) {
    if (observation == nullptr || map_pokemon == nullptr || !has_core_il2cpp_api(api) ||
        !runtime_object_is_exact_class(
            api, map_pokemon, "Niantic.Holoholo.Map", "WildMapPokemon"
        )) return false;
    void *encounter_proto = nullptr;
    void *wild_pokemon = nullptr;
    const bool has_encounter_response = read_runtime_field(api, map_pokemon, "egws", &encounter_proto) &&
        runtime_object_is_exact_class(
            api, encounter_proto, "Holoholo.Rpc", "EncounterOutProto"
        ) && read_runtime_field(api, encounter_proto, "Pokemon", &wild_pokemon) &&
        runtime_object_is_exact_class(api, wild_pokemon, "Holoholo.Rpc", "WildPokemonProto");
    if (!has_encounter_response) {
        if (!read_runtime_field(api, map_pokemon, "egwr", &wild_pokemon) ||
            !runtime_object_is_exact_class(
                api, wild_pokemon, "Holoholo.Rpc", "WildPokemonProto"
            )) {
            __android_log_print(
                ANDROID_LOG_WARN,
                kLogTag,
                "runtime encounter reader no WildPokemonProto egws=%p egwr=%p",
                encounter_proto,
                wild_pokemon
            );
            return false;
        }
    }

    void *pokemon = nullptr;
    if (!read_runtime_field(api, wild_pokemon, "Pokemon", &pokemon) ||
        !runtime_object_is_exact_class(api, pokemon, "Holoholo.Rpc", "PokemonProto")) {
        if (!read_runtime_field(api, map_pokemon, "egwt", &pokemon) ||
            !runtime_object_is_exact_class(api, pokemon, "Holoholo.Rpc", "PokemonProto")) {
            __android_log_print(
                ANDROID_LOG_WARN,
                kLogTag,
                "runtime encounter reader no PokemonProto wild=%p egwt=%p",
                wild_pokemon,
                pokemon
            );
            return false;
        }
    }

    if (!read_runtime_field(api, wild_pokemon, "EncounterId", &observation->encounter_id) ||
        !read_runtime_field(api, wild_pokemon, "Latitude", &observation->latitude) ||
        !read_runtime_field(api, wild_pokemon, "Longitude", &observation->longitude)) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "runtime encounter reader location fields unavailable pokemon=%p",
            wild_pokemon
        );
        return false;
    }
    if (!read_runtime_field(api, pokemon, "pokemonId_", &observation->species_id) ||
        !read_runtime_field(api, pokemon, "individualAttack_", &observation->individual_attack) ||
        !read_runtime_field(api, pokemon, "individualDefense_", &observation->individual_defense) ||
        !read_runtime_field(api, pokemon, "individualStamina_", &observation->individual_stamina)) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "runtime encounter reader pokemon fields unavailable pokemon=%p",
            pokemon
        );
        return false;
    }

    void *pokemon_display = nullptr;
    if (read_runtime_field(api, pokemon, "pokemonDisplay_", &pokemon_display) &&
        runtime_object_is_exact_class(
            api, pokemon_display, "Holoholo.Rpc", "PokemonDisplayProto"
        )) {
        observation->has_shiny = read_runtime_field(
            api, pokemon_display, "Shiny", &observation->shiny
        );
    }
    return pogo_runtime::valid_runtime_encounter_observation(*observation);
}

bool read_encounter_from_map_pokemon(
    const Il2CppApi &api,
    void *map_pokemon,
    pogo_runtime::RuntimeEncounterObservation *observation
) {
    if (observation == nullptr || map_pokemon == nullptr || !has_core_il2cpp_api(api) ||
        !runtime_object_is_exact_class(api, map_pokemon, "Niantic.Holoholo.Map", "MapPokemon")) {
        return false;
    }
    uint64_t encounter_id = 0U;
    if (!read_map_pokemon_id(api, map_pokemon, &encounter_id)) return false;
    const void *get_pokemon = find_map_method(
        api, api.object_get_class(map_pokemon), "get_Pokemon", 0U
    );
    if (get_pokemon == nullptr) return false;
    void *exception = nullptr;
    void *pokemon = api.runtime_invoke(get_pokemon, map_pokemon, nullptr, &exception);
    if (exception != nullptr || !runtime_object_is_exact_class(
            api, pokemon, "Holoholo.Rpc", "PokemonProto")) return false;
    if (!read_runtime_field(api, pokemon, "pokemonId_", &observation->species_id) ||
        !read_runtime_field(api, pokemon, "individualAttack_", &observation->individual_attack) ||
        !read_runtime_field(api, pokemon, "individualDefense_", &observation->individual_defense) ||
        !read_runtime_field(api, pokemon, "individualStamina_", &observation->individual_stamina)) {
        return false;
    }
    observation->encounter_id = encounter_id;
    void *pokemon_display = nullptr;
    if (read_runtime_field(api, pokemon, "pokemonDisplay_", &pokemon_display) &&
        runtime_object_is_exact_class(api, pokemon_display, "Holoholo.Rpc", "PokemonDisplayProto")) {
        observation->has_shiny = read_runtime_field(
            api, pokemon_display, "Shiny", &observation->shiny
        );
    }
    const void *get_location = find_map_method(
        api, api.object_get_class(map_pokemon), "get_Location", 0U
    );
    if (get_location != nullptr) {
        exception = nullptr;
        void *location = api.runtime_invoke(get_location, map_pokemon, nullptr, &exception);
        if (exception == nullptr && is_probable_managed_pointer(location)) {
            read_runtime_field(api, location, "Latitude", &observation->latitude);
            read_runtime_field(api, location, "Longitude", &observation->longitude);
        }
    }
    return pogo_runtime::valid_runtime_encounter_observation(*observation);
}

bool read_encounter_from_map_object(
    const Il2CppApi &api,
    void *map_object,
    pogo_runtime::RuntimeEncounterObservation *observation
) {
    if (read_encounter_from_wild_map_pokemon(api, map_object, observation)) return true;
    return read_encounter_from_map_pokemon(api, map_object, observation);
}

bool read_active_runtime_encounter(
    const Il2CppApi &api,
    void *encounter_pokemon,
    void *encounter_state,
    pogo_runtime::RuntimeEncounterObservation *observation
) {
    if (observation == nullptr || !has_core_il2cpp_api(api)) return false;
    // Unity object enumeration is main-thread-only. The owners are resolved
    // through Zenject during the explicit diagnostic and retained only while
    // the verified runtime session is alive.
    void *map_pokemon = nullptr;
    if (encounter_pokemon != nullptr) {
        read_runtime_field(api, encounter_pokemon, "<MapPokemon>k__BackingField", &map_pokemon);
        if (read_encounter_from_map_object(api, map_pokemon, observation)) return true;
    }
    if (encounter_state != nullptr) {
        map_pokemon = nullptr;
        read_runtime_field(api, encounter_state, "<MapPokemon>k__BackingField", &map_pokemon);
        if (read_encounter_from_map_object(api, map_pokemon, observation)) return true;
    }
    return false;
}

bool send_runtime_encounter_payload(
    ProbeContext &context,
    const pogo_runtime::RuntimeEncounterObservation &observation
) {
    std::vector<uint8_t> envelope;
    if (!pogo_runtime::encode_runtime_encounter_observation(
            observation,
            static_cast<uint64_t>(now_epoch_millis()),
            static_cast<uint64_t>(now_elapsed_nanos()),
            &envelope
        )) return false;
    return send_internal_message(context.fd, pogo_runtime::kRuntimeObservationMagic, envelope);
}

bool send_runtime_nearby_payload(
    ProbeContext &context,
    const pogo_runtime::RuntimeNearbyObservation &observation
) {
    std::vector<uint8_t> envelope;
    if (!pogo_runtime::encode_runtime_nearby_observation(
            observation,
            static_cast<uint64_t>(now_epoch_millis()),
            static_cast<uint64_t>(now_elapsed_nanos()),
            &envelope
        )) return false;
    const bool sent = send_internal_message(context.fd, pogo_runtime::kRuntimeObservationMagic, envelope);
    __android_log_print(
        sent ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "runtime observation nearby spawns=%zu sent=%d",
        observation.spawns.size(),
        sent ? 1 : 0
    );
    return sent;
}

bool send_runtime_forts_payload(
    ProbeContext &context,
    const pogo_runtime::RuntimeFortsObservation &observation
) {
    std::vector<uint8_t> envelope;
    if (!pogo_runtime::encode_runtime_forts_observation(
            observation,
            static_cast<uint64_t>(now_epoch_millis()),
            static_cast<uint64_t>(now_elapsed_nanos()),
            &envelope
        )) return false;
    const bool sent = send_internal_message(context.fd, pogo_runtime::kRuntimeObservationMagic, envelope);
    __android_log_print(
        sent ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "runtime observation forts=%zu sent=%d",
        observation.forts.size(),
        sent ? 1 : 0
    );
    return sent;
}

bool send_runtime_lifecycle(
    ProbeContext &context,
    uint32_t lifecycle_wire
) {
    std::vector<uint8_t> envelope;
    if (!pogo_runtime::encode_runtime_lifecycle_observation(
            lifecycle_wire,
            static_cast<uint64_t>(now_epoch_millis()),
            static_cast<uint64_t>(now_elapsed_nanos()),
            &envelope
        )) return false;
    const bool sent = send_internal_message(context.fd, pogo_runtime::kRuntimeObservationMagic, envelope);
    __android_log_print(
        sent ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "runtime observation lifecycle wire=%u sent=%d",
        lifecycle_wire,
        sent ? 1 : 0
    );
    return sent;
}

bool send_runtime_throw_diagnostic(
    ProbeContext &context,
    const pogo_runtime::RuntimeThrowDiagnosticObservation &diagnostic
) {
    std::vector<uint8_t> envelope;
    if (!pogo_runtime::encode_runtime_throw_diagnostic_observation(
            diagnostic,
            static_cast<uint64_t>(now_epoch_millis()),
            static_cast<uint64_t>(now_elapsed_nanos()),
            &envelope
        )) return false;
    const bool sent = send_internal_message(
        context.fd,
        pogo_runtime::kRuntimeObservationMagic,
        envelope
    );
    __android_log_print(
        sent ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "runtime throw diagnostic stage=%u encounter=%llu ball=%d flags=0x%x sent=%d",
        diagnostic.stage,
        static_cast<unsigned long long>(diagnostic.encounter_id),
        diagnostic.ball_type,
        diagnostic.flags,
        sent ? 1 : 0
    );
    return sent;
}

bool send_pending_runtime_throw_events(ProbeContext &context) {
    RuntimeThrowEventRecord event;
    while (context.throw_events.pop(&event)) {
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "runtime throw event dispatch seq=%llu stage=%u object=%p encounter=%llu",
            static_cast<unsigned long long>(event.sequence),
            event.diagnostic.stage,
            reinterpret_cast<void *>(event.ball_instance),
            static_cast<unsigned long long>(event.diagnostic.encounter_id)
        );
        if (!send_runtime_throw_diagnostic(context, event.diagnostic)) return false;
    }
    return true;
}

void *runtime_observation_thread(void *opaque_context) {
    auto *context = static_cast<ProbeContext *>(opaque_context);
    if (context == nullptr) return nullptr;

    bool was_encounter = false;
    bool sent_initial_state = false;
    uint64_t last_encounter_id = 0U;
    uint32_t lifecycle_heartbeat_ticks = 0U;
    uint32_t nearby_ticks = 0U;
    uint32_t encounter_owner_refresh_ticks = 0U;
    while (context->observation_running.load() && kill(getpid(), 0) == 0) {
        if (!send_pending_runtime_throw_events(*context)) break;
        bool active_encounter = false;
        pogo_runtime::RuntimeEncounterObservation observation{};
        const Il2CppApi &api = context->il2cpp_api;
        if (encounter_owner_refresh_ticks >= 4U) {
            refresh_runtime_encounter_owners(*context);
            encounter_owner_refresh_ticks = 0U;
        }
        ++encounter_owner_refresh_ticks;
        const RuntimeBinding binding = runtime_binding_snapshot(*context);
        if (binding.map_entity_read_verified &&
            context->main_thread_bridge.ready && nearby_ticks >= 2U) {
            pogo_runtime::RuntimeNearbyObservation nearby_observation;
            pogo_runtime::RuntimeFortsObservation forts_observation;
            if (request_main_thread_map_snapshot(
                    *context, &nearby_observation, &forts_observation)) {
                if (!send_runtime_nearby_payload(*context, nearby_observation) ||
                    (binding.forts_read_verified &&
                        !send_runtime_forts_payload(*context, forts_observation))) break;
            }
            nearby_ticks = 0U;
        }
        ++nearby_ticks;
        if (context->il2cpp_api_ready && binding.encounter_read_verified &&
            api.domain_get != nullptr && api.thread_attach != nullptr &&
            api.thread_detach != nullptr &&
            (binding.encounter_pokemon != nullptr || binding.encounter_state != nullptr)) {
            std::lock_guard<std::mutex> binding_lock(context->binding_mutex);
            const RuntimeBinding encounter_binding = context->binding;
            Il2CppDomain *domain = api.domain_get();
            Il2CppThread *thread = domain == nullptr ? nullptr : api.thread_attach(domain);
            if (thread != nullptr) {
                active_encounter = read_active_runtime_encounter(
                    api,
                    encounter_binding.encounter_pokemon,
                    encounter_binding.encounter_state,
                    &observation
                );
                if (active_encounter) {
                    context->active_encounter_id.store(observation.encounter_id);
                }
                api.thread_detach(thread);
            }
        }

        if (active_encounter) {
            if (!was_encounter || last_encounter_id != observation.encounter_id) {
                __android_log_print(
                    ANDROID_LOG_INFO,
                    kLogTag,
                    "runtime observation encounter active id=%llu species=%d shiny=%d",
                    static_cast<unsigned long long>(observation.encounter_id),
                    observation.species_id,
                    observation.has_shiny && observation.shiny ? 1 : 0
                );
            }
            if (!send_runtime_encounter_payload(*context, observation)) break;
            was_encounter = true;
            last_encounter_id = observation.encounter_id;
        } else {
            const bool left_encounter = was_encounter;
            if (was_encounter) {
                __android_log_print(
                    ANDROID_LOG_INFO,
                    kLogTag,
                    "runtime observation lifecycle left encounter"
                );
            }
            // The controller may reconnect after the one-shot diagnostic that
            // starts this observer. Periodic lifecycle heartbeats let a new
            // bridge client converge to OVERWORLD without another diagnostic.
            const bool heartbeat_due = lifecycle_heartbeat_ticks >= 4U;
            if (!sent_initial_state || was_encounter || last_encounter_id != 0U || heartbeat_due) {
                if (!send_runtime_lifecycle(*context, 4U)) break;
                lifecycle_heartbeat_ticks = 0U;
            }
            ++lifecycle_heartbeat_ticks;
            was_encounter = false;
            last_encounter_id = 0U;
            if (left_encounter) context->active_encounter_id.store(0U);
            sent_initial_state = true;
        }
        if (!send_pending_runtime_throw_events(*context)) break;
        usleep(100000U);
    }
    context->observation_running.store(false);
    return nullptr;
}

bool start_runtime_observation(ProbeContext &context) {
    if (!runtime_binding_snapshot(context).encounter_read_verified) return false;
    if (context.observation_thread_started) {
        if (context.observation_running.load()) return false;
        pthread_join(context.observation_thread, nullptr);
        context.observation_thread_started = false;
    }
    context.observation_running.store(true);
    const int result = pthread_create(
        &context.observation_thread,
        nullptr,
        runtime_observation_thread,
        &context
    );
    if (result != 0) {
        context.observation_running.store(false);
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "runtime observation thread create failed result=%d",
            result
        );
        return false;
    }
    context.observation_thread_started = true;
    __android_log_print(ANDROID_LOG_INFO, kLogTag, "runtime observation thread started");
    return true;
}

void stop_runtime_observation(ProbeContext &context) {
    if (!context.observation_thread_started) return;
    context.observation_running.store(false);
    pthread_join(context.observation_thread, nullptr);
    context.observation_thread_started = false;
}
