#include <android/log.h>
#include <errno.h>
#include <fcntl.h>
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
constexpr const char *kTempStatePath = "/data/adb/pogo_root_automation/runtime.status.tmp";
constexpr uint32_t kRuntimeEventMagic = 0x504F474FU;  // POGO
constexpr uint32_t kRuntimeProtocolVersion = 1U;

enum class RuntimeEventType : uint32_t {
    kTargetAttached = 1U,
};

struct RuntimeEvent {
    uint32_t magic;
    uint32_t protocol_version;
    uint32_t event_type;
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

void persist_runtime_event(const RuntimeEvent &event) {
    if (mkdir(kStateDirectory, 0700) != 0 && errno != EEXIST) {
        __android_log_print(
            ANDROID_LOG_ERROR,
            kLogTag,
            "cannot create state directory: errno=%d",
            errno
        );
        return;
    }

    const int fd = open(kTempStatePath, O_CREAT | O_WRONLY | O_TRUNC | O_CLOEXEC, 0600);
    if (fd < 0) {
        __android_log_print(
            ANDROID_LOG_ERROR,
            kLogTag,
            "cannot open runtime state: errno=%d",
            errno
        );
        return;
    }

    dprintf(fd, "protocol=%u\n", event.protocol_version);
    dprintf(fd, "pid=%d\n", event.pid);
    dprintf(fd, "process=%s\n", event.process_name);
    dprintf(fd, "seen_at_epoch_ms=%lld\n", now_epoch_millis());
    fsync(fd);
    close(fd);

    if (rename(kTempStatePath, kStatePath) != 0) {
        __android_log_print(
            ANDROID_LOG_ERROR,
            kLogTag,
            "cannot publish runtime state: errno=%d",
            errno
        );
    }
}

void companion_handler(int fd) {
    RuntimeEvent event{};
    if (!read_full(fd, &event, sizeof(event))) {
        return;
    }

    if (event.magic != kRuntimeEventMagic ||
        event.protocol_version != kRuntimeProtocolVersion ||
        event.event_type != static_cast<uint32_t>(RuntimeEventType::kTargetAttached)) {
        return;
    }

    event.process_name[sizeof(event.process_name) - 1U] = '\0';
    persist_runtime_event(event);
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

        if (is_target_process(process_name)) {
            RuntimeEvent event{};
            event.magic = kRuntimeEventMagic;
            event.protocol_version = kRuntimeProtocolVersion;
            event.event_type = static_cast<uint32_t>(RuntimeEventType::kTargetAttached);
            event.pid = static_cast<int32_t>(getpid());
            snprintf(event.process_name, sizeof(event.process_name), "%s", process_name);

            const int companion_fd = api_->connectCompanion();
            if (companion_fd >= 0) {
                if (!write_full(companion_fd, &event, sizeof(event))) {
                    __android_log_print(
                        ANDROID_LOG_ERROR,
                        kLogTag,
                        "failed to send runtime event"
                    );
                }
                close(companion_fd);
            }

            __android_log_print(
                ANDROID_LOG_INFO,
                kLogTag,
                "target process observed: %s pid=%d",
                process_name,
                event.pid
            );
        }

        env_->ReleaseStringUTFChars(args->nice_name, process_name);

        // M1 only reports lifecycle. M2 will retain the target library when real
        // read-only game bindings are introduced.
        api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
    }

private:
    zygisk::Api *api_ = nullptr;
    JNIEnv *env_ = nullptr;
};

REGISTER_ZYGISK_MODULE(PogoAutomationModule)
REGISTER_ZYGISK_COMPANION(companion_handler)
