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
        initialize_main_thread_bridge(*context, env_);
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
