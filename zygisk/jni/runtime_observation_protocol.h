#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace pogo_runtime {

constexpr uint32_t kRuntimeObservationMagic = 0x504F4749U;
constexpr uint32_t kRuntimeObservationEnvelopeVersion = 1U;
constexpr uint32_t kLifecycleObservationType = 1U;
constexpr uint32_t kLifecyclePayloadVersion = 1U;
constexpr uint32_t kEncounterObservationType = 3U;
constexpr uint32_t kFortsObservationType = 4U;
constexpr uint32_t kRuntimeNearbyPayloadVersion = 1U;
constexpr uint32_t kRuntimeNearbyPayloadMagic = 0x504F474EU;  // POGN
constexpr uint32_t kRuntimeFortsPayloadVersion = 1U;
constexpr uint32_t kRuntimeFortsPayloadMagic = 0x504F4746U;  // POGF
/** Structured IL2CPP observation; distinct from raw EncounterOutProto v1. */
constexpr uint32_t kRuntimeEncounterPayloadVersion = 2U;
constexpr uint32_t kRuntimeEncounterPayloadMagic = 0x504F4745U;  // POGE
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

struct RuntimeEncounterObservation {
    uint64_t encounter_id = 0U;
    int32_t species_id = 0;
    int32_t individual_attack = 0;
    int32_t individual_defense = 0;
    int32_t individual_stamina = 0;
    bool shiny = false;
    bool has_shiny = false;
    double latitude = 0.0;
    double longitude = 0.0;
};

struct RuntimeNearbySpawnObservation {
    uint64_t spawn_id = 0U;
    int32_t species_id = 0;
    double latitude = 0.0;
    double longitude = 0.0;
};

struct RuntimeNearbyObservation {
    std::vector<RuntimeNearbySpawnObservation> spawns;
};

struct RuntimeFortObservation {
    std::string fort_id;
    int32_t type = 0;
    double latitude = 0.0;
    double longitude = 0.0;
    bool spin_available = false;
};

