class TransferModule final : public RuntimeFeatureModuleBase {
public:
    Module id() const override { return Module::kTransfer; }
    const char *name() const override { return "transfer"; }

    bool available(const RuntimeBinding &) const override {
        // The service/module boundary is independent now, but this build does
        // not yet contain a verified client-owned transfer executor.
        return false;
    }
};
