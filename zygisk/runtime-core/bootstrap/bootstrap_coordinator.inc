namespace pogo_runtime_core {

#if defined(__aarch64__)
constexpr const char *kCoreAbi = "arm64-v8a";
#elif defined(__x86_64__)
constexpr const char *kCoreAbi = "x86_64";
#else
constexpr const char *kCoreAbi = "unsupported";
#endif

class BootstrapCoordinator {
public:
    int start(const CoreBootstrapArgsV1 *args) {
        if (args == nullptr || args->protocol != kCoreBootstrapProtocolV1 ||
            args->struct_size != sizeof(CoreBootstrapArgsV1) ||
            args->companion_fd < 0 || args->process_pid <= 0 ||
            args->jni_env == nullptr ||
            strnlen(args->process_name, sizeof(args->process_name)) >=
                sizeof(args->process_name) ||
            !is_target_process(args->process_name)) return kCoreStartInvalidArgs;

        bool expected = false;
        if (!started_.compare_exchange_strong(expected, true)) {
            return kCoreStartDuplicate;
        }
        auto *context = new ProbeContext{};
        context->fd = args->companion_fd;
        context->pid = args->process_pid;
        copy_string(context->process_name, sizeof(context->process_name),
            args->process_name);
        pogo_runtime_bootstrap::Status loaded;
        loaded.phase = pogo_runtime_bootstrap::Phase::kCoreLoaded;
        loaded.abi = kCoreAbi;
        const char *translation = mapping_contains("libhoudini")
            ? "houdini"
            : mapping_contains("libndk_translation") ? "ndk_translation" : "";
        loaded.translation_layer = translation;
        if (!publish_runtime_bootstrap_status(*context, std::move(loaded))) {
            abandon(context);
            return kCoreStartWorkerUnavailable;
        }

        // The loader hook must be installed before control returns to the
        // target. A mapped library still cannot satisfy the init gate.
        const char *failure_code = nullptr;
        if (translation[0] != '\0') {
            failure_code = "unsupported_translation";
        } else {
#if defined(__x86_64__)
            failure_code = "unsupported_abi";
#else
            failure_code = pogo_runtime_lifecycle::observer().install();
#endif
        }
        pogo_runtime_bootstrap::Status gate;
        gate.phase = failure_code == nullptr
            ? pogo_runtime_bootstrap::Phase::kLoaderObserverActive
            : pogo_runtime_bootstrap::Phase::kFailed;
        gate.abi = kCoreAbi;
        gate.translation_layer = translation;
        if (failure_code != nullptr) {
            gate.error_code = failure_code;
            gate.message = "lifecycle observer unavailable; restart target process";
        }
        if (!publish_runtime_bootstrap_status(*context, std::move(gate))) {
            pogo_runtime_lifecycle::observer().deactivate();
            abandon(context);
            return kCoreStartWorkerUnavailable;
        }

        (void)initialize_main_thread_bridge(*context,
            static_cast<JNIEnv *>(args->jni_env));

        pthread_t thread{};
        if (pthread_create(&thread, nullptr, binding_probe_thread, context) != 0) {
            pogo_runtime_lifecycle::observer().deactivate();
            if (failure_code == nullptr) {
                pogo_runtime_bootstrap::Status failed;
                failed.phase = pogo_runtime_bootstrap::Phase::kFailed;
                failed.abi = kCoreAbi;
                failed.error_code = "runtime_worker_unavailable";
                failed.message = "core command worker could not start";
                (void)publish_runtime_bootstrap_status(*context, std::move(failed));
            }
            shutdown_main_thread_bridge(*context);
            close(context->fd);
            delete context;
            // The terminal failure was already published. Core owns the FD.
            return 0;
        }
        pthread_detach(thread);
        return 0;  // Core command worker now owns the FD and volatile state.
    }

private:
    void abandon(ProbeContext *context) {
        shutdown_main_thread_bridge(*context);
        delete context;
        started_.store(false);
    }

    std::atomic<bool> started_{false};
};

BootstrapCoordinator &bootstrap_coordinator() {
    static BootstrapCoordinator coordinator;
    return coordinator;
}

}  // namespace pogo_runtime_core
