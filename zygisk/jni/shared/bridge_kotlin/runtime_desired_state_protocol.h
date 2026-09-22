#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "runtime_command_protocol.h"

namespace pogo_runtime_desired_state {

constexpr uint32_t kMarker = 0x52445354U;  // RDST
constexpr uint32_t kSchemaVersion = 1U;
constexpr uint32_t kDesiredStateSetAction = 9U;
constexpr uint32_t kMaxDiscardLimits = 64U;
constexpr uint64_t kMaxSettleDelayMs = 60000U;
constexpr uint64_t kMaxExpiryWindowNs = 30'000'000'000ULL;
constexpr size_t kMaxLogicalIdBytes = 256U;
constexpr size_t kMaxBuildFingerprintBytes = 4096U;

struct State {
    uint64_t config_revision = 0U;
    bool enabled = false;
    bool catch_spin_armed = false;
    bool auto_catch = false;
    bool auto_spin = false;
    bool auto_encounter = false;
    bool catch_all = true;
    bool auto_walk_to_fort = false;
    uint64_t spin_settle_delay_ms = 0U;
    uint64_t catch_settle_delay_ms = 0U;
    bool auto_discard = false;
    std::vector<std::pair<uint32_t, uint32_t>> discard_limits;
    bool auto_transfer = false;
    double minimum_iv_percent_to_keep = 80.0;
    bool keep_unknown_iv = true;
    bool keep_shiny = true;
    bool keep_hundo = true;
    bool keep_special_background = true;
    bool keep_favorite = true;
    bool keep_legendary = true;
    bool keep_mythical = true;
};

struct Request {
    uint64_t message_seq = 0U;
    std::string runtime_session_id;
    std::string request_id;
    State state;
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
    const uint8_t wire = input[*offset];
    ++*offset;
    if (wire > 1U) return false;
    *value = wire == 1U;
    return true;
}

inline bool read_f64(
    const std::vector<uint8_t> &input,
    size_t *offset,
    double *value
) {
    uint64_t bits = 0U;
    if (value == nullptr || !pogo_runtime::read_be64(input, offset, &bits)) return false;
    std::memcpy(value, &bits, sizeof(bits));
    return std::isfinite(*value);
}

inline bool logical_string_size(const std::string &value, size_t max_bytes) {
    return !value.empty() && value.size() <= max_bytes;
}

inline bool parse(const std::vector<uint8_t> &input, Request *request) {
    if (request == nullptr) return false;
    *request = Request{};
    size_t offset = 0U;
    uint32_t message_type = 0U;
    uint32_t payload_version = 0U;
    uint32_t marker = 0U;
    uint32_t schema_version = 0U;
    if (!pogo_runtime::read_be32(input, &offset, &message_type) ||
        !pogo_runtime::read_be64(input, &offset, &request->message_seq) ||
        !pogo_runtime::read_be32(input, &offset, &payload_version) ||
        !pogo_runtime::read_string(input, &offset, &request->runtime_session_id) ||
        !pogo_runtime::read_string(input, &offset, &request->request_id) ||
        !pogo_runtime::read_be32(input, &offset, &marker) ||
        !pogo_runtime::read_be32(input, &offset, &schema_version) ||
        marker != kMarker || schema_version != kSchemaVersion ||
        !pogo_runtime::read_be64(input, &offset, &request->state.config_revision) ||
        !read_bool(input, &offset, &request->state.enabled) ||
        !read_bool(input, &offset, &request->state.catch_spin_armed) ||
        !read_bool(input, &offset, &request->state.auto_catch) ||
        !read_bool(input, &offset, &request->state.auto_spin) ||
        !read_bool(input, &offset, &request->state.auto_encounter) ||
        !read_bool(input, &offset, &request->state.catch_all) ||
        !read_bool(input, &offset, &request->state.auto_walk_to_fort) ||
        !pogo_runtime::read_be64(input, &offset, &request->state.spin_settle_delay_ms) ||
        !pogo_runtime::read_be64(input, &offset, &request->state.catch_settle_delay_ms) ||
        !read_bool(input, &offset, &request->state.auto_discard)) return false;

    uint32_t limit_count = 0U;
    if (!pogo_runtime::read_be32(input, &offset, &limit_count) ||
        limit_count > kMaxDiscardLimits) return false;
    request->state.discard_limits.clear();
    request->state.discard_limits.reserve(limit_count);
    uint32_t previous_item_id = 0U;
    for (uint32_t index = 0U; index < limit_count; ++index) {
        uint32_t item_id = 0U;
        uint32_t max_count = 0U;
        if (!pogo_runtime::read_be32(input, &offset, &item_id) ||
            !pogo_runtime::read_be32(input, &offset, &max_count) ||
            item_id == 0U || item_id <= previous_item_id) return false;
        request->state.discard_limits.emplace_back(item_id, max_count);
        previous_item_id = item_id;
    }

    if (!read_bool(input, &offset, &request->state.auto_transfer) ||
        !read_f64(input, &offset, &request->state.minimum_iv_percent_to_keep) ||
        !read_bool(input, &offset, &request->state.keep_unknown_iv) ||
        !read_bool(input, &offset, &request->state.keep_shiny) ||
        !read_bool(input, &offset, &request->state.keep_hundo) ||
        !read_bool(input, &offset, &request->state.keep_special_background) ||
        !read_bool(input, &offset, &request->state.keep_favorite) ||
        !read_bool(input, &offset, &request->state.keep_legendary) ||
        !read_bool(input, &offset, &request->state.keep_mythical) ||
        !pogo_runtime::read_be64(input, &offset, &request->expires_at_elapsed_ns) ||
        !pogo_runtime::read_be32(input, &offset, &request->pid) ||
        !pogo_runtime::read_string(input, &offset, &request->process_name) ||
        !pogo_runtime::read_string(input, &offset, &request->package_name) ||
        !pogo_runtime::read_string(input, &offset, &request->build_fingerprint)) return false;

    return message_type == pogo_runtime::kBridgeCommandType &&
        payload_version == pogo_runtime::kCommandPayloadVersion &&
        request->message_seq > 0U &&
        request->state.config_revision > 0U &&
        request->state.spin_settle_delay_ms <= kMaxSettleDelayMs &&
        request->state.catch_settle_delay_ms <= kMaxSettleDelayMs &&
        request->state.minimum_iv_percent_to_keep >= 0.0 &&
        request->state.minimum_iv_percent_to_keep <= 100.0 &&
        request->expires_at_elapsed_ns > 0U &&
        request->pid > 0U &&
        logical_string_size(request->runtime_session_id, kMaxLogicalIdBytes) &&
        logical_string_size(request->request_id, kMaxLogicalIdBytes) &&
        logical_string_size(request->process_name, kMaxLogicalIdBytes) &&
        logical_string_size(request->package_name, kMaxLogicalIdBytes) &&
        logical_string_size(request->build_fingerprint, kMaxBuildFingerprintBytes) &&
        offset == input.size();
}

}  // namespace pogo_runtime_desired_state
