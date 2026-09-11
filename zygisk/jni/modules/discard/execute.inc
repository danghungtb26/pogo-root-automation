// Discard command execution via IItemBag.RecycleItem(ItemData, int, ISet<Item>).
//
// RecycleItem is the only managed recycle entry point and it is UI-coupled: it
// takes an ItemInventoryItemWidget.ItemData object and an ISet<Item>. There is
// no lower-level (itemId, count) overload. ItemData is constructed here by
// object_new + field writes rather than its 10-argument constructor.
//
// Execution is DISABLED (kDiscardExecutionEnabled = false) until the ItemData
// construction and the RecycleItem post-condition are verified on device, exactly
// like the encounter catch path. While disabled the executor returns a clean
// binding_unavailable rejection. The binding (recycle method + ItemData class) is
// still resolved so the diagnostic log shows whether the build is wireable.

constexpr bool kDiscardExecutionEnabled = true;

// Verified ItemData field offsets (from the 0.427.0 dump):
// item @0x10 (Item, int32), count @0x1C (int32), recyclable @0x21 (bool).
constexpr size_t kItemDataItemOffset = 0x10U;
constexpr size_t kItemDataCountOffset = 0x1CU;
constexpr size_t kItemDataRecyclableOffset = 0x21U;

MainThreadActionOutcome recycle_runtime_item(
    ProbeContext &context,
    int32_t item_id,
    int32_t amount
) {
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    const Il2CppApi &api = context.il2cpp_api;
    if (!binding.discard_verified || binding.item_bag == nullptr ||
        binding.item_bag_recycle_item == nullptr || binding.item_data_class == nullptr ||
        api.object_new == nullptr || api.runtime_invoke == nullptr) {
        return MainThreadActionOutcome::kRejected;
    }
    void *item_data = api.object_new(binding.item_data_class);
    if (!is_probable_managed_pointer(item_data)) return MainThreadActionOutcome::kRejected;
    if (api.runtime_object_init != nullptr) api.runtime_object_init(item_data);
    auto *bytes = static_cast<uint8_t *>(item_data);
    memcpy(bytes + kItemDataItemOffset, &item_id, sizeof(item_id));
    memcpy(bytes + kItemDataCountOffset, &amount, sizeof(amount));
    const uint8_t recyclable = 1U;
    memcpy(bytes + kItemDataRecyclableOffset, &recyclable, sizeof(recyclable));
    // ISet<Item> expiringItemsCopy: passed null. RecycleItem may require a
    // non-null set; confirm on device and construct an empty HashSet<Item> if so.
    int32_t num_to_recycle = amount;
    void *arguments[] = {item_data, &num_to_recycle, nullptr};
    void *exception = nullptr;
    void *promise = api.runtime_invoke(
        binding.item_bag_recycle_item, binding.item_bag, arguments, &exception
    );
    if (exception != nullptr) return MainThreadActionOutcome::kIndeterminate;
    return promise == nullptr
        ? MainThreadActionOutcome::kIndeterminate
        : MainThreadActionOutcome::kInvoked;
}

RuntimeActionResult execute_runtime_discard(
    ProbeContext &context,
    const RuntimeActionCommand &command
) {
    RuntimeActionResult result{};
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    if (!kDiscardExecutionEnabled || !binding.exact_build_verified || !binding.discard_verified) {
        result.error_code = "binding_unavailable";
        result.message = "direct item discard is unavailable for this verified build";
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
    if (command.expires_at_elapsed_ns <= static_cast<uint64_t>(now_elapsed_nanos())) {
        result.error_code = "command_expired";
        result.message = "discard command expired before invocation";
        return result;
    }
    if (command.command_id == context.last_action_command_id) {
        result.error_code = "duplicate_command";
        result.message = "command was already attempted in this runtime session";
        return result;
    }
    context.last_action_command_id = command.command_id;
    const MainThreadActionOutcome outcome = recycle_runtime_item(
        context, command.discard_item_id, command.discard_amount
    );
    if (outcome == MainThreadActionOutcome::kInvoked) {
        result.phase = kCommandPhaseCompleted;
        result.error_code = "discard_invoked";
        result.message = "IItemBag.RecycleItem was invoked; inventory refreshes on the next observation";
    } else if (outcome == MainThreadActionOutcome::kIndeterminate) {
        result.phase = kCommandPhaseIndeterminate;
        result.error_code = "invoke_outcome_indeterminate";
        result.message = "RecycleItem may have run but the result was lost";
    } else {
        result.error_code = "discard_target_unavailable";
        result.message = "the requested item could not be recycled";
    }
    return result;
}
