#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>

#include "runtime_observation_protocol.h"

struct RuntimeThrowEventRecord {
    pogo_runtime::RuntimeThrowDiagnosticObservation diagnostic;
    uintptr_t ball_instance = 0U;
    uint64_t sequence = 0U;
};

struct RuntimeThrowEventQueue {
    static constexpr size_t kCapacity = 32U;

    std::mutex mutex;
    RuntimeThrowEventRecord records[kCapacity]{};
    size_t head = 0U;
    size_t size = 0U;
    uint64_t next_sequence = 1U;

    void push(
        const pogo_runtime::RuntimeThrowDiagnosticObservation &diagnostic,
        uintptr_t ball_instance
    ) {
        std::lock_guard<std::mutex> lock(mutex);
        const size_t index = (head + size) % kCapacity;
        if (size == kCapacity) {
            head = (head + 1U) % kCapacity;
        } else {
            ++size;
        }
        records[index].diagnostic = diagnostic;
        records[index].ball_instance = ball_instance;
        records[index].sequence = next_sequence++;
    }

    bool pop(RuntimeThrowEventRecord *record) {
        if (record == nullptr) return false;
        std::lock_guard<std::mutex> lock(mutex);
        if (size == 0U) return false;
        *record = records[head];
        head = (head + 1U) % kCapacity;
        --size;
        return true;
    }
};
