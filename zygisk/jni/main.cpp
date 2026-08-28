#include <android/log.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "zygisk.hpp"

namespace {
constexpr const char *kLogTag = "PogoRootAutomation";
constexpr const char *kGooglePlayProcess = "com.nianticlabs.pokemongo";
constexpr const char *kGalaxyProcess = "com.nianticlabs.pokemongo.ares";
constexpr const char *kStateDirectory = "/data/adb/pogo_root_automation";
constexpr const char *kStatePath = "/data/adb/pogo_root_automation/runtime.status";
constexpr const char *kTempStatePrefix = "/data/adb/pogo_root_automation/runtime.status.tmp";
constexpr uint32_t kRuntimeEventMagic = 0x504F474FU;  // POGO
constexpr uint32_t kRuntimeProtocolVersion = 2U;
constexpr int kProbeAttempts = 60;
constexpr useconds_t kProbeDelayUs = 500000U;

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
};

struct RuntimeEvent {
    uint32_t magic;
    uint32_t protocol_version;
    uint32_t event_type;
    int32_t pid;
    uint32_t probe_flags;
    uint32_t il2cpp_symbol_count;
    char process_name[128];
    char il2cpp_path[512];
    char unity_path[512];
    char translation_layer[32];
};

struct RuntimeState {
    uint32_t protocol_version = kRuntimeProtocolVersion;
    int32_t pid = 0;
    uint32_t probe_flags = 0U;
    uint32_t il2cpp_symbol_count = 0U;
    char process_name[128]{};
    char il2cpp_path[512]{};
    char unity_path[512]{};
    char translation_layer[32]{};
};

struct ProbeContext {
    int fd;
    int32_t pid;
    char process_name[128];
};

bool is_target_process(const char *process_name) {
    if (process_name == nullptr) {
        return false;
    }

    return strcmp(process_name, kGooglePlayProcess) == 0 ||
           strcmp(process_name, kGalaxyProcess) == 0;
}

bool write_full(int fd, const void *buffer, size_t size) {
    const auto *cursor = static_cast<const uint8_t *>(buffer);
    size_t remaining = size;

    while (remaining > 0U) {
        const ssize_t written = write(fd, cursor, remaining);
        if (written <= 0) {
            return false;
        }
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
        if (count <= 0) {
            return false;
        }
        cursor += count;
        remaining -= static_cast<size_t>(count);
    }
    return true;
}

long long now_epoch_millis() {
    timespec value{};
    if (clock_gettime(CLOCK_REALTIME, &value) != 0) {
        return 0LL;
    }

    return static_cast<long long>(value.tv_sec) * 1000LL +
           static_cast<long long>(value.tv_nsec / 1000000L);
}

void copy_string(char *destination, size_t destination_size, const char *source) {
    if (destination == nullptr || destination_size == 0U) {
        return;
    }

    snprintf(destination, destination_size, "%s", source == nullptr ? "" : source);
}

