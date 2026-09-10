class CatchSpinModule final : public RuntimeFeatureModuleBase {
public:
    Module id() const override { return Module::kCatchSpin; }
    const char *name() const override { return "catch_spin"; }

    bool available(const RuntimeBinding &binding) const override {
        // The current shared observation source is encounter-gated. Requiring
        // it here prevents this module from reporting ENABLED when its event
        // loop cannot actually start.
        return binding.exact_build_verified && binding.encounter_read_verified;
    }
};
