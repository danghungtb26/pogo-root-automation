class DiscardModule final : public RuntimeFeatureModuleBase {
public:
    Module id() const override { return Module::kDiscard; }
    const char *name() const override { return "discard"; }

    bool available(const RuntimeBinding &) const override {
        // The service/module boundary is independent now, but this build does
        // not yet contain a verified client-owned discard executor.
        return false;
    }
};
