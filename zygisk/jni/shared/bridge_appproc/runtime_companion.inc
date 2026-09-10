bool probe_il2cpp_runtime(RuntimeEvent *event, Il2CppApi *resolved_api) {
    if (event == nullptr || event->il2cpp_path[0] == '\0') return false;

    Il2CppApi api{};
    const bool core_api = resolve_il2cpp_api(event->il2cpp_path, &api);
    event->il2cpp_symbol_count = api.symbol_count;
    event->il2cpp_required_symbol_count = kRequiredIl2cppCoreSymbolCount;
    if (!core_api) {
        close_il2cpp_api(&api);
        return false;
    }

    if (resolved_api != nullptr) {
        *resolved_api = api;
        resolved_api->handle = nullptr;
        resolved_api->owns_handle = false;
    }

    event->probe_flags |= kIl2cppApiAvailable;
    // Export discovery is safe as a read-only ELF operation, but calling the
    // managed IL2CPP domain/assembly APIs from a Zygisk thread during Unity's
    // il2cpp_init window is not. Pokémon GO aborts or dereferences null in
    // that window. Defer assembly/class survey until a verified runtime
    // lifecycle hook exists; never trade game stability for diagnostics.
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "IL2CPP exports discovered; managed survey deferred until runtime is initialized"
    );
    close_il2cpp_api(&api);
    return true;
}

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
    RuntimeState state{};
    RuntimeEvent event{};
    while (read_full(fd, &event, sizeof(event))) {
        if (event.magic != kRuntimeEventMagic || event.protocol_version != kRuntimeProtocolVersion) {
            __android_log_print(
                ANDROID_LOG_ERROR,
                kLogTag,
                "companion_handler rejected event magic=0x%08x protocol=%u",
                event.magic,
                event.protocol_version
            );
            return;
        }
        event.process_name[sizeof(event.process_name) - 1U] = '\0';
        event.il2cpp_path[sizeof(event.il2cpp_path) - 1U] = '\0';
        event.unity_path[sizeof(event.unity_path) - 1U] = '\0';
        event.translation_layer[sizeof(event.translation_layer) - 1U] = '\0';
        event.assembly_csharp_name[sizeof(event.assembly_csharp_name) - 1U] = '\0';
        event.candidate_classes[sizeof(event.candidate_classes) - 1U] = '\0';

        if (event.event_type == static_cast<uint32_t>(RuntimeEventType::kTargetAttached)) {
            __android_log_print(
                ANDROID_LOG_INFO,
                kLogTag,
                "companion TargetAttached pid=%d process=%s",
                event.pid,
                event.process_name
            );
            state = RuntimeState{};
            state.protocol_version = event.protocol_version;
            state.pid = event.pid;
            copy_string(state.process_name, sizeof(state.process_name), event.process_name);
            state.runtime_message_seq = 1U;
            snprintf(
                state.runtime_session_id,
                sizeof(state.runtime_session_id),
                "%d-%lld",
                state.pid,
                now_epoch_millis()
            );
            const size_t session_length = strnlen(state.runtime_session_id, sizeof(state.runtime_session_id));
            if (session_length < sizeof(state.runtime_session_id) - 1U) {
                snprintf(
                    state.runtime_session_id + session_length,
                    sizeof(state.runtime_session_id) - session_length,
                    "-%llu",
                    static_cast<unsigned long long>(++g_session_counter)
                );
            }
            persist_runtime_state(state);
            continue;
        }
        if (event.event_type == static_cast<uint32_t>(RuntimeEventType::kBindingProbe) && event.pid == state.pid) {
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
            copy_string(state.assembly_csharp_name, sizeof(state.assembly_csharp_name), event.assembly_csharp_name);
            copy_string(state.candidate_classes, sizeof(state.candidate_classes), event.candidate_classes);
            persist_runtime_state(state);
            if ((event.probe_flags & kProbeComplete) != 0U) {
                const int broker_fd = dup(fd);
                if (broker_fd >= 0) start_runtime_bridge_broker(state, broker_fd);
                close(fd);
                return;
            }
        }
    }
}

constexpr uint32_t kCommandPhaseStarted = 2U;
constexpr uint32_t kCommandPhaseRejected = 4U;
constexpr uint32_t kCommandPhaseCompleted = 3U;
constexpr uint32_t kCommandPhaseIndeterminate = 7U;
