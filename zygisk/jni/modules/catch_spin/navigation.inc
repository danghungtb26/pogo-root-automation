RuntimeAutoFortWalkCandidateProducer g_runtime_fort_walk_candidates;

void publish_runtime_walk_candidate(
    ProbeContext &context,
    const pogo_runtime::RuntimePointWalkCandidate &candidate
) {
    std::vector<uint8_t> envelope;
    if (pogo_runtime::encode_runtime_point_walk_candidate(
            candidate,
            static_cast<uint64_t>(now_epoch_millis()),
            static_cast<uint64_t>(now_elapsed_nanos()),
            &envelope)) {
        (void)send_internal_message(context.fd, pogo_runtime::kRuntimeObservationMagic, envelope);
    }
}

void pause_runtime_walk_candidate(ProbeContext &context, const char *reason) {
    if (reason != nullptr) {
        __android_log_print(ANDROID_LOG_INFO, kLogTag, "native fort walk paused reason=%s", reason);
    }
    for (const auto &candidate : g_runtime_fort_walk_candidates.pause_candidate()) {
        publish_runtime_walk_candidate(context, candidate);
    }
}

void pause_runtime_walk_candidate_if_busy(ProbeContext &context, const RuntimeCatchSpinConfig &config) {
    if (!config.auto_walk_to_fort || !config.armed || !g_runtime_active.load() ||
        context.active_encounter_id.load() != 0U || runtime_direct_catch_is_pending(context) ||
        runtime_spin_is_pending(context) || runtime_encounter_request_is_pending(context) ||
        g_runtime_catch_spin_coordinator.suspended || runtime_transfer_blocks_catch_spin() ||
        runtime_discard_blocks_catch_spin()) {
        pause_runtime_walk_candidate(context, "navigation disabled or action in progress");
    }
}

void update_runtime_walk_candidate(ProbeContext &context, const RuntimeCatchSpinConfig &config,
    const pogo_runtime::RuntimeCatchSpinObservation &snapshot) {
    if (!config.auto_walk_to_fort) return;
    for (const auto &candidate : g_runtime_fort_walk_candidates.update_candidates(snapshot)) {
        publish_runtime_walk_candidate(context, candidate);
    }
}
