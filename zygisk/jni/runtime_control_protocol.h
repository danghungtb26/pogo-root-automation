#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace pogo_runtime_control {

constexpr uint32_t kBridgeCommandType = 4U;
constexpr uint32_t kPayloadVersion = 1U;
constexpr uint32_t kMarker = 0x52544354U;  // ASCII "RTCT"

enum class Action : uint32_t {
    kStart = 1U,
    kStop = 2U,
    kDiagnostic = 3U,
};

struct Request {
    uint64_t message_seq = 0U;
    std::string runtime_session_id;
    std::string request_id;
    Action action = Action::kDiagnostic;
    uint64_t expires_at_elapsed_ns = 0U;
    uint32_t pid = 0U;
    std::string process_name;
    std::string package_name;
};

inline bool read_u32(
    const std::vector<uint8_t> &input,
    size_t *offset,
    uint32_t *value
) {
    if (offset == nullptr || value == nullptr || *offset > input.size() ||
        input.size() - *offset < 4U) return false;
    const uint8_t *data = input.data() + *offset;
    *value = (static_cast<uint32_t>(data[0]) << 24U) |
        (static_cast<uint32_t>(data[1]) << 16U) |
        (static_cast<uint32_t>(data[2]) << 8U) |
        static_cast<uint32_t>(data[3]);
    *offset += 4U;
    return true;
}

inline bool read_u64(
    const std::vector<uint8_t> &input,
    size_t *offset,
    uint64_t *value
) {
    if (offset == nullptr || value == nullptr || *offset > input.size() ||
        input.size() - *offset < 8U) return false;
    uint64_t result = 0U;
    for (size_t index = 0U; index < 8U; ++index) {
        result = (result << 8U) | input[*offset + index];
    }
    *value = result;
    *offset += 8U;
    return true;
}

inline bool read_string(
    const std::vector<uint8_t> &input,
    size_t *offset,
    std::string *value
) {
    uint32_t length = 0U;
    if (value == nullptr || !read_u32(input, offset, &length) ||
        offset == nullptr || *offset > input.size() ||
        input.size() - *offset < length) return false;
    value->assign(
        reinterpret_cast<const char *>(input.data() + *offset),
        static_cast<size_t>(length)
    );
    *offset += length;
    return true;
}

inline bool parse(const std::vector<uint8_t> &command, Request *request) {
    if (request == nullptr) return false;
    *request = Request{};

    size_t offset = 0U;
    uint32_t message_type = 0U;
    uint32_t payload_version = 0U;
    uint32_t marker = 0U;
    uint32_t action_wire = 0U;
    if (!read_u32(command, &offset, &message_type) ||
        !read_u64(command, &offset, &request->message_seq) ||
        !read_u32(command, &offset, &payload_version) ||
        !read_string(command, &offset, &request->runtime_session_id) ||
        !read_string(command, &offset, &request->request_id) ||
        !read_u32(command, &offset, &marker) ||
        !read_u32(command, &offset, &action_wire) ||
        !read_u64(command, &offset, &request->expires_at_elapsed_ns) ||
        !read_u32(command, &offset, &request->pid) ||
        !read_string(command, &offset, &request->process_name) ||
        !read_string(command, &offset, &request->package_name)) {
        return false;
    }

    if (message_type != kBridgeCommandType || payload_version != kPayloadVersion ||
        marker != kMarker || request->message_seq == 0U ||
        request->runtime_session_id.empty() || request->request_id.empty() ||
        request->expires_at_elapsed_ns == 0U || request->pid == 0U ||
        request->process_name.empty() || request->package_name.empty() ||
        offset != command.size()) {
        return false;
    }

    switch (action_wire) {
        case static_cast<uint32_t>(Action::kStart):
            request->action = Action::kStart;
            return true;
        case static_cast<uint32_t>(Action::kStop):
            request->action = Action::kStop;
            return true;
        case static_cast<uint32_t>(Action::kDiagnostic):
            request->action = Action::kDiagnostic;
            return true;
        default:
            return false;
    }
}

}  // namespace pogo_runtime_control