bool find_mapping_path(const char *needle, char *output, size_t output_size) {
    if (needle == nullptr || output == nullptr || output_size == 0U) {
        return false;
    }

    FILE *maps = fopen("/proc/self/maps", "re");
    if (maps == nullptr) {
        return false;
    }

    bool found = false;
    char line[1536]{};
    char path[1024]{};
    while (fgets(line, sizeof(line), maps) != nullptr) {
        path[0] = '\0';
        const int parsed = sscanf(
            line,
            "%*s %*s %*s %*s %*s %1023s",
            path
        );
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

uint32_t resolve_il2cpp_api_symbols(const char *il2cpp_path) {
    if (il2cpp_path == nullptr || il2cpp_path[0] == '\0') {
        return 0U;
    }

    void *handle = dlopen(il2cpp_path, RTLD_NOW | RTLD_NOLOAD);
    if (handle == nullptr) {
        handle = dlopen("libil2cpp.so", RTLD_NOW | RTLD_NOLOAD);
    }
    if (handle == nullptr) {
        return 0U;
    }

    const char *symbols[] = {
        "il2cpp_domain_get",
        "il2cpp_domain_get_assemblies",
        "il2cpp_assembly_get_image",
        "il2cpp_image_get_name",
        "il2cpp_class_from_name",
        "il2cpp_class_get_field_from_name",
        "il2cpp_field_get_value",
        "il2cpp_object_get_class",
    };

    uint32_t resolved = 0U;
    for (const char *symbol : symbols) {
        if (dlsym(handle, symbol) != nullptr) {
            resolved += 1U;
        }
    }

    dlclose(handle);
    return resolved;
}

void persist_runtime_state(const RuntimeState &state) {
    if (mkdir(kStateDirectory, 0700) != 0 && errno != EEXIST) {
        __android_log_print(
            ANDROID_LOG_ERROR,
            kLogTag,
            "cannot create state directory: errno=%d",
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
            "cannot open runtime state: errno=%d",
            errno
        );
        return;
    }

    const bool probe_complete = (state.probe_flags & kProbeComplete) != 0U;
    const bool il2cpp_loaded = (state.probe_flags & kIl2cppLoaded) != 0U;
    const bool unity_loaded = (state.probe_flags & kUnityLoaded) != 0U;
    const bool api_available = (state.probe_flags & kIl2cppApiAvailable) != 0U;

    dprintf(fd, "protocol=%u\n", state.protocol_version);
    dprintf(fd, "pid=%d\n", state.pid);
    dprintf(fd, "process=%s\n", state.process_name);
    dprintf(fd, "seen_at_epoch_ms=%lld\n", now_epoch_millis());
    dprintf(fd, "native_probe_state=%s\n", probe_complete ? "complete" : "pending");
    dprintf(fd, "native_libil2cpp_loaded=%d\n", il2cpp_loaded ? 1 : 0);
    dprintf(fd, "native_libunity_loaded=%d\n", unity_loaded ? 1 : 0);
    dprintf(fd, "native_il2cpp_api_available=%d\n", api_available ? 1 : 0);
    dprintf(fd, "native_il2cpp_symbol_count=%u\n", state.il2cpp_symbol_count);
    dprintf(fd, "native_libil2cpp_path=%s\n", state.il2cpp_path);
    dprintf(fd, "native_libunity_path=%s\n", state.unity_path);
    dprintf(fd, "native_translation_layer=%s\n", state.translation_layer);
    fsync(fd);
    close(fd);

    if (rename(temp_path, kStatePath) != 0) {
        __android_log_print(
            ANDROID_LOG_ERROR,
            kLogTag,
            "cannot publish runtime state: errno=%d",
            errno
        );
    }
}

void companion_handler(int fd) {
    RuntimeState state{};
    RuntimeEvent event{};

    while (read_full(fd, &event, sizeof(event))) {
        if (event.magic != kRuntimeEventMagic ||
            event.protocol_version != kRuntimeProtocolVersion) {
            return;
        }

        event.process_name[sizeof(event.process_name) - 1U] = '\0';
        event.il2cpp_path[sizeof(event.il2cpp_path) - 1U] = '\0';
        event.unity_path[sizeof(event.unity_path) - 1U] = '\0';
        event.translation_layer[sizeof(event.translation_layer) - 1U] = '\0';

        if (event.event_type == static_cast<uint32_t>(RuntimeEventType::kTargetAttached)) {
            state = RuntimeState{};
            state.protocol_version = event.protocol_version;
            state.pid = event.pid;
            copy_string(state.process_name, sizeof(state.process_name), event.process_name);
            persist_runtime_state(state);
            continue;
        }

        if (event.event_type == static_cast<uint32_t>(RuntimeEventType::kBindingProbe) &&
            event.pid == state.pid) {
            state.probe_flags = event.probe_flags;
            state.il2cpp_symbol_count = event.il2cpp_symbol_count;
            copy_string(state.il2cpp_path, sizeof(state.il2cpp_path), event.il2cpp_path);
            copy_string(state.unity_path, sizeof(state.unity_path), event.unity_path);
            copy_string(
                state.translation_layer,
                sizeof(state.translation_layer),
                event.translation_layer
            );
            persist_runtime_state(state);
        }
    }
}

void *binding_probe_thread(void *opaque_context) {
    auto *context = static_cast<ProbeContext *>(opaque_context);
    if (context == nullptr) {
        return nullptr;
    }

    RuntimeEvent event{};
    event.magic = kRuntimeEventMagic;
    event.protocol_version = kRuntimeProtocolVersion;
    event.event_type = static_cast<uint32_t>(RuntimeEventType::kBindingProbe);
    event.pid = context->pid;
    copy_string(event.process_name, sizeof(event.process_name), context->process_name);
    copy_string(event.translation_layer, sizeof(event.translation_layer), "none");

    for (int attempt = 0; attempt < kProbeAttempts; ++attempt) {
        const bool il2cpp_loaded = find_mapping_path(
            "libil2cpp.so",
            event.il2cpp_path,
            sizeof(event.il2cpp_path)
        );
        const bool unity_loaded = find_mapping_path(
            "libunity.so",
            event.unity_path,
            sizeof(event.unity_path)
        );

        if (il2cpp_loaded) {
            event.probe_flags |= kIl2cppLoaded;
        }
        if (unity_loaded) {
            event.probe_flags |= kUnityLoaded;
        }

        if (mapping_contains("libhoudini")) {
            event.probe_flags |= kHoudiniTranslation;
            copy_string(event.translation_layer, sizeof(event.translation_layer), "houdini");
        } else if (mapping_contains("libndk_translation")) {
            event.probe_flags |= kNdkTranslation;
            copy_string(
                event.translation_layer,
                sizeof(event.translation_layer),
                "ndk_translation"
            );
        }

        if (il2cpp_loaded) {
            event.il2cpp_symbol_count = resolve_il2cpp_api_symbols(event.il2cpp_path);
            if (event.il2cpp_symbol_count > 0U) {
                event.probe_flags |= kIl2cppApiAvailable;
            }
            break;
        }

        usleep(kProbeDelayUs);
    }

    event.probe_flags |= kProbeComplete;
    if (!write_full(context->fd, &event, sizeof(event))) {
        __android_log_print(
            ANDROID_LOG_ERROR,
            kLogTag,
            "failed to publish binding probe for pid=%d",
            event.pid
        );
    } else {
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "binding probe: il2cpp=%d unity=%d api_symbols=%u translation=%s",
            (event.probe_flags & kIl2cppLoaded) != 0U,
            (event.probe_flags & kUnityLoaded) != 0U,
            event.il2cpp_symbol_count,
            event.translation_layer
        );
    }

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
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
        if (args == nullptr || args->nice_name == nullptr) {
            api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        const char *process_name = env_->GetStringUTFChars(args->nice_name, nullptr);
        if (process_name == nullptr) {
            api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        target_process_ = is_target_process(process_name);
        if (target_process_) {
            process_pid_ = static_cast<int32_t>(getpid());
            copy_string(process_name_, sizeof(process_name_), process_name);

            companion_fd_ = api_->connectCompanion();
            if (companion_fd_ >= 0) {
                RuntimeEvent event{};
                event.magic = kRuntimeEventMagic;
                event.protocol_version = kRuntimeProtocolVersion;
                event.event_type = static_cast<uint32_t>(RuntimeEventType::kTargetAttached);
                event.pid = process_pid_;
                copy_string(event.process_name, sizeof(event.process_name), process_name_);

                if (!write_full(companion_fd_, &event, sizeof(event))) {
                    __android_log_print(
                        ANDROID_LOG_ERROR,
                        kLogTag,
                        "failed to send runtime attach event"
                    );
                    close(companion_fd_);
                    companion_fd_ = -1;
                } else if (!api_->exemptFd(companion_fd_)) {
                    __android_log_print(
                        ANDROID_LOG_ERROR,
                        kLogTag,
                        "failed to exempt companion fd for post-specialize probe"
                    );
                    close(companion_fd_);
                    companion_fd_ = -1;
                }
            }

            __android_log_print(
                ANDROID_LOG_INFO,
                kLogTag,
                "target process observed: %s pid=%d persistent_probe=%d",
                process_name_,
                process_pid_,
                companion_fd_ >= 0
            );
        }

        env_->ReleaseStringUTFChars(args->nice_name, process_name);

        if (!target_process_ || companion_fd_ < 0) {
            api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
        }
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *) override {
        if (!target_process_ || companion_fd_ < 0) {
            return;
        }

        auto *context = new ProbeContext{};
        context->fd = companion_fd_;
        context->pid = process_pid_;
        copy_string(context->process_name, sizeof(context->process_name), process_name_);
        companion_fd_ = -1;

        pthread_t thread{};
        const int create_result = pthread_create(&thread, nullptr, binding_probe_thread, context);
        if (create_result != 0) {
            __android_log_print(
                ANDROID_LOG_ERROR,
                kLogTag,
                "cannot start binding probe thread: error=%d",
                create_result
            );
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
