#pragma once

#include <cstdint>
#include <vector>

#include "runtime_observation_protocol.h"

namespace pogo_runtime {

constexpr uint32_t kCatchSpinRequestObservationType = 9U;
constexpr uint32_t kRuntimeCatchSpinPayloadVersion = 1U;
constexpr uint32_t kRuntimeCatchSpinPayloadMagic = 0x504F4743U;  // POGC
constexpr uint32_t kCatchSpinNearbyPresent = 1U << 0U;
constexpr uint32_t kCatchSpinFortsPresent = 1U << 1U;
constexpr uint32_t kCatchSpinInventoryPresent = 1U << 2U;
constexpr uint32_t kCatchSpinPlayerPositionPresent = 1U << 3U;

struct RuntimeCatchSpinObservation {
    uint64_t cycle_id = 0U;
    bool nearby_available = false;
    RuntimeNearbyObservation nearby;
    bool forts_available = false;
    RuntimeFortsObservation forts;
    bool inventory_available = false;
    RuntimeInventoryObservation inventory;
    bool has_player_position = false;
    double player_latitude = 0.0;
    double player_longitude = 0.0;
};

inline bool append_payload_blob(
    std::vector<uint8_t> *output,
    const std::vector<uint8_t> &payload
) {
    if (output == nullptr || payload.size() > 4U * 1024U * 1024U) return false;
    append_u32(output, static_cast<uint32_t>(payload.size()));
    output->insert(output->end(), payload.begin(), payload.end());
    return true;
}

inline bool encode_runtime_catch_spin_payload(
    const RuntimeCatchSpinObservation &value,
    std::vector<uint8_t> *payload
) {
    if (payload == nullptr || value.cycle_id == 0U) return false;
    if (value.has_player_position && (
            !std::isfinite(value.player_latitude) || value.player_latitude < -90.0 ||
            value.player_latitude > 90.0 || !std::isfinite(value.player_longitude) ||
            value.player_longitude < -180.0 || value.player_longitude > 180.0)) {
        return false;
    }
    std::vector<uint8_t> nearby_payload;
    std::vector<uint8_t> forts_payload;
    std::vector<uint8_t> inventory_payload;
    if (value.nearby_available && !encode_runtime_nearby_payload(value.nearby, &nearby_payload)) {
        return false;
    }
    if (value.forts_available && !encode_runtime_forts_payload(value.forts, &forts_payload)) {
        return false;
    }
    if (value.inventory_available &&
        !encode_runtime_inventory_payload(value.inventory, &inventory_payload)) {
        return false;
    }

    uint32_t flags = 0U;
    if (value.nearby_available) flags |= kCatchSpinNearbyPresent;
    if (value.forts_available) flags |= kCatchSpinFortsPresent;
    if (value.inventory_available) flags |= kCatchSpinInventoryPresent;
    if (value.has_player_position) flags |= kCatchSpinPlayerPositionPresent;

    payload->clear();
    append_u32(payload, kRuntimeCatchSpinPayloadMagic);
    append_u64(payload, value.cycle_id);
    append_u32(payload, flags);
    if (value.has_player_position) {
        append_f64(payload, value.player_latitude);
        append_f64(payload, value.player_longitude);
    }
    if (!append_payload_blob(payload, nearby_payload)) return false;
    if (!append_payload_blob(payload, forts_payload)) return false;
    if (!append_payload_blob(payload, inventory_payload)) return false;
    return payload->size() <= 4U * 1024U * 1024U;
}

inline bool encode_runtime_catch_spin_observation(
    const RuntimeCatchSpinObservation &value,
    uint64_t observed_at_epoch_ms,
    uint64_t observed_at_elapsed_ns,
    std::vector<uint8_t> *envelope
) {
    if (envelope == nullptr) return false;
    std::vector<uint8_t> payload;
    if (!encode_runtime_catch_spin_payload(value, &payload)) return false;
    envelope->clear();
    append_u32(envelope, kRuntimeObservationEnvelopeVersion);
    append_u32(envelope, kCatchSpinRequestObservationType);
    append_u32(envelope, kRuntimeCatchSpinPayloadVersion);
    append_u32(envelope, static_cast<uint32_t>(payload.size()));
    envelope->insert(envelope->end(), payload.begin(), payload.end());
    append_u64(envelope, observed_at_epoch_ms);
    append_u64(envelope, observed_at_elapsed_ns);
    return true;
}

}  // namespace pogo_runtime
