bool handle_controller(int fd, BrokerContext &context) {
    if (!authorized_controller(fd)) return true;
    if (!send_runtime_ready(fd, context)) return true;

    while (kill(context.pid, 0) == 0) {
        if (!broker_is_current(context.runtime_session_id)) return false;
        pollfd descriptors[2]{};
        descriptors[0].fd = fd;
        descriptors[0].events = POLLIN;
        descriptors[1].fd = context.runtime_fd;
        descriptors[1].events = POLLIN;
        if (poll(descriptors, 2, 1000) <= 0) continue;

        if ((descriptors[1].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) return false;
        if ((descriptors[1].revents & POLLIN) != 0) {
            CompanionMessageType message_type{};
            RuntimeEvent runtime_event{};
            std::vector<uint8_t> runtime_result;
            std::vector<uint8_t> runtime_capabilities;
            std::vector<uint8_t> runtime_observation;
            if (!read_companion_message(
                    context.runtime_fd,
                    &message_type,
                    &runtime_event,
                    &runtime_result,
                    &runtime_capabilities,
                    &runtime_observation
                )) {
                return false;
            }
            if (message_type == CompanionMessageType::kRuntimeResult) {
                std::string command_id;
                uint32_t phase = 0U;
                std::string error_code;
                std::string message;
                uint32_t catch_outcome_wire = 0U;
                std::string captured_pokemon_id;
                if (!parse_runtime_result(
                        runtime_result,
                        &command_id,
                        &phase,
                        &error_code,
                        &message,
                        &catch_outcome_wire,
                        &captured_pokemon_id
                    ) ||
                    !send_command_result(
                        fd,
                        context,
                        command_id.c_str(),
                        phase,
                        error_code.c_str(),
                        message.c_str(),
                        catch_outcome_wire,
                        captured_pokemon_id.c_str()
                    )) {
                    return true;
                }
            }
            if (message_type == CompanionMessageType::kRuntimeCapabilities) {
                std::string fingerprint;
                std::vector<std::string> capabilities;
                bool strong_identity = false;
                const bool parsed = parse_runtime_capabilities(
                        runtime_capabilities,
                        &fingerprint,
                        &capabilities,
                        &strong_identity
                    );
                __android_log_print(
                    parsed ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
                    kLogTag,
                    "broker runtime capabilities parsed=%d bytes=%zu count=%zu strong=%d",
                    parsed ? 1 : 0,
                    runtime_capabilities.size(),
                    capabilities.size(),
                    strong_identity ? 1 : 0
                );
                if (!parsed || !send_runtime_ready_with_values(
                        fd,
                        context,
                        fingerprint.c_str(),
                        capabilities,
                        strong_identity
                    )) return false;
            }
            if (message_type == CompanionMessageType::kRuntimeObservation) {
                uint32_t observation_type = 0U;
                uint32_t payload_version = 0U;
                std::vector<uint8_t> map_target_payload;
                uint64_t observed_at_epoch_ms = 0U;
                uint64_t observed_at_elapsed_ns = 0U;
                if (!parse_runtime_observation_envelope(
                        runtime_observation,
                        &observation_type,
                        &payload_version,
                        &map_target_payload,
                        &observed_at_epoch_ms,
                        &observed_at_elapsed_ns
                    )) return false;
                if (observation_type == pogo_runtime::kMapTargetObservationType &&
                    payload_version == pogo_runtime::kMapTargetPayloadVersion) {
                    if (!send_map_target_observation(
                            fd,
                            context,
                            map_target_payload,
                            observed_at_epoch_ms,
                            observed_at_elapsed_ns
                        )) return false;
                } else if (observation_type == pogo_runtime::kEncounterObservationType &&
                    payload_version == pogo_runtime::kRuntimeEncounterPayloadVersion) {
                    if (!send_runtime_encounter_observation(
                            fd,
                            context,
                            map_target_payload,
                            observed_at_epoch_ms,
                        observed_at_elapsed_ns
                    )) return false;
                } else if (observation_type == 2U &&
                    payload_version == pogo_runtime::kRuntimeNearbyPayloadVersion) {
                    if (!send_runtime_nearby_observation(
                            fd,
                            context,
                            map_target_payload,
                            observed_at_epoch_ms,
                            observed_at_elapsed_ns
                        )) return false;
                } else if (observation_type == pogo_runtime::kFortsObservationType &&
                    payload_version == pogo_runtime::kRuntimeFortsPayloadVersion) {
                    if (!send_runtime_forts_observation(
                            fd,
                            context,
                            map_target_payload,
                            observed_at_epoch_ms,
                            observed_at_elapsed_ns
                        )) return false;
                } else if (observation_type == pogo_runtime::kInventoryObservationType &&
                    payload_version == pogo_runtime::kRuntimeInventoryPayloadVersion) {
                    if (!send_runtime_inventory_observation(
                            fd,
                            context,
                            map_target_payload,
                            observed_at_epoch_ms,
                        observed_at_elapsed_ns
                    )) return false;
                } else if (observation_type == pogo_runtime::kCatchSpinRequestObservationType &&
                    payload_version == pogo_runtime::kRuntimeCatchSpinPayloadVersion) {
                    if (!send_runtime_catch_spin_observation(
                            fd,
                            context,
                            map_target_payload,
                            observed_at_epoch_ms,
                            observed_at_elapsed_ns
                        )) return false;
                } else if (observation_type == pogo_runtime::kLifecycleObservationType &&
                    payload_version == pogo_runtime::kLifecyclePayloadVersion &&
                    map_target_payload.size() == 4U) {
                    uint32_t lifecycle_wire = 0U;
                    size_t lifecycle_offset = 0U;
                    if (!read_be32(map_target_payload, &lifecycle_offset, &lifecycle_wire) ||
                        lifecycle_wire == 0U || lifecycle_wire > 7U ||
                        !send_runtime_lifecycle_observation(
                            fd,
                            context,
                            map_target_payload,
                            lifecycle_wire,
                            observed_at_epoch_ms,
                            observed_at_elapsed_ns
                        )) return false;
                }
            }
        }

        if ((descriptors[0].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) return true;
        if ((descriptors[0].revents & POLLIN) != 0) {
            uint16_t message_type = 0U;
            uint64_t message_seq = 0U;
            std::vector<uint8_t> payload;
            if (!read_bridge_frame(fd, &message_type, &message_seq, &payload)) return true;
            if (message_type != kBridgeCommandType) continue;

            __android_log_print(
                ANDROID_LOG_INFO,
                kLogTag,
                "bridge controller command received seq=%llu bytes=%zu",
                static_cast<unsigned long long>(message_seq),
                payload.size()
            );

            size_t offset = 0U;
            uint32_t payload_version = 0U;
            std::string session_id;
            std::string command_id;
            if (!read_be32(payload, &offset, &payload_version) || payload_version != 1U ||
                !read_string(payload, &offset, &session_id) ||
                !read_string(payload, &offset, &command_id) ||
                session_id != context.runtime_session_id) return true;
            if (!send_runtime_command(context.runtime_fd, message_type, message_seq, payload)) return false;
        }
    }
    return false;
}

bool finish_runtime_bridge_broker(const char *runtime_session_id) {
    std::lock_guard<std::mutex> lock(g_broker_mutex);
    if (runtime_session_id != nullptr && g_broker_session_id == runtime_session_id) {
        g_broker_running = false;
        g_broker_session_id.clear();
        return true;
    }
    return false;
}

void *runtime_bridge_broker_thread(void *opaque_context) {
    auto *context = static_cast<BrokerContext *>(opaque_context);
    if (context == nullptr) {
        return nullptr;
    }

    int server = -1;
    {
        // Serialize socket replacement with a newer session starting. A stale
        // broker may still be unwinding, but it must never remove the current
        // session's socket during cleanup.
        std::lock_guard<std::mutex> lock(g_broker_mutex);
        if (g_broker_session_id != context->runtime_session_id) {
            close(context->runtime_fd);
            delete context;
            return nullptr;
        }
        server = make_bridge_server();
    }
    if (server < 0) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "cannot create runtime bridge socket");
        close(context->runtime_fd);
        finish_runtime_bridge_broker(context->runtime_session_id);
        delete context;
        return nullptr;
    }

    while (kill(context->pid, 0) == 0 && broker_is_current(context->runtime_session_id)) {
        pollfd descriptor{};
        descriptor.fd = server;
        descriptor.events = POLLIN;
        const int polled = poll(&descriptor, 1, 1000);
        if (polled <= 0) continue;
        if (!broker_is_current(context->runtime_session_id)) break;
        const int client = accept4(server, nullptr, nullptr, SOCK_CLOEXEC);
        if (client < 0) continue;
        const bool runtime_alive = handle_controller(client, *context);
        close(client);
        if (!runtime_alive) break;
    }

    close(server);
    close(context->runtime_fd);
    finish_runtime_bridge_broker(context->runtime_session_id);
    delete context;
    return nullptr;
}

void start_runtime_bridge_broker(const RuntimeState &state, int runtime_fd) {
    if (runtime_fd < 0) return;
    {
        std::lock_guard<std::mutex> lock(g_broker_mutex);
        if (g_broker_running && g_broker_session_id == state.runtime_session_id) {
            close(runtime_fd);
            return;
        }
        g_broker_running = true;
        g_broker_session_id = state.runtime_session_id;
    }

    auto *context = new BrokerContext{};
    context->pid = state.pid;
    context->runtime_fd = runtime_fd;
    copy_string(context->process_name, sizeof(context->process_name), state.process_name);
    copy_string(
        context->runtime_session_id,
        sizeof(context->runtime_session_id),
        state.runtime_session_id
    );
    pthread_t thread{};
    if (pthread_create(&thread, nullptr, runtime_bridge_broker_thread, context) != 0) {
        close(runtime_fd);
        delete context;
        finish_runtime_bridge_broker(state.runtime_session_id);
        return;
    }
    pthread_detach(thread);
}
