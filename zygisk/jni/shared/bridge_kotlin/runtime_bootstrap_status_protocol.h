#pragma once

#include <algorithm>
#include <climits>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace pogo_runtime_bootstrap {

constexpr uint32_t kPayloadVersion = 1U;
constexpr uint32_t kMarker = 0x52425354U;
constexpr uint32_t kSchemaVersion = 1U;
constexpr uint32_t kMaxIdBytes = 256U;
constexpr uint32_t kMaxBuildBytes = 4096U;
constexpr uint32_t kMaxAbiBytes = 64U;
constexpr uint32_t kMaxErrorBytes = 256U;
constexpr uint32_t kMaxMessageBytes = 4096U;
constexpr uint32_t kMaxCapabilities = 256U;
constexpr uint32_t kMaxPayloadBytes = 4U * 1024U * 1024U;

enum class Phase : uint32_t {
    kCreated = 1U,
    kCoreLoaded = 2U,
    kLoaderObserverActive = 3U,
    kIl2cppLibrarySeen = 4U,
    kIl2cppInitialized = 5U,
    kBuildVerified = 6U,
    kMainThreadReady = 7U,
    kBindingsReady = 8U,
    kLiveReady = 9U,
    kFailed = 10U,
};

struct Status {
    std::string session_id;
    int32_t pid = 0;
    std::string process_name;
    uint64_t sequence = 0U;
    Phase phase = Phase::kCreated;
    std::string game_build;
    std::string abi;
    std::string translation_layer;
    bool recoverable = false;
    std::string error_code;
    std::string message;
    bool strong_identity_verified = false;
    std::vector<std::string> capabilities;
    uint64_t observed_at_epoch_ms = 0U;
    uint64_t observed_at_elapsed_ns = 0U;
};

inline void append_u32(std::vector<uint8_t> *output, uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) {
        output->push_back(static_cast<uint8_t>((value >> shift) & 0xffU));
    }
}

inline void append_u64(std::vector<uint8_t> *output, uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        output->push_back(static_cast<uint8_t>((value >> shift) & 0xffU));
    }
}

inline void append_string(std::vector<uint8_t> *output, const std::string &value) {
    append_u32(output, static_cast<uint32_t>(value.size()));
    output->insert(output->end(), value.begin(), value.end());
}

inline void append_optional(
    std::vector<uint8_t> *output,
    const std::string &value
) {
    output->push_back(value.empty() ? 0U : 1U);
    if (!value.empty()) append_string(output, value);
}

inline bool valid_string(const std::string &value, size_t limit) {
    return !value.empty() && value.size() <= limit;
}

inline bool encode(const Status &status, std::vector<uint8_t> *output) {
    if (output == nullptr || !valid_string(status.session_id, kMaxIdBytes) ||
        status.pid <= 0 || !valid_string(status.process_name, kMaxIdBytes) ||
        status.sequence == 0U || status.sequence > INT64_MAX ||
        status.observed_at_epoch_ms > INT64_MAX ||
        status.observed_at_elapsed_ns > INT64_MAX ||
        static_cast<uint32_t>(status.phase) < static_cast<uint32_t>(Phase::kCreated) ||
        static_cast<uint32_t>(status.phase) > static_cast<uint32_t>(Phase::kFailed) ||
        status.game_build.size() > kMaxBuildBytes ||
        status.abi.size() > kMaxAbiBytes ||
        status.translation_layer.size() > kMaxAbiBytes ||
        status.error_code.size() > kMaxErrorBytes ||
        status.message.size() > kMaxMessageBytes ||
        status.capabilities.size() > kMaxCapabilities) return false;
    if (status.phase == Phase::kFailed &&
        (status.error_code.empty() || status.strong_identity_verified ||
         !status.capabilities.empty())) return false;
    if (status.phase == Phase::kLiveReady &&
        (!status.strong_identity_verified || status.game_build.empty() ||
         status.capabilities.empty())) return false;
    if (status.phase != Phase::kLiveReady &&
        !status.capabilities.empty()) return false;
    if (static_cast<uint32_t>(status.phase) <
            static_cast<uint32_t>(Phase::kBuildVerified) &&
        (!status.game_build.empty() || status.strong_identity_verified)) return false;
    for (size_t index = 0U; index < status.capabilities.size(); ++index) {
        if (!valid_string(status.capabilities[index], kMaxErrorBytes) ||
            (index > 0U && status.capabilities[index - 1U] >= status.capabilities[index])) {
            return false;
        }
    }

    output->clear();
    append_u32(output, kPayloadVersion);
    append_u32(output, kMarker);
    append_u32(output, kSchemaVersion);
    append_string(output, status.session_id);
    append_u32(output, static_cast<uint32_t>(status.pid));
    append_string(output, status.process_name);
    append_u64(output, status.sequence);
    append_u32(output, static_cast<uint32_t>(status.phase));
    append_optional(output, status.game_build);
    append_optional(output, status.abi);
    append_optional(output, status.translation_layer);
    output->push_back(status.recoverable ? 1U : 0U);
    append_optional(output, status.error_code);
    append_optional(output, status.message);
    output->push_back(status.strong_identity_verified ? 1U : 0U);
    append_u32(output, static_cast<uint32_t>(status.capabilities.size()));
    for (const std::string &capability : status.capabilities) {
        append_string(output, capability);
    }
    append_u64(output, status.observed_at_epoch_ms);
    append_u64(output, status.observed_at_elapsed_ns);
    return output->size() <= kMaxPayloadBytes;
}

