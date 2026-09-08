#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace pogo_runtime {

constexpr uint32_t kRuntimeObservationMagic = 0x504F4749U;
constexpr uint32_t kRuntimeObservationEnvelopeVersion = 1U;
constexpr uint32_t kMapTargetObservationType = 7U;
constexpr uint32_t kMapTargetPayloadVersion = 1U;
constexpr uint32_t kMaxObservationStringBytes = 65536U;

struct MapTargetObservation {
    std::string tap_id;
    double target_latitude = 0.0;
    double target_longitude = 0.0;
    float screen_x = 0.0F;
    float screen_y = 0.0F;
    int32_t viewport_width = 0;
    int32_t viewport_height = 0;
    std::string camera_snapshot_id;
};

inline void append_u32(std::vector<uint8_t> *output, uint32_t value) {
    if (output == nullptr) return;
    output->push_back(static_cast<uint8_t>((value >> 24U) & 0xffU));
    output->push_back(static_cast<uint8_t>((value >> 16U) & 0xffU));
    output->push_back(static_cast<uint8_t>((value >> 8U) & 0xffU));
    output->push_back(static_cast<uint8_t>(value & 0xffU));
}

inline void append_u64(std::vector<uint8_t> *output, uint64_t value) {
    if (output == nullptr) return;
    for (int shift = 56; shift >= 0; shift -= 8) {
        output->push_back(static_cast<uint8_t>((value >> shift) & 0xffU));
    }
}

inline void append_f32(std::vector<uint8_t> *output, float value) {
    uint32_t bits = 0U;
    static_assert(sizeof(bits) == sizeof(value));
    memcpy(&bits, &value, sizeof(bits));
    append_u32(output, bits);
}

inline void append_f64(std::vector<uint8_t> *output, double value) {
    uint64_t bits = 0U;
    static_assert(sizeof(bits) == sizeof(value));
    memcpy(&bits, &value, sizeof(bits));
    append_u64(output, bits);
}

inline bool append_string(std::vector<uint8_t> *output, const std::string &value) {
    if (output == nullptr || value.size() > kMaxObservationStringBytes) return false;
    append_u32(output, static_cast<uint32_t>(value.size()));
    output->insert(output->end(), value.begin(), value.end());
    return true;
}

inline bool valid_map_target_observation(const MapTargetObservation &value) {
    return !value.tap_id.empty() && value.tap_id.size() <= kMaxObservationStringBytes &&
        std::isfinite(value.target_latitude) && value.target_latitude >= -90.0 &&
        value.target_latitude <= 90.0 && std::isfinite(value.target_longitude) &&
        value.target_longitude >= -180.0 && value.target_longitude <= 180.0 &&
        std::isfinite(value.screen_x) && std::isfinite(value.screen_y) &&
        value.viewport_width > 0 && value.viewport_height > 0 &&
        value.screen_x >= 0.0F && value.screen_x <= static_cast<float>(value.viewport_width) &&
        value.screen_y >= 0.0F && value.screen_y <= static_cast<float>(value.viewport_height) &&
        value.camera_snapshot_id.size() <= kMaxObservationStringBytes;
}

/** Encodes the payload consumed by MapTargetPayloadCodec on the Kotlin side. */
inline bool encode_map_target_payload(
    const MapTargetObservation &value,
    std::vector<uint8_t> *payload
) {
    if (payload == nullptr || !valid_map_target_observation(value)) return false;
    payload->clear();
    append_u32(payload, kMapTargetPayloadVersion);
    if (!append_string(payload, value.tap_id)) return false;
    append_f64(payload, value.target_latitude);
    append_f64(payload, value.target_longitude);
    append_f32(payload, value.screen_x);
    append_f32(payload, value.screen_y);
    append_u32(payload, static_cast<uint32_t>(value.viewport_width));
    append_u32(payload, static_cast<uint32_t>(value.viewport_height));
    payload->push_back(value.camera_snapshot_id.empty() ? 0U : 1U);
    if (!value.camera_snapshot_id.empty() && !append_string(payload, value.camera_snapshot_id)) return false;
    return true;
}

/**
 * Encodes the private target-process envelope. The companion adds runtime
 * identity and bridge sequencing before forwarding it as ObservationEvent.
 */
inline bool encode_runtime_map_target_observation(
    const MapTargetObservation &value,
    uint64_t observed_at_epoch_ms,
    uint64_t observed_at_elapsed_ns,
    std::vector<uint8_t> *envelope
) {
    if (envelope == nullptr) return false;
    std::vector<uint8_t> payload;
    if (!encode_map_target_payload(value, &payload)) return false;

    envelope->clear();
    append_u32(envelope, kRuntimeObservationEnvelopeVersion);
    append_u32(envelope, kMapTargetObservationType);
    append_u32(envelope, kMapTargetPayloadVersion);
    append_u32(envelope, static_cast<uint32_t>(payload.size()));
    envelope->insert(envelope->end(), payload.begin(), payload.end());
    append_u64(envelope, observed_at_epoch_ms);
    append_u64(envelope, observed_at_elapsed_ns);
    return true;
}

}  // namespace pogo_runtime
