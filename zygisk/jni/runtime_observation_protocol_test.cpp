#include "runtime_observation_protocol.h"

#include <cassert>

namespace {
uint32_t read_u32(const std::vector<uint8_t> &bytes, size_t offset) {
    assert(offset + 4U <= bytes.size());
    return (static_cast<uint32_t>(bytes[offset]) << 24U) |
        (static_cast<uint32_t>(bytes[offset + 1U]) << 16U) |
        (static_cast<uint32_t>(bytes[offset + 2U]) << 8U) |
        static_cast<uint32_t>(bytes[offset + 3U]);
}
}  // namespace

int main() {
    pogo_runtime::MapTargetObservation observation;
    observation.tap_id = "tap-1";
    observation.target_latitude = 10.25;
    observation.target_longitude = 106.75;
    observation.screen_x = 540.0F;
    observation.screen_y = 960.0F;
    observation.viewport_width = 1080;
    observation.viewport_height = 1920;
    observation.camera_snapshot_id = "camera-1";

    std::vector<uint8_t> payload;
    assert(pogo_runtime::encode_map_target_payload(observation, &payload));
    assert(read_u32(payload, 0U) == pogo_runtime::kMapTargetPayloadVersion);

    std::vector<uint8_t> envelope;
    assert(pogo_runtime::encode_runtime_map_target_observation(observation, 11U, 22U, &envelope));
    assert(read_u32(envelope, 0U) == pogo_runtime::kRuntimeObservationEnvelopeVersion);
    assert(read_u32(envelope, 4U) == pogo_runtime::kMapTargetObservationType);
    assert(read_u32(envelope, 8U) == pogo_runtime::kMapTargetPayloadVersion);
    assert(read_u32(envelope, 12U) == payload.size());

    observation.screen_x = 1081.0F;
    assert(!pogo_runtime::encode_map_target_payload(observation, &payload));

    pogo_runtime::RuntimeFortsObservation forts;
    forts.forts.push_back({"fort-1", 0, 21.0, 105.0, true});
    std::vector<uint8_t> forts_envelope;
    assert(pogo_runtime::encode_runtime_forts_observation(
        forts, 33U, 44U, &forts_envelope
    ));
    assert(read_u32(forts_envelope, 0U) == pogo_runtime::kRuntimeObservationEnvelopeVersion);
    assert(read_u32(forts_envelope, 4U) == pogo_runtime::kFortsObservationType);
    assert(read_u32(forts_envelope, 8U) == pogo_runtime::kRuntimeFortsPayloadVersion);

    forts.forts.front().type = 2;
    assert(!pogo_runtime::encode_runtime_forts_payload(forts, &payload));
    return 0;
}
