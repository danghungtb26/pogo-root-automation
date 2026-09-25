#pragma once

#include "runtime_observation_protocol.h"

namespace pogo_runtime {

constexpr uint32_t kPointWalkCandidateObservationType = 12U;
constexpr uint32_t kPointWalkCandidatePayloadVersion = 1U;
constexpr uint32_t kPointWalkCandidatePayloadMagic = 0x504F4743U;  // POGC
constexpr char kPointWalkCandidateCapability[] = "POINT_WALK_CANDIDATE";
constexpr size_t kMaxPointWalkCandidateIdBytes = 128U;

enum class PointWalkCandidateKind : uint32_t {
    kWalk = 1U,
    kStop = 2U,
    kArrived = 3U,
};

struct RuntimePointWalkCandidate {
    PointWalkCandidateKind kind = PointWalkCandidateKind::kWalk;
    std::string candidate_id;
    double latitude = 0.0;
    double longitude = 0.0;
    bool force = false;
};

inline bool valid_point_walk_coordinate(double latitude, double longitude) {
    return std::isfinite(latitude) && std::isfinite(longitude) &&
        latitude >= -90.0 && latitude <= 90.0 &&
        longitude >= -180.0 && longitude <= 180.0;
}

inline bool encode_runtime_point_walk_candidate(
    const RuntimePointWalkCandidate &value,
    uint64_t epoch_ms,
    uint64_t elapsed_ns,
    std::vector<uint8_t> *envelope
) {
    if (envelope == nullptr || value.candidate_id.empty() ||
        value.candidate_id.size() > kMaxPointWalkCandidateIdBytes) return false;
    const uint32_t kind = static_cast<uint32_t>(value.kind);
    if (kind < static_cast<uint32_t>(PointWalkCandidateKind::kWalk) ||
        kind > static_cast<uint32_t>(PointWalkCandidateKind::kArrived)) return false;
    const bool is_walk = value.kind == PointWalkCandidateKind::kWalk;
    if ((!is_walk && value.force) ||
        (is_walk && !valid_point_walk_coordinate(value.latitude, value.longitude))) return false;

    std::vector<uint8_t> payload;
    append_u32(&payload, kPointWalkCandidatePayloadMagic);
    append_u32(&payload, kind);
    append_u32(&payload, value.force ? 1U : 0U);
    if (!append_string(&payload, value.candidate_id)) return false;
    if (is_walk) {
        append_f64(&payload, value.latitude);
        append_f64(&payload, value.longitude);
    }

    envelope->clear();
    append_u32(envelope, kRuntimeObservationEnvelopeVersion);
    append_u32(envelope, kPointWalkCandidateObservationType);
    append_u32(envelope, kPointWalkCandidatePayloadVersion);
    append_u32(envelope, static_cast<uint32_t>(payload.size()));
    envelope->insert(envelope->end(), payload.begin(), payload.end());
    append_u64(envelope, epoch_ms);
    append_u64(envelope, elapsed_ns);
    return true;
}

}  // namespace pogo_runtime
