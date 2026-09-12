// catch_spin owns the native map-automation actions: direct-map catch (12) and
// spin (4). Kotlin no longer plans or submits either action.
constexpr uint32_t kCatchSpinActionTags[] = {4U, 12U};
// Control action 6 applies the complete runtime-owned policy snapshot.
constexpr uint32_t kCatchSpinControlActions[] = {6U};

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
        // The observer owns the complete scan/filter/action loop. No Kotlin pull
        // request is needed after this module is enabled.
        (void)context;
        reset_runtime_catch_spin_coordinator();
        return true;
    }

    void on_disable(ProbeContext &context) override {
        reset_runtime_spin_state(context);
        reset_runtime_catch_spin_coordinator();
    }

    void observe(ObserverTickContext &tick) override {
        run_runtime_catch_spin_observer_tick(tick);
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
            RuntimeActionResult config_result;
            if (!runtime_catch_spin_config_allows_action(action.action_tag, &config_result)) {
                *result = std::move(config_result);
            } else {
                *result = execute_runtime_catch(context, action);
            }
            return true;
        }
        if (parse_runtime_spin_command(command, &action)) {
            *command_id = action.command_id;
            RuntimeActionResult config_result;
            if (!runtime_catch_spin_config_allows_action(action.action_tag, &config_result)) {
                *result = std::move(config_result);
            } else {
                *result = execute_runtime_spin(context, action);
            }
            return true;
        }
        return false;
    }

    // CONFIG_SET remains the only control-plane input for the autonomous loop.
    bool handle_control_action(
        ProbeContext &context,
        uint32_t control_action,
        const std::vector<uint8_t> &command,
        const char * /*request_id*/,
        uint64_t /*cycle_id*/
    ) override {
        if (control_action == pogo_runtime_catch_spin_config::kConfigSetAction) {
            return apply_runtime_catch_spin_config(context, command);
        }
        return false;
    }
};

CatchSpinModule g_catch_spin_module;
RuntimeModuleRegistration g_catch_spin_module_registration{&g_catch_spin_module, nullptr};
static const bool kCatchSpinModuleRegistered =
    append_runtime_module_registration(&g_catch_spin_module_registration);
