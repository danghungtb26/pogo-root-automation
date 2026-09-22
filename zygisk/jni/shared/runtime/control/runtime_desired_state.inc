namespace {
std::mutex g_runtime_desired_state_mutex;
bool g_runtime_desired_state_present = false;
std::string g_runtime_desired_state_session_id;
pogo_runtime_desired_state::State g_runtime_desired_state;
uint64_t g_runtime_desired_state_received_at_ns = 0U;
uint64_t g_runtime_desired_state_expires_at_ns = 0U;

bool runtime_desired_state_equal(
    const pogo_runtime_desired_state::State &left,
    const pogo_runtime_desired_state::State &right
) {
    return left.config_revision == right.config_revision &&
        left.enabled == right.enabled &&
        left.catch_spin_armed == right.catch_spin_armed &&
        left.auto_catch == right.auto_catch &&
        left.auto_spin == right.auto_spin &&
        left.auto_encounter == right.auto_encounter &&
        left.catch_all == right.catch_all &&
        left.auto_walk_to_fort == right.auto_walk_to_fort &&
        left.spin_settle_delay_ms == right.spin_settle_delay_ms &&
        left.catch_settle_delay_ms == right.catch_settle_delay_ms &&
        left.auto_discard == right.auto_discard &&
        left.discard_limits == right.discard_limits &&
        left.auto_transfer == right.auto_transfer &&
        left.minimum_iv_percent_to_keep == right.minimum_iv_percent_to_keep &&
        left.keep_unknown_iv == right.keep_unknown_iv &&
        left.keep_shiny == right.keep_shiny &&
        left.keep_hundo == right.keep_hundo &&
        left.keep_special_background == right.keep_special_background &&
        left.keep_favorite == right.keep_favorite &&
        left.keep_legendary == right.keep_legendary &&
        left.keep_mythical == right.keep_mythical;
}

bool runtime_desired_revision_newer(uint64_t candidate, uint64_t current) {
    const uint64_t distance = candidate - current;
    return distance != 0U && distance <= (UINT64_MAX / 2U);
}

bool runtime_desired_identity_matches(
    const ProbeContext &context,
    const pogo_runtime_desired_state::Request &request
) {
    // The root companion authenticates the controller UID and checks the
    // session id before forwarding the frame to this process. ProbeContext
    // deliberately does not duplicate that broker-owned session state.
    if (request.pid != static_cast<uint32_t>(context.pid) ||
        request.process_name != context.process_name ||
        request.package_name != context.process_name) return false;

    // A desired snapshot is allowed during the probe-only portion of a new
    // session. The exact build fingerprint is checked again before reconcile;
    // accepting the broker's unverified identity here does not authorize any
    // managed call or gameplay mutation.
    const std::string unverified_fingerprint =
        std::string("unverified|") + context.process_name;
    return request.build_fingerprint == kVerifiedBuildFingerprint ||
        request.build_fingerprint == unverified_fingerprint;
}
}  // namespace

struct RuntimeDesiredStateSnapshot {
    bool present = false;
    std::string runtime_session_id;
    pogo_runtime_desired_state::State state;
    uint64_t received_at_ns = 0U;
    uint64_t expires_at_ns = 0U;
};

[[maybe_unused]] bool snapshot_runtime_desired_state(RuntimeDesiredStateSnapshot *output) {
    if (output == nullptr) return false;
    std::lock_guard<std::mutex> lock(g_runtime_desired_state_mutex);
    output->present = g_runtime_desired_state_present;
    output->runtime_session_id = g_runtime_desired_state_session_id;
    output->state = g_runtime_desired_state;
    output->received_at_ns = g_runtime_desired_state_received_at_ns;
    output->expires_at_ns = g_runtime_desired_state_expires_at_ns;
    return output->present;
}

bool accept_runtime_desired_state(
    ProbeContext &context,
    const pogo_runtime_desired_state::Request &request
) {
    constexpr uint32_t kAcceptedPhase = 1U;  // CommandPhase.ACCEPTED
    const uint64_t now = static_cast<uint64_t>(now_elapsed_nanos());
    if (!runtime_desired_identity_matches(context, request)) {
        return send_runtime_rejected(
            context.fd,
            request.request_id.c_str(),
            "runtime_identity_mismatch",
            "desired state identity does not match the runtime session"
        );
    }
    if (request.expires_at_elapsed_ns <= now) {
        return send_runtime_rejected(
            context.fd,
            request.request_id.c_str(),
            "desired_state_expired",
            "desired state expired before native receipt"
        );
    }
    if (request.expires_at_elapsed_ns - now >
        pogo_runtime_desired_state::kMaxExpiryWindowNs) {
        return send_runtime_rejected(
            context.fd,
            request.request_id.c_str(),
            "desired_state_ttl_too_long",
            "desired state expiry exceeds the session handoff window"
        );
    }

    {
        std::lock_guard<std::mutex> lock(g_runtime_desired_state_mutex);
        if (g_runtime_desired_state_session_id != request.runtime_session_id) {
            g_runtime_desired_state_present = false;
            g_runtime_desired_state_session_id = request.runtime_session_id;
            g_runtime_desired_state = pogo_runtime_desired_state::State{};
            g_runtime_desired_state_expires_at_ns = 0U;
        }

        if (g_runtime_desired_state_present) {
            const uint64_t current_revision = g_runtime_desired_state.config_revision;
            if (request.state.config_revision == current_revision) {
                if (!runtime_desired_state_equal(request.state, g_runtime_desired_state)) {
                    return send_runtime_rejected(
                        context.fd,
                        request.request_id.c_str(),
                        "desired_state_revision_conflict",
                        "same desired-state revision contains different fields"
                    );
                }
            } else if (!runtime_desired_revision_newer(
                    request.state.config_revision,
                    current_revision
                )) {
                return send_runtime_rejected(
                    context.fd,
                    request.request_id.c_str(),
                    "desired_state_stale",
                    "desired-state revision is older than the native snapshot"
                );
            } else {
                g_runtime_desired_state = request.state;
                g_runtime_desired_state_received_at_ns = now;
                g_runtime_desired_state_expires_at_ns = request.expires_at_elapsed_ns;
            }
        } else {
            g_runtime_desired_state = request.state;
            g_runtime_desired_state_present = true;
            g_runtime_desired_state_received_at_ns = now;
            g_runtime_desired_state_expires_at_ns = request.expires_at_elapsed_ns;
        }
    }

    // Receipt means only that the complete snapshot reached native RAM. The
    // apply/ready/outcome states are emitted by the later reconcile path.
    return send_runtime_result(
        context.fd,
        request.request_id.c_str(),
        kAcceptedPhase,
        "",
        "desired_state_received"
    );
}

bool handle_runtime_desired_state(
    ProbeContext &context,
    const std::vector<uint8_t> &command
) {
    pogo_runtime_desired_state::Request request;
    if (!pogo_runtime_desired_state::parse(command, &request)) return false;
    return accept_runtime_desired_state(context, request);
}
