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
constexpr uint32_t kRuntimeEventMagic = 0x504F474FU;
constexpr uint32_t kRuntimeProtocolVersion = 4U;
constexpr int kProbeAttempts = 60;
constexpr int kAssemblySurveyAttempts = 20;
constexpr useconds_t kProbeDelayUs = 500000U;
constexpr uint32_t kRequiredIl2cppCoreSymbolCount = 10U;
constexpr size_t kMaxAssembliesToInspect = 4096U;
constexpr size_t kMaxClassesToInspect = 50000U;
constexpr uint32_t kMaxCandidateClasses = 32U;

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
    char process_name[128]{};
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

void copy_string(char *destination, size_t destination_size, const char *source) {
    if (destination == nullptr || destination_size == 0U) return;
    snprintf(destination, destination_size, "%s", source == nullptr ? "" : source);
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
    if (mkdir(kStateDirectory, 0700) != 0 && errno != EEXIST) return;
    char temp_path[192]{};
    snprintf(temp_path, sizeof(temp_path), "%s.%d", kTempStatePrefix, state.pid);
    const int fd = open(temp_path, O_CREAT | O_WRONLY | O_TRUNC | O_CLOEXEC, 0600);
    if (fd < 0) return;

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
    }
}

void companion_handler(int fd) {
    RuntimeState state{};
    RuntimeEvent event{};
    while (read_full(fd, &event, sizeof(event))) {
        if (event.magic != kRuntimeEventMagic || event.protocol_version != kRuntimeProtocolVersion) return;
        event.process_name[sizeof(event.process_name) - 1U] = '\0';
        event.il2cpp_path[sizeof(event.il2cpp_path) - 1U] = '\0';
        event.unity_path[sizeof(event.unity_path) - 1U] = '\0';
        event.translation_layer[sizeof(event.translation_layer) - 1U] = '\0';
        event.assembly_csharp_name[sizeof(event.assembly_csharp_name) - 1U] = '\0';
        event.candidate_classes[sizeof(event.candidate_classes) - 1U] = '\0';

        if (event.event_type == static_cast<uint32_t>(RuntimeEventType::kTargetAttached)) {
            state = RuntimeState{};
            state.protocol_version = event.protocol_version;
            state.pid = event.pid;
            copy_string(state.process_name, sizeof(state.process_name), event.process_name);
            persist_runtime_state(state);
            continue;
        }
        if (event.event_type == static_cast<uint32_t>(RuntimeEventType::kBindingProbe) && event.pid == state.pid) {
            state.probe_flags = event.probe_flags;
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
        }
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
                event.il2cpp_required_symbol_count = kRequiredIl2cppCoreSymbolCount;
                copy_string(event.process_name, sizeof(event.process_name), process_name_);
                if (!write_full(companion_fd_, &event, sizeof(event)) || !api_->exemptFd(companion_fd_)) {
                    close(companion_fd_);
                    companion_fd_ = -1;
                }
            }
        }
        env_->ReleaseStringUTFChars(args->nice_name, process_name);
        if (!target_process_ || companion_fd_ < 0) api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *) override {
        if (!target_process_ || companion_fd_ < 0) return;
        auto *context = new ProbeContext{};
        context->fd = companion_fd_;
        context->pid = process_pid_;
        copy_string(context->process_name, sizeof(context->process_name), process_name_);
        companion_fd_ = -1;
        pthread_t thread{};
        const int result = pthread_create(&thread, nullptr, binding_probe_thread, context);
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
