// Native runtime mirror for catch_spin settings. Persistence remains owned by
// the Android repository; this store is reset with the runtime host and is only
// trusted after the full, identity-checked CONFIG_SET frame is applied.
struct RuntimeCatchSpinConfig {
    uint64_t revision = 0U;
    bool configured = false;
    bool armed = false;
    bool auto_catch = false;
    bool auto_spin = false;
    bool auto_encounter = false;
    bool catch_all = true;
    uint64_t spin_settle_delay_ms = 0U;
    uint64_t catch_settle_delay_ms = 0U;
};

// Defined by coordinator.inc, which is included immediately after this store.
void reset_runtime_catch_spin_coordinator();

std::mutex g_runtime_catch_spin_config_mutex;
RuntimeCatchSpinConfig g_runtime_catch_spin_config;

bool send_runtime_control_completed(
    int fd,
    const char *request_id,
    const char *message
);

bool send_runtime_rejected(
    int fd,
    const char *command_id,
    const char *error_code,
    const char *message
);

extern std::atomic<bool> g_runtime_active;

RuntimeCatchSpinConfig runtime_catch_spin_config_snapshot() {
    std::lock_guard<std::mutex> lock(g_runtime_catch_spin_config_mutex);
    return g_runtime_catch_spin_config;
}

bool runtime_catch_spin_config_allows_action(
    uint32_t action_tag,
    RuntimeActionResult *result
) {
    if (result == nullptr) return false;
    *result = RuntimeActionResult{};
    const RuntimeCatchSpinConfig config = runtime_catch_spin_config_snapshot();
    if (!config.configured) {
        result->error_code = "catch_spin_config_missing";
        result->message = "catch_spin config must be applied before gameplay actions";
        return false;
    }
    if (!config.armed) {
        result->error_code = "catch_spin_disarmed";
        result->message = "catch_spin is disarmed";
        return false;
    }
    const bool allowed = action_tag == 12U
        ? config.auto_catch && config.catch_all
        : action_tag == 4U && config.auto_spin;
    if (!allowed) {
        result->error_code = "catch_spin_action_disabled";
        result->message = "the requested catch_spin action is disabled by config";
        return false;
    }
    return true;
}

void reset_runtime_catch_spin_config() {
    {
        std::lock_guard<std::mutex> lock(g_runtime_catch_spin_config_mutex);
        g_runtime_catch_spin_config = RuntimeCatchSpinConfig{};
    }
    reset_runtime_catch_spin_coordinator();
}

bool apply_runtime_catch_spin_config(
    ProbeContext &context,
    const std::vector<uint8_t> &command
) {
    pogo_runtime_catch_spin_config::Request request;
    if (!pogo_runtime_catch_spin_config::parse(command, &request)) return false;

    if (!g_runtime_active.load()) {
        return send_runtime_rejected(
            context.fd,
            request.request_id.c_str(),
            "runtime_inactive",
            "runtime host must be STARTed before catch_spin config"
        );
    }
    if (request.pid != static_cast<uint32_t>(context.pid) ||
        request.process_name != context.process_name ||
        request.package_name != context.process_name) {
        return send_runtime_rejected(
            context.fd,
            request.request_id.c_str(),
            "runtime_identity_mismatch",
            "catch_spin config identity does not match target process"
        );
    }
    if (request.build_fingerprint != kVerifiedBuildFingerprint) {
        return send_runtime_rejected(
            context.fd,
            request.request_id.c_str(),
            "build_fingerprint_mismatch",
            "catch_spin config build fingerprint is not verified"
        );
    }
    if (request.expires_at_elapsed_ns <= static_cast<uint64_t>(now_elapsed_nanos())) {
        return send_runtime_rejected(
            context.fd,
            request.request_id.c_str(),
            "config_expired",
            "catch_spin config expired before apply"
        );
    }

    bool applied = false;
    bool conflict = false;
    {
        std::lock_guard<std::mutex> lock(g_runtime_catch_spin_config_mutex);
        if (request.config_revision < g_runtime_catch_spin_config.revision) {
            conflict = true;
        } else if (request.config_revision == g_runtime_catch_spin_config.revision &&
            g_runtime_catch_spin_config.configured &&
            (request.armed != g_runtime_catch_spin_config.armed ||
                request.auto_catch != g_runtime_catch_spin_config.auto_catch ||
                request.auto_spin != g_runtime_catch_spin_config.auto_spin ||
                request.auto_encounter != g_runtime_catch_spin_config.auto_encounter ||
                request.catch_all != g_runtime_catch_spin_config.catch_all ||
                request.spin_settle_delay_ms != g_runtime_catch_spin_config.spin_settle_delay_ms ||
                request.catch_settle_delay_ms != g_runtime_catch_spin_config.catch_settle_delay_ms)) {
            conflict = true;
        } else if (request.config_revision > g_runtime_catch_spin_config.revision) {
            g_runtime_catch_spin_config.revision = request.config_revision;
            g_runtime_catch_spin_config.configured = true;
            g_runtime_catch_spin_config.armed = request.armed;
            g_runtime_catch_spin_config.auto_catch = request.auto_catch;
            g_runtime_catch_spin_config.auto_spin = request.auto_spin;
            g_runtime_catch_spin_config.auto_encounter = request.auto_encounter;
            g_runtime_catch_spin_config.catch_all = request.catch_all;
            g_runtime_catch_spin_config.spin_settle_delay_ms = request.spin_settle_delay_ms;
            g_runtime_catch_spin_config.catch_settle_delay_ms = request.catch_settle_delay_ms;
            applied = true;
        }
    }

    if (conflict) {
        const RuntimeCatchSpinConfig current = runtime_catch_spin_config_snapshot();
        const bool stale = request.config_revision < current.revision;
        return send_runtime_rejected(
            context.fd,
            request.request_id.c_str(),
            stale ? "stale_config_revision" : "config_revision_conflict",
            stale
                ? "catch_spin config revision is older than the applied revision"
                : "catch_spin config revision is already used by a different snapshot"
        );
    }

    const RuntimeCatchSpinConfig current = runtime_catch_spin_config_snapshot();
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "catch_spin config %s revision=%llu armed=%d catch=%d spin=%d encounter=%d catch_all=%d spin_delay_ms=%llu catch_delay_ms=%llu",
        applied ? "applied" : "already applied",
        static_cast<unsigned long long>(current.revision),
        current.armed ? 1 : 0,
        current.auto_catch ? 1 : 0,
        current.auto_spin ? 1 : 0,
        current.auto_encounter ? 1 : 0,
        current.catch_all ? 1 : 0,
        static_cast<unsigned long long>(current.spin_settle_delay_ms),
        static_cast<unsigned long long>(current.catch_settle_delay_ms)
    );
    if (applied) reset_runtime_catch_spin_coordinator();
    char ack_message[128];
    snprintf(
        ack_message,
        sizeof(ack_message),
        "catch_spin config %s revision=%llu",
        applied ? "applied" : "already applied",
        static_cast<unsigned long long>(current.revision)
    );
    return send_runtime_control_completed(
        context.fd,
        request.request_id.c_str(),
        ack_message
    );
}
