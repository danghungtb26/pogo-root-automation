#include "core_bootstrap_abi.h"
#include "bootstrap_coordinator.inc"

extern "C" __attribute__((visibility("default")))
int pogo_runtime_core_start_v1(const CoreBootstrapArgsV1 *args) {
    return pogo_runtime_core::bootstrap_coordinator().start(args);
}
