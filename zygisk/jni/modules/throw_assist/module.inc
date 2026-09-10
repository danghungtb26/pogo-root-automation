class ThrowAssistModule final : public RuntimeFeatureModuleBase {
public:
    Module id() const override { return Module::kThrowAssist; }
    const char *name() const override { return "throw_assist"; }

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
            // context only while THROW_ASSIST is enabled.
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
};
