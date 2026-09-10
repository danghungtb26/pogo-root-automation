// Observation payload senders: encode a Runtime*Observation and write it to
// the runtime bridge fd. Split out of runtime_observation.inc to keep that
// file under the repository 500-line limit; include before it in main.cpp.

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

bool send_runtime_inventory_payload(
    ProbeContext &context,
    const pogo_runtime::RuntimeInventoryObservation &observation
) {
    std::vector<uint8_t> envelope;
    if (!pogo_runtime::encode_runtime_inventory_observation(
            observation,
            static_cast<uint64_t>(now_epoch_millis()),
            static_cast<uint64_t>(now_elapsed_nanos()),
            &envelope
        )) return false;
    const bool sent = send_internal_message(
        context.fd, pogo_runtime::kRuntimeObservationMagic, envelope
    );
    __android_log_print(
        sent ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "runtime observation inventory items=%zu sent=%d",
        observation.items.size(),
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
