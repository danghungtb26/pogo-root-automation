class DiscardModule final : public RuntimeFeatureModuleBase {
public:
    Module id() const override { return Module::kDiscard; }
    const char *name() const override { return "discard"; }

    bool available(const RuntimeBinding &binding) const override {
        // Requires the discard binding (RecycleItem + ItemData) and the executor
        // to be enabled. kDiscardExecutionEnabled stays false until the ItemData
        // construction and RecycleItem post-condition are verified on device, so
        // this stays unavailable and DiscardItem actions are cleanly rejected
        // rather than firing an unverified recycle.
        return kDiscardExecutionEnabled && binding.exact_build_verified &&
            binding.discard_verified;
    }
};
