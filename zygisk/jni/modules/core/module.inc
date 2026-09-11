// Runtime-core module: owns the runtime lifecycle control actions (START, STOP,
// DIAGNOSTIC). It is always present and is never enable/disable'd like a feature
// module; it exists so the control dispatcher routes every control action through
// the module owner map instead of a hardcoded lifecycle switch. The lifecycle
// handlers are defined in runtime_control.inc (included later) and forward-declared
// in main.cpp before this module.
constexpr uint32_t kRuntimeCoreControlActions[] = {
    1U,  // START      (pogo_runtime_control::Action::kStart)
    2U,  // STOP       (pogo_runtime_control::Action::kStop)
    3U,  // DIAGNOSTIC (pogo_runtime_control::Action::kDiagnostic)
};

class RuntimeCoreModule final : public RuntimeFeatureModuleBase {
public:
    Module id() const override { return Module::kRuntimeCore; }
    const char *name() const override { return "runtime-core"; }

    // Always available; each lifecycle action self-gates (e.g. STARTed check).
    bool available(const RuntimeBinding &) const override { return true; }

    ModuleActionOwnership action_ownership() const override {
        ModuleActionOwnership ownership;
        ownership.control_actions = kRuntimeCoreControlActions;
        ownership.control_action_count =
            sizeof(kRuntimeCoreControlActions) / sizeof(uint32_t);
        return ownership;
    }

    bool handle_control_action(
        ProbeContext &context,
        uint32_t control_action,
        const char *request_id,
        uint64_t /*cycle_id*/
    ) override {
        // Raw wire values (see kRuntimeCoreControlActions) mirror
        // pogo_runtime_control::Action; kept numeric to match the gameplay-tag
        // style and avoid a cross-namespace reference from inside the module.
        switch (control_action) {
            case 1U:  // START
                return activate_runtime(context, request_id);
            case 2U:  // STOP
                return stop_runtime(context, request_id);
            case 3U:  // DIAGNOSTIC
                return run_runtime_control_diagnostic(context, request_id);
            default:
                return false;
        }
    }
};

RuntimeCoreModule g_runtime_core_module;
RuntimeModuleRegistration g_runtime_core_module_registration{&g_runtime_core_module, nullptr};
static const bool kRuntimeCoreModuleRegistered =
    append_runtime_module_registration(&g_runtime_core_module_registration);
