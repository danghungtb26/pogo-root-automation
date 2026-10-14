#include <android/dlext.h>

namespace pogo_runtime_lifecycle {

struct Observer {
    RuntimeInlineHook loader_hook;
    RuntimeInlineHook init_hook;
    std::atomic<bool> active{false};
    std::atomic<bool> library_seen{false};
    std::atomic<bool> init_hook_attempted{false};
    std::atomic<bool> init_return_observed{false};
    std::atomic<bool> init_succeeded{false};
    std::atomic<const char *> error{nullptr};
    std::atomic<uint64_t> installed_at_ns{0U};
    std::atomic<uint64_t> library_seen_at_ns{0U};
};

Observer g_observer;
constexpr uint64_t kLoaderEventTimeoutNs = 300'000'000'000ULL;
constexpr uint64_t kInitEventTimeoutNs = 60'000'000'000ULL;

bool is_il2cpp_library_name(const char *filename) {
    if (filename == nullptr) return false;
    const char *name = strrchr(filename, '/');
    name = name == nullptr ? filename : name + 1;
    return strcmp(name, "libil2cpp.so") == 0;
}

bool is_loader_image(const char *filename) {
    if (filename == nullptr) return false;
    const char *name = strrchr(filename, '/');
    name = name == nullptr ? filename : name + 1;
    return strcmp(name, "libdl.so") == 0 || strcmp(name, "linker64") == 0;
}

void fail_once(const char *reason) {
    const char *expected = nullptr;
    (void)g_observer.error.compare_exchange_strong(expected, reason,
        std::memory_order_release, std::memory_order_relaxed);
}

#if defined(__aarch64__)
using LoaderFunction = void *(*)(const char *, int, const android_dlextinfo *);
using InitFunction = int (*)(const char *);

int hooked_il2cpp_init(const char *domain_name) {
    const auto original = reinterpret_cast<InitFunction>(g_observer.init_hook.trampoline);
    if (original == nullptr) {
        fail_once("init_trampoline_unavailable");
        return 0;
    }
    const int result = original(domain_name);
    const int original_errno = errno;
    bool first_return = false;
    if (g_observer.active.load(std::memory_order_acquire) &&
        g_observer.init_return_observed.compare_exchange_strong(first_return, true)) {
        if (result == 1) {
            g_observer.init_succeeded.store(true, std::memory_order_release);
        } else {
            fail_once("il2cpp_init_failed");
        }
    }
    errno = original_errno;
    return result;
}

void *hooked_android_dlopen_ext(
    const char *filename,
    int flags,
    const android_dlextinfo *info
) {
    const auto original = reinterpret_cast<LoaderFunction>(g_observer.loader_hook.trampoline);
    if (original == nullptr) {
        fail_once("loader_trampoline_unavailable");
        return nullptr;
    }
    void *handle = original(filename, flags, info);
    const int original_errno = errno;
    if (handle != nullptr && g_observer.active.load(std::memory_order_acquire) &&
        is_il2cpp_library_name(filename)) {
        bool first_library = false;
        if (g_observer.library_seen.compare_exchange_strong(first_library, true)) {
            g_observer.library_seen_at_ns.store(
                static_cast<uint64_t>(now_elapsed_nanos()), std::memory_order_release);
        }
        bool expected = false;
        if (g_observer.init_hook_attempted.compare_exchange_strong(expected, true)) {
            void *target = dlsym(handle, "il2cpp_init");
            Dl_info symbol_info{};
            constexpr uint32_t kExpectedInitPrologue[4] = {
                0xA9BF4FFEU, 0xAA0003F3U, 0xD0FE6F61U, 0x913C4021U
            };
            if (target == nullptr || dladdr(target, &symbol_info) == 0 ||
                !is_il2cpp_library_name(symbol_info.dli_fname) ||
                !install_inline_hook(&g_observer.init_hook, target,
                    kExpectedInitPrologue,
                    reinterpret_cast<const void *>(&hooked_il2cpp_init))) {
                fail_once("init_hook_install_failed");
            }
        }
    }
    errno = original_errno;
    return handle;
}
#endif

const char *install() {
#if defined(__aarch64__)
    if (mapping_contains("libil2cpp.so")) return "il2cpp_already_mapped";
    void *target = dlsym(RTLD_DEFAULT, "android_dlopen_ext");
    if (target == nullptr) return "loader_symbol_unavailable";
    Dl_info loader_info{};
    if (dladdr(target, &loader_info) == 0 ||
        !is_loader_image(loader_info.dli_fname)) return "loader_symbol_owner_unverified";
    g_observer.active.store(true, std::memory_order_release);
    if (!install_checked_lifecycle_hook(&g_observer.loader_hook, target,
            reinterpret_cast<const void *>(&hooked_android_dlopen_ext))) {
        g_observer.active.store(false, std::memory_order_release);
        return "loader_hook_unsupported_prologue";
    }
    g_observer.installed_at_ns.store(
        static_cast<uint64_t>(now_elapsed_nanos()), std::memory_order_release);
    return nullptr;
#else
    return "unsupported_abi";
#endif
}

void deactivate() {
    g_observer.active.store(false, std::memory_order_release);
}

bool publish_phase(ProbeContext &context, pogo_runtime_bootstrap::Phase phase) {
    pogo_runtime_bootstrap::Status status;
    status.phase = phase;
    status.abi = "arm64-v8a";
    return publish_runtime_bootstrap_status(context, std::move(status));
}

bool advance(ProbeContext &context) {
    if (!g_observer.active.load(std::memory_order_acquire)) return true;
    auto phase = g_runtime_bootstrap_phase.load(std::memory_order_acquire);
    if (phase == pogo_runtime_bootstrap::Phase::kFailed) {
        deactivate();
        return true;
    }
    const bool library_seen = g_observer.library_seen.load(std::memory_order_acquire);
    if (library_seen && phase == pogo_runtime_bootstrap::Phase::kLoaderObserverActive) {
        if (!publish_phase(context, pogo_runtime_bootstrap::Phase::kIl2cppLibrarySeen)) return false;
        phase = pogo_runtime_bootstrap::Phase::kIl2cppLibrarySeen;
    }
    if (g_observer.init_succeeded.load(std::memory_order_acquire) &&
        phase == pogo_runtime_bootstrap::Phase::kIl2cppLibrarySeen) {
        if (!publish_phase(context, pogo_runtime_bootstrap::Phase::kIl2cppInitialized)) return false;
        phase = pogo_runtime_bootstrap::Phase::kIl2cppInitialized;
    }

    const uint64_t now = static_cast<uint64_t>(now_elapsed_nanos());
    const uint64_t start = g_observer.installed_at_ns.load(std::memory_order_acquire);
    const uint64_t seen_at = g_observer.library_seen_at_ns.load(std::memory_order_acquire);
    if (!library_seen && start != 0U && now - start >= kLoaderEventTimeoutNs) {
        fail_once("loader_event_missing");
    } else if (library_seen && !g_observer.init_succeeded.load(std::memory_order_acquire) &&
               seen_at != 0U && now - seen_at >= kInitEventTimeoutNs) {
        fail_once("init_event_missing");
    }
    const char *failure = g_observer.error.load(std::memory_order_acquire);
    if (failure == nullptr) return true;
    pogo_runtime_bootstrap::Status failed;
    failed.phase = pogo_runtime_bootstrap::Phase::kFailed;
    failed.abi = "arm64-v8a";
    failed.error_code = failure;
    failed.message = "lifecycle observer failed; restart target process";
    deactivate();
    return publish_runtime_bootstrap_status(context, std::move(failed));
}

class LibraryLifecycleObserver {
public:
    virtual ~LibraryLifecycleObserver() = default;
    virtual const char *install() = 0;
    virtual bool advance(ProbeContext &context) = 0;
    virtual void deactivate() = 0;
};

class Arm64LibraryLifecycleObserver final : public LibraryLifecycleObserver {
public:
    const char *install() override { return pogo_runtime_lifecycle::install(); }
    bool advance(ProbeContext &context) override {
        return pogo_runtime_lifecycle::advance(context);
    }
    void deactivate() override { pogo_runtime_lifecycle::deactivate(); }
};

LibraryLifecycleObserver &observer() {
    static Arm64LibraryLifecycleObserver instance;
    return instance;
}

}  // namespace pogo_runtime_lifecycle
