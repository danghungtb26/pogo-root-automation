#include <android/log.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <mutex>
#include <vector>
#include <string>

#include "zygisk.hpp"
#include "runtime_command_protocol.h"

namespace {
constexpr const char *kLogTag = "PogoRootAutomation";
constexpr const char *kGooglePlayProcess = "com.nianticlabs.pokemongo";
constexpr const char *kGalaxyProcess = "com.nianticlabs.pokemongo.ares";
constexpr const char *kStateDirectory = "/data/adb/pogo_root_automation";
constexpr const char *kStatePath = "/data/adb/pogo_root_automation/runtime.status";
constexpr const char *kTempStatePrefix = "/data/adb/pogo_root_automation/runtime.status.tmp";
constexpr const char *kBridgeSocketPath = "/data/adb/pogo_root_automation/runtime.sock";
constexpr const char *kControllerUidPath = "/data/adb/pogo_root_automation/controller.uids";
constexpr uint32_t kRuntimeEventMagic = 0x504F474FU;
constexpr uint32_t kRuntimeProtocolVersion = 4U;
constexpr uint16_t kBridgeProtocolVersion = 2U;
constexpr uint32_t kBridgeCommandType = pogo_runtime::kBridgeCommandType;
constexpr uint32_t kBridgeCommandResultType = 5U;
constexpr uint32_t kBridgeHardMessageBytes = 4U * 1024U * 1024U;
constexpr uint32_t kRuntimeCommandMagic = 0x504F4743U;
constexpr uint32_t kRuntimeResultMagic = 0x504F4752U;
constexpr int kProbeAttempts = 60;
constexpr int kAssemblySurveyAttempts = 20;
constexpr useconds_t kProbeDelayUs = 500000U;
constexpr uint32_t kRequiredIl2cppCoreSymbolCount = 10U;
constexpr size_t kMaxAssembliesToInspect = 4096U;
constexpr size_t kMaxClassesToInspect = 50000U;
constexpr uint32_t kMaxCandidateClasses = 32U;
uint64_t g_session_counter = 0U;

enum class RuntimeEventType : uint32_t {
    kTargetAttached = 1U,
    kBindingProbe = 2U,
};

enum RuntimeProbeFlags : uint32_t {
    kProbeComplete = 1U << 0U,
    kIl2cppLoaded = 1U << 1U,
    kUnityLoaded = 1U << 2U,
    kIl2cppApiAvailable = 1U << 3U,
    kHoudiniTranslation = 1U << 4U,
    kNdkTranslation = 1U << 5U,
    kAssemblySurveyComplete = 1U << 6U,
    kAssemblyCSharpFound = 1U << 7U,
    kClassSurveyComplete = 1U << 8U,
};

struct RuntimeEvent {
    uint32_t magic;
    uint32_t protocol_version;
    uint32_t event_type;
    int32_t pid;
    uint32_t probe_flags;
    uint32_t il2cpp_symbol_count;
    uint32_t il2cpp_required_symbol_count;
    uint32_t assembly_count;
    uint32_t class_count;
    uint32_t candidate_class_count;
    char process_name[128];
    char il2cpp_path[512];
    char unity_path[512];
    char translation_layer[32];
    char assembly_csharp_name[64];
    char candidate_classes[2048];
};

struct RuntimeState {
    uint32_t protocol_version = kRuntimeProtocolVersion;
    int32_t pid = 0;
    uint32_t probe_flags = 0U;
    uint32_t il2cpp_symbol_count = 0U;
    uint32_t il2cpp_required_symbol_count = kRequiredIl2cppCoreSymbolCount;
    uint32_t assembly_count = 0U;
    uint32_t class_count = 0U;
    uint32_t candidate_class_count = 0U;
    uint64_t runtime_message_seq = 0U;
    char process_name[128]{};
    char runtime_session_id[64]{};
    char il2cpp_path[512]{};
    char unity_path[512]{};
    char translation_layer[32]{};
    char assembly_csharp_name[64]{};
    char candidate_classes[2048]{};
};

struct ProbeContext {
    int fd;
    int32_t pid;
    char process_name[128];
};

struct BrokerContext {
    int32_t pid;
    int runtime_fd;
    char process_name[128];
    char runtime_session_id[64];
    uint64_t next_message_seq = 1U;
};

std::mutex g_broker_mutex;
bool g_broker_running = false;
std::string g_broker_session_id;

struct Il2CppDomain;
struct Il2CppAssembly;
struct Il2CppImage;
struct Il2CppThread;
struct Il2CppClass;

using Il2CppDomainGet = Il2CppDomain *(*)();
using Il2CppThreadAttach = Il2CppThread *(*)(Il2CppDomain *);
using Il2CppThreadDetach = void (*)(Il2CppThread *);
using Il2CppDomainGetAssemblies = const Il2CppAssembly **(*)(const Il2CppDomain *, size_t *);
using Il2CppAssemblyGetImage = const Il2CppImage *(*)(const Il2CppAssembly *);
using Il2CppImageGetName = const char *(*)(const Il2CppImage *);
using Il2CppClassFromName = Il2CppClass *(*)(const Il2CppImage *, const char *, const char *);
using Il2CppClassGetFieldFromName = void *(*)(Il2CppClass *, const char *);
using Il2CppFieldGetValue = void (*)(void *, void *, void *);
using Il2CppObjectGetClass = Il2CppClass *(*)(void *);
using Il2CppImageGetClassCount = size_t (*)(const Il2CppImage *);
using Il2CppImageGetClass = Il2CppClass *(*)(const Il2CppImage *, size_t);
using Il2CppClassGetName = const char *(*)(Il2CppClass *);
using Il2CppClassGetNamespace = const char *(*)(Il2CppClass *);

struct Il2CppApi {
    void *handle = nullptr;
    uint32_t symbol_count = 0U;
    Il2CppDomainGet domain_get = nullptr;
    Il2CppThreadAttach thread_attach = nullptr;
    Il2CppThreadDetach thread_detach = nullptr;
    Il2CppDomainGetAssemblies domain_get_assemblies = nullptr;
    Il2CppAssemblyGetImage assembly_get_image = nullptr;
    Il2CppImageGetName image_get_name = nullptr;
    Il2CppClassFromName class_from_name = nullptr;
    Il2CppClassGetFieldFromName class_get_field_from_name = nullptr;
    Il2CppFieldGetValue field_get_value = nullptr;
    Il2CppObjectGetClass object_get_class = nullptr;
    Il2CppImageGetClassCount image_get_class_count = nullptr;
    Il2CppImageGetClass image_get_class = nullptr;
    Il2CppClassGetName class_get_name = nullptr;
    Il2CppClassGetNamespace class_get_namespace = nullptr;
};

bool is_target_process(const char *process_name) {
    return process_name != nullptr &&
        (strcmp(process_name, kGooglePlayProcess) == 0 ||
         strcmp(process_name, kGalaxyProcess) == 0);
}

bool write_full(int fd, const void *buffer, size_t size) {
    const auto *cursor = static_cast<const uint8_t *>(buffer);
    size_t remaining = size;
    while (remaining > 0U) {
        const ssize_t written = write(fd, cursor, remaining);
        if (written <= 0) return false;
        cursor += written;
        remaining -= static_cast<size_t>(written);
    }
    return true;
}

bool read_full(int fd, void *buffer, size_t size) {
    auto *cursor = static_cast<uint8_t *>(buffer);
    size_t remaining = size;
    while (remaining > 0U) {
        const ssize_t count = read(fd, cursor, remaining);
        if (count <= 0) return false;
        cursor += count;
        remaining -= static_cast<size_t>(count);
    }
    return true;
}

long long now_epoch_millis() {
    timespec value{};
    if (clock_gettime(CLOCK_REALTIME, &value) != 0) return 0LL;
    return static_cast<long long>(value.tv_sec) * 1000LL +
        static_cast<long long>(value.tv_nsec / 1000000L);
}

long long now_elapsed_nanos() {
    timespec value{};
    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) return 0LL;
    return static_cast<long long>(value.tv_sec) * 1000000000LL +
        static_cast<long long>(value.tv_nsec);
}

