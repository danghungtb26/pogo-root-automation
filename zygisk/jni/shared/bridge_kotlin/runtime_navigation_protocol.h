#pragma once
#include "runtime_observation_protocol.h"

namespace pogo_runtime {
constexpr uint32_t kNavigationObservationType = 11U;
constexpr uint32_t kNavigationPayloadVersion = 1U;
constexpr uint32_t kNavigationPayloadMagic = 0x504f4757U;  // POGW
enum class NavigationKind : uint32_t { kStop = 0, kWalk = 1, kArrived = 2 };

struct RuntimeNavigation {
    NavigationKind kind = NavigationKind::kStop;
    std::string fort_id;
    double latitude = 0;
    double longitude = 0;
    std::string reason;
};

inline bool valid_navigation_coordinate(double latitude, double longitude) {
    return std::isfinite(latitude) && std::isfinite(longitude) &&
        latitude >= -90 && latitude <= 90 && longitude >= -180 && longitude <= 180;
}

inline bool encode_runtime_navigation(
    const RuntimeNavigation &value, uint64_t epoch_ms, uint64_t elapsed_ns,
    std::vector<uint8_t> *envelope
) {
    if (envelope == nullptr || static_cast<uint32_t>(value.kind) > 2U ||
        value.fort_id.size() > 4096 || value.reason.size() > 4096 ||
        !valid_navigation_coordinate(value.latitude, value.longitude) ||
        (value.kind != NavigationKind::kStop && value.fort_id.empty())) return false;
    std::vector<uint8_t> payload;
    append_u32(&payload, kNavigationPayloadMagic);
    append_u32(&payload, static_cast<uint32_t>(value.kind));
    append_string(&payload, value.fort_id);
    append_f64(&payload, value.latitude);
    append_f64(&payload, value.longitude);
    append_string(&payload, value.reason);
    envelope->clear();
    append_u32(envelope, kRuntimeObservationEnvelopeVersion);
    append_u32(envelope, kNavigationObservationType);
    append_u32(envelope, kNavigationPayloadVersion);
    append_u32(envelope, static_cast<uint32_t>(payload.size()));
    envelope->insert(envelope->end(), payload.begin(), payload.end());
    append_u64(envelope, epoch_ms);
    append_u64(envelope, elapsed_ns);
    return true;
}
}  // namespace pogo_runtime
