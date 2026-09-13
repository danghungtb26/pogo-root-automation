constexpr uint32_t kDiscardActionTags[] = {5U};  // DiscardItem
constexpr uint32_t kDiscardControlActions[] = {8U};  // DISCARD_CONFIG_SET

class DiscardModule final : public RuntimeFeatureModuleBase {
public:
    Module id() const override { return Module::kDiscard; }
    const char *name() const override { return "discard"; }

    bool available(const RuntimeBinding &binding) const override {
        // Auto-discard needs both halves of the verified binding: inventory is
        // the trigger and RecycleItem is the mutation.
        return binding.exact_build_verified &&
            binding.inventory_read_verified && kDiscardExecutionEnabled &&
            binding.discard_verified;
    }

    bool on_enable(ProbeContext &context) override {
        reset_runtime_discard_state(context);
        return true;
    }

    void on_disable(ProbeContext &context) override {
        reset_runtime_discard_state(context);
    }

    void observe(ObserverTickContext &tick) override {
        // Own trigger: inventory is maintenance, poll at a low cadence (~30 s at
        // the observer's ~100 ms tick). Read on the observer's attached thread.
        constexpr uint64_t kInventoryEveryTicks = 300U;
        if (runtime_discard_has_pending()) {
            (void)request_main_thread_poll_runtime_discard(tick.context);
        }
        if (!tick.binding.inventory_read_verified || tick.tick % kInventoryEveryTicks != 0U) return;
        pogo_runtime::RuntimeInventoryObservation inventory;
        if (!read_runtime_inventory(tick.context.il2cpp_api, tick.binding, &inventory)) return;
        send_runtime_inventory_payload(tick.context, inventory);

        const RuntimeDiscardConfig config = runtime_discard_config_snapshot();
        if (!config.configured || !config.auto_discard || runtime_discard_has_pending() ||
            runtime_transfer_blocks_catch_spin() || runtime_discard_blocks_catch_spin() ||
            runtime_direct_catch_is_pending(tick.context) || runtime_spin_is_pending(tick.context) ||
            runtime_encounter_request_is_pending(tick.context) ||
            tick.context.active_encounter_id.load() != 0U) return;
        for (const auto &stack : inventory.items) {
            int32_t max_count = 0;
            if (!runtime_discard_config_find_limit(config, stack.item_id, &max_count) ||
                stack.count <= max_count) continue;
            const int32_t amount = stack.count - max_count;
            __android_log_print(
                ANDROID_LOG_INFO,
                kLogTag,
                "native discard candidate item_id=%d count=%d max=%d amount=%d",
                stack.item_id,
                stack.count,
                max_count,
                amount
            );
            const MainThreadActionOutcome outcome = request_main_thread_start_runtime_discard(
                tick.context, stack.item_id, amount, nullptr
            );
            __android_log_print(
                outcome == MainThreadActionOutcome::kInvoked ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
                kLogTag,
                "native discard candidate queued item_id=%d amount=%d outcome=%u",
                stack.item_id,
                amount,
                static_cast<unsigned>(outcome)
            );
            break;
        }
    }

    ModuleActionOwnership action_ownership() const override {
        ModuleActionOwnership ownership;
        ownership.gameplay_tags = kDiscardActionTags;
        ownership.gameplay_tag_count = sizeof(kDiscardActionTags) / sizeof(uint32_t);
        ownership.control_actions = kDiscardControlActions;
        ownership.control_action_count = sizeof(kDiscardControlActions) / sizeof(uint32_t);
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

    bool handle_control_action(
        ProbeContext &context,
        uint32_t control_action,
        const std::vector<uint8_t> &command,
        const char * /*request_id*/,
        uint64_t /*cycle_id*/
    ) override {
        if (control_action == pogo_runtime_discard_config::kConfigSetAction) {
            return apply_runtime_discard_config(context, command);
        }
        return false;
    }
};

DiscardModule g_discard_module;
RuntimeModuleRegistration g_discard_module_registration{&g_discard_module, nullptr};
static const bool kDiscardModuleRegistered =
    append_runtime_module_registration(&g_discard_module_registration);
