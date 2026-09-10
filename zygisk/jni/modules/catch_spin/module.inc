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
        // Best-effort mode-B: install the map-query batch-callback hook so server
        // map updates drive the dirty flag. If it cannot install (e.g. the
        // prologue is not yet captured for this build), the observer falls back
        // to the periodic poll, so enabling still succeeds.
        install_runtime_map_query_hook(context);
        return true;
    }

    void on_disable(ProbeContext &context) override {
        // Leave any installed trampoline in place (it stays transparent by
        // calling the original), but detach the dirty-mark context so no marks
        // are attributed while the module is disabled.
        (void)context;
        g_map_query_hook_context = nullptr;
    }
};
