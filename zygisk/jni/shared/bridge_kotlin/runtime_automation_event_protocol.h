#pragma once

#include <cstdint>
#include <vector>

#include "runtime_observation_protocol.h"

namespace pogo_runtime {

constexpr uint32_t kAutomationEventObservationType = 10U;
constexpr uint32_t kRuntimeAutomationEventPayloadVersion = 1U;
constexpr uint32_t kRuntimeAutomationEventPayloadMagic = 0x504F4745U;  // POGE

enum class RuntimeAutomationEventType : uint32_t {
    kPokemonFound = 1U,
    kPokemonCaught = 2U,
    kPokemonFled = 3U,
    kPokemonTransferred = 4U,
    kPokemonTransferTriggered = 5U,
    kPokemonTransferFailed = 6U,
    kItemDiscardTriggered = 7U,
    kItemDiscarded = 8U,
    kItemDiscardFailed = 9U,
};

struct RuntimeAutomationEvent {
    RuntimeAutomationEventType type = RuntimeAutomationEventType::kPokemonFound;
    uint64_t primary_id = 0U;
    uint64_t secondary_id = 0U;
};

inline bool encode_runtime_automation_event_payload(
    const RuntimeAutomationEvent &value,
    std::vector<uint8_t> *payload
) {
    if (payload == nullptr || value.primary_id == 0U) {
        return false;
    }
    const uint32_t event_type = static_cast<uint32_t>(value.type);
    if (event_type == 0U || event_type > 9U) return false;
    payload->clear();
    append_u32(payload, kRuntimeAutomationEventPayloadMagic);
    append_u32(payload, event_type);
    append_u64(payload, value.primary_id);
    append_u64(payload, value.secondary_id);
    return true;
}

inline bool encode_runtime_automation_event(
    const RuntimeAutomationEvent &value,
    uint64_t observed_at_epoch_ms,
    uint64_t observed_at_elapsed_ns,
    std::vector<uint8_t> *envelope
) {
    if (envelope == nullptr) return false;
    std::vector<uint8_t> payload;
    if (!encode_runtime_automation_event_payload(value, &payload)) return false;
    envelope->clear();
    append_u32(envelope, kRuntimeObservationEnvelopeVersion);
    append_u32(envelope, kAutomationEventObservationType);
    append_u32(envelope, kRuntimeAutomationEventPayloadVersion);
    append_u32(envelope, static_cast<uint32_t>(payload.size()));
    envelope->insert(envelope->end(), payload.begin(), payload.end());
    append_u64(envelope, observed_at_epoch_ms);
    append_u64(envelope, observed_at_elapsed_ns);
    return true;
}

}  // namespace pogo_runtime
