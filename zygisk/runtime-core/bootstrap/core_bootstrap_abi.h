#pragma once

#include <cstdint>
#include <cstddef>

// Shared loader/core ABI. On success the core owns companion_fd. On failure
// the loader retains it and publishes a terminal bootstrap error.
struct CoreBootstrapArgsV1 {
    uint32_t protocol;
    uint32_t struct_size;
    int32_t companion_fd;
    int32_t process_pid;
    void *jni_env;
    char process_name[128];
};
static_assert(sizeof(CoreBootstrapArgsV1) == 152U);
static_assert(offsetof(CoreBootstrapArgsV1, process_name) == 24U);

constexpr uint32_t kCoreBootstrapProtocolV1 = 1;
constexpr int kCoreStartInvalidArgs = -1;
constexpr int kCoreStartDuplicate = -2;
constexpr int kCoreStartWorkerUnavailable = -3;

extern "C" __attribute__((visibility("default")))
int pogo_runtime_core_start_v1(const CoreBootstrapArgsV1 *args);
