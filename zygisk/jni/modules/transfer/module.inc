constexpr uint32_t kTransferActionTags[] = {6U};  // TransferPokemon
constexpr uint32_t kTransferControlActions[] = {7U};  // TRANSFER_CONFIG_SET

class TransferModule final : public RuntimeFeatureModuleBase {
public:
    Module id() const override { return Module::kTransfer; }
    const char *name() const override { return "transfer"; }

    bool available(const RuntimeBinding &binding) const override {
        // Transfer is also the native consumer of the authoritative catch event.
        // It remains unavailable unless the exact release binding is verified.
        return binding.exact_build_verified && kTransferExecutionEnabled &&
            binding.transfer_verified;
    }

    bool on_enable(ProbeContext &context) override {
        reset_runtime_transfer_state(context);
        return true;
    }

    void on_disable(ProbeContext &context) override {
        reset_runtime_transfer_state(context);
    }

    void observe(ObserverTickContext &tick) override {
        if (runtime_transfer_has_pending()) {
            (void)request_main_thread_poll_runtime_transfer(tick.context);
        }
    }

    bool handle_action(
        ProbeContext &context,
        uint32_t /*action_tag*/,
        const std::vector<uint8_t> &command,
        RuntimeActionResult *result,
        std::string *command_id
    ) override {
        RuntimeActionCommand action;
        if (!parse_runtime_transfer_command(command, &action)) return false;
        *command_id = action.command_id;
        *result = execute_runtime_transfer(context, action);
        return true;
    }

    bool handle_control_action(
        ProbeContext &context,
        uint32_t control_action,
        const std::vector<uint8_t> &command,
        const char * /*request_id*/,
        uint64_t /*cycle_id*/
    ) override {
        if (control_action == pogo_runtime_transfer_config::kConfigSetAction) {
            return apply_runtime_transfer_config(context, command);
        }
        return false;
    }

    ModuleActionOwnership action_ownership() const override {
        ModuleActionOwnership ownership;
        ownership.gameplay_tags = kTransferActionTags;
        ownership.gameplay_tag_count = sizeof(kTransferActionTags) / sizeof(uint32_t);
        ownership.control_actions = kTransferControlActions;
        ownership.control_action_count = sizeof(kTransferControlActions) / sizeof(uint32_t);
        return ownership;
    }
};

TransferModule g_transfer_module;
RuntimeModuleRegistration g_transfer_module_registration{&g_transfer_module, nullptr};
static const bool kTransferModuleRegistered =
    append_runtime_module_registration(&g_transfer_module_registration);
