#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pogo_runtime_ui_status {

constexpr uint32_t kPayloadVersion = 1U;
constexpr uint32_t kMarker = 0x52555354U;  // RUST
constexpr uint32_t kSchemaVersion = 1U;
constexpr uint32_t kMaxCapabilities = 256U;
constexpr uint32_t kMaxModules = 8U;
constexpr uint32_t kMaxErrorCodeBytes = 256U;
constexpr uint32_t kMaxErrorMessageBytes = 4096U;
constexpr uint32_t kMaxLogicalIdBytes = 256U;
constexpr uint32_t kMaxBuildFingerprintBytes = 4096U;

enum class NativeLifecycle : uint32_t {
    kUnknown = 0U,
    kAttachedIdle = 1U,
    kStarting = 2U,
    kDiagnosticPending = 3U,
    kBindingReady = 4U,
    kApplying = 5U,
    kReady = 6U,
    kStopping = 7U,
    kError = 8U,
};

enum class ModuleState : uint32_t {
    kUnknown = 0U,
    kRegistered = 1U,
    kConfigured = 2U,
    kReady = 3U,
    kDisabled = 4U,
    kError = 5U,
};

struct Module {
    uint32_t wire = 0U;
    std::string name;
    ModuleState state = ModuleState::kUnknown;
    bool has_revision = false;
    uint64_t revision = 0U;
    std::string error_code;
};

struct Status {
    std::string runtime_session_id;
    uint32_t pid = 0U;
    std::string process_name;
    std::string package_name;
    std::string build_fingerprint;
    NativeLifecycle lifecycle = NativeLifecycle::kUnknown;
    bool strong_identity_verified = false;
    std::vector<std::string> capabilities;
    bool has_desired_revision = false;
    uint64_t desired_revision = 0U;
    bool has_applied_revision = false;
    uint64_t applied_revision = 0U;
    bool ready = false;
    std::vector<Module> modules;
    std::string error_code;
    std::string error_message;
    uint64_t observed_at_epoch_ms = 0U;
    uint64_t observed_at_elapsed_ns = 0U;
};

inline bool append_u32(std::vector<uint8_t> *output, uint32_t value) {
    if (output == nullptr) return false;
    output->push_back(static_cast<uint8_t>((value >> 24U) & 0xffU));
    output->push_back(static_cast<uint8_t>((value >> 16U) & 0xffU));
    output->push_back(static_cast<uint8_t>((value >> 8U) & 0xffU));
    output->push_back(static_cast<uint8_t>(value & 0xffU));
    return true;
}

inline bool append_u64(std::vector<uint8_t> *output, uint64_t value) {
    if (output == nullptr) return false;
    for (int shift = 56; shift >= 0; shift -= 8) {
        output->push_back(static_cast<uint8_t>((value >> shift) & 0xffU));
    }
    return true;
}

inline bool append_string(
    std::vector<uint8_t> *output,
    const std::string &value,
    size_t max_bytes
) {
    if (output == nullptr || value.empty() || value.size() > max_bytes) return false;
    if (!append_u32(output, static_cast<uint32_t>(value.size()))) return false;
    output->insert(output->end(), value.begin(), value.end());
    return true;
}

inline bool append_bool(std::vector<uint8_t> *output, bool value) {
    if (output == nullptr) return false;
    output->push_back(value ? 1U : 0U);
    return true;
}

inline bool append_nullable_revision(
    std::vector<uint8_t> *output,
    bool present,
    uint64_t revision
) {
    if (!append_bool(output, present)) return false;
    return !present || (revision > 0U && append_u64(output, revision));
}

inline bool encode_runtime_ui_status_payload(
    const Status &status,
    std::vector<uint8_t> *payload
) {
    if (payload == nullptr ||
        status.runtime_session_id.empty() ||
        status.pid == 0U ||
        status.process_name.empty() ||
        status.package_name.empty() ||
        status.build_fingerprint.empty() ||
        status.capabilities.size() > kMaxCapabilities ||
        status.modules.size() > kMaxModules ||
        status.error_code.size() > kMaxErrorCodeBytes ||
        status.error_message.size() > kMaxErrorMessageBytes ||
        status.runtime_session_id.size() > kMaxLogicalIdBytes ||
        status.process_name.size() > kMaxLogicalIdBytes ||
        status.package_name.size() > kMaxLogicalIdBytes ||
        status.build_fingerprint.size() > kMaxBuildFingerprintBytes) return false;
    if (status.has_desired_revision && status.desired_revision == 0U) return false;
    if (status.has_applied_revision && status.applied_revision == 0U) return false;

    payload->clear();
    if (!append_u32(payload, kPayloadVersion) ||
        !append_u32(payload, kMarker) ||
        !append_u32(payload, kSchemaVersion) ||
        !append_string(payload, status.runtime_session_id, kMaxLogicalIdBytes) ||
        !append_u32(payload, status.pid) ||
        !append_string(payload, status.process_name, kMaxLogicalIdBytes) ||
        !append_string(payload, status.package_name, kMaxLogicalIdBytes) ||
        !append_string(payload, status.build_fingerprint, kMaxBuildFingerprintBytes) ||
        !append_u32(payload, static_cast<uint32_t>(status.lifecycle)) ||
        !append_bool(payload, status.strong_identity_verified) ||
        !append_u32(payload, static_cast<uint32_t>(status.capabilities.size()))) return false;

    for (size_t index = 0U; index < status.capabilities.size(); ++index) {
        if (status.capabilities[index].empty() ||
            status.capabilities[index].size() > kMaxErrorCodeBytes ||
            (index > 0U && status.capabilities[index - 1U] >= status.capabilities[index]) ||
            !append_string(payload, status.capabilities[index], kMaxErrorCodeBytes)) return false;
    }
    if (!append_nullable_revision(
            payload, status.has_desired_revision, status.desired_revision) ||
        !append_nullable_revision(
            payload, status.has_applied_revision, status.applied_revision) ||
        !append_bool(payload, status.ready) ||
        !append_u32(payload, static_cast<uint32_t>(status.modules.size()))) return false;

    for (const Module &module : status.modules) {
        if (module.wire == 0U || module.name.empty() ||
            module.name.size() > kMaxLogicalIdBytes ||
            module.error_code.size() > kMaxErrorCodeBytes ||
            !append_u32(payload, module.wire) ||
            !append_string(payload, module.name, kMaxLogicalIdBytes) ||
            !append_u32(payload, static_cast<uint32_t>(module.state)) ||
            !append_nullable_revision(payload, module.has_revision, module.revision) ||
            !append_bool(payload, !module.error_code.empty())) return false;
        if (!module.error_code.empty() &&
            !append_string(payload, module.error_code, kMaxErrorCodeBytes)) return false;
    }
    if (!append_bool(payload, !status.error_code.empty())) return false;
    if (!status.error_code.empty() &&
        !append_string(payload, status.error_code, kMaxErrorCodeBytes)) return false;
    if (!append_bool(payload, !status.error_message.empty())) return false;
    if (!status.error_message.empty() &&
        !append_string(payload, status.error_message, kMaxErrorMessageBytes)) return false;
    return append_u64(payload, status.observed_at_epoch_ms) &&
        append_u64(payload, status.observed_at_elapsed_ns);
}

}  // namespace pogo_runtime_ui_status
