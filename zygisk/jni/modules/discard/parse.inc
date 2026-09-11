// Discard command parsing.
//
// Decodes the DISCARD action frame (action tag 5) that BridgeActionCodec emits:
// itemId + amount, wrapped in the standard runtime command envelope. Mirrors the
// spin/catch parsers.

bool parse_runtime_discard_command(
    const std::vector<uint8_t> &command,
    RuntimeActionCommand *parsed
) {
    if (parsed == nullptr) return false;
    *parsed = RuntimeActionCommand{};
    size_t offset = 0U;
    uint32_t message_type = 0U;
    uint32_t payload_version = 0U;
    uint8_t lifecycle_present = 0U;
    if (!read_be32(command, &offset, &message_type) ||
        !read_be64(command, &offset, &parsed->message_seq) ||
        !read_be32(command, &offset, &payload_version) ||
        !read_string(command, &offset, &parsed->session_id) ||
        !read_string(command, &offset, &parsed->command_id) ||
        !read_be32(command, &offset, &parsed->action_tag) ||
        parsed->action_tag != 5U ||
        !read_be32(command, &offset, reinterpret_cast<uint32_t *>(&parsed->discard_item_id)) ||
        !read_be32(command, &offset, reinterpret_cast<uint32_t *>(&parsed->discard_amount)) ||
        !read_be64(command, &offset, &parsed->based_on_observation_seq) ||
        offset >= command.size()) return false;
    lifecycle_present = command[offset++];
    if (lifecycle_present > 1U) return false;
    parsed->expected_lifecycle_present = lifecycle_present != 0U;
    if (parsed->expected_lifecycle_present &&
        !read_be32(command, &offset, &parsed->expected_lifecycle)) return false;
    if (!read_be64(command, &offset, &parsed->expires_at_elapsed_ns) ||
        !read_be32(command, &offset, &parsed->pid) ||
        !read_string(command, &offset, &parsed->process_name) ||
        !read_string(command, &offset, &parsed->package_name) ||
        !read_string(command, &offset, &parsed->build_fingerprint)) return false;
    return message_type == kBridgeCommandType && payload_version == 1U &&
        parsed->message_seq > 0U && !parsed->session_id.empty() &&
        !parsed->command_id.empty() && parsed->discard_item_id > 0 &&
        parsed->discard_amount > 0 && parsed->based_on_observation_seq > 0U &&
        offset == command.size();
}
