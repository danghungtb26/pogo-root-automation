void persist_runtime_state(const RuntimeState &state);

using BrokerRequestOwners = std::unordered_map<std::string, int>;

void broker_drop_client(
    std::vector<int> *clients,
    BrokerRequestOwners *requests,
    int fd
) {
    if (clients == nullptr || requests == nullptr) return;
    close(fd);
    clients->erase(std::remove(clients->begin(), clients->end(), fd), clients->end());
    for (auto it = requests->begin(); it != requests->end();) {
        if (it->second == fd) it = requests->erase(it);
        else ++it;
    }
}

template <typename Sender>
void broker_broadcast(
    std::vector<int> *clients,
    BrokerRequestOwners *requests,
    Sender send
) {
    const std::vector<int> recipients = *clients;
    for (int fd : recipients) {
        if (!send(fd)) broker_drop_client(clients, requests, fd);
    }
}

bool broker_accept_probe_event(BrokerContext *context, const RuntimeEvent &event) {
    if (context == nullptr || event.magic != kRuntimeEventMagic ||
        event.protocol_version != kRuntimeProtocolVersion ||
        event.event_type != static_cast<uint32_t>(RuntimeEventType::kBindingProbe) ||
        event.pid != context->pid ||
        strnlen(event.process_name, sizeof(event.process_name)) >=
            sizeof(event.process_name) ||
        strnlen(event.il2cpp_path, sizeof(event.il2cpp_path)) >=
            sizeof(event.il2cpp_path) ||
        strnlen(event.unity_path, sizeof(event.unity_path)) >=
            sizeof(event.unity_path) ||
        strnlen(event.translation_layer, sizeof(event.translation_layer)) >=
            sizeof(event.translation_layer) ||
        strnlen(event.assembly_csharp_name, sizeof(event.assembly_csharp_name)) >=
            sizeof(event.assembly_csharp_name) ||
        strnlen(event.candidate_classes, sizeof(event.candidate_classes)) >=
            sizeof(event.candidate_classes) ||
        strcmp(event.process_name, context->process_name) != 0) return false;

    RuntimeState &state = context->state;
    state.probe_flags = event.probe_flags;
    state.runtime_message_seq += 1U;
    state.il2cpp_symbol_count = event.il2cpp_symbol_count;
    state.il2cpp_required_symbol_count = event.il2cpp_required_symbol_count;
    state.assembly_count = event.assembly_count;
    state.class_count = event.class_count;
    state.candidate_class_count = event.candidate_class_count;
    copy_string(state.il2cpp_path, sizeof(state.il2cpp_path), event.il2cpp_path);
    copy_string(state.unity_path, sizeof(state.unity_path), event.unity_path);
    copy_string(state.translation_layer, sizeof(state.translation_layer), event.translation_layer);
    copy_string(state.assembly_csharp_name, sizeof(state.assembly_csharp_name),
        event.assembly_csharp_name);
    copy_string(state.candidate_classes, sizeof(state.candidate_classes), event.candidate_classes);
    persist_runtime_state(state);
    return true;
}

