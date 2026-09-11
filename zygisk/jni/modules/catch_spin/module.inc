// catch_spin owns the map-automation actions only: direct-map catch (12) and
// spin (4). The encounter-based flow (open/throw/snapshot/berry) belongs to the
// encounter module.
constexpr uint32_t kCatchSpinActionTags[] = {4U, 12U};
// Control actions: 5 = SCAN_MAP (pogo_runtime_control::Action::kScanMap).
constexpr uint32_t kCatchSpinControlActions[] = {5U};

class CatchSpinModule final : public RuntimeFeatureModuleBase {
public:
    Module id() const override { return Module::kCatchSpin; }
    const char *name() const override { return "catch_spin"; }

    bool available(const RuntimeBinding &binding) const override {
        // Enable when at least one observation source is verified. Nearby
        // pokemon reads require encounter_read_verified (shared wild/encounter
        // proto field schema); fort/spin reads require only forts_read_verified.
        // Either alone is enough to start the shared observer and drive useful
        // catch-spin automation (e.g. spin-only or the out-of-balls -> spin
        // path). The observer loop self-gates each source on its own flag.
        return binding.exact_build_verified &&
            (binding.encounter_read_verified || binding.forts_read_verified);
    }

    bool on_enable(ProbeContext &context) override {
        // Nearby/forts are read on demand via the SCAN_MAP pull path; there is no
        // map-query inline hook to install.
        (void)context;
        return true;
    }

    void on_disable(ProbeContext &context) override {
        (void)context;
    }

    ModuleActionOwnership action_ownership() const override {
        ModuleActionOwnership ownership;
        ownership.gameplay_tags = kCatchSpinActionTags;
        ownership.gameplay_tag_count = sizeof(kCatchSpinActionTags) / sizeof(uint32_t);
        ownership.control_actions = kCatchSpinControlActions;
        ownership.control_action_count = sizeof(kCatchSpinControlActions) / sizeof(uint32_t);
        return ownership;
    }

    bool handle_action(
        ProbeContext &context,
        uint32_t /*action_tag*/,  // owner map routes only tags 4/12 here
        const std::vector<uint8_t> &command,
        RuntimeActionResult *result,
        std::string *command_id
    ) override {
        RuntimeActionCommand action;
        // Direct-map catch (tag 12). parse accepts the wider catch tag set, but the
        // owner map only routes 12 here; execute_runtime_catch takes the direct path.
        if (parse_runtime_catch_command(command, &action)) {
            *command_id = action.command_id;
            *result = execute_runtime_catch(context, action);
            return true;
        }
        if (parse_runtime_spin_command(command, &action)) {
            *command_id = action.command_id;
            *result = execute_runtime_spin(context, action);
            return true;
        }
        return false;
    }

    // SCAN_MAP (control action 5, declared above) is this module's world-snapshot
    // pull: it reads nearby/forts/inventory/player on the main thread and replies
    // REQUEST_CATCH_SPIN. The map/inventory readers it calls stay shared runtime
    // infra (main-thread bridge, map/inventory readers), not module-private state.
    bool handle_control_action(
        ProbeContext &context,
        uint32_t /*control_action*/,  // only SCAN_MAP is owned
        const char *request_id,
        uint64_t cycle_id
    ) override {
        return run_runtime_scan_map(context, request_id, cycle_id);
    }
};

CatchSpinModule g_catch_spin_module;
RuntimeModuleRegistration g_catch_spin_module_registration{&g_catch_spin_module, nullptr};
static const bool kCatchSpinModuleRegistered =
    append_runtime_module_registration(&g_catch_spin_module_registration);
