bool send_runtime_ui_status(ProbeContext &context);
extern std::atomic<bool> g_runtime_active;

namespace {
std::vector<std::string> runtime_ui_status_capabilities(const RuntimeBinding &binding) {
    std::vector<std::string> capabilities;
    if (!binding.exact_build_verified) return capabilities;
    if (binding.use_berry_verified) capabilities.emplace_back("USE_BERRY");
    if (binding.encounter_read_verified) {
        capabilities.emplace_back("ENCOUNTER");
        capabilities.emplace_back("READ_LIFECYCLE");
    }
    if (binding.map_entity_read_verified) capabilities.emplace_back("READ_NEARBY");
    if (binding.forts_read_verified) capabilities.emplace_back("READ_FORTS");
    if (binding.inventory_read_verified) capabilities.emplace_back("READ_INVENTORY");
    if (binding.map_pokemon_tap_event_verified) capabilities.emplace_back("READ_MAP_TARGET");
    if (kOpenEncounterExecutionEnabled && binding.dynamic_tappables_service != nullptr &&
        binding.dynamic_tappables_tap != nullptr) capabilities.emplace_back("OPEN_ENCOUNTER");
    if (kCatchExecutionEnabled && binding.catch_throw_verified) capabilities.emplace_back("CATCH");
    if (kDirectCatchExecutionEnabled && binding.direct_catch_verified) {
        capabilities.emplace_back("DIRECT_CATCH");
    }
    if (kSpinExecutionEnabled && binding.spin_verified) capabilities.emplace_back("SPIN");
    if (kTransferExecutionEnabled && binding.transfer_verified) {
        capabilities.emplace_back("TRANSFER_POKEMON");
    }
    if (binding.throw_hooks_installed) capabilities.emplace_back("OBSERVE_THROW_EVENTS");
    std::sort(capabilities.begin(), capabilities.end());
    capabilities.erase(std::unique(capabilities.begin(), capabilities.end()), capabilities.end());
    return capabilities;
}

bool runtime_ui_catch_desired(const RuntimeCatchSpinConfig &config) {
    return config.configured && config.armed &&
        ((config.auto_catch && config.catch_all) || config.auto_spin || config.auto_walk_to_fort);
}

bool runtime_ui_module_error(
    const RuntimeBinding &binding,
    pogo_runtime_module::Module module,
    bool desired,
    const char **error_code
) {
    if (error_code == nullptr) return false;
    *error_code = nullptr;
    const auto *feature = pogo_runtime_module::resolve_module(module);
    if (!desired || feature == nullptr || !binding.exact_build_verified) return false;
    if (!feature->available(binding)) {
        *error_code = "runtime_module_unavailable";
        return true;
    }
    return false;
}

pogo_runtime_ui_status::Module runtime_ui_module_status(
    const RuntimeBinding &binding,
    pogo_runtime_module::Module module,
    uint32_t wire,
    const char *name,
    bool desired,
    bool configured,
    uint64_t revision,
    bool *ready,
    std::string *first_error
) {
    pogo_runtime_ui_status::Module status;
    status.wire = wire;
    status.name = name;
    status.has_revision = configured;
    status.revision = revision;
    if (module == pogo_runtime_module::Module::kRuntimeCore) {
        status.has_revision = false;
        status.revision = 0U;
        status.state = g_runtime_active.load(std::memory_order_acquire)
            ? pogo_runtime_ui_status::ModuleState::kReady
            : pogo_runtime_ui_status::ModuleState::kRegistered;
        return status;
    }
    const auto *feature = pogo_runtime_module::resolve_module(module);
    if (feature == nullptr) {
        status.state = pogo_runtime_ui_status::ModuleState::kError;
        status.error_code = "runtime_module_unregistered";
    } else if (!configured) {
        status.state = pogo_runtime_ui_status::ModuleState::kRegistered;
    } else {
        const char *error_code = nullptr;
        if (runtime_ui_module_error(binding, module, desired, &error_code)) {
            status.state = pogo_runtime_ui_status::ModuleState::kError;
            status.error_code = error_code;
        } else if (!desired) {
            status.state = pogo_runtime_ui_status::ModuleState::kDisabled;
        } else if (feature->enabled()) {
            status.state = pogo_runtime_ui_status::ModuleState::kReady;
        } else {
            status.state = pogo_runtime_ui_status::ModuleState::kConfigured;
        }
    }
    if (status.state == pogo_runtime_ui_status::ModuleState::kError && first_error != nullptr &&
        first_error->empty()) {
        *first_error = status.error_code;
    }
    if (ready != nullptr && desired && status.state != pogo_runtime_ui_status::ModuleState::kReady) {
        *ready = false;
    }
    return status;
}
}  // namespace

