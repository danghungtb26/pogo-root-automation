namespace pogo_runtime_module {
namespace {
void clear_throw_events(ProbeContext &context) {
    RuntimeThrowEventRecord discarded{};
    while (context.throw_events.pop(&discarded)) {
    }
}

class RuntimeFeatureModuleBase {
public:
    virtual ~RuntimeFeatureModuleBase() = default;
    virtual Module id() const = 0;
    virtual const char *name() const = 0;
    virtual bool available(const RuntimeBinding &binding) const = 0;
    virtual bool on_enable(ProbeContext &) { return true; }
    virtual void on_disable(ProbeContext &) {}

    bool enabled() const { return enabled_.load(); }
    void set_enabled(bool value) { enabled_.store(value); }

private:
    std::atomic<bool> enabled_{false};
};

#include "modules/catch_spin/module.inc"
#include "modules/discard/module.inc"
#include "modules/transfer/module.inc"
#include "modules/throw_assist/module.inc"

constexpr size_t kMaxRuntimeFeatureModules = 8U;
constexpr uint32_t kModuleLoadCompletedPhase = 3U;
constexpr uint32_t kModuleLoadFailedPhase = 5U;
constexpr const char *kModuleLoadCommandPrefix = "runtime-module-load:";

class RuntimeModuleRegistry {
public:
    bool register_module(RuntimeFeatureModuleBase &module) {
        if (count_ >= kMaxRuntimeFeatureModules || resolve(module.id()) != nullptr) {
            return false;
        }
        modules_[count_++] = &module;
        return true;
    }

    RuntimeFeatureModuleBase *resolve(Module module) const {
        for (size_t index = 0U; index < count_; ++index) {
            RuntimeFeatureModuleBase *feature = modules_[index];
            if (feature != nullptr && feature->id() == module) return feature;
        }
        return nullptr;
    }

    bool any_enabled() const {
        for (size_t index = 0U; index < count_; ++index) {
            const RuntimeFeatureModuleBase *feature = modules_[index];
            if (feature != nullptr && feature->enabled()) return true;
        }
        return false;
    }

    void disable_all(ProbeContext &context) {
        for (size_t index = 0U; index < count_; ++index) {
            RuntimeFeatureModuleBase *feature = modules_[index];
            if (feature == nullptr || !feature->enabled()) continue;
            feature->set_enabled(false);
            feature->on_disable(context);
        }
    }

    size_t size() const { return count_; }

private:
    RuntimeFeatureModuleBase *modules_[kMaxRuntimeFeatureModules]{};
    size_t count_ = 0U;
};

RuntimeModuleRegistry g_runtime_module_registry;
CatchSpinModule g_catch_spin_module;
DiscardModule g_discard_module;
TransferModule g_transfer_module;
ThrowAssistModule g_throw_assist_module;
bool g_registry_initialization_attempted = false;
bool g_registry_initialized = false;

RuntimeFeatureModuleBase *resolve_module(Module module) {
    return g_runtime_module_registry.resolve(module);
}

bool any_enabled() {
    return g_runtime_module_registry.any_enabled();
}

bool ensure_observer_for_enabled_modules(ProbeContext &context) {
    if (!any_enabled()) return true;
    if (context.observation_running.load()) return true;
    return start_runtime_observation(context);
}

bool publish_module_load_status(
    int fd,
    const RuntimeFeatureModuleBase &module,
    bool loaded
) {
    char command_id[64]{};
    snprintf(
        command_id,
        sizeof(command_id),
        "%s%u",
        kModuleLoadCommandPrefix,
        static_cast<unsigned int>(module.id())
    );
    return send_runtime_result(
        fd,
        command_id,
        loaded ? kModuleLoadCompletedPhase : kModuleLoadFailedPhase,
        loaded ? "" : "runtime_module_load_failed",
        loaded ? "native module registered" : "native module registration failed"
    );
}

bool register_and_publish(int fd, RuntimeFeatureModuleBase &module) {
    const bool loaded = g_runtime_module_registry.register_module(module);
    const bool published = publish_module_load_status(fd, module, loaded);
    __android_log_print(
        loaded ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR,
        kLogTag,
        "runtime module register name=%s id=%u loaded=%d status_sent=%d",
        module.name(),
        static_cast<unsigned int>(module.id()),
        loaded ? 1 : 0,
        published ? 1 : 0
    );
    return loaded;
}
}  // namespace

bool initialize_runtime_modules(int status_fd) {
    if (g_registry_initialization_attempted) return g_registry_initialized;
    g_registry_initialization_attempted = true;

    bool registered = true;
    registered = register_and_publish(status_fd, g_catch_spin_module) && registered;
    registered = register_and_publish(status_fd, g_discard_module) && registered;
    registered = register_and_publish(status_fd, g_transfer_module) && registered;
    registered = register_and_publish(status_fd, g_throw_assist_module) && registered;

    g_registry_initialized = registered && g_runtime_module_registry.size() == 4U;
    __android_log_print(
        g_registry_initialized ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR,
        kLogTag,
        "runtime module registry initialized=%d count=%zu",
        g_registry_initialized ? 1 : 0,
        g_runtime_module_registry.size()
    );
    return g_registry_initialized;
}

bool module_registry_initialized() {
    return g_registry_initialized;
}

bool module_enabled(Module module) {
    RuntimeFeatureModuleBase *feature = resolve_module(module);
    return feature != nullptr && feature->enabled();
}

const char *module_name(Module module) {
    RuntimeFeatureModuleBase *feature = resolve_module(module);
    return feature == nullptr ? "unknown" : feature->name();
}

bool set_module_enabled(
    ProbeContext &context,
    Module module,
    bool enabled,
    std::string *error_code,
    std::string *message
) {
    if (!g_registry_initialized) {
        if (error_code != nullptr) *error_code = "runtime_module_registry_uninitialized";
        if (message != nullptr) *message = "runtime feature module registry is not initialized";
        return false;
    }

    RuntimeFeatureModuleBase *feature = resolve_module(module);
    if (feature == nullptr) {
        if (error_code != nullptr) *error_code = "unknown_runtime_module";
        if (message != nullptr) *message = "runtime feature module is not registered";
        return false;
    }

    if (!enabled) {
        feature->set_enabled(false);
        feature->on_disable(context);
        if (!any_enabled()) stop_runtime_observation(context);
        if (message != nullptr) {
            *message = std::string(feature->name()) + " module disabled";
        }
        return true;
    }

    if (feature->enabled()) {
        if (message != nullptr) {
            *message = std::string(feature->name()) + " module already enabled";
        }
        return true;
    }

    const RuntimeBinding binding = runtime_binding_snapshot(context);
    if (!feature->available(binding)) {
        if (error_code != nullptr) *error_code = "runtime_module_unavailable";
        if (message != nullptr) {
            *message = std::string(feature->name()) +
                " module has no verified binding for this game build";
        }
        return false;
    }

    if (!feature->on_enable(context)) {
        if (error_code != nullptr) *error_code = "runtime_module_start_failed";
        if (message != nullptr) {
            *message = std::string(feature->name()) + " module failed to initialize";
        }
        return false;
    }

    feature->set_enabled(true);
    if (!ensure_observer_for_enabled_modules(context)) {
        feature->set_enabled(false);
        feature->on_disable(context);
        if (error_code != nullptr) *error_code = "runtime_observer_unavailable";
        if (message != nullptr) {
            *message = std::string(feature->name()) +
                " module could not start the shared runtime observer";
        }
        return false;
    }

    if (message != nullptr) {
        *message = std::string(feature->name()) + " module enabled";
    }
    return true;
}

void disable_all_modules(ProbeContext &context) {
    if (g_registry_initialized) {
        g_runtime_module_registry.disable_all(context);
    }
    stop_runtime_observation(context);
    context.active_encounter_id.store(0U);
}

bool gameplay_action_allowed(uint32_t action_tag, std::string *reason) {
    const bool catch_spin = module_enabled(Module::kCatchSpin);
    const bool throw_assist = module_enabled(Module::kThrowAssist);
    bool allowed = false;
    switch (action_tag) {
        case 2U:
        case 3U:
        case 4U:
        case 9U:
        case 11U:
        case 12U:
            allowed = catch_spin;
            break;
        case 10U:
            allowed = catch_spin && throw_assist;
            break;
        case 5U:
            allowed = module_enabled(Module::kDiscard);
            break;
        case 6U:
            allowed = module_enabled(Module::kTransfer);
            break;
        case 8U:
            allowed = throw_assist;
            break;
        default:
            return true;
    }
    if (!allowed && reason != nullptr) {
        *reason = "required runtime feature module is disabled";
    }
    return allowed;
}
}  // namespace pogo_runtime_module
