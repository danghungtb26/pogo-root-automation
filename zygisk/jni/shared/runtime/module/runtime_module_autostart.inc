namespace {
constexpr Module kAutoStartModules[] = {
    Module::kCatchSpin,
    Module::kDiscard,
    Module::kTransfer,
};

bool catch_spin_desired_by_config() {
    const RuntimeCatchSpinConfig config = runtime_catch_spin_config_snapshot();
    return config.configured && config.armed &&
        ((config.auto_catch && config.catch_all) || config.auto_spin || config.auto_walk_to_fort);
}

bool discard_desired_by_config() {
    const RuntimeDiscardConfig config = runtime_discard_config_snapshot();
    return config.configured && config.auto_discard;
}

bool transfer_desired_by_config() {
    const RuntimeTransferConfig config = runtime_transfer_config_snapshot();
    return config.configured && config.auto_transfer;
}

bool module_desired_by_config(Module module) {
    switch (module) {
        case Module::kCatchSpin:
            return catch_spin_desired_by_config();
        case Module::kDiscard:
            return discard_desired_by_config();
        case Module::kTransfer:
            return transfer_desired_by_config();
        default:
            return false;
    }
}
}  // namespace

#include "runtime_managed_config.inc"

void sync_auto_enabled_modules(ProbeContext &context) {
    if (!runtime_managed_configs_ready()) {
        maybe_log_managed_config_gate();
        return;
    }
    if (!map_runtime_ready(context)) {
        discover_scene_runtime_owners(context);
        probe_map_live(context);
    }
    if (!map_runtime_ready(context)) return;

    for (Module module : kAutoStartModules) {
        RuntimeFeatureModuleBase *feature = resolve_module(module);
        if (feature == nullptr) continue;

        const bool desired = module_desired_by_config(module);
        const bool enabled = feature->enabled();
        if (desired == enabled) continue;

        std::string error_code;
        std::string message;
        const bool changed = set_module_enabled(
            context,
            module,
            desired,
            &error_code,
            &message
        );
        __android_log_print(
            changed ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
            kLogTag,
            "auto module sync name=%s desired=%d changed=%d error=%s",
            feature->name(),
            desired ? 1 : 0,
            changed ? 1 : 0,
            changed ? "" : error_code.c_str()
        );
    }
}
