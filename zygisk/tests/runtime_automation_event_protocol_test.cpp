#include <cassert>
#include <cstdint>
#include <vector>

#include "runtime_automation_event_protocol.h"

int main() {
    using namespace pogo_runtime;
    std::vector<uint8_t> payload;
    for (uint32_t wire = 1; wire <= 14; ++wire) {
        RuntimeAutomationEvent event{
            static_cast<RuntimeAutomationEventType>(wire), UINT64_MAX, 1U
        };
        assert(encode_runtime_automation_event_payload(event, &payload));
        // Kotlin's DataInputStream decoder consumes exactly these big-endian
        // fields; high-bit storage IDs must survive the signed Long boundary.
        assert(payload.size() == 32U);
        assert(payload[0] == 'P' && payload[1] == 'O');
        assert(payload[2] == 'G' && payload[3] == 'E');
        assert(payload[4] == 0 && payload[5] == 0 && payload[6] == 0);
        assert(payload[7] == wire);
        for (size_t index = 8; index < 16; ++index) assert(payload[index] == 0xff);
        assert(payload[23] == 1);
        std::vector<uint8_t> envelope;
        assert(encode_runtime_automation_event(event, 2U, 3U, &envelope));
        assert(envelope.size() == 64U);
        assert(envelope[11] == 2U); // Names/detail require payload v2.
        assert(envelope[7] == 10U);  // AUTOMATION_EVENT observation type.
        assert(envelope[15] == 32U);
    }
    assert(!encode_runtime_automation_event_payload(
        {RuntimeAutomationEventType::kSpinCompleted, 0U, 0U}, &payload));
    assert(!encode_runtime_automation_event_payload(
        {static_cast<RuntimeAutomationEventType>(15U), 1U, 0U}, &payload));
    assert(!encode_runtime_automation_event_payload(
        {RuntimeAutomationEventType::kSpinCompleted, 1U, 0U}, nullptr));
    RuntimeAutomationEvent named{RuntimeAutomationEventType::kItemDiscardFailed, 2U, 9U,
        "Great Ball", "Inventory unavailable"};
    assert(encode_runtime_automation_event_payload(named, &payload));
    assert(payload[27] == 10U);
    assert(std::string(payload.begin() + 28, payload.begin() + 38) == "Great Ball");
    named.subject_name.assign(257, 'x');
    assert(!encode_runtime_automation_event_payload(named, &payload));
    named.subject_name = "Great Ball";
    named.detail.assign(513, 'x');
    assert(!encode_runtime_automation_event_payload(named, &payload));
}