void copy_string(char *destination, size_t destination_size, const char *source) {
    if (destination == nullptr || destination_size == 0U) return;
    snprintf(destination, destination_size, "%s", source == nullptr ? "" : source);
}

void append_u32(std::vector<uint8_t> *output, uint32_t value) {
    if (output == nullptr) return;
    output->push_back(static_cast<uint8_t>((value >> 24U) & 0xffU));
    output->push_back(static_cast<uint8_t>((value >> 16U) & 0xffU));
    output->push_back(static_cast<uint8_t>((value >> 8U) & 0xffU));
    output->push_back(static_cast<uint8_t>(value & 0xffU));
}

void append_u64(std::vector<uint8_t> *output, uint64_t value) {
    if (output == nullptr) return;
    for (int shift = 56; shift >= 0; shift -= 8) {
        output->push_back(static_cast<uint8_t>((value >> shift) & 0xffU));
    }
}

void append_string(std::vector<uint8_t> *output, const char *value) {
    const char *safe_value = value == nullptr ? "" : value;
    const size_t length = strnlen(safe_value, 65536U);
    append_u32(output, static_cast<uint32_t>(length));
    output->insert(output->end(), safe_value, safe_value + length);
}

void append_nullable_string(std::vector<uint8_t> *output, const char *value) {
    output->push_back(value == nullptr ? 0U : 1U);
    if (value != nullptr) append_string(output, value);
}

bool read_be32(const std::vector<uint8_t> &input, size_t *offset, uint32_t *value) {
    if (offset == nullptr || value == nullptr || *offset + 4U > input.size()) return false;
    *value = (static_cast<uint32_t>(input[*offset]) << 24U) |
        (static_cast<uint32_t>(input[*offset + 1U]) << 16U) |
        (static_cast<uint32_t>(input[*offset + 2U]) << 8U) |
        static_cast<uint32_t>(input[*offset + 3U]);
    *offset += 4U;
    return true;
}

bool read_string(const std::vector<uint8_t> &input, size_t *offset, std::string *value) {
    uint32_t length = 0U;
    if (value == nullptr || !read_be32(input, offset, &length) || length > 65536U ||
        *offset + length > input.size()) return false;
    value->assign(reinterpret_cast<const char *>(input.data() + *offset), length);
    *offset += length;
    return true;
}

bool authorized_controller(int fd) {
    struct ucred credentials{};
    socklen_t length = sizeof(credentials);
    if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &credentials, &length) != 0) return false;

    FILE *allowlist = fopen(kControllerUidPath, "re");
    if (allowlist == nullptr) return false;
    bool allowed = false;
    char line[32]{};
    while (fgets(line, sizeof(line), allowlist) != nullptr) {
        char *end = nullptr;
        const long uid = strtol(line, &end, 10);
        if (end != line && uid >= 0L && static_cast<uid_t>(uid) == credentials.uid) {
            allowed = true;
            break;
        }
    }
    fclose(allowlist);
    return allowed;
}

bool encode_bridge_frame(
    uint16_t message_type,
    uint64_t message_seq,
    const std::vector<uint8_t> &payload,
    std::vector<uint8_t> *frame
) {
    if (frame == nullptr || payload.size() > kBridgeHardMessageBytes) return false;
    frame->clear();
    frame->reserve(16U + payload.size());
    append_u32(frame, static_cast<uint32_t>(payload.size()));
    frame->push_back(static_cast<uint8_t>((kBridgeProtocolVersion >> 8U) & 0xffU));
    frame->push_back(static_cast<uint8_t>(kBridgeProtocolVersion & 0xffU));
    frame->push_back(static_cast<uint8_t>((message_type >> 8U) & 0xffU));
    frame->push_back(static_cast<uint8_t>(message_type & 0xffU));
    append_u64(frame, message_seq);
    frame->insert(frame->end(), payload.begin(), payload.end());
    return true;
}

bool send_bridge_frame(int fd, uint16_t message_type, uint64_t message_seq, const std::vector<uint8_t> &payload) {
    std::vector<uint8_t> frame;
    if (!encode_bridge_frame(message_type, message_seq, payload, &frame)) return false;
    return write_full(fd, frame.data(), frame.size());
}

