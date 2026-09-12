#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "runtime_command_protocol.h"

namespace pogo_runtime_transfer_config {

constexpr uint32_t kMarker = 0x54524346U;  // TRCF
constexpr uint32_t kModuleWire = 3U;       // TRANSFER
constexpr uint32_t kConfigSetAction = 7U;
constexpr uint32_t kSchemaVersion = 1U;

struct Request {
    uint64_t message_seq = 0U;
    std::string runtime_session_id;
    std::string request_id;
    uint64_t config_revision = 0U;
    bool auto_transfer = false;
    bool keep_unknown_iv = true;
    double minimum_iv_percent_to_keep = 80.0;
    bool keep_shiny = true;
    bool keep_hundo = true;
    bool keep_special_background = true;
    bool keep_favorite = true;
    bool keep_legendary = true;
    bool keep_mythical = true;
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

inline bool read_double(
    const std::vector<uint8_t> &input,
    size_t *offset,
    double *value
) {
    uint64_t bits = 0U;
    if (value == nullptr || !pogo_runtime::read_be64(input, offset, &bits)) return false;
    std::memcpy(value, &bits, sizeof(*value));
    return std::isfinite(*value);
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
        !read_bool(input, &offset, &request->auto_transfer) ||
        !read_bool(input, &offset, &request->keep_unknown_iv) ||
        !read_double(input, &offset, &request->minimum_iv_percent_to_keep) ||
        !read_bool(input, &offset, &request->keep_shiny) ||
        !read_bool(input, &offset, &request->keep_hundo) ||
        !read_bool(input, &offset, &request->keep_special_background) ||
        !read_bool(input, &offset, &request->keep_favorite) ||
        !read_bool(input, &offset, &request->keep_legendary) ||
        !read_bool(input, &offset, &request->keep_mythical) ||
        !pogo_runtime::read_be64(input, &offset, &request->expires_at_elapsed_ns) ||
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
        request->minimum_iv_percent_to_keep >= 0.0 &&
        request->minimum_iv_percent_to_keep <= 100.0 &&
        request->expires_at_elapsed_ns > 0U && request->pid > 0U &&
        !request->process_name.empty() && !request->package_name.empty() &&
        !request->build_fingerprint.empty() && offset == input.size();
}

}  // namespace pogo_runtime_transfer_config
