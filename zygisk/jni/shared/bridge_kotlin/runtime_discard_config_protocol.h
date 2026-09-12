#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "runtime_command_protocol.h"

namespace pogo_runtime_discard_config {

constexpr uint32_t kMarker = 0x44534346U;  // DSCF
constexpr uint32_t kModuleWire = 2U;       // DISCARD
constexpr uint32_t kConfigSetAction = 8U;
constexpr uint32_t kSchemaVersion = 1U;
constexpr uint32_t kMaxLimits = 64U;

struct Limit {
    int32_t item_id = 0;
    int32_t max_count = 0;
};

struct Request {
    uint64_t message_seq = 0U;
    std::string runtime_session_id;
    std::string request_id;
    uint64_t config_revision = 0U;
    bool auto_discard = false;
    Limit limits[kMaxLimits]{};
    uint32_t limit_count = 0U;
    uint64_t expires_at_elapsed_ns = 0U;
    uint32_t pid = 0U;
    std::string process_name;
    std::string package_name;
    std::string build_fingerprint;
};

inline bool read_bool(
    const std::vector<uint8_t> &input,
    size_t *offset,
    bool *value
) {
    if (offset == nullptr || value == nullptr || *offset >= input.size()) return false;
    const uint8_t raw = input[*offset];
    ++(*offset);
    if (raw > 1U) return false;
    *value = raw != 0U;
    return true;
}

inline bool parse(const std::vector<uint8_t> &input, Request *request) {
    if (request == nullptr) return false;
    *request = Request{};

    size_t offset = 0U;
    uint32_t message_type = 0U;
    uint32_t payload_version = 0U;
    uint32_t marker = 0U;
    uint32_t module_wire = 0U;
    uint32_t schema_version = 0U;
    if (!pogo_runtime::read_be32(input, &offset, &message_type) ||
        !pogo_runtime::read_be64(input, &offset, &request->message_seq) ||
        !pogo_runtime::read_be32(input, &offset, &payload_version) ||
        !pogo_runtime::read_string(input, &offset, &request->runtime_session_id) ||
        !pogo_runtime::read_string(input, &offset, &request->request_id) ||
        !pogo_runtime::read_be32(input, &offset, &marker) || marker != kMarker ||
        !pogo_runtime::read_be32(input, &offset, &module_wire) || module_wire != kModuleWire ||
        !pogo_runtime::read_be32(input, &offset, &schema_version) ||
        schema_version != kSchemaVersion ||
        !pogo_runtime::read_be64(input, &offset, &request->config_revision) ||
        !read_bool(input, &offset, &request->auto_discard) ||
        !pogo_runtime::read_be32(input, &offset, &request->limit_count) ||
        request->limit_count > kMaxLimits) {
        return false;
    }

    for (uint32_t index = 0U; index < request->limit_count; ++index) {
        uint32_t item_id = 0U;
        uint32_t max_count = 0U;
        if (!pogo_runtime::read_be32(input, &offset, &item_id) ||
            !pogo_runtime::read_be32(input, &offset, &max_count) ||
            item_id == 0U || max_count > 2147483647U) {
            return false;
        }
        for (uint32_t previous = 0U; previous < index; ++previous) {
            if (request->limits[previous].item_id == static_cast<int32_t>(item_id)) {
                return false;
            }
        }
        request->limits[index] = Limit{
            static_cast<int32_t>(item_id),
            static_cast<int32_t>(max_count),
        };
    }

    if (!pogo_runtime::read_be64(input, &offset, &request->expires_at_elapsed_ns) ||
        !pogo_runtime::read_be32(input, &offset, &request->pid) ||
        !pogo_runtime::read_string(input, &offset, &request->process_name) ||
        !pogo_runtime::read_string(input, &offset, &request->package_name) ||
        !pogo_runtime::read_string(input, &offset, &request->build_fingerprint)) {
        return false;
    }

    return message_type == pogo_runtime::kBridgeCommandType &&
        payload_version == pogo_runtime::kCommandPayloadVersion &&
        request->message_seq > 0U && !request->runtime_session_id.empty() &&
        !request->request_id.empty() && request->config_revision > 0U &&
        request->expires_at_elapsed_ns > 0U && request->pid > 0U &&
        !request->process_name.empty() && !request->package_name.empty() &&
        !request->build_fingerprint.empty() && offset == input.size();
}

}  // namespace pogo_runtime_discard_config