bool read_bridge_frame(
    int fd,
    uint16_t *message_type,
    uint64_t *message_seq,
    std::vector<uint8_t> *payload
) {
    uint8_t length_bytes[4]{};
    if (!read_full(fd, length_bytes, sizeof(length_bytes))) return false;
    const uint32_t length = (static_cast<uint32_t>(length_bytes[0]) << 24U) |
        (static_cast<uint32_t>(length_bytes[1]) << 16U) |
        (static_cast<uint32_t>(length_bytes[2]) << 8U) |
        static_cast<uint32_t>(length_bytes[3]);
    if (length > kBridgeHardMessageBytes) return false;

    uint8_t header[12]{};
    if (!read_full(fd, header, sizeof(header))) return false;
    const uint16_t protocol = static_cast<uint16_t>(header[0] << 8U | header[1]);
    if (protocol != kBridgeProtocolVersion) return false;
    if (message_type != nullptr) *message_type = static_cast<uint16_t>(header[2] << 8U | header[3]);
    if (message_seq != nullptr) {
        *message_seq = 0U;
        for (size_t index = 0U; index < 8U; ++index) {
            *message_seq = (*message_seq << 8U) | header[4U + index];
        }
    }
    if (payload == nullptr) return false;
    payload->resize(length);
    return length == 0U || read_full(fd, payload->data(), payload->size());
}

bool send_internal_message(int fd, uint32_t magic, const std::vector<uint8_t> &payload) {
    if (payload.size() > kBridgeHardMessageBytes) return false;
    std::vector<uint8_t> envelope;
    envelope.reserve(8U + payload.size());
    append_u32(&envelope, magic);
    append_u32(&envelope, static_cast<uint32_t>(payload.size()));
    envelope.insert(envelope.end(), payload.begin(), payload.end());
    return write_full(fd, envelope.data(), envelope.size());
}

bool read_internal_message(int fd, uint32_t expected_magic, std::vector<uint8_t> *payload) {
    uint8_t header[8]{};
    if (payload == nullptr || !read_full(fd, header, sizeof(header))) return false;
    const uint32_t magic = (static_cast<uint32_t>(header[0]) << 24U) |
        (static_cast<uint32_t>(header[1]) << 16U) |
        (static_cast<uint32_t>(header[2]) << 8U) |
        static_cast<uint32_t>(header[3]);
    const uint32_t length = (static_cast<uint32_t>(header[4]) << 24U) |
        (static_cast<uint32_t>(header[5]) << 16U) |
        (static_cast<uint32_t>(header[6]) << 8U) |
        static_cast<uint32_t>(header[7]);
    if (magic != expected_magic || length > kBridgeHardMessageBytes) return false;
    payload->resize(length);
    return length == 0U || read_full(fd, payload->data(), payload->size());
}

int make_bridge_server() {
    if (mkdir(kStateDirectory, 0700) != 0 && errno != EEXIST) return -1;
    if (chmod(kStateDirectory, 0711) != 0) return -1;
    unlink(kBridgeSocketPath);
    const int server = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (server < 0) return -1;
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    if (strlen(kBridgeSocketPath) >= sizeof(address.sun_path)) {
        close(server);
        return -1;
    }
    strncpy(address.sun_path, kBridgeSocketPath, sizeof(address.sun_path) - 1U);
    if (bind(server, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) != 0 ||
        chmod(kBridgeSocketPath, 0666) != 0 || listen(server, 1) != 0) {
        close(server);
        unlink(kBridgeSocketPath);
        return -1;
    }
    return server;
}

void append_optional_absent(std::vector<uint8_t> *output) {
    output->push_back(0U);
}

enum class CompanionMessageType {
    kRuntimeEvent,
    kRuntimeResult,
};

bool read_companion_message(
    int fd,
    CompanionMessageType *message_type,
    RuntimeEvent *runtime_event,
    std::vector<uint8_t> *runtime_result
) {
    uint8_t magic_bytes[4]{};
    if (message_type == nullptr || !read_full(fd, magic_bytes, sizeof(magic_bytes))) return false;
    const uint32_t magic = (static_cast<uint32_t>(magic_bytes[0]) << 24U) |
        (static_cast<uint32_t>(magic_bytes[1]) << 16U) |
        (static_cast<uint32_t>(magic_bytes[2]) << 8U) |
        static_cast<uint32_t>(magic_bytes[3]);
    if (magic == kRuntimeEventMagic) {
        if (runtime_event == nullptr) return false;
        memset(runtime_event, 0, sizeof(*runtime_event));
        memcpy(runtime_event, magic_bytes, sizeof(magic_bytes));
        if (!read_full(
                fd,
                reinterpret_cast<uint8_t *>(runtime_event) + sizeof(magic_bytes),
                sizeof(*runtime_event) - sizeof(magic_bytes)
            )) return false;
        *message_type = CompanionMessageType::kRuntimeEvent;
        return true;
    }
    if (magic != kRuntimeResultMagic || runtime_result == nullptr) return false;
    uint8_t length_bytes[4]{};
    if (!read_full(fd, length_bytes, sizeof(length_bytes))) return false;
    const uint32_t length = (static_cast<uint32_t>(length_bytes[0]) << 24U) |
        (static_cast<uint32_t>(length_bytes[1]) << 16U) |
        (static_cast<uint32_t>(length_bytes[2]) << 8U) |
        static_cast<uint32_t>(length_bytes[3]);
    if (length > kBridgeHardMessageBytes) return false;
    runtime_result->resize(length);
    if (length != 0U && !read_full(fd, runtime_result->data(), runtime_result->size())) return false;
    *message_type = CompanionMessageType::kRuntimeResult;
    return true;
}

bool broker_is_current(const char *runtime_session_id) {
    std::lock_guard<std::mutex> lock(g_broker_mutex);
    return runtime_session_id != nullptr &&
        g_broker_running &&
        g_broker_session_id == runtime_session_id;
}

bool send_runtime_ready(int fd, BrokerContext &context) {
    const uint64_t message_seq = context.next_message_seq++;
    std::vector<uint8_t> payload;
    append_u32(&payload, 1U);
    append_string(&payload, context.runtime_session_id);
    append_u64(&payload, message_seq);
    append_u32(&payload, static_cast<uint32_t>(context.pid));
    append_string(&payload, context.process_name);
    append_string(&payload, context.process_name);

    char fingerprint[192]{};
    snprintf(fingerprint, sizeof(fingerprint), "unverified|%s", context.process_name);
    append_string(&payload, fingerprint);
    append_u32(&payload, 0U);  // No mutation/read capability is announced by the probe-only runtime.
    append_optional_absent(&payload);  // version name
    append_optional_absent(&payload);  // version code
    append_u64(&payload, static_cast<uint64_t>(now_epoch_millis()));
    append_u64(&payload, static_cast<uint64_t>(now_elapsed_nanos()));
    payload.push_back(0U);  // strongIdentityVerified=false; probe has no verified build binding.
    return send_bridge_frame(fd, 2U, message_seq, payload);
}

