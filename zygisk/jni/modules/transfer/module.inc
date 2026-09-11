constexpr uint32_t kTransferActionTags[] = {6U};  // TransferPokemon

class TransferModule final : public RuntimeFeatureModuleBase {
public:
    Module id() const override { return Module::kTransfer; }
    const char *name() const override { return "transfer"; }

    bool available(const RuntimeBinding &binding) const override {
        // Transfer is execution-only (no observation concern). It becomes
        // available once the release binding is verified; execution stays gated
        // behind kTransferExecutionEnabled until the ABI is confirmed on device,
        // so TransferPokemon commands are cleanly rejected until then.
        return binding.exact_build_verified && kTransferExecutionEnabled &&
            binding.transfer_verified;
    }

    ModuleActionOwnership action_ownership() const override {
        ModuleActionOwnership ownership;
        ownership.gameplay_tags = kTransferActionTags;
        ownership.gameplay_tag_count = sizeof(kTransferActionTags) / sizeof(uint32_t);
        return ownership;
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
};

TransferModule g_transfer_module;
RuntimeModuleRegistration g_transfer_module_registration{&g_transfer_module, nullptr};
static const bool kTransferModuleRegistered =
    append_runtime_module_registration(&g_transfer_module_registration);
