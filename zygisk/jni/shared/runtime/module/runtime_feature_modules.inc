namespace pogo_runtime_module {
namespace {
void clear_throw_events(ProbeContext &context) {
    RuntimeThrowEventRecord discarded{};
    while (context.throw_events.pop(&discarded)) {
    }
}

// Declarative action ownership. A module returns the gameplay action tags and
// control actions it owns; the registry builds a direct owner map at registration
// time so dispatch is an O(1) lookup instead of probing every module per command.
struct ModuleActionOwnership {
    const uint32_t *gameplay_tags = nullptr;
    size_t gameplay_tag_count = 0U;
    const uint32_t *control_actions = nullptr;
    size_t control_action_count = 0U;
};

class RuntimeFeatureModuleBase {
public:
    virtual ~RuntimeFeatureModuleBase() = default;
    virtual Module id() const = 0;
    virtual const char *name() const = 0;
    virtual bool available(const RuntimeBinding &binding) const = 0;
    virtual bool on_enable(ProbeContext &) { return true; }
    virtual void on_disable(ProbeContext &) {}
    // Called each observer tick while this module is enabled, on the observer's
    // attached il2cpp thread. Each module reads its own concern and decides its
    // own cadence via ObserverTickContext::tick. Default: nothing to observe.
    virtual void observe(ObserverTickContext &) {}

    // Declares the gameplay action tags and control actions this module owns.
    // The registry reads it once at registration to build the owner map. Keeping
    // ownership + parsing + execution inside the module means no central dispatch
    // or gate switch has to know any module's commands.
    virtual ModuleActionOwnership action_ownership() const { return ModuleActionOwnership{}; }
    // Parse + execute a command this module owns. Called only when the registry
    // routed this tag to this module and the module is enabled. *command_id is
    // pre-set to the frame's command id (usable for rejections); overwrite it with
    // the parsed action's id on success. Returns true if handled (result filled).
    virtual bool handle_action(
        ProbeContext & /*context*/,
        uint32_t /*action_tag*/,
        const std::vector<uint8_t> & /*command*/,
        RuntimeActionResult * /*result*/,
        std::string * /*command_id*/
    ) {
        return false;
    }

    // Handles a control action this module owns (declared via action_ownership).
    // control_action is the raw wire value so a module owning several actions
    // (e.g. the runtime-core module: START/STOP/DIAGNOSTIC) can tell them apart.
    virtual bool handle_control_action(
        ProbeContext & /*context*/,
        uint32_t /*control_action*/,
        const char * /*request_id*/,
        uint64_t /*cycle_id*/
    ) {
        return false;
    }

    bool enabled() const { return enabled_.load(); }
    void set_enabled(bool value) { enabled_.store(value); }

private:
    std::atomic<bool> enabled_{false};
};

// Self-registration: each module.inc defines its own instance plus a static
// registration entry appended to this intrusive list at static-init time. Adding
// or removing a module is therefore a single-file change — the framework keeps no
// hardcoded module list and no fixed module count.
struct RuntimeModuleRegistration {
    RuntimeFeatureModuleBase *module;
    RuntimeModuleRegistration *next;
};
RuntimeModuleRegistration *g_runtime_module_registration_head = nullptr;
bool append_runtime_module_registration(RuntimeModuleRegistration *entry) {
    if (entry == nullptr || entry->module == nullptr) return false;
    entry->next = g_runtime_module_registration_head;
    g_runtime_module_registration_head = entry;
    return true;
}

#include "../../../modules/catch_spin/module.inc"
#include "../../../modules/discard/module.inc"
#include "../../../modules/transfer/module.inc"
#include "../../../modules/encounter/module.inc"
#include "../../../modules/core/module.inc"

constexpr size_t kMaxRuntimeFeatureModules = 8U;
// Owner-map bounds: gameplay action tags and control actions index a direct table.
// Grow if a new tag/action exceeds these; out-of-range values are simply unowned.
constexpr uint32_t kMaxGameplayActionTag = 32U;
constexpr uint32_t kMaxControlAction = 16U;
constexpr uint32_t kModuleLoadCompletedPhase = 3U;
constexpr uint32_t kModuleLoadFailedPhase = 5U;
constexpr const char *kModuleLoadCommandPrefix = "runtime-module-load:";

class RuntimeModuleRegistry {
public:
    bool register_module(RuntimeFeatureModuleBase &module) {
        if (count_ >= kMaxRuntimeFeatureModules || resolve(module.id()) != nullptr) {
            return false;
        }
        // Build the direct owner map from the module's declared ownership so
        // dispatch is an O(1) lookup. First owner of a tag wins (registration
        // order); a tag/action outside the table bounds stays unowned.
        const ModuleActionOwnership ownership = module.action_ownership();
        for (size_t i = 0U; i < ownership.gameplay_tag_count; ++i) {
            const uint32_t tag = ownership.gameplay_tags[i];
            if (tag < kMaxGameplayActionTag && gameplay_owner_[tag] == nullptr) {
                gameplay_owner_[tag] = &module;
            }
        }
        for (size_t i = 0U; i < ownership.control_action_count; ++i) {
            const uint32_t action = ownership.control_actions[i];
            if (action < kMaxControlAction && control_owner_[action] == nullptr) {
                control_owner_[action] = &module;
            }
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

    RuntimeFeatureModuleBase *resolve_action_owner(uint32_t action_tag) const {
        return action_tag < kMaxGameplayActionTag ? gameplay_owner_[action_tag] : nullptr;
    }

    RuntimeFeatureModuleBase *resolve_control_owner(uint32_t control_action) const {
        return control_action < kMaxControlAction ? control_owner_[control_action] : nullptr;
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

    void observe_enabled(ObserverTickContext &tick) {
        for (size_t index = 0U; index < count_; ++index) {
            RuntimeFeatureModuleBase *feature = modules_[index];
            if (feature != nullptr && feature->enabled()) feature->observe(tick);
        }
    }

    size_t size() const { return count_; }

private:
    RuntimeFeatureModuleBase *modules_[kMaxRuntimeFeatureModules]{};
    RuntimeFeatureModuleBase *gameplay_owner_[kMaxGameplayActionTag]{};
    RuntimeFeatureModuleBase *control_owner_[kMaxControlAction]{};
    size_t count_ = 0U;
};

RuntimeModuleRegistry g_runtime_module_registry;
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
    if (!runtime_binding_snapshot(context).map_entity_read_verified) {
        // Give late-created map services one safe main-thread discovery pass
        // before the observer owns the binding for the module session.
        discover_scene_runtime_owners(context);
    }
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

// Named-namespace entry point (declared in the prelude) so the shared observer
// thread can dispatch a tick to every enabled module's observe() hook.
void observe_enabled_modules(ObserverTickContext &tick) {
    g_runtime_module_registry.observe_enabled(tick);
}

bool initialize_runtime_modules(int status_fd) {
    if (g_registry_initialization_attempted) return g_registry_initialized;
    g_registry_initialization_attempted = true;

    // Walk the self-registration list built at static-init by each module.inc.
    // No hardcoded module list or fixed count: modules add themselves.
    bool registered = g_runtime_module_registration_head != nullptr;
    for (RuntimeModuleRegistration *entry = g_runtime_module_registration_head;
         entry != nullptr; entry = entry->next) {
        if (entry->module == nullptr) continue;
        registered = register_and_publish(status_fd, *entry->module) && registered;
    }

    g_registry_initialized = registered && g_runtime_module_registry.size() > 0U;
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

enum class GameplayDispatchStatus { kUnowned, kModuleDisabled, kHandled };

// Route a gameplay command to the module that owns its action tag via the O(1)
// owner map. Ownership (action_ownership) and execution (handle_action) live inside
// each module, so the control loop needs no per-action switch or gate. *command_id
// is pre-set to the
// frame command id for rejection messages; the owning module overwrites it on a
// successful parse.
GameplayDispatchStatus dispatch_gameplay_action(
    ProbeContext &context,
    uint32_t action_tag,
    const std::vector<uint8_t> &command,
    RuntimeActionResult *result,
    std::string *command_id,
    std::string *reject_reason
) {
    RuntimeFeatureModuleBase *owner = g_runtime_module_registry.resolve_action_owner(action_tag);
    if (owner == nullptr) return GameplayDispatchStatus::kUnowned;
    if (!owner->enabled()) {
        if (reject_reason != nullptr) *reject_reason = "required runtime feature module is disabled";
        return GameplayDispatchStatus::kModuleDisabled;
    }
    if (!owner->handle_action(context, action_tag, command, result, command_id)) {
        // The owner claims the tag but could not parse the frame: keep the loop
        // fail-closed rather than silently dropping the command.
        return GameplayDispatchStatus::kUnowned;
    }
    return GameplayDispatchStatus::kHandled;
}

// Route a control action to the module that owns it (e.g. SCAN_MAP -> catch_spin,
// START/STOP/DIAGNOSTIC -> runtime-core). Returns
// the module handler's result, or a fail-closed rejection if unowned.
bool dispatch_control_action(
    ProbeContext &context,
    uint32_t control_action,
    const char *request_id,
    uint64_t cycle_id
) {
    RuntimeFeatureModuleBase *owner = g_runtime_module_registry.resolve_control_owner(control_action);
    if (owner == nullptr) {
        return send_runtime_rejected(
            context.fd,
            request_id,
            "control_action_unowned",
            "no runtime feature module owns this control action"
        );
    }
    return owner->handle_control_action(context, control_action, request_id, cycle_id);
}
}  // namespace pogo_runtime_module
