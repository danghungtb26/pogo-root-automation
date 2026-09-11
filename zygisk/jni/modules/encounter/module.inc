// encounter owns the encounter-based flow: open the encounter (2), the in-encounter
// catch throw (3 default, 9 close-preview, 10 structured throw), the berry assist
// (8), and the GO snapshot (11; not yet executed natively). Direct-map catch (12)
// and spin (4) stay with catch_spin.
constexpr uint32_t kEncounterActionTags[] = {2U, 3U, 8U, 9U, 10U, 11U};

class EncounterModule final : public RuntimeFeatureModuleBase {
public:
    Module id() const override { return Module::kEncounter; }
    const char *name() const override { return "encounter"; }

    bool available(const RuntimeBinding &binding) const override {
        return binding.exact_build_verified && binding.encounter_read_verified &&
            (binding.use_berry_verified || binding.catch_throw_verified ||
             binding.throw_hook_binding_verified || binding.throw_hooks_installed);
    }

    bool on_enable(ProbeContext &context) override {
        RuntimeBinding binding = runtime_binding_snapshot(context);
        if (!binding.throw_hooks_installed && binding.throw_hook_binding_verified) {
            install_runtime_throw_hooks(context);
            binding = runtime_binding_snapshot(context);
        }
        if (binding.throw_hooks_installed) {
            // Keep trampolines installed once, but arm their module-owned event
            // context only while the encounter module is enabled.
            g_throw_hook_context = &context;
        }
        clear_throw_events(context);
        // Throw quality is a requested client-side profile, not a guarantee of
        // a remote/server capture outcome.
        return binding.use_berry_verified || binding.catch_throw_verified ||
            binding.throw_hooks_installed;
    }

    void on_disable(ProbeContext &context) override {
        g_throw_hook_context = nullptr;
        clear_throw_events(context);
    }

    ModuleActionOwnership action_ownership() const override {
        ModuleActionOwnership ownership;
        ownership.gameplay_tags = kEncounterActionTags;
        ownership.gameplay_tag_count = sizeof(kEncounterActionTags) / sizeof(uint32_t);
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
        if (parse_runtime_use_berry_command(command, &action)) {
            *command_id = action.command_id;
            *result = execute_runtime_use_berry(context, action);
            return true;
        }
        // Open the encounter (tag 2).
        if (parse_runtime_open_encounter_command(command, &action)) {
            *command_id = action.command_id;
            *result = execute_runtime_open_encounter(context, action);
            return true;
        }
        // In-encounter catch throw (tags 3/9/10). The owner map keeps direct-map
        // catch (12) with catch_spin; execute_runtime_catch takes the encounter path
        // for these tags.
        if (parse_runtime_catch_command(command, &action)) {
            *command_id = action.command_id;
            *result = execute_runtime_catch(context, action);
            return true;
        }
        return false;
    }
};

EncounterModule g_encounter_module;
RuntimeModuleRegistration g_encounter_module_registration{&g_encounter_module, nullptr};
static const bool kEncounterModuleRegistered =
    append_runtime_module_registration(&g_encounter_module_registration);
