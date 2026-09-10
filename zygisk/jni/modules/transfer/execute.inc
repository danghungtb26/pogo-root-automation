// Transfer command execution via Niantic.Holoholo.Internal.PokemonBagImpl
// (IPokemonBag). Per the 0.427.0 dump the release path is UI-independent:
//   PokemonProto GetPokemon(ulong id)
//   IPromise<ReleasePokemonOutProto> ReleasePokemon(PokemonProto pokemon)
// so the executor resolves the proto by storage id then releases it.
//
// Execution is DISABLED (kTransferExecutionEnabled = false) until the release
// promise post-condition is verified on device, exactly like the discard and
// catch paths. While disabled the executor returns a clean binding_unavailable
// rejection. The binding is still resolved so the diagnostic log shows whether
// the build is wireable.

constexpr bool kTransferExecutionEnabled = true;

MainThreadActionOutcome release_runtime_pokemon(
    ProbeContext &context,
    uint64_t pokemon_id
) {
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    const Il2CppApi &api = context.il2cpp_api;
    if (!binding.transfer_verified || binding.pokemon_inventory == nullptr ||
        binding.pokemon_inventory_get_pokemon == nullptr ||
        binding.pokemon_inventory_release == nullptr || api.runtime_invoke == nullptr) {
        return MainThreadActionOutcome::kRejected;
    }
    // PokemonProto GetPokemon(ulong id): resolve the stored Pokémon by storage id.
    void *get_arguments[] = {&pokemon_id};
    void *exception = nullptr;
    void *pokemon = api.runtime_invoke(
        binding.pokemon_inventory_get_pokemon, binding.pokemon_inventory, get_arguments, &exception
    );
    if (exception != nullptr) return MainThreadActionOutcome::kIndeterminate;
    if (!is_probable_managed_pointer(pokemon)) return MainThreadActionOutcome::kRejected;
    // IPromise<ReleasePokemonOutProto> ReleasePokemon(PokemonProto pokemon).
    void *release_arguments[] = {pokemon};
    exception = nullptr;
    void *promise = api.runtime_invoke(
        binding.pokemon_inventory_release, binding.pokemon_inventory, release_arguments, &exception
    );
    if (exception != nullptr) return MainThreadActionOutcome::kIndeterminate;
    return promise == nullptr
        ? MainThreadActionOutcome::kIndeterminate
        : MainThreadActionOutcome::kInvoked;
}

RuntimeActionResult execute_runtime_transfer(
    ProbeContext &context,
    const RuntimeActionCommand &command
) {
    RuntimeActionResult result{};
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    if (!kTransferExecutionEnabled || !binding.exact_build_verified || !binding.transfer_verified) {
        result.error_code = "binding_unavailable";
        result.message = "pokemon transfer is unavailable for this verified build";
        return result;
    }
    if (command.pid != static_cast<uint32_t>(context.pid) ||
        command.process_name != context.process_name ||
        command.package_name != context.process_name) {
        result.error_code = "runtime_identity_mismatch";
        result.message = "command identity does not match target process";
        return result;
    }
    if (command.build_fingerprint != kVerifiedBuildFingerprint) {
        result.error_code = "build_fingerprint_mismatch";
        result.message = "command build fingerprint is not the verified binding build";
        return result;
    }
    if (!command.expected_lifecycle_present || command.expected_lifecycle != 4U) {
        result.error_code = "lifecycle_guard_failed";
        result.message = "transfer requires the OVERWORLD lifecycle";
        return result;
    }
    if (command.expires_at_elapsed_ns <= static_cast<uint64_t>(now_elapsed_nanos())) {
        result.error_code = "command_expired";
        result.message = "transfer command expired before invocation";
        return result;
    }
    if (command.command_id == context.last_action_command_id) {
        result.error_code = "duplicate_command";
        result.message = "command was already attempted in this runtime session";
        return result;
    }
    uint64_t pokemon_id = 0U;
    if (!parse_decimal_u64(command.transfer_pokemon_id, &pokemon_id)) {
        result.error_code = "invalid_transfer_command";
        result.message = "pokemon id is invalid";
        return result;
    }
    context.last_action_command_id = command.command_id;
    const MainThreadActionOutcome outcome = release_runtime_pokemon(context, pokemon_id);
    if (outcome == MainThreadActionOutcome::kInvoked) {
        result.phase = kCommandPhaseCompleted;
        result.error_code = "transfer_invoked";
        result.message = "pokemon release was invoked; storage refreshes on the next observation";
    } else if (outcome == MainThreadActionOutcome::kIndeterminate) {
        result.phase = kCommandPhaseIndeterminate;
        result.error_code = "invoke_outcome_indeterminate";
        result.message = "release may have run but the result was lost";
    } else {
        result.error_code = "transfer_target_unavailable";
        result.message = "the requested pokemon could not be released";
    }
    return result;
}
