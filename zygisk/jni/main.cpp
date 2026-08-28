#include <android/log.h>
#include <string.h>

#include "zygisk.hpp"

namespace {
constexpr const char *kLogTag = "PogoRootAutomation";
constexpr const char *kGooglePlayProcess = "com.nianticlabs.pokemongo";
constexpr const char *kGalaxyProcess = "com.nianticlabs.pokemongo.ares";

bool is_target_process(const char *process_name) {
    if (process_name == nullptr) {
        return false;
    }

    return strcmp(process_name, kGooglePlayProcess) == 0 ||
           strcmp(process_name, kGalaxyProcess) == 0;
}
}  // namespace

class PogoAutomationModule : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        api_ = api;
        env_ = env;
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
        const char *process_name = env_->GetStringUTFChars(args->nice_name, nullptr);
        const bool target = is_target_process(process_name);

        if (target) {
            __android_log_print(
                ANDROID_LOG_INFO,
                kLogTag,
                "target process observed: %s",
                process_name
            );
        } else {
            api_->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
        }

        env_->ReleaseStringUTFChars(args->nice_name, process_name);
    }

private:
    zygisk::Api *api_ = nullptr;
    JNIEnv *env_ = nullptr;
};

REGISTER_ZYGISK_MODULE(PogoAutomationModule)
