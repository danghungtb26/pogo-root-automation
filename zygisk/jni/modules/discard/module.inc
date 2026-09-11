constexpr uint32_t kDiscardActionTags[] = {5U};  // DiscardItem

class DiscardModule final : public RuntimeFeatureModuleBase {
public:
    Module id() const override { return Module::kDiscard; }
    const char *name() const override { return "discard"; }

    bool available(const RuntimeBinding &binding) const override {
        // Available when it can at least observe inventory. Enabling it turns on
        // inventory observation (its observe() hook). Discard execution stays
        // gated by kDiscardExecutionEnabled until the RecycleItem/ItemData path is
        // verified on device; DiscardItem actions are cleanly rejected until then.
        return binding.exact_build_verified &&
            (binding.inventory_read_verified ||
             (kDiscardExecutionEnabled && binding.discard_verified));
    }

    void observe(ObserverTickContext &tick) override {
        // Own trigger: inventory is maintenance, poll at a low cadence (~30 s at
        // the observer's ~100 ms tick). Read on the observer's attached thread.
        constexpr uint64_t kInventoryEveryTicks = 300U;
        if (!tick.binding.inventory_read_verified) return;
        if (tick.tick % kInventoryEveryTicks != 0U) return;
        pogo_runtime::RuntimeInventoryObservation inventory;
        if (read_runtime_inventory(tick.context.il2cpp_api, tick.binding, &inventory)) {
            send_runtime_inventory_payload(tick.context, inventory);
        }
    }

    ModuleActionOwnership action_ownership() const override {
        ModuleActionOwnership ownership;
        ownership.gameplay_tags = kDiscardActionTags;
        ownership.gameplay_tag_count = sizeof(kDiscardActionTags) / sizeof(uint32_t);
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
        if (!parse_runtime_discard_command(command, &action)) return false;
        *command_id = action.command_id;
        *result = execute_runtime_discard(context, action);
        return true;
    }
};

DiscardModule g_discard_module;
RuntimeModuleRegistration g_discard_module_registration{&g_discard_module, nullptr};
static const bool kDiscardModuleRegistered =
    append_runtime_module_registration(&g_discard_module_registration);
