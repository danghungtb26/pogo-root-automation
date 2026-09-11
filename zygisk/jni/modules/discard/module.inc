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
};
