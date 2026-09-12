// Native transfer policy mirror. The Android app persists the settings; this
// snapshot is the only policy source consulted by the native transfer module
// during the current runtime session.

struct RuntimeTransferConfig {
    uint64_t revision = 0U;
    bool configured = false;
    bool auto_transfer = false;
    bool keep_unknown_iv = true;
    double minimum_iv_percent_to_keep = 80.0;
    bool keep_shiny = true;
    bool keep_hundo = true;
    bool keep_special_background = true;
    bool keep_favorite = true;
    bool keep_legendary = true;
    bool keep_mythical = true;
};

std::mutex g_runtime_transfer_config_mutex;
RuntimeTransferConfig g_runtime_transfer_config;

RuntimeTransferConfig runtime_transfer_config_snapshot() {
    std::lock_guard<std::mutex> lock(g_runtime_transfer_config_mutex);
    return g_runtime_transfer_config;
}

void reset_runtime_transfer_config() {
    std::lock_guard<std::mutex> lock(g_runtime_transfer_config_mutex);
    g_runtime_transfer_config = RuntimeTransferConfig{};
}

bool runtime_transfer_config_equals(
    const RuntimeTransferConfig &current,
    const pogo_runtime_transfer_config::Request &request
) {
    return current.configured &&
        current.auto_transfer == request.auto_transfer &&
        current.keep_unknown_iv == request.keep_unknown_iv &&
        current.minimum_iv_percent_to_keep == request.minimum_iv_percent_to_keep &&
        current.keep_shiny == request.keep_shiny &&
        current.keep_hundo == request.keep_hundo &&
        current.keep_special_background == request.keep_special_background &&
        current.keep_favorite == request.keep_favorite &&
        current.keep_legendary == request.keep_legendary &&
        current.keep_mythical == request.keep_mythical;
}

bool apply_runtime_transfer_config(
    ProbeContext &context,
    const std::vector<uint8_t> &command
) {
    pogo_runtime_transfer_config::Request request;
    if (!pogo_runtime_transfer_config::parse(command, &request)) return false;

    if (!g_runtime_active.load()) {
        return send_runtime_rejected(
            context.fd,
            request.request_id.c_str(),
            "runtime_inactive",
            "runtime host must be STARTed before transfer config"
        );
    }
    if (request.pid != static_cast<uint32_t>(context.pid) ||
        request.process_name != context.process_name ||
        request.package_name != context.process_name) {
        return send_runtime_rejected(
            context.fd,
            request.request_id.c_str(),
            "runtime_identity_mismatch",
            "transfer config identity does not match target process"
        );
    }
    if (request.build_fingerprint != kVerifiedBuildFingerprint) {
        return send_runtime_rejected(
            context.fd,
            request.request_id.c_str(),
            "build_fingerprint_mismatch",
            "transfer config build fingerprint is not verified"
        );
    }
    if (request.expires_at_elapsed_ns <= static_cast<uint64_t>(now_elapsed_nanos())) {
        return send_runtime_rejected(
            context.fd,
            request.request_id.c_str(),
            "config_expired",
            "transfer config expired before apply"
        );
    }

    bool applied = false;
    bool conflict = false;
    {
        std::lock_guard<std::mutex> lock(g_runtime_transfer_config_mutex);
        if (request.config_revision < g_runtime_transfer_config.revision) {
            conflict = true;
        } else if (request.config_revision == g_runtime_transfer_config.revision &&
            g_runtime_transfer_config.configured &&
            !runtime_transfer_config_equals(g_runtime_transfer_config, request)) {
            conflict = true;
        } else if (request.config_revision > g_runtime_transfer_config.revision) {
            g_runtime_transfer_config.revision = request.config_revision;
            g_runtime_transfer_config.configured = true;
            g_runtime_transfer_config.auto_transfer = request.auto_transfer;
            g_runtime_transfer_config.keep_unknown_iv = request.keep_unknown_iv;
            g_runtime_transfer_config.minimum_iv_percent_to_keep =
                request.minimum_iv_percent_to_keep;
            g_runtime_transfer_config.keep_shiny = request.keep_shiny;
            g_runtime_transfer_config.keep_hundo = request.keep_hundo;
            g_runtime_transfer_config.keep_special_background = request.keep_special_background;
            g_runtime_transfer_config.keep_favorite = request.keep_favorite;
            g_runtime_transfer_config.keep_legendary = request.keep_legendary;
            g_runtime_transfer_config.keep_mythical = request.keep_mythical;
            applied = true;
        }
    }

    if (conflict) {
        const RuntimeTransferConfig current = runtime_transfer_config_snapshot();
        const bool stale = request.config_revision < current.revision;
        return send_runtime_rejected(
            context.fd,
            request.request_id.c_str(),
            stale ? "stale_config_revision" : "config_revision_conflict",
            stale
                ? "transfer config revision is older than the applied revision"
                : "transfer config revision is already used by a different snapshot"
        );
    }

    const RuntimeTransferConfig current = runtime_transfer_config_snapshot();
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "transfer config %s revision=%llu auto=%d min_iv=%.2f keep_unknown=%d "
        "shiny=%d hundo=%d background=%d favorite=%d legendary=%d mythical=%d",
        applied ? "applied" : "already applied",
        static_cast<unsigned long long>(current.revision),
        current.auto_transfer ? 1 : 0,
        current.minimum_iv_percent_to_keep,
        current.keep_unknown_iv ? 1 : 0,
        current.keep_shiny ? 1 : 0,
        current.keep_hundo ? 1 : 0,
        current.keep_special_background ? 1 : 0,
        current.keep_favorite ? 1 : 0,
        current.keep_legendary ? 1 : 0,
        current.keep_mythical ? 1 : 0
    );
    char ack_message[128];
    snprintf(
        ack_message,
        sizeof(ack_message),
        "transfer config %s revision=%llu",
        applied ? "applied" : "already applied",
        static_cast<unsigned long long>(current.revision)
    );
    return send_runtime_control_completed(
        context.fd,
        request.request_id.c_str(),
        ack_message
    );
}