bool send_command_result(
    int fd,
    BrokerContext &context,
    const char *command_id,
    const char *error_code,
    const char *message
) {
    if (command_id == nullptr || command_id[0] == '\0') return false;
    const uint64_t message_seq = context.next_message_seq++;
    std::vector<uint8_t> payload;
    append_u32(&payload, 1U);
    append_string(&payload, context.runtime_session_id);
    append_u64(&payload, message_seq);
    append_string(&payload, command_id);
    append_u32(&payload, 4U);  // CommandPhase.REJECTED
    append_nullable_string(&payload, error_code);
    append_nullable_string(&payload, message);
    append_u64(&payload, static_cast<uint64_t>(now_epoch_millis()));
    append_u64(&payload, static_cast<uint64_t>(now_elapsed_nanos()));
    return send_bridge_frame(fd, kBridgeCommandResultType, message_seq, payload);
}

bool send_runtime_command(int fd, uint16_t message_type, uint64_t message_seq, const std::vector<uint8_t> &payload) {
    std::vector<uint8_t> command;
    command.reserve(12U + payload.size());
    append_u32(&command, static_cast<uint32_t>(message_type));
    append_u64(&command, message_seq);
    command.insert(command.end(), payload.begin(), payload.end());
    return send_internal_message(fd, kRuntimeCommandMagic, command);
}

bool parse_runtime_result(
    const std::vector<uint8_t> &payload,
    std::string *command_id,
    std::string *error_code,
    std::string *message
) {
    size_t offset = 0U;
    uint32_t version = 0U;
    if (command_id == nullptr || error_code == nullptr || message == nullptr ||
        !read_be32(payload, &offset, &version) || version != 1U ||
        !read_string(payload, &offset, command_id) ||
        !read_string(payload, &offset, error_code) ||
        !read_string(payload, &offset, message)) return false;
    return offset == payload.size();
}

bool handle_controller(int fd, BrokerContext &context) {
    if (!authorized_controller(fd)) return true;
    if (!send_runtime_ready(fd, context)) return true;

    while (kill(context.pid, 0) == 0) {
        if (!broker_is_current(context.runtime_session_id)) return false;
        pollfd descriptors[2]{};
        descriptors[0].fd = fd;
        descriptors[0].events = POLLIN;
        descriptors[1].fd = context.runtime_fd;
        descriptors[1].events = POLLIN;
        if (poll(descriptors, 2, 1000) <= 0) continue;

        if ((descriptors[1].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) return false;
        if ((descriptors[1].revents & POLLIN) != 0) {
            CompanionMessageType message_type{};
            RuntimeEvent runtime_event{};
            std::vector<uint8_t> runtime_result;
            if (!read_companion_message(context.runtime_fd, &message_type, &runtime_event, &runtime_result)) {
                return false;
            }
            if (message_type == CompanionMessageType::kRuntimeResult) {
                std::string command_id;
                std::string error_code;
                std::string message;
                if (!parse_runtime_result(runtime_result, &command_id, &error_code, &message) ||
                    !send_command_result(fd, context, command_id.c_str(), error_code.c_str(), message.c_str())) {
                    return true;
                }
            }
        }

        if ((descriptors[0].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) return true;
        if ((descriptors[0].revents & POLLIN) != 0) {
            uint16_t message_type = 0U;
            uint64_t message_seq = 0U;
            std::vector<uint8_t> payload;
            if (!read_bridge_frame(fd, &message_type, &message_seq, &payload)) return true;
            if (message_type != kBridgeCommandType) continue;

            size_t offset = 0U;
            uint32_t payload_version = 0U;
            std::string session_id;
            std::string command_id;
            if (!read_be32(payload, &offset, &payload_version) || payload_version != 1U ||
                !read_string(payload, &offset, &session_id) ||
                !read_string(payload, &offset, &command_id) ||
                session_id != context.runtime_session_id) return true;
            if (!send_runtime_command(context.runtime_fd, message_type, message_seq, payload)) return false;
        }
    }
    return false;
}

bool finish_runtime_bridge_broker(const char *runtime_session_id) {
    std::lock_guard<std::mutex> lock(g_broker_mutex);
    if (runtime_session_id != nullptr && g_broker_session_id == runtime_session_id) {
        g_broker_running = false;
        g_broker_session_id.clear();
        unlink(kBridgeSocketPath);
        return true;
    }
    return false;
}

void *runtime_bridge_broker_thread(void *opaque_context) {
    auto *context = static_cast<BrokerContext *>(opaque_context);
    if (context == nullptr) {
        return nullptr;
    }

    int server = -1;
    {
        // Serialize socket replacement with a newer session starting. A stale
        // broker may still be unwinding, but it must never remove the current
        // session's socket during cleanup.
        std::lock_guard<std::mutex> lock(g_broker_mutex);
        if (g_broker_session_id != context->runtime_session_id) {
            close(context->runtime_fd);
            delete context;
            return nullptr;
        }
        server = make_bridge_server();
    }
    if (server < 0) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "cannot create runtime bridge socket");
        close(context->runtime_fd);
        finish_runtime_bridge_broker(context->runtime_session_id);
        delete context;
        return nullptr;
    }

    while (kill(context->pid, 0) == 0 && broker_is_current(context->runtime_session_id)) {
        pollfd descriptor{};
        descriptor.fd = server;
        descriptor.events = POLLIN;
        const int polled = poll(&descriptor, 1, 1000);
        if (polled <= 0) continue;
        if (!broker_is_current(context->runtime_session_id)) break;
        const int client = accept4(server, nullptr, nullptr, SOCK_CLOEXEC);
        if (client < 0) continue;
        const bool runtime_alive = handle_controller(client, *context);
        close(client);
        if (!runtime_alive) break;
    }

    close(server);
    close(context->runtime_fd);
    finish_runtime_bridge_broker(context->runtime_session_id);
    delete context;
    return nullptr;
}

