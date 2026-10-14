bool broker_replay_snapshot(int fd, BrokerContext &context) {
    if (!send_runtime_bootstrap_status(fd, context)) return false;
    if (context.has_runtime_ready && !send_runtime_ready_with_values(
            fd, context, context.build_fingerprint.c_str(),
            context.runtime_capabilities, context.strong_identity)) return false;
    if (!context.latest_runtime_ui_status.empty()) {
        std::vector<uint8_t> payload;
        if (!rewrite_runtime_ui_status_session(
                context.latest_runtime_ui_status, context.runtime_session_id, &payload) ||
            !send_bridge_frame(fd, 10U, context.next_message_seq++, payload)) return false;
    }
    return true;
}

void *runtime_bridge_broker_thread(void *opaque_context) {
    auto *context = static_cast<BrokerContext *>(opaque_context);
    if (context == nullptr) return nullptr;

    int server = -1;
    {
        std::unique_lock<std::mutex> lock(g_broker_mutex);
        const bool listener_released = g_broker_cv.wait_for(
            lock, std::chrono::seconds(5), [&] {
                return g_broker_bound_session_id.empty() ||
                    g_broker_session_id != context->runtime_session_id;
            }
        );
        if (!listener_released) {
            lock.unlock();
            close(context->runtime_fd);
            finish_runtime_bridge_broker(context->runtime_session_id);
            delete context;
            return nullptr;
        }
        if (g_broker_session_id != context->runtime_session_id) {
            close(context->runtime_fd);
            delete context;
            return nullptr;
        }
        server = make_bridge_server();
        if (server >= 0) g_broker_bound_session_id = context->runtime_session_id;
    }
    if (server < 0) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag,
            "cannot create runtime bridge socket");
        close(context->runtime_fd);
        finish_runtime_bridge_broker(context->runtime_session_id);
        delete context;
        return nullptr;
    }

    std::vector<int> clients;
    BrokerRequestOwners requests;
    std::unordered_map<int, uint64_t> client_sequences;
    while (kill(context->pid, 0) == 0 && broker_is_current(context->runtime_session_id)) {
        std::vector<pollfd> descriptors;
        descriptors.reserve(2U + clients.size());
        descriptors.push_back({server, POLLIN, 0});
        descriptors.push_back({context->runtime_fd, POLLIN, 0});
        for (int fd : clients) descriptors.push_back({fd, POLLIN, 0});
        const int polled = poll(descriptors.data(), descriptors.size(), 1000);
        if (polled < 0 && errno == EINTR) continue;
        if (polled < 0 || !broker_is_current(context->runtime_session_id)) break;
        if (polled == 0) continue;

        const short runtime_events = descriptors[1].revents;
        if ((runtime_events & POLLIN) != 0 &&
            !broker_read_runtime(context, &clients, &requests)) break;
        if ((runtime_events & (POLLERR | POLLHUP | POLLNVAL)) != 0 &&
            (runtime_events & POLLIN) == 0) {
            if (context->latest_bootstrap_status.phase !=
                    pogo_runtime_bootstrap::Phase::kFailed) break;
            close(context->runtime_fd);
            context->runtime_fd = -1;
        }

        if ((descriptors[0].revents & POLLIN) != 0) {
            const int client = accept4(server, nullptr, nullptr, SOCK_CLOEXEC);
            if (client >= 0) {
                const timeval send_timeout{0, 250000};
                const bool accepted = clients.size() < 4U &&
                    authorized_controller(client) &&
                    setsockopt(client, SOL_SOCKET, SO_SNDTIMEO,
                        &send_timeout, sizeof(send_timeout)) == 0 &&
                    broker_replay_snapshot(client, *context);
                if (accepted) {
                    clients.push_back(client);
                    client_sequences[client] = 0U;
                } else {
                    close(client);
                }
            }
        }

        bool stream_failed = false;
        for (size_t index = 2U; index < descriptors.size(); ++index) {
            const int fd = descriptors[index].fd;
            if (std::find(clients.begin(), clients.end(), fd) == clients.end()) continue;
            const short events = descriptors[index].revents;
            bool keep = (events & (POLLERR | POLLHUP | POLLNVAL)) == 0;
            bool runtime_alive = true;
            if (keep && (events & POLLIN) != 0) {
                keep = broker_forward_controller_command(context, fd,
                    &client_sequences[fd], &requests, &runtime_alive);
            }
            if (!keep) {
                broker_drop_client(&clients, &requests, fd);
                client_sequences.erase(fd);
            }
            if (!runtime_alive) {
                stream_failed = true;
                break;
            }
        }
        if (stream_failed) break;
    }

    for (int fd : clients) close(fd);
    close(server);
    {
        std::lock_guard<std::mutex> lock(g_broker_mutex);
        if (g_broker_bound_session_id == context->runtime_session_id) {
            g_broker_bound_session_id.clear();
            g_broker_cv.notify_all();
        }
    }
    if (context->runtime_fd >= 0) close(context->runtime_fd);
    finish_runtime_bridge_broker(context->runtime_session_id);
    delete context;
    return nullptr;
}

void start_runtime_bridge_broker(const RuntimeState &state, int runtime_fd) {
    if (runtime_fd < 0 || state.runtime_session_id[0] == '\0') {
        if (runtime_fd >= 0) close(runtime_fd);
        return;
    }
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
    context->state = state;
    copy_string(context->process_name, sizeof(context->process_name), state.process_name);
    copy_string(context->runtime_session_id, sizeof(context->runtime_session_id),
        state.runtime_session_id);
    context->latest_bootstrap_status.session_id = state.runtime_session_id;
    context->latest_bootstrap_status.pid = state.pid;
    context->latest_bootstrap_status.process_name = state.process_name;
    context->latest_bootstrap_status.sequence = 1U;
    context->latest_bootstrap_status.phase = pogo_runtime_bootstrap::Phase::kCreated;
    context->latest_bootstrap_status.observed_at_epoch_ms =
        state.attach_epoch_ms;
    context->latest_bootstrap_status.observed_at_elapsed_ns =
        state.attach_elapsed_ns;
    pthread_t thread{};
    if (pthread_create(&thread, nullptr, runtime_bridge_broker_thread, context) != 0) {
        close(runtime_fd);
        delete context;
        finish_runtime_bridge_broker(state.runtime_session_id);
        return;
    }
    pthread_detach(thread);
}