struct RuntimeFortsObservation {
    std::vector<RuntimeFortObservation> forts;
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

inline bool valid_runtime_encounter_observation(const RuntimeEncounterObservation &value) {
    return value.encounter_id != 0U && value.species_id > 0 &&
        value.individual_attack >= 0 && value.individual_attack <= 15 &&
        value.individual_defense >= 0 && value.individual_defense <= 15 &&
        value.individual_stamina >= 0 && value.individual_stamina <= 15 &&
        std::isfinite(value.latitude) && value.latitude >= -90.0 && value.latitude <= 90.0 &&
        std::isfinite(value.longitude) && value.longitude >= -180.0 && value.longitude <= 180.0;
}

inline bool valid_runtime_nearby_observation(const RuntimeNearbyObservation &value) {
    if (value.spawns.size() > 512U) return false;
    for (const RuntimeNearbySpawnObservation &spawn : value.spawns) {
        if (spawn.spawn_id == 0U || spawn.species_id <= 0 ||
            !std::isfinite(spawn.latitude) || spawn.latitude < -90.0 ||
            spawn.latitude > 90.0 || !std::isfinite(spawn.longitude) ||
            spawn.longitude < -180.0 || spawn.longitude > 180.0) return false;
    }
    return true;
}

inline bool valid_runtime_forts_observation(const RuntimeFortsObservation &value) {
    if (value.forts.size() > 512U) return false;
    for (const RuntimeFortObservation &fort : value.forts) {
        if (fort.fort_id.empty() || fort.fort_id.size() > kMaxObservationStringBytes ||
            fort.type < 0 || fort.type > 1 || !std::isfinite(fort.latitude) ||
            fort.latitude < -90.0 || fort.latitude > 90.0 ||
            !std::isfinite(fort.longitude) || fort.longitude < -180.0 ||
            fort.longitude > 180.0) return false;
    }
    return true;
}

inline bool encode_runtime_nearby_payload(
    const RuntimeNearbyObservation &value,
    std::vector<uint8_t> *payload
) {
    if (payload == nullptr || !valid_runtime_nearby_observation(value)) return false;
    payload->clear();
    append_u32(payload, kRuntimeNearbyPayloadMagic);
    append_u32(payload, static_cast<uint32_t>(value.spawns.size()));
    for (const RuntimeNearbySpawnObservation &spawn : value.spawns) {
        append_u64(payload, spawn.spawn_id);
        append_u32(payload, static_cast<uint32_t>(spawn.species_id));
        append_f64(payload, spawn.latitude);
        append_f64(payload, spawn.longitude);
    }
    return true;
}

inline bool encode_runtime_forts_payload(
    const RuntimeFortsObservation &value,
    std::vector<uint8_t> *payload
) {
    if (payload == nullptr || !valid_runtime_forts_observation(value)) return false;
    payload->clear();
    append_u32(payload, kRuntimeFortsPayloadMagic);
    append_u32(payload, static_cast<uint32_t>(value.forts.size()));
    for (const RuntimeFortObservation &fort : value.forts) {
        if (!append_string(payload, fort.fort_id)) return false;
        append_u32(payload, static_cast<uint32_t>(fort.type));
        append_f64(payload, fort.latitude);
        append_f64(payload, fort.longitude);
        payload->push_back(fort.spin_available ? 1U : 0U);
    }
    return true;
}

inline bool encode_runtime_nearby_observation(
    const RuntimeNearbyObservation &value,
    uint64_t observed_at_epoch_ms,
    uint64_t observed_at_elapsed_ns,
    std::vector<uint8_t> *envelope
) {
    if (envelope == nullptr) return false;
    std::vector<uint8_t> payload;
    if (!encode_runtime_nearby_payload(value, &payload)) return false;
    envelope->clear();
    append_u32(envelope, kRuntimeObservationEnvelopeVersion);
    append_u32(envelope, 2U);
    append_u32(envelope, kRuntimeNearbyPayloadVersion);
    append_u32(envelope, static_cast<uint32_t>(payload.size()));
    envelope->insert(envelope->end(), payload.begin(), payload.end());
    append_u64(envelope, observed_at_epoch_ms);
    append_u64(envelope, observed_at_elapsed_ns);
    return true;
}

inline bool encode_runtime_forts_observation(
    const RuntimeFortsObservation &value,
    uint64_t observed_at_epoch_ms,
    uint64_t observed_at_elapsed_ns,
    std::vector<uint8_t> *envelope
) {
    if (envelope == nullptr) return false;
    std::vector<uint8_t> payload;
    if (!encode_runtime_forts_payload(value, &payload)) return false;
    envelope->clear();
    append_u32(envelope, kRuntimeObservationEnvelopeVersion);
    append_u32(envelope, kFortsObservationType);
    append_u32(envelope, kRuntimeFortsPayloadVersion);
    append_u32(envelope, static_cast<uint32_t>(payload.size()));
    envelope->insert(envelope->end(), payload.begin(), payload.end());
    append_u64(envelope, observed_at_epoch_ms);
    append_u64(envelope, observed_at_elapsed_ns);
    return true;
}

inline bool encode_runtime_encounter_payload(
    const RuntimeEncounterObservation &value,
    std::vector<uint8_t> *payload
) {
    if (payload == nullptr || !valid_runtime_encounter_observation(value)) return false;
    payload->clear();
    append_u32(payload, kRuntimeEncounterPayloadMagic);
    append_u64(payload, value.encounter_id);
    append_u32(payload, static_cast<uint32_t>(value.species_id));
    append_u32(payload, static_cast<uint32_t>(value.individual_attack));
    append_u32(payload, static_cast<uint32_t>(value.individual_defense));
    append_u32(payload, static_cast<uint32_t>(value.individual_stamina));
    payload->push_back(value.has_shiny ? 1U : 0U);
    if (value.has_shiny) payload->push_back(value.shiny ? 1U : 0U);
    append_f64(payload, value.latitude);
    append_f64(payload, value.longitude);
    return true;
}

inline bool encode_runtime_lifecycle_observation(
    uint32_t lifecycle_wire,
    uint64_t observed_at_epoch_ms,
    uint64_t observed_at_elapsed_ns,
    std::vector<uint8_t> *envelope
) {
    if (envelope == nullptr || lifecycle_wire == 0U) return false;
    envelope->clear();
    append_u32(envelope, kRuntimeObservationEnvelopeVersion);
    append_u32(envelope, kLifecycleObservationType);
    append_u32(envelope, kLifecyclePayloadVersion);
    append_u32(envelope, 4U);
    append_u32(envelope, lifecycle_wire);
    append_u64(envelope, observed_at_epoch_ms);
    append_u64(envelope, observed_at_elapsed_ns);
    return true;
}

inline bool encode_runtime_encounter_observation(
    const RuntimeEncounterObservation &value,
    uint64_t observed_at_epoch_ms,
    uint64_t observed_at_elapsed_ns,
    std::vector<uint8_t> *envelope
) {
    if (envelope == nullptr) return false;
    std::vector<uint8_t> payload;
    if (!encode_runtime_encounter_payload(value, &payload)) return false;
    envelope->clear();
    append_u32(envelope, kRuntimeObservationEnvelopeVersion);
    append_u32(envelope, kEncounterObservationType);
    append_u32(envelope, kRuntimeEncounterPayloadVersion);
    append_u32(envelope, static_cast<uint32_t>(payload.size()));
    envelope->insert(envelope->end(), payload.begin(), payload.end());
    append_u64(envelope, observed_at_epoch_ms);
    append_u64(envelope, observed_at_elapsed_ns);
    return true;
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