void start_runtime_bridge_broker(const RuntimeState &state, int runtime_fd) {
    if (runtime_fd < 0) return;
    {
        std::lock_guard<std::mutex> lock(g_broker_mutex);
        if (g_broker_running && g_broker_session_id == state.runtime_session_id) {
            close(runtime_fd);
            return;
        }
        g_broker_running = true;
        g_broker_session_id = state.runtime_session_id;
    }

    auto *context = new BrokerContext{};
    context->pid = state.pid;
    context->runtime_fd = runtime_fd;
    copy_string(context->process_name, sizeof(context->process_name), state.process_name);
    copy_string(
        context->runtime_session_id,
        sizeof(context->runtime_session_id),
        state.runtime_session_id
    );
    pthread_t thread{};
    if (pthread_create(&thread, nullptr, runtime_bridge_broker_thread, context) != 0) {
        close(runtime_fd);
        delete context;
        finish_runtime_bridge_broker(state.runtime_session_id);
        return;
    }
    pthread_detach(thread);
}

bool find_mapping_path(const char *needle, char *output, size_t output_size) {
    if (needle == nullptr || output == nullptr || output_size == 0U) return false;
    FILE *maps = fopen("/proc/self/maps", "re");
    if (maps == nullptr) return false;

    bool found = false;
    char line[1536]{};
    char path[1024]{};
    while (fgets(line, sizeof(line), maps) != nullptr) {
        path[0] = '\0';
        const int parsed = sscanf(line, "%*s %*s %*s %*s %*s %1023s", path);
        if (parsed == 1 && strstr(path, needle) != nullptr) {
            copy_string(output, output_size, path);
            found = true;
            break;
        }
    }
    fclose(maps);
    return found;
}

bool mapping_contains(const char *needle) {
    char ignored[1024]{};
    return find_mapping_path(needle, ignored, sizeof(ignored));
}

template <typename T>
T resolve_symbol(void *handle, const char *name, uint32_t *resolved_count) {
    void *symbol = dlsym(handle, name);
    if (symbol != nullptr && resolved_count != nullptr) *resolved_count += 1U;
    return reinterpret_cast<T>(symbol);
}

bool has_core_il2cpp_api(const Il2CppApi &api) {
    return api.domain_get != nullptr && api.thread_attach != nullptr &&
        api.thread_detach != nullptr && api.domain_get_assemblies != nullptr &&
        api.assembly_get_image != nullptr && api.image_get_name != nullptr &&
        api.class_from_name != nullptr && api.class_get_field_from_name != nullptr &&
        api.field_get_value != nullptr && api.object_get_class != nullptr;
}

bool has_class_survey_api(const Il2CppApi &api) {
    return api.image_get_class_count != nullptr && api.image_get_class != nullptr &&
        api.class_get_name != nullptr && api.class_get_namespace != nullptr;
}

bool resolve_il2cpp_api(const char *il2cpp_path, Il2CppApi *api) {
    if (il2cpp_path == nullptr || il2cpp_path[0] == '\0' || api == nullptr) return false;
    api->handle = dlopen(il2cpp_path, RTLD_NOW | RTLD_NOLOAD);
    if (api->handle == nullptr) api->handle = dlopen("libil2cpp.so", RTLD_NOW | RTLD_NOLOAD);
    if (api->handle == nullptr) return false;

#define RESOLVE(field, type, name) api->field = resolve_symbol<type>(api->handle, name, &api->symbol_count)
    RESOLVE(domain_get, Il2CppDomainGet, "il2cpp_domain_get");
    RESOLVE(thread_attach, Il2CppThreadAttach, "il2cpp_thread_attach");
    RESOLVE(thread_detach, Il2CppThreadDetach, "il2cpp_thread_detach");
    RESOLVE(domain_get_assemblies, Il2CppDomainGetAssemblies, "il2cpp_domain_get_assemblies");
    RESOLVE(assembly_get_image, Il2CppAssemblyGetImage, "il2cpp_assembly_get_image");
    RESOLVE(image_get_name, Il2CppImageGetName, "il2cpp_image_get_name");
    RESOLVE(class_from_name, Il2CppClassFromName, "il2cpp_class_from_name");
    RESOLVE(class_get_field_from_name, Il2CppClassGetFieldFromName, "il2cpp_class_get_field_from_name");
    RESOLVE(field_get_value, Il2CppFieldGetValue, "il2cpp_field_get_value");
    RESOLVE(object_get_class, Il2CppObjectGetClass, "il2cpp_object_get_class");
    RESOLVE(image_get_class_count, Il2CppImageGetClassCount, "il2cpp_image_get_class_count");
    RESOLVE(image_get_class, Il2CppImageGetClass, "il2cpp_image_get_class");
    RESOLVE(class_get_name, Il2CppClassGetName, "il2cpp_class_get_name");
    RESOLVE(class_get_namespace, Il2CppClassGetNamespace, "il2cpp_class_get_namespace");
#undef RESOLVE

    return has_core_il2cpp_api(*api);
}

void close_il2cpp_api(Il2CppApi *api) {
    if (api != nullptr && api->handle != nullptr) {
        dlclose(api->handle);
        api->handle = nullptr;
    }
}

char ascii_lower(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
}

bool contains_ignore_case(const char *value, const char *needle) {
    if (value == nullptr || needle == nullptr || needle[0] == '\0') return false;
    for (const char *start = value; *start != '\0'; ++start) {
        const char *left = start;
        const char *right = needle;
        while (*left != '\0' && *right != '\0' && ascii_lower(*left) == ascii_lower(*right)) {
            ++left;
            ++right;
        }
        if (*right == '\0') return true;
    }
    return false;
}

bool is_candidate_class(const char *name, const char *name_space) {
    const char *keywords[] = {
        "map", "pokemon", "spawn", "encounter", "fort", "nearby", "wild", "inventory"
    };
    for (const char *keyword : keywords) {
        if (contains_ignore_case(name, keyword) || contains_ignore_case(name_space, keyword)) return true;
    }
    return false;
}

void append_candidate(RuntimeEvent *event, const char *name_space, const char *name) {
    if (event == nullptr || name == nullptr || event->candidate_class_count >= kMaxCandidateClasses) return;
    char qualified[256]{};
    if (name_space != nullptr && name_space[0] != '\0') {
        snprintf(qualified, sizeof(qualified), "%s.%s", name_space, name);
    } else {
        snprintf(qualified, sizeof(qualified), "%s", name);
    }

    const size_t used = strnlen(event->candidate_classes, sizeof(event->candidate_classes));
    if (used >= sizeof(event->candidate_classes) - 1U) return;
    const char *separator = used == 0U ? "" : ";";
    const int written = snprintf(
        event->candidate_classes + used,
        sizeof(event->candidate_classes) - used,
        "%s%s",
        separator,
        qualified
    );
    if (written > 0 && static_cast<size_t>(written) < sizeof(event->candidate_classes) - used) {
        event->candidate_class_count += 1U;
    }
}

