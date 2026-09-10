#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace pogo_runtime {
constexpr uint32_t kBridgeCommandType = 4U;
constexpr uint32_t kCommandPayloadVersion = 1U;
constexpr uint32_t kMaxCommandStringBytes = 65536U;

struct RuntimeCommandPrefix {
    uint32_t message_type = 0U;
    uint64_t message_seq = 0U;
    uint32_t payload_version = 0U;
    std::string session_id;
    std::string command_id;
};

inline bool read_be32(
    const std::vector<uint8_t> &input,
    size_t *offset,
    uint32_t *value
) {
    if (offset == nullptr || value == nullptr || *offset > input.size() ||
        input.size() - *offset < 4U) return false;
    *value = (static_cast<uint32_t>(input[*offset]) << 24U) |
        (static_cast<uint32_t>(input[*offset + 1U]) << 16U) |
        (static_cast<uint32_t>(input[*offset + 2U]) << 8U) |
        static_cast<uint32_t>(input[*offset + 3U]);
    *offset += 4U;
    return true;
}

inline bool read_be64(
    const std::vector<uint8_t> &input,
    size_t *offset,
    uint64_t *value
) {
    if (offset == nullptr || value == nullptr || *offset > input.size() || input.size() - *offset < 8U) {
        return false;
    }
    *value = 0U;
    for (size_t index = 0U; index < 8U; ++index) {
        *value = (*value << 8U) | input[*offset + index];
    }
    *offset += 8U;
    return true;
}

inline bool read_string(
    const std::vector<uint8_t> &input,
    size_t *offset,
    std::string *value
) {
    uint32_t length = 0U;
    if (value == nullptr || !read_be32(input, offset, &length) ||
        length > kMaxCommandStringBytes || *offset > input.size() ||
        input.size() - *offset < length) return false;
    value->assign(reinterpret_cast<const char *>(input.data() + *offset), length);
    *offset += length;
    return true;
}

/**
 * Parses only the common command prefix. The action body is intentionally
 * opaque until a verified client-owned binding is installed.
 */
inline bool parse_runtime_command_prefix(
    const std::vector<uint8_t> &input,
    RuntimeCommandPrefix *prefix
) {
    if (prefix == nullptr) return false;
    size_t offset = 0U;
    if (!read_be32(input, &offset, &prefix->message_type) ||
        !read_be64(input, &offset, &prefix->message_seq) ||
        !read_be32(input, &offset, &prefix->payload_version) ||
        !read_string(input, &offset, &prefix->session_id) ||
        !read_string(input, &offset, &prefix->command_id)) return false;
    return prefix->message_type == kBridgeCommandType &&
        prefix->payload_version == kCommandPayloadVersion &&
        prefix->message_seq > 0U &&
        !prefix->session_id.empty() &&
        !prefix->command_id.empty();
}
}  // namespace pogo_runtime
