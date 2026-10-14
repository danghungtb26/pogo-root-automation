void persist_runtime_state(const RuntimeState &state) {
    if (mkdir(kStateDirectory, 0700) != 0 && errno != EEXIST) {
        __android_log_print(
            ANDROID_LOG_ERROR,
            kLogTag,
            "cannot create runtime state directory: errno=%d",
            errno
        );
        return;
    }
    char temp_path[192]{};
    snprintf(temp_path, sizeof(temp_path), "%s.%d", kTempStatePrefix, state.pid);
    const int fd = open(temp_path, O_CREAT | O_WRONLY | O_TRUNC | O_CLOEXEC, 0600);
    if (fd < 0) {
        __android_log_print(
            ANDROID_LOG_ERROR,
            kLogTag,
            "cannot open runtime state temp file pid=%d: errno=%d",
            state.pid,
            errno
        );
        return;
    }

    const bool probe_complete = (state.probe_flags & kProbeComplete) != 0U;
    const bool il2cpp_loaded = (state.probe_flags & kIl2cppLoaded) != 0U;
    const bool unity_loaded = (state.probe_flags & kUnityLoaded) != 0U;
    const bool api_available = (state.probe_flags & kIl2cppApiAvailable) != 0U;
    const bool assembly_survey_complete = (state.probe_flags & kAssemblySurveyComplete) != 0U;
    const bool assembly_csharp_found = (state.probe_flags & kAssemblyCSharpFound) != 0U;
    const bool class_survey_complete = (state.probe_flags & kClassSurveyComplete) != 0U;

    dprintf(fd, "protocol=%u\n", state.protocol_version);
    dprintf(fd, "pid=%d\n", state.pid);
    dprintf(fd, "process=%s\n", state.process_name);
    dprintf(fd, "runtime_session_id=%s\n", state.runtime_session_id);
    dprintf(fd, "runtime_message_seq=%llu\n", static_cast<unsigned long long>(state.runtime_message_seq));
    dprintf(fd, "seen_at_epoch_ms=%lld\n", now_epoch_millis());
    dprintf(fd, "native_probe_state=%s\n", probe_complete ? "complete" : "pending");
    dprintf(fd, "native_libil2cpp_loaded=%d\n", il2cpp_loaded ? 1 : 0);
    dprintf(fd, "native_libunity_loaded=%d\n", unity_loaded ? 1 : 0);
    dprintf(fd, "native_il2cpp_api_available=%d\n", api_available ? 1 : 0);
    dprintf(fd, "native_il2cpp_symbol_count=%u\n", state.il2cpp_symbol_count);
    dprintf(fd, "native_il2cpp_required_symbol_count=%u\n", state.il2cpp_required_symbol_count);
    dprintf(fd, "native_libil2cpp_path=%s\n", state.il2cpp_path);
    dprintf(fd, "native_libunity_path=%s\n", state.unity_path);
    dprintf(fd, "native_translation_layer=%s\n", state.translation_layer);
    dprintf(fd, "native_assembly_survey_state=%s\n", assembly_survey_complete ? "complete" : "unavailable");
    dprintf(fd, "native_assembly_count=%u\n", state.assembly_count);
    dprintf(fd, "native_assembly_csharp_found=%d\n", assembly_csharp_found ? 1 : 0);
    dprintf(fd, "native_assembly_csharp_name=%s\n", state.assembly_csharp_name);
    dprintf(fd, "native_class_survey_state=%s\n", class_survey_complete ? "complete" : "unavailable");
    dprintf(fd, "native_class_count=%u\n", state.class_count);
    dprintf(fd, "native_candidate_class_count=%u\n", state.candidate_class_count);
    dprintf(fd, "native_candidate_classes=%s\n", state.candidate_classes);
    fsync(fd);
    close(fd);
    if (rename(temp_path, kStatePath) != 0) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "cannot publish runtime state: errno=%d", errno);
    } else {
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "runtime state published pid=%d process=%s",
            state.pid,
            state.process_name
        );
    }
}

void companion_handler(int fd) {
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "companion_handler started fd=%d pid=%d",
        fd,
        getpid()
    );
    RuntimeEvent event{};
    if (!read_full(fd, &event, sizeof(event)) ||
        event.magic != kRuntimeEventMagic ||
        event.protocol_version != kRuntimeProtocolVersion ||
        event.event_type != static_cast<uint32_t>(RuntimeEventType::kTargetAttached) ||
        event.pid <= 0 ||
        strnlen(event.process_name, sizeof(event.process_name)) >=
            sizeof(event.process_name) ||
        !is_target_process(event.process_name)) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag,
            "companion rejected initial target event");
        close(fd);
        return;
    }

    RuntimeState state{};
    state.protocol_version = event.protocol_version;
    state.pid = event.pid;
    copy_string(state.process_name, sizeof(state.process_name), event.process_name);
    state.runtime_message_seq = 1U;
    state.attach_epoch_ms = static_cast<uint64_t>(now_epoch_millis());
    state.attach_elapsed_ns = static_cast<uint64_t>(now_elapsed_nanos());
    snprintf(state.runtime_session_id, sizeof(state.runtime_session_id), "%d-%lld-%llu",
        state.pid, now_epoch_millis(),
        static_cast<unsigned long long>(g_session_counter.fetch_add(1U) + 1U));
    persist_runtime_state(state);
    // Send a verified read-only core descriptor before the broker takes
    // ownership of the runtime stream.
    pogo_core_handoff::Reply handoff;
    const int core_fd = pogo_core_handoff::open_verified_core(&handoff);
    const bool sent = pogo_core_handoff::send_reply(fd, handoff, core_fd);
    if (core_fd >= 0) close(core_fd);
    if (!sent) {
        close(fd);
        return;
    }
    // Keep one owned descriptor after this handler returns. The original is
    // closed before the broker starts reading, so the duplicate has no peer
    // reader on the companion side.
    const int broker_fd = dup(fd);
    close(fd);
    if (broker_fd >= 0) start_runtime_bridge_broker(state, broker_fd);
}