bool survey_il2cpp_assemblies(Il2CppApi *api, RuntimeEvent *event, const Il2CppImage **csharp_image) {
    if (api == nullptr || event == nullptr || csharp_image == nullptr ||
        api->domain_get == nullptr || api->thread_attach == nullptr ||
        api->thread_detach == nullptr || api->domain_get_assemblies == nullptr ||
        api->assembly_get_image == nullptr || api->image_get_name == nullptr) return false;

    Il2CppDomain *domain = api->domain_get();
    if (domain == nullptr) return false;
    Il2CppThread *thread = api->thread_attach(domain);
    if (thread == nullptr) return false;

    size_t assembly_count = 0U;
    const Il2CppAssembly **assemblies = api->domain_get_assemblies(domain, &assembly_count);
    if (assemblies == nullptr || assembly_count == 0U) {
        api->thread_detach(thread);
        return false;
    }

    event->assembly_count = assembly_count > 0xFFFFFFFFULL ? 0xFFFFFFFFU : static_cast<uint32_t>(assembly_count);
    const size_t inspect_count = assembly_count < kMaxAssembliesToInspect ? assembly_count : kMaxAssembliesToInspect;
    for (size_t index = 0U; index < inspect_count; ++index) {
        const Il2CppAssembly *assembly = assemblies[index];
        if (assembly == nullptr) continue;
        const Il2CppImage *image = api->assembly_get_image(assembly);
        if (image == nullptr) continue;
        const char *name = api->image_get_name(image);
        if (name == nullptr) continue;
        if (strcmp(name, "Assembly-CSharp.dll") == 0 || strcmp(name, "Assembly-CSharp") == 0) {
            event->probe_flags |= kAssemblyCSharpFound;
            copy_string(event->assembly_csharp_name, sizeof(event->assembly_csharp_name), name);
            *csharp_image = image;
            break;
        }
    }

    api->thread_detach(thread);
    event->probe_flags |= kAssemblySurveyComplete;
    return true;
}

void survey_candidate_classes(Il2CppApi *api, const Il2CppImage *image, RuntimeEvent *event) {
    if (api == nullptr || image == nullptr || event == nullptr || !has_class_survey_api(*api)) return;

    const size_t class_count = api->image_get_class_count(image);
    event->class_count = class_count > 0xFFFFFFFFULL ? 0xFFFFFFFFU : static_cast<uint32_t>(class_count);
    const size_t inspect_count = class_count < kMaxClassesToInspect ? class_count : kMaxClassesToInspect;

    for (size_t index = 0U; index < inspect_count && event->candidate_class_count < kMaxCandidateClasses; ++index) {
        Il2CppClass *klass = api->image_get_class(image, index);
        if (klass == nullptr) continue;
        const char *name = api->class_get_name(klass);
        const char *name_space = api->class_get_namespace(klass);
        if (name != nullptr && is_candidate_class(name, name_space)) {
            append_candidate(event, name_space, name);
        }
    }
    event->probe_flags |= kClassSurveyComplete;
}

void probe_il2cpp_runtime(RuntimeEvent *event) {
    if (event == nullptr || event->il2cpp_path[0] == '\0') return;

    Il2CppApi api{};
    const bool core_api = resolve_il2cpp_api(event->il2cpp_path, &api);
    event->il2cpp_symbol_count = api.symbol_count;
    event->il2cpp_required_symbol_count = kRequiredIl2cppCoreSymbolCount;
    if (!core_api) {
        close_il2cpp_api(&api);
        return;
    }

    event->probe_flags |= kIl2cppApiAvailable;
    for (int attempt = 0; attempt < kAssemblySurveyAttempts; ++attempt) {
        const Il2CppImage *csharp_image = nullptr;
        if (survey_il2cpp_assemblies(&api, event, &csharp_image)) {
            if (csharp_image != nullptr) survey_candidate_classes(&api, csharp_image, event);
            break;
        }
        usleep(kProbeDelayUs);
    }
    close_il2cpp_api(&api);
}

void persist_runtime_state(const RuntimeState &state) {
    if (mkdir(kStateDirectory, 0700) != 0 && errno != EEXIST) {
        __android_log_print(
            ANDROID_LOG_ERROR,
            kLogTag,
            "cannot create runtime state directory: errno=%d",
            errno
        );
        return;
    }
    char temp_path[192]{};
    snprintf(temp_path, sizeof(temp_path), "%s.%d", kTempStatePrefix, state.pid);
    const int fd = open(temp_path, O_CREAT | O_WRONLY | O_TRUNC | O_CLOEXEC, 0600);
    if (fd < 0) {
        __android_log_print(
            ANDROID_LOG_ERROR,
            kLogTag,
            "cannot open runtime state temp file pid=%d: errno=%d",
            state.pid,
            errno
        );
        return;
    }

    const bool probe_complete = (state.probe_flags & kProbeComplete) != 0U;
    const bool il2cpp_loaded = (state.probe_flags & kIl2cppLoaded) != 0U;
    const bool unity_loaded = (state.probe_flags & kUnityLoaded) != 0U;
    const bool api_available = (state.probe_flags & kIl2cppApiAvailable) != 0U;
    const bool assembly_survey_complete = (state.probe_flags & kAssemblySurveyComplete) != 0U;
    const bool assembly_csharp_found = (state.probe_flags & kAssemblyCSharpFound) != 0U;
    const bool class_survey_complete = (state.probe_flags & kClassSurveyComplete) != 0U;

    dprintf(fd, "protocol=%u\n", state.protocol_version);
    dprintf(fd, "pid=%d\n", state.pid);
    dprintf(fd, "process=%s\n", state.process_name);
    dprintf(fd, "runtime_session_id=%s\n", state.runtime_session_id);
    dprintf(fd, "runtime_message_seq=%llu\n", static_cast<unsigned long long>(state.runtime_message_seq));
    dprintf(fd, "seen_at_epoch_ms=%lld\n", now_epoch_millis());
    dprintf(fd, "native_probe_state=%s\n", probe_complete ? "complete" : "pending");
    dprintf(fd, "native_libil2cpp_loaded=%d\n", il2cpp_loaded ? 1 : 0);
    dprintf(fd, "native_libunity_loaded=%d\n", unity_loaded ? 1 : 0);
    dprintf(fd, "native_il2cpp_api_available=%d\n", api_available ? 1 : 0);
    dprintf(fd, "native_il2cpp_symbol_count=%u\n", state.il2cpp_symbol_count);
    dprintf(fd, "native_il2cpp_required_symbol_count=%u\n", state.il2cpp_required_symbol_count);
    dprintf(fd, "native_libil2cpp_path=%s\n", state.il2cpp_path);
    dprintf(fd, "native_libunity_path=%s\n", state.unity_path);
    dprintf(fd, "native_translation_layer=%s\n", state.translation_layer);
    dprintf(fd, "native_assembly_survey_state=%s\n", assembly_survey_complete ? "complete" : "unavailable");
    dprintf(fd, "native_assembly_count=%u\n", state.assembly_count);
    dprintf(fd, "native_assembly_csharp_found=%d\n", assembly_csharp_found ? 1 : 0);
    dprintf(fd, "native_assembly_csharp_name=%s\n", state.assembly_csharp_name);
    dprintf(fd, "native_class_survey_state=%s\n", class_survey_complete ? "complete" : "unavailable");
    dprintf(fd, "native_class_count=%u\n", state.class_count);
    dprintf(fd, "native_candidate_class_count=%u\n", state.candidate_class_count);
    dprintf(fd, "native_candidate_classes=%s\n", state.candidate_classes);
    fsync(fd);
    close(fd);
    if (rename(temp_path, kStatePath) != 0) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "cannot publish runtime state: errno=%d", errno);
    } else {
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "runtime state published pid=%d process=%s",
            state.pid,
            state.process_name
        );
    }
}

