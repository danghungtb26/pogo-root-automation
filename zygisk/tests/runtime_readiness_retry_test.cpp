#include <cassert>
#include "shared/runtime/control/runtime_readiness_retry.h"

int main() {
    RuntimeReadinessRetry retry;
    constexpr uint64_t second = 1000000000ULL;
    assert(!retry.due(100 * second, false)); // idle process cannot probe
    retry.start(second);
    assert(!retry.due(3 * second, false)); // Unity settling
    assert(retry.due(4 * second, false));
    assert(!retry.due(8 * second, false)); // failed probe backs off
    assert(!retry.due(9 * second, true)); // never rebind under active modules
    assert(retry.due(10 * second, false));
    retry.completed();
    assert(!retry.due(100 * second, false));
    retry.start(101 * second);
    assert(retry.due(104 * second, false)); // restart schedules a fresh probe
    retry.stop();
    assert(!retry.due(200 * second, false));
}
