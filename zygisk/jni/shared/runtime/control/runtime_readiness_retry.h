#pragma once
#include <cstdint>

// Owned by the native control thread. START is probe-only; managed discovery
// waits for Unity initialization and retries without controller-side timers.
class RuntimeReadinessRetry {
public:
    void start(uint64_t now) { pending_ = true; next_ = now + 3000000000ULL; }
    void stop() { pending_ = false; }
    bool due(uint64_t now, bool modules_enabled) {
        if (!pending_ || modules_enabled || now < next_) return false;
        next_ = now + 5000000000ULL;
        return true;
    }
    void completed() { pending_ = false; }
private:
    bool pending_ = false;
    uint64_t next_ = 0;
};