void companion_handler(int fd) {
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "companion_handler started fd=%d pid=%d",
        fd,
        getpid()
    );
    RuntimeState state{};
    RuntimeEvent event{};
    while (read_full(fd, &event, sizeof(event))) {
        if (event.magic != kRuntimeEventMagic || event.protocol_version != kRuntimeProtocolVersion) {
            __android_log_print(
                ANDROID_LOG_ERROR,
                kLogTag,
                "companion_handler rejected event magic=0x%08x protocol=%u",
                event.magic,
                event.protocol_version
            );
            return;
        }
        event.process_name[sizeof(event.process_name) - 1U] = '\0';
        event.il2cpp_path[sizeof(event.il2cpp_path) - 1U] = '\0';
        event.unity_path[sizeof(event.unity_path) - 1U] = '\0';
        event.translation_layer[sizeof(event.translation_layer) - 1U] = '\0';
        event.assembly_csharp_name[sizeof(event.assembly_csharp_name) - 1U] = '\0';
        event.candidate_classes[sizeof(event.candidate_classes) - 1U] = '\0';

        if (event.event_type == static_cast<uint32_t>(RuntimeEventType::kTargetAttached)) {
            __android_log_print(
                ANDROID_LOG_INFO,
                kLogTag,
                "companion TargetAttached pid=%d process=%s",
                event.pid,
                event.process_name
            );
            state = RuntimeState{};
            state.protocol_version = event.protocol_version;
            state.pid = event.pid;
            copy_string(state.process_name, sizeof(state.process_name), event.process_name);
            state.runtime_message_seq = 1U;
            snprintf(
                state.runtime_session_id,
                sizeof(state.runtime_session_id),
                "%d-%lld",
                state.pid,
                now_epoch_millis()
            );
            const size_t session_length = strnlen(state.runtime_session_id, sizeof(state.runtime_session_id));
            if (session_length < sizeof(state.runtime_session_id) - 1U) {
                snprintf(
                    state.runtime_session_id + session_length,
                    sizeof(state.runtime_session_id) - session_length,
                    "-%llu",
                    static_cast<unsigned long long>(++g_session_counter)
                );
            }
            persist_runtime_state(state);
            continue;
        }
        if (event.event_type == static_cast<uint32_t>(RuntimeEventType::kBindingProbe) && event.pid == state.pid) {
            state.probe_flags = event.probe_flags;
            state.runtime_message_seq += 1U;
            state.il2cpp_symbol_count = event.il2cpp_symbol_count;
            state.il2cpp_required_symbol_count = event.il2cpp_required_symbol_count;
            state.assembly_count = event.assembly_count;
            state.class_count = event.class_count;
            state.candidate_class_count = event.candidate_class_count;
            copy_string(state.il2cpp_path, sizeof(state.il2cpp_path), event.il2cpp_path);
            copy_string(state.unity_path, sizeof(state.unity_path), event.unity_path);
            copy_string(state.translation_layer, sizeof(state.translation_layer), event.translation_layer);
            copy_string(state.assembly_csharp_name, sizeof(state.assembly_csharp_name), event.assembly_csharp_name);
            copy_string(state.candidate_classes, sizeof(state.candidate_classes), event.candidate_classes);
            persist_runtime_state(state);
            if ((event.probe_flags & kProbeComplete) != 0U) {
                const int broker_fd = dup(fd);
                if (broker_fd >= 0) start_runtime_bridge_broker(state, broker_fd);
                close(fd);
                return;
            }
        }
    }
}

bool send_runtime_rejected(int fd, const char *command_id) {
    if (command_id == nullptr || command_id[0] == '\0') return false;
    std::vector<uint8_t> result;
    append_u32(&result, 1U);
    append_string(&result, command_id);
    append_string(&result, "binding_not_implemented");
    append_string(&result, "runtime probe has no client-owned action binding");
    return send_internal_message(fd, kRuntimeResultMagic, result);
}

void runtime_command_channel_loop(const ProbeContext &context) {
    while (kill(getpid(), 0) == 0) {
        std::vector<uint8_t> command;
        if (!read_internal_message(context.fd, kRuntimeCommandMagic, &command)) return;

        pogo_runtime::RuntimeCommandPrefix prefix;
        if (!pogo_runtime::parse_runtime_command_prefix(command, &prefix)) return;
        // Probe-only mode keeps the versioned action payload opaque. A real
        // binding will parse and validate the remaining fields before invoke.
        if (!send_runtime_rejected(context.fd, prefix.command_id.c_str())) return;
    }
}

