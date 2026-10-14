bool forward_runtime_observation(
    int fd,
    BrokerContext &context,
    const std::vector<uint8_t> &runtime_observation
) {
    uint32_t observation_type = 0U;
    uint32_t payload_version = 0U;
    std::vector<uint8_t> payload;
    uint64_t observed_at_epoch_ms = 0U;
    uint64_t observed_at_elapsed_ns = 0U;
    if (!parse_runtime_observation_envelope(
            runtime_observation,
            &observation_type,
            &payload_version,
            &payload,
            &observed_at_epoch_ms,
            &observed_at_elapsed_ns
        )) return false;

    if (observation_type == pogo_runtime::kMapTargetObservationType &&
        payload_version == pogo_runtime::kMapTargetPayloadVersion) {
        return send_map_target_observation(
            fd, context, payload, observed_at_epoch_ms, observed_at_elapsed_ns
        );
    }
    if (observation_type == pogo_runtime::kEncounterObservationType &&
        payload_version == pogo_runtime::kRuntimeEncounterPayloadVersion) {
        return send_runtime_encounter_observation(
            fd, context, payload, observed_at_epoch_ms, observed_at_elapsed_ns
        );
    }
    if (observation_type == 2U &&
        payload_version == pogo_runtime::kRuntimeNearbyPayloadVersion) {
        return send_runtime_nearby_observation(
            fd, context, payload, observed_at_epoch_ms, observed_at_elapsed_ns
        );
    }
    if (observation_type == pogo_runtime::kFortsObservationType &&
        payload_version == pogo_runtime::kRuntimeFortsPayloadVersion) {
        return send_runtime_forts_observation(
            fd, context, payload, observed_at_epoch_ms, observed_at_elapsed_ns
        );
    }
    if (observation_type == pogo_runtime::kInventoryObservationType &&
        payload_version == pogo_runtime::kRuntimeInventoryPayloadVersion) {
        return send_runtime_inventory_observation(
            fd, context, payload, observed_at_epoch_ms, observed_at_elapsed_ns
        );
    }
    if (observation_type == pogo_runtime::kCatchSpinRequestObservationType &&
        payload_version == pogo_runtime::kRuntimeCatchSpinPayloadVersion) {
        return send_runtime_catch_spin_observation(
            fd, context, payload, observed_at_epoch_ms, observed_at_elapsed_ns
        );
    }
    if (observation_type == pogo_runtime::kAutomationEventObservationType &&
        payload_version == pogo_runtime::kRuntimeAutomationEventPayloadVersion) {
        return send_runtime_automation_event_observation(
            fd, context, payload, observed_at_epoch_ms, observed_at_elapsed_ns
        );
    }
    if (observation_type == pogo_runtime::kNavigationObservationType &&
        payload_version == pogo_runtime::kNavigationPayloadVersion) {
        return send_runtime_navigation_observation(
            fd, context, payload, observed_at_epoch_ms, observed_at_elapsed_ns
        );
    }
    if (observation_type == pogo_runtime::kPointWalkCandidateObservationType &&
        payload_version == pogo_runtime::kPointWalkCandidatePayloadVersion) {
        return send_runtime_walk_candidate_observation(
            fd, context, payload, observed_at_epoch_ms, observed_at_elapsed_ns
        );
    }
    if (observation_type == pogo_runtime::kLifecycleObservationType &&
        payload_version == pogo_runtime::kLifecyclePayloadVersion &&
        payload.size() == 4U) {
        uint32_t lifecycle_wire = 0U;
        size_t offset = 0U;
        return read_be32(payload, &offset, &lifecycle_wire) &&
            lifecycle_wire > 0U && lifecycle_wire <= 7U &&
            send_runtime_lifecycle_observation(
                fd, context, payload, lifecycle_wire,
                observed_at_epoch_ms, observed_at_elapsed_ns
            );
    }
    return true;
}
