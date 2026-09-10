class TransferModule final : public RuntimeFeatureModuleBase {
public:
    Module id() const override { return Module::kTransfer; }
    const char *name() const override { return "transfer"; }

    bool available(const RuntimeBinding &binding) const override {
        // Transfer is execution-only (no observation concern). It becomes
        // available once the release binding is verified; execution stays gated
        // behind kTransferExecutionEnabled until the ABI is confirmed on device,
        // so TransferPokemon commands are cleanly rejected until then.
        return binding.exact_build_verified && kTransferExecutionEnabled &&
            binding.transfer_verified;
    }
};
