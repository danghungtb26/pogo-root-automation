#include "runtime_command_protocol.h"

#include <cassert>

namespace {
void append_u32(std::vector<uint8_t> *output, uint32_t value) {
    output->push_back(static_cast<uint8_t>((value >> 24U) & 0xffU));
    output->push_back(static_cast<uint8_t>((value >> 16U) & 0xffU));
    output->push_back(static_cast<uint8_t>((value >> 8U) & 0xffU));
    output->push_back(static_cast<uint8_t>(value & 0xffU));
}

void append_u64(std::vector<uint8_t> *output, uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        output->push_back(static_cast<uint8_t>((value >> shift) & 0xffU));
    }
}

void append_string(std::vector<uint8_t> *output, const char *value) {
    const std::string text = value;
    append_u32(output, static_cast<uint32_t>(text.size()));
    output->insert(output->end(), text.begin(), text.end());
}
}  // namespace

int main() {
    std::vector<uint8_t> command;
    append_u32(&command, pogo_runtime::kBridgeCommandType);
    append_u64(&command, 17U);
    append_u32(&command, pogo_runtime::kCommandPayloadVersion);
    append_string(&command, "runtime-session");
    append_string(&command, "command-1");
    // Full AutomationCommand body follows the prefix and must remain opaque
    // to the probe-only runtime.
    command.insert(command.end(), {0x00U, 0x00U, 0x00U, 0x03U, 0xCAU, 0xFEU});

    pogo_runtime::RuntimeCommandPrefix prefix;
    assert(pogo_runtime::parse_runtime_command_prefix(command, &prefix));
    assert(prefix.message_seq == 17U);
    assert(prefix.session_id == "runtime-session");
    assert(prefix.command_id == "command-1");

    command.pop_back();
    assert(pogo_runtime::parse_runtime_command_prefix(command, &prefix));

    return 0;
}
