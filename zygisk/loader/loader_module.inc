#include "../runtime-core/bootstrap/core_bootstrap_abi.h"

class PogoLoaderModule : public zygisk::ModuleBase {
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
        const char *name = env_->GetStringUTFChars(args->nice_name, nullptr);
        if (name == nullptr) {
            api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }
        const bool target = is_target_process(name);
        if (target) {
            pid_ = static_cast<int32_t>(getpid());
            copy_string(process_name_, sizeof(process_name_), name);
            companion_fd_ = api_->connectCompanion();
            if (companion_fd_ >= 0) {
                RuntimeEvent event{};
                event.magic = kRuntimeEventMagic;
                event.protocol_version = kRuntimeProtocolVersion;
                event.event_type = static_cast<uint32_t>(RuntimeEventType::kTargetAttached);
                event.pid = pid_;
                event.il2cpp_required_symbol_count = kRequiredIl2cppCoreSymbolCount;
                copy_string(event.process_name, sizeof(event.process_name), process_name_);
                if (!write_full(companion_fd_, &event, sizeof(event)) ||
                    !api_->exemptFd(companion_fd_)) {
                    close(companion_fd_);
                    companion_fd_ = -1;
                } else {
                    const timeval timeout{10, 0};
                    const bool timeout_set = setsockopt(companion_fd_, SOL_SOCKET,
                        SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0;
                    received_ = timeout_set && pogo_core_handoff::receive_reply(
                        companion_fd_, &reply_, &core_fd_);
                    const timeval no_timeout{0, 0};
                    if (setsockopt(companion_fd_, SOL_SOCKET, SO_RCVTIMEO,
                            &no_timeout, sizeof(no_timeout)) != 0) received_ = false;
                    if (!received_ && core_fd_ >= 0) {
                        close(core_fd_);
                        core_fd_ = -1;
                    }
                    if (received_ && core_fd_ >= 0 && !api_->exemptFd(core_fd_)) {
                        close(core_fd_);
                        core_fd_ = -1;
                        received_ = false;
                    }
                }
            }
        }
        env_->ReleaseStringUTFChars(args->nice_name, name);
        if (!target || companion_fd_ < 0) {
            api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
        }
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *) override {
        if (companion_fd_ < 0) return;
        const char *error = nullptr;
        if (!received_) error = "core_handoff_unavailable";
        else if (reply_.result == uint32_t(pogo_core_handoff::Result::kCoreMissing))
            error = "core_missing";
        else if (reply_.result == uint32_t(pogo_core_handoff::Result::kManifestInvalid))
            error = "core_manifest_invalid";
        else if (reply_.result == uint32_t(pogo_core_handoff::Result::kCoreAbi) ||
                 (core_fd_ >= 0 && !pogo_core_handoff::elf_matches_abi(core_fd_)))
            error = "core_abi_mismatch";
        else if (!pogo_core_handoff::verify_reply(reply_, core_fd_))
            error = "core_integrity_failure";
        if (error != nullptr) {
            fail(error);
            return;
        }

        android_dlextinfo info{};
        info.flags = ANDROID_DLEXT_USE_LIBRARY_FD;
        info.library_fd = core_fd_;
        core_handle_ = android_dlopen_ext("libpogo_runtime_core.so",
            RTLD_NOW | RTLD_LOCAL, &info);
        close(core_fd_);
        core_fd_ = -1;
        if (core_handle_ == nullptr) {
            fail("core_load_failure");
            return;
        }
        using Start = int (*)(const CoreBootstrapArgsV1 *);
        auto start = reinterpret_cast<Start>(dlsym(core_handle_,
            "pogo_runtime_core_start_v1"));
        if (start == nullptr) {
            fail("core_abi_mismatch");
            return;
        }
        CoreBootstrapArgsV1 args{};
        args.protocol = kCoreBootstrapProtocolV1;
        args.struct_size = sizeof(args);
        args.companion_fd = companion_fd_;
        args.process_pid = pid_;
        args.jni_env = env_;
        copy_string(args.process_name, sizeof(args.process_name), process_name_);
        const int result = start(&args);
        if (result != 0) {
            fail(result == kCoreStartInvalidArgs || result == kCoreStartDuplicate
                ? "core_abi_mismatch" : "core_start_failure");
            return;
        }
        companion_fd_ = -1;
    }

private:
    void fail(const char *code) {
        pogo_runtime_bootstrap::Status status;
        status.session_id = std::string("native-pending|") + process_name_;
        status.pid = pid_;
        status.process_name = process_name_;
        status.sequence = 1U;
        status.phase = pogo_runtime_bootstrap::Phase::kFailed;
        status.error_code = code;
        status.message = "runtime core bootstrap failed";
        status.observed_at_epoch_ms = uint64_t(now_epoch_millis());
        status.observed_at_elapsed_ns = uint64_t(now_elapsed_nanos());
        std::vector<uint8_t> payload;
        if (pogo_runtime_bootstrap::encode(status, &payload)) {
            (void)send_internal_message(companion_fd_,
                kRuntimeBootstrapStatusMagic, payload);
        }
        __android_log_print(ANDROID_LOG_ERROR, kLogTag,
            "core bootstrap failed: %s", code);
        if (core_fd_ >= 0) close(core_fd_);
        if (core_handle_ != nullptr) dlclose(core_handle_);
        close(companion_fd_);
        core_fd_ = -1;
        companion_fd_ = -1;
    }

    zygisk::Api *api_ = nullptr;
    JNIEnv *env_ = nullptr;
    int companion_fd_ = -1;
    int core_fd_ = -1;
    int32_t pid_ = 0;
    bool received_ = false;
    char process_name_[128]{};
    pogo_core_handoff::Reply reply_{};
    void *core_handle_ = nullptr;
};

REGISTER_ZYGISK_MODULE(PogoLoaderModule)
REGISTER_ZYGISK_COMPANION(companion_handler)
