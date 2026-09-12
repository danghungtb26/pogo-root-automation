// Native mirror of the Kotlin-owned auto-discard policy.

constexpr size_t kMaxRuntimeDiscardLimits = pogo_runtime_discard_config::kMaxLimits;

struct RuntimeDiscardConfig {
    uint64_t revision = 0U;
    bool configured = false;
    bool auto_discard = false;
    pogo_runtime_discard_config::Limit limits[kMaxRuntimeDiscardLimits]{};
    uint32_t limit_count = 0U;
};

std::mutex g_runtime_discard_config_mutex;
RuntimeDiscardConfig g_runtime_discard_config;

RuntimeDiscardConfig runtime_discard_config_snapshot() {
    std::lock_guard<std::mutex> lock(g_runtime_discard_config_mutex);
    return g_runtime_discard_config;
}

void reset_runtime_discard_config() {
    std::lock_guard<std::mutex> lock(g_runtime_discard_config_mutex);
    g_runtime_discard_config = RuntimeDiscardConfig{};
}

bool runtime_discard_config_equals(
    const RuntimeDiscardConfig &current,
    const pogo_runtime_discard_config::Request &request
) {
    if (!current.configured || current.auto_discard != request.auto_discard ||
        current.limit_count != request.limit_count) return false;
    for (uint32_t index = 0U; index < current.limit_count; ++index) {
        if (current.limits[index].item_id != request.limits[index].item_id ||
            current.limits[index].max_count != request.limits[index].max_count) return false;
    }
    return true;
}

bool runtime_discard_config_find_limit(
    const RuntimeDiscardConfig &config,
    int32_t item_id,
    int32_t *max_count
) {
    if (max_count == nullptr || item_id <= 0) return false;
    for (uint32_t index = 0U; index < config.limit_count; ++index) {
        if (config.limits[index].item_id != item_id) continue;
        *max_count = config.limits[index].max_count;
        return true;
    }
    return false;
}

bool apply_runtime_discard_config(
    ProbeContext &context,
    const std::vector<uint8_t> &command
) {
    pogo_runtime_discard_config::Request request;
    if (!pogo_runtime_discard_config::parse(command, &request)) return false;

    if (!g_runtime_active.load()) {
        return send_runtime_rejected(
            context.fd,
            request.request_id.c_str(),
            "runtime_inactive",
            "runtime host must be STARTed before discard config"
        );
    }
    if (request.pid != static_cast<uint32_t>(context.pid) ||
        request.process_name != context.process_name ||
        request.package_name != context.process_name) {
        return send_runtime_rejected(
            context.fd,
            request.request_id.c_str(),
            "runtime_identity_mismatch",
            "discard config identity does not match target process"
        );
    }
    if (request.build_fingerprint != kVerifiedBuildFingerprint) {
        return send_runtime_rejected(
            context.fd,
            request.request_id.c_str(),
            "build_fingerprint_mismatch",
            "discard config build fingerprint is not verified"
        );
    }
    if (request.expires_at_elapsed_ns <= static_cast<uint64_t>(now_elapsed_nanos())) {
        return send_runtime_rejected(
            context.fd,
            request.request_id.c_str(),
            "config_expired",
            "discard config expired before apply"
        );
    }

    bool applied = false;
    bool conflict = false;
    {
        std::lock_guard<std::mutex> lock(g_runtime_discard_config_mutex);
        if (request.config_revision < g_runtime_discard_config.revision) {
            conflict = true;
        } else if (request.config_revision == g_runtime_discard_config.revision &&
            g_runtime_discard_config.configured &&
            !runtime_discard_config_equals(g_runtime_discard_config, request)) {
            conflict = true;
        } else if (request.config_revision > g_runtime_discard_config.revision) {
            g_runtime_discard_config.revision = request.config_revision;
            g_runtime_discard_config.configured = true;
            g_runtime_discard_config.auto_discard = request.auto_discard;
            g_runtime_discard_config.limit_count = request.limit_count;
            for (uint32_t index = 0U; index < request.limit_count; ++index) {
                g_runtime_discard_config.limits[index] = request.limits[index];
            }
            applied = true;
        }
    }

    if (conflict) {
        const RuntimeDiscardConfig current = runtime_discard_config_snapshot();
        const bool stale = request.config_revision < current.revision;
        return send_runtime_rejected(
            context.fd,
            request.request_id.c_str(),
            stale ? "stale_config_revision" : "config_revision_conflict",
            stale
                ? "discard config revision is older than the applied revision"
                : "discard config revision is already used by a different snapshot"
        );
    }

    const RuntimeDiscardConfig current = runtime_discard_config_snapshot();
    if (applied) reset_runtime_discard_state(context);
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "discard config %s revision=%llu auto=%d limits=%u",
        applied ? "applied" : "already applied",
        static_cast<unsigned long long>(current.revision),
        current.auto_discard ? 1 : 0,
        current.limit_count
    );
    char ack_message[128];
    snprintf(
        ack_message,
        sizeof(ack_message),
        "discard config %s revision=%llu",
        applied ? "applied" : "already applied",
        static_cast<unsigned long long>(current.revision)
    );
    return send_runtime_control_completed(
        context.fd,
        request.request_id.c_str(),
        ack_message
    );
}