bool send_runtime_ui_status(ProbeContext &context) {
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    RuntimeDesiredStateSnapshot desired;
    const bool has_desired = snapshot_runtime_desired_state(&desired);
    const uint64_t applied_revision =
        g_runtime_applied_desired_revision.load(std::memory_order_acquire);
    const RuntimeCatchSpinConfig catch_spin = runtime_catch_spin_config_snapshot();
    const RuntimeDiscardConfig discard = runtime_discard_config_snapshot();
    const RuntimeTransferConfig transfer = runtime_transfer_config_snapshot();
    const bool catch_desired = runtime_ui_catch_desired(catch_spin);
    const bool discard_desired = discard.configured && discard.auto_discard;
    const bool transfer_desired = transfer.configured && transfer.auto_transfer;
    bool modules_ready = true;
    std::string first_error;
    pogo_runtime_ui_status::Status status;
    status.runtime_session_id = std::string("native-pending|") + context.process_name;
    status.pid = static_cast<uint32_t>(context.pid);
    status.process_name = context.process_name;
    status.package_name = context.process_name;
    status.build_fingerprint = binding.exact_build_verified
        ? kVerifiedBuildFingerprint
        : std::string("unverified|") + context.process_name;
    status.strong_identity_verified = binding.exact_build_verified;
    status.capabilities = runtime_ui_status_capabilities(binding);
    status.has_desired_revision = has_desired;
    status.desired_revision = has_desired ? desired.state.config_revision : 0U;
    status.has_applied_revision = applied_revision > 0U;
    status.applied_revision = applied_revision;
    status.modules.emplace_back(runtime_ui_module_status(
        binding,
        pogo_runtime_module::Module::kRuntimeCore,
        100U,
        "runtime-core",
        false,
        pogo_runtime_module::module_registry_initialized(),
        0U,
        &modules_ready,
        &first_error
    ));
    status.modules.emplace_back(runtime_ui_module_status(
        binding,
        pogo_runtime_module::Module::kCatchSpin,
        1U,
        "catch_spin",
        catch_desired,
        catch_spin.configured,
        catch_spin.revision,
        &modules_ready,
        &first_error
    ));
    status.modules.emplace_back(runtime_ui_module_status(
        binding,
        pogo_runtime_module::Module::kDiscard,
        2U,
        "discard",
        discard_desired,
        discard.configured,
        discard.revision,
        &modules_ready,
        &first_error
    ));
    status.modules.emplace_back(runtime_ui_module_status(
        binding,
        pogo_runtime_module::Module::kTransfer,
        3U,
        "transfer",
        transfer_desired,
        transfer.configured,
        transfer.revision,
        &modules_ready,
        &first_error
    ));

    const bool active = g_runtime_active.load(std::memory_order_acquire);
    const bool map_ready = context.map_live_verified.load(std::memory_order_acquire);
    const bool desired_enabled = has_desired && desired.state.enabled;
    const bool revision_applied = has_desired && applied_revision == desired.state.config_revision;
    status.ready = active && binding.exact_build_verified &&
        (!desired_enabled || map_ready) && revision_applied && modules_ready && first_error.empty();
    if (!active) {
        status.lifecycle = pogo_runtime_ui_status::NativeLifecycle::kAttachedIdle;
    } else if (!binding.exact_build_verified) {
        status.lifecycle = pogo_runtime_ui_status::NativeLifecycle::kDiagnosticPending;
    } else if (!revision_applied) {
        status.lifecycle = map_ready
            ? pogo_runtime_ui_status::NativeLifecycle::kApplying
            : pogo_runtime_ui_status::NativeLifecycle::kBindingReady;
    } else if (!first_error.empty()) {
        status.lifecycle = pogo_runtime_ui_status::NativeLifecycle::kError;
    } else if (status.ready) {
        status.lifecycle = pogo_runtime_ui_status::NativeLifecycle::kReady;
    } else {
        status.lifecycle = pogo_runtime_ui_status::NativeLifecycle::kBindingReady;
    }
    if (!first_error.empty()) {
        status.error_code = first_error;
        status.error_message = "one or more desired native modules are unavailable";
    } else if (desired_enabled && !map_ready) {
        status.error_code = "runtime_map_not_ready";
        status.error_message = "native desired state is waiting for a live map";
    }
    status.observed_at_epoch_ms = static_cast<uint64_t>(now_epoch_millis());
    status.observed_at_elapsed_ns = static_cast<uint64_t>(now_elapsed_nanos());

    std::vector<uint8_t> payload;
    if (!pogo_runtime_ui_status::encode_runtime_ui_status_payload(status, &payload)) return false;
    return send_internal_message(context.fd, kRuntimeUiStatusMagic, payload);
}
