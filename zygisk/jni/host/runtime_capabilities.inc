bool send_runtime_capability_update(int fd, const RuntimeBinding &binding) {
    if (!binding.exact_build_verified) return false;
    std::vector<std::string> capabilities;
    if (binding.use_berry_verified) capabilities.emplace_back("USE_BERRY");
    if (binding.encounter_read_verified) {
        capabilities.emplace_back("READ_LIFECYCLE");
        capabilities.emplace_back("ENCOUNTER");
    }
    if (binding.map_entity_read_verified) capabilities.emplace_back("READ_NEARBY");
    if (binding.forts_read_verified) capabilities.emplace_back("READ_FORTS");
    if (binding.inventory_read_verified) capabilities.emplace_back("READ_INVENTORY");
    if (kOpenEncounterExecutionEnabled && binding.dynamic_tappables_service != nullptr &&
        binding.dynamic_tappables_tap != nullptr) {
        capabilities.emplace_back("OPEN_ENCOUNTER");
    }
    if (kCatchExecutionEnabled && binding.catch_throw_verified) {
        capabilities.emplace_back("CATCH");
    }
    if (kDirectCatchExecutionEnabled && binding.direct_catch_verified) {
        capabilities.emplace_back("DIRECT_CATCH");
    }
    if (kSpinExecutionEnabled && binding.spin_verified) capabilities.emplace_back("SPIN");
    if (kTransferExecutionEnabled && binding.transfer_verified) {
        capabilities.emplace_back("TRANSFER_POKEMON");
    }
    if (binding.throw_hooks_installed) capabilities.emplace_back("OBSERVE_THROW_EVENTS");
    if (capabilities.empty()) return false;

    std::vector<uint8_t> payload;
    append_u32(&payload, 1U);
    append_string(&payload, kVerifiedBuildFingerprint);
    append_u32(&payload, static_cast<uint32_t>(capabilities.size()));
    for (const std::string &capability : capabilities) {
        append_string(&payload, capability.c_str());
    }
    payload.push_back(1U);
    return send_internal_message(fd, kRuntimeCapabilitiesMagic, payload);
}