class Reader {
public:
    explicit Reader(const std::vector<uint8_t> &bytes) : bytes_(bytes) {}

    bool u32(uint32_t *value) {
        if (value == nullptr || bytes_.size() - offset_ < 4U) return false;
        *value = 0U;
        for (size_t index = 0U; index < 4U; ++index) {
            *value = (*value << 8U) | bytes_[offset_++];
        }
        return true;
    }

    bool u64(uint64_t *value) {
        if (value == nullptr || bytes_.size() - offset_ < 8U) return false;
        *value = 0U;
        for (size_t index = 0U; index < 8U; ++index) {
            *value = (*value << 8U) | bytes_[offset_++];
        }
        return true;
    }

    bool boolean(bool *value) {
        if (value == nullptr || offset_ >= bytes_.size() || bytes_[offset_] > 1U) return false;
        *value = bytes_[offset_++] == 1U;
        return true;
    }

    bool string(std::string *value, size_t maximum) {
        uint32_t length = 0U;
        if (value == nullptr || !u32(&length) || length == 0U || length > maximum ||
            bytes_.size() - offset_ < length) return false;
        value->assign(reinterpret_cast<const char *>(bytes_.data() + offset_), length);
        offset_ += length;
        return true;
    }

    bool optional(std::string *value, size_t maximum) {
        bool present = false;
        if (value == nullptr || !boolean(&present)) return false;
        if (!present) {
            value->clear();
            return true;
        }
        return string(value, maximum);
    }

    bool done() const { return offset_ == bytes_.size(); }

private:
    const std::vector<uint8_t> &bytes_;
    size_t offset_ = 0U;
};

inline bool decode(const std::vector<uint8_t> &payload, Status *status) {
    if (status == nullptr || payload.size() > kMaxPayloadBytes) return false;
    Reader reader(payload);
    uint32_t payload_version = 0U;
    uint32_t marker = 0U;
    uint32_t schema_version = 0U;
    uint32_t pid = 0U;
    uint32_t phase = 0U;
    uint32_t capability_count = 0U;
    Status parsed;
    if (!reader.u32(&payload_version) || payload_version != kPayloadVersion ||
        !reader.u32(&marker) || marker != kMarker ||
        !reader.u32(&schema_version) || schema_version != kSchemaVersion ||
        !reader.string(&parsed.session_id, kMaxIdBytes) ||
        !reader.u32(&pid) || pid == 0U || pid > INT32_MAX ||
        !reader.string(&parsed.process_name, kMaxIdBytes) ||
        !reader.u64(&parsed.sequence) ||
        !reader.u32(&phase) || phase < 1U || phase > 10U ||
        !reader.optional(&parsed.game_build, kMaxBuildBytes) ||
        !reader.optional(&parsed.abi, kMaxAbiBytes) ||
        !reader.optional(&parsed.translation_layer, kMaxAbiBytes) ||
        !reader.boolean(&parsed.recoverable) ||
        !reader.optional(&parsed.error_code, kMaxErrorBytes) ||
        !reader.optional(&parsed.message, kMaxMessageBytes) ||
        !reader.boolean(&parsed.strong_identity_verified) ||
        !reader.u32(&capability_count) || capability_count > kMaxCapabilities) return false;
    parsed.pid = static_cast<int32_t>(pid);
    parsed.phase = static_cast<Phase>(phase);
    for (uint32_t index = 0U; index < capability_count; ++index) {
        std::string capability;
        if (!reader.string(&capability, kMaxErrorBytes)) return false;
        parsed.capabilities.push_back(std::move(capability));
    }
    if (!reader.u64(&parsed.observed_at_epoch_ms) ||
        !reader.u64(&parsed.observed_at_elapsed_ns) || !reader.done()) return false;
    std::vector<uint8_t> canonical;
    if (!encode(parsed, &canonical) || canonical != payload) return false;
    *status = std::move(parsed);
    return true;
}

}  // namespace pogo_runtime_bootstrap
