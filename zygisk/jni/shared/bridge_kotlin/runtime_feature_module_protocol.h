#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "runtime_command_protocol.h"

namespace pogo_runtime_module {
constexpr uint32_t kMarker = 0x52544D44U;  // RTMD

enum class Module : uint32_t {
    kCatchSpin = 1U,
    kDiscard = 2U,
    kTransfer = 3U,
    kEncounter = 4U,
    // System module owning the runtime lifecycle control actions (START/STOP/
    // DIAGNOSTIC). Always present, never user enable/disable'd, so it is above the
    // feature-module enable/disable range checked by is_valid_module below.
    kRuntimeCore = 100U,
};

enum class Action : uint32_t {
    kEnable = 1U,
    kDisable = 2U,
};

struct Request {
    uint64_t message_seq = 0U;
    std::string runtime_session_id;
    std::string request_id;
    Module module = Module::kCatchSpin;
    Action action = Action::kDisable;
    uint64_t expires_at_elapsed_ns = 0U;
    uint32_t pid = 0U;
    std::string process_name;
    std::string package_name;
};

inline bool valid_module(uint32_t value) {
    return value >= static_cast<uint32_t>(Module::kCatchSpin) &&
        value <= static_cast<uint32_t>(Module::kEncounter);
}

inline bool valid_action(uint32_t value) {
    return value == static_cast<uint32_t>(Action::kEnable) ||
        value == static_cast<uint32_t>(Action::kDisable);
}

inline bool parse(const std::vector<uint8_t> &input, Request *request) {
    if (request == nullptr) return false;
    size_t offset = 0U;
    uint32_t message_type = 0U;
    uint32_t payload_version = 0U;
    uint32_t marker = 0U;
    uint32_t module = 0U;
    uint32_t action = 0U;
    if (!pogo_runtime::read_be32(input, &offset, &message_type) ||
        !pogo_runtime::read_be64(input, &offset, &request->message_seq) ||
        !pogo_runtime::read_be32(input, &offset, &payload_version) ||
        !pogo_runtime::read_string(input, &offset, &request->runtime_session_id) ||
        !pogo_runtime::read_string(input, &offset, &request->request_id) ||
        !pogo_runtime::read_be32(input, &offset, &marker) || marker != kMarker ||
        !pogo_runtime::read_be32(input, &offset, &module) || !valid_module(module) ||
        !pogo_runtime::read_be32(input, &offset, &action) || !valid_action(action) ||
        !pogo_runtime::read_be64(input, &offset, &request->expires_at_elapsed_ns) ||
        !pogo_runtime::read_be32(input, &offset, &request->pid) ||
        !pogo_runtime::read_string(input, &offset, &request->process_name) ||
        !pogo_runtime::read_string(input, &offset, &request->package_name)) return false;

    request->module = static_cast<Module>(module);
    request->action = static_cast<Action>(action);
    return message_type == pogo_runtime::kBridgeCommandType &&
        payload_version == pogo_runtime::kCommandPayloadVersion &&
        request->message_seq > 0U &&
        !request->runtime_session_id.empty() &&
        !request->request_id.empty() &&
        request->expires_at_elapsed_ns > 0U &&
        request->pid > 0U &&
        !request->process_name.empty() &&
        !request->package_name.empty() &&
        offset == input.size();
}
}  // namespace pogo_runtime_module
