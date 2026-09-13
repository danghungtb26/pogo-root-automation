RuntimeAutoFortNavigation g_runtime_fort_navigation;

void publish_runtime_navigation(ProbeContext &context, const pogo_runtime::RuntimeNavigation &command) {
    std::vector<uint8_t> envelope;
    if (pogo_runtime::encode_runtime_navigation(command,
            static_cast<uint64_t>(now_epoch_millis()), static_cast<uint64_t>(now_elapsed_nanos()),
            &envelope)) {
        (void)send_internal_message(context.fd, pogo_runtime::kRuntimeObservationMagic, envelope);
    }
}

void pause_runtime_navigation(ProbeContext &context, const char *reason) {
    publish_runtime_navigation(context, g_runtime_fort_navigation.pause(reason));
}

void pause_runtime_navigation_if_busy(ProbeContext &context, const RuntimeCatchSpinConfig &config) {
    if (!config.auto_walk_to_fort || !config.armed || !g_runtime_active.load() ||
        context.active_encounter_id.load() != 0U || runtime_direct_catch_is_pending(context) ||
        runtime_spin_is_pending(context) || runtime_encounter_request_is_pending(context) ||
        g_runtime_catch_spin_coordinator.suspended || runtime_transfer_blocks_catch_spin() ||
        runtime_discard_blocks_catch_spin()) {
        pause_runtime_navigation(context, "navigation disabled or action in progress");
    }
}

void update_runtime_navigation(ProbeContext &context, const RuntimeCatchSpinConfig &config,
    const pogo_runtime::RuntimeCatchSpinObservation &snapshot) {
    if (!config.auto_walk_to_fort) return;
    for (const auto &command : g_runtime_fort_navigation.update(snapshot)) {
        publish_runtime_navigation(context, command);
    }
}