void *binding_probe_thread(void *opaque_context) {
    auto *context = static_cast<ProbeContext *>(opaque_context);
    if (context == nullptr) return nullptr;

    RuntimeEvent event{};
    event.magic = kRuntimeEventMagic;
    event.protocol_version = kRuntimeProtocolVersion;
    event.event_type = static_cast<uint32_t>(RuntimeEventType::kBindingProbe);
    event.pid = context->pid;
    event.il2cpp_required_symbol_count = kRequiredIl2cppCoreSymbolCount;
    copy_string(event.process_name, sizeof(event.process_name), context->process_name);
    copy_string(event.translation_layer, sizeof(event.translation_layer), "none");

    for (int attempt = 0; attempt < kProbeAttempts; ++attempt) {
        const bool il2cpp_loaded = find_mapping_path("libil2cpp.so", event.il2cpp_path, sizeof(event.il2cpp_path));
        const bool unity_loaded = find_mapping_path("libunity.so", event.unity_path, sizeof(event.unity_path));
        if (il2cpp_loaded) event.probe_flags |= kIl2cppLoaded;
        if (unity_loaded) event.probe_flags |= kUnityLoaded;
        if (mapping_contains("libhoudini")) {
            event.probe_flags |= kHoudiniTranslation;
            copy_string(event.translation_layer, sizeof(event.translation_layer), "houdini");
        } else if (mapping_contains("libndk_translation")) {
            event.probe_flags |= kNdkTranslation;
            copy_string(event.translation_layer, sizeof(event.translation_layer), "ndk_translation");
        }
        if (il2cpp_loaded) {
            probe_il2cpp_runtime(&event);
            break;
        }
        usleep(kProbeDelayUs);
    }

    event.probe_flags |= kProbeComplete;
    if (!write_full(context->fd, &event, sizeof(event))) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "failed to publish binding probe for pid=%d", event.pid);
    } else {
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "probe: il2cpp=%d symbols=%u core_required=%u assemblies=%u classes=%u candidates=%u translation=%s",
            (event.probe_flags & kIl2cppLoaded) != 0U,
            event.il2cpp_symbol_count,
            event.il2cpp_required_symbol_count,
            event.assembly_count,
            event.class_count,
            event.candidate_class_count,
            event.translation_layer
        );
    }
    runtime_command_channel_loop(*context);
    close(context->fd);
    delete context;
    return nullptr;
}
}  // namespace

class PogoAutomationModule : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        api_ = api;
        env_ = env;
        __android_log_print(ANDROID_LOG_INFO, kLogTag, "onLoad pid=%d", getpid());
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
        if (args == nullptr || args->nice_name == nullptr) {
            __android_log_print(
                ANDROID_LOG_ERROR,
                kLogTag,
                "preAppSpecialize process=<unavailable> target=0 reason=missing_args_or_nice_name"
            );
            api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }
        const char *process_name = env_->GetStringUTFChars(args->nice_name, nullptr);
        if (process_name == nullptr) {
            __android_log_print(
                ANDROID_LOG_ERROR,
                kLogTag,
                "preAppSpecialize process=<unavailable> target=0 reason=GetStringUTFChars_failed"
            );
            api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }
        target_process_ = is_target_process(process_name);
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "preAppSpecialize process=%s target=%d",
            process_name,
            target_process_ ? 1 : 0
        );
        if (target_process_) {
            process_pid_ = static_cast<int32_t>(getpid());
            copy_string(process_name_, sizeof(process_name_), process_name);
            companion_fd_ = api_->connectCompanion();
            __android_log_print(
                ANDROID_LOG_INFO,
                kLogTag,
                "connectCompanion fd=%d",
                companion_fd_
            );
            if (companion_fd_ >= 0) {
                RuntimeEvent event{};
                event.magic = kRuntimeEventMagic;
                event.protocol_version = kRuntimeProtocolVersion;
                event.event_type = static_cast<uint32_t>(RuntimeEventType::kTargetAttached);
                event.pid = process_pid_;
                event.il2cpp_required_symbol_count = kRequiredIl2cppCoreSymbolCount;
                copy_string(event.process_name, sizeof(event.process_name), process_name_);
                const bool event_write_succeeded = write_full(companion_fd_, &event, sizeof(event));
                __android_log_print(
                    event_write_succeeded ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR,
                    kLogTag,
                    "target event write=%d",
                    event_write_succeeded ? 1 : 0
                );
                bool exempt_succeeded = false;
                if (event_write_succeeded) {
                    exempt_succeeded = api_->exemptFd(companion_fd_);
                    __android_log_print(
                        exempt_succeeded ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR,
                        kLogTag,
                        "exemptFd=%d",
                        exempt_succeeded ? 1 : 0
                    );
                } else {
                    __android_log_print(
                        ANDROID_LOG_ERROR,
                        kLogTag,
                        "exemptFd=skipped reason=target_event_write_failed"
                    );
                }
                if (!event_write_succeeded || !exempt_succeeded) {
                    close(companion_fd_);
                    companion_fd_ = -1;
                }
            } else {
                __android_log_print(
                    ANDROID_LOG_ERROR,
                    kLogTag,
                    "connectCompanion failed fd=%d",
                    companion_fd_
                );
            }
        }
        env_->ReleaseStringUTFChars(args->nice_name, process_name);
        if (!target_process_ || companion_fd_ < 0) {
            __android_log_print(
                ANDROID_LOG_INFO,
                kLogTag,
                "DLCLOSE_MODULE_LIBRARY target=%d companion_fd=%d",
                target_process_ ? 1 : 0,
                companion_fd_
            );
            api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
        }
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *) override {
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "postAppSpecialize target=%d companion_fd=%d",
            target_process_ ? 1 : 0,
            companion_fd_
        );
        if (!target_process_ || companion_fd_ < 0) return;
        auto *context = new ProbeContext{};
        context->fd = companion_fd_;
        context->pid = process_pid_;
        copy_string(context->process_name, sizeof(context->process_name), process_name_);
        companion_fd_ = -1;
        pthread_t thread{};
        const int result = pthread_create(&thread, nullptr, binding_probe_thread, context);
        __android_log_print(
            result == 0 ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR,
            kLogTag,
            "pthread_create result=%d",
            result
        );
        if (result != 0) {
            close(context->fd);
            delete context;
            return;
        }
        pthread_detach(thread);
    }

private:
    zygisk::Api *api_ = nullptr;
    JNIEnv *env_ = nullptr;
    bool target_process_ = false;
    int companion_fd_ = -1;
    int32_t process_pid_ = 0;
    char process_name_[128]{};
};

REGISTER_ZYGISK_MODULE(PogoAutomationModule)
REGISTER_ZYGISK_COMPANION(companion_handler)