bool broker_read_runtime(
    BrokerContext *context,
    std::vector<int> *clients,
    BrokerRequestOwners *requests
) {
    CompanionMessageType message_type{};
    RuntimeEvent event{};
    std::vector<uint8_t> result;
    std::vector<uint8_t> capabilities_payload;
    std::vector<uint8_t> ui_status;
    std::vector<uint8_t> observation;
    std::vector<uint8_t> bootstrap_status_payload;
    if (!read_companion_message(context->runtime_fd, &message_type, &event, &result,
            &capabilities_payload, &ui_status, &observation,
            &bootstrap_status_payload)) return false;

    if (message_type == CompanionMessageType::kRuntimeBootstrapStatus) {
        pogo_runtime_bootstrap::Status update;
        if (!pogo_runtime_bootstrap::decode(bootstrap_status_payload, &update) ||
            update.pid != context->pid || update.process_name != context->process_name ||
            update.sequence <= context->last_runtime_bootstrap_sequence ||
            update.observed_at_elapsed_ns <
                context->latest_bootstrap_status.observed_at_elapsed_ns ||
            context->latest_bootstrap_status.phase ==
                pogo_runtime_bootstrap::Phase::kFailed ||
            static_cast<uint32_t>(update.phase) <=
                static_cast<uint32_t>(context->latest_bootstrap_status.phase) ||
            (update.phase != pogo_runtime_bootstrap::Phase::kFailed &&
             static_cast<uint32_t>(update.phase) >
                static_cast<uint32_t>(context->latest_bootstrap_status.phase) + 1U)) {
            return false;
        }
        context->last_runtime_bootstrap_sequence = update.sequence;
        update.session_id = context->runtime_session_id;
        update.sequence = context->latest_bootstrap_status.sequence + 1U;
        context->latest_bootstrap_status = std::move(update);
        if (context->latest_bootstrap_status.phase !=
            pogo_runtime_bootstrap::Phase::kLiveReady) {
            context->has_runtime_ready = false;
            context->strong_identity = false;
            context->build_fingerprint.clear();
            context->runtime_capabilities.clear();
            context->latest_runtime_ui_status.clear();
        }
        broker_broadcast(clients, requests, [&](int fd) {
            return send_runtime_bootstrap_status(fd, *context);
        });
        return true;
    }

    if (message_type == CompanionMessageType::kRuntimeEvent) {
        return broker_accept_probe_event(context, event);
    }
    if (message_type == CompanionMessageType::kRuntimeResult) {
        std::string command_id;
        uint32_t phase = 0U;
        std::string error_code;
        std::string message;
        uint32_t catch_outcome_wire = 0U;
        std::string captured_pokemon_id;
        if (!parse_runtime_result(result, &command_id, &phase, &error_code, &message,
                &catch_outcome_wire, &captured_pokemon_id)) return false;
        const auto owner = requests->find(command_id);
        if (owner == requests->end()) return true;
        const int fd = owner->second;
        if (!send_command_result(fd, *context, command_id.c_str(), phase,
                error_code.c_str(), message.c_str(), catch_outcome_wire,
                captured_pokemon_id.c_str())) {
            broker_drop_client(clients, requests, fd);
        } else if (phase >= 3U) {
            requests->erase(command_id);
        }
        return true;
    }
    if (message_type == CompanionMessageType::kRuntimeCapabilities) {
        std::string fingerprint;
        std::vector<std::string> parsed;
        bool strong = false;
        if (!parse_runtime_capabilities(capabilities_payload, &fingerprint, &parsed,
                &strong)) return false;
        if (context->latest_bootstrap_status.phase !=
                pogo_runtime_bootstrap::Phase::kLiveReady ||
            !strong || fingerprint.empty()) return true;
        std::sort(parsed.begin(), parsed.end());
        if (std::adjacent_find(parsed.begin(), parsed.end()) != parsed.end() ||
            fingerprint != context->latest_bootstrap_status.game_build ||
            parsed != context->latest_bootstrap_status.capabilities) return false;
        context->has_runtime_ready = true;
        context->strong_identity = strong;
        context->build_fingerprint = fingerprint;
        context->runtime_capabilities = parsed;
        broker_broadcast(clients, requests, [&](int fd) {
            return send_runtime_ready_with_values(fd, *context, fingerprint.c_str(),
                parsed, strong);
        });
        return true;
    }
    if (message_type == CompanionMessageType::kRuntimeUiStatus) {
        if (context->latest_bootstrap_status.phase !=
            pogo_runtime_bootstrap::Phase::kLiveReady) return true;
        if (ui_status.empty() || ui_status.size() > kBridgeHardMessageBytes) return false;
        std::vector<uint8_t> payload;
        if (!rewrite_runtime_ui_status_session(ui_status,
                context->runtime_session_id, &payload)) return false;
        context->latest_runtime_ui_status = ui_status;
        broker_broadcast(clients, requests, [&](int fd) {
            return send_bridge_frame(fd, 10U, context->next_message_seq++, payload);
        });
        return true;
    }
    if (message_type == CompanionMessageType::kRuntimeObservation) {
        if (context->latest_bootstrap_status.phase !=
            pogo_runtime_bootstrap::Phase::kLiveReady) return true;
        broker_broadcast(clients, requests, [&](int fd) {
            return forward_runtime_observation(fd, *context, observation);
        });
        return true;
    }
    return false;
}

bool broker_forward_controller_command(
    BrokerContext *context,
    int fd,
    uint64_t *last_sequence,
    BrokerRequestOwners *requests,
    bool *runtime_alive
) {
    uint16_t message_type = 0U;
    uint64_t message_seq = 0U;
    std::vector<uint8_t> payload;
    if (!read_bridge_frame(fd, &message_type, &message_seq, &payload)) return false;
    if (message_seq <= *last_sequence) return false;
    *last_sequence = message_seq;
    if (message_type != kBridgeCommandType) return true;

    size_t offset = 0U;
    uint32_t payload_version = 0U;
    std::string session_id;
    std::string command_id;
    if (!read_be32(payload, &offset, &payload_version) || payload_version != 1U ||
        !read_string(payload, &offset, &session_id) ||
        !read_string(payload, &offset, &command_id) || command_id.empty() ||
        session_id != context->runtime_session_id || requests->size() >= 1024U ||
        requests->find(command_id) != requests->end()) return false;
    if (context->latest_bootstrap_status.phase ==
            pogo_runtime_bootstrap::Phase::kFailed || context->runtime_fd < 0) {
        return send_command_result(fd, *context, command_id.c_str(),
            kCommandPhaseRejected, "runtime_failed",
            context->latest_bootstrap_status.error_code.c_str());
    }
    if (!send_runtime_command(context->runtime_fd, message_type, message_seq, payload)) {
        *runtime_alive = false;
        return false;
    }
    requests->emplace(command_id, fd);
    return true;
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
