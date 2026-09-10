// Keep the native runtime in focused units while preserving one translation
// unit for Zygisk's registration macros and the existing internal linkage.
#include <sys/mman.h>
#include "shared/runtime_native_prelude.inc"
#include "shared/runtime_feature_module_protocol.h"
#include "shared/runtime_native_common.inc"
#include "shared/runtime_bridge_protocol.inc"
#include "shared/runtime_bridge_broker.inc"
#include "shared/runtime_probe_elf.inc"
#include "shared/runtime_probe_inspection.inc"
#include "shared/runtime_probe_owners.inc"
#include "shared/runtime_probe_discovery.inc"
#include "modules/catch_spin/direct_map_bindings.inc"
#include "shared/runtime_encounter_owners.inc"
#include "shared/runtime_probe_candidates.inc"
#include "shared/runtime_main_thread_bridge.inc"
#include "shared/runtime_scene_owners.inc"
#include "shared/runtime_main_thread_loader.inc"
#include "shared/runtime_main_thread_unity.inc"
#include "shared/runtime_main_thread_actions.inc"
#include "modules/catch_spin/main_thread_direct_actions.inc"
#include "shared/runtime_probe_gesture.inc"
#include "shared/runtime_probe_diagnostic.inc"
#include "shared/runtime_probe_inventory.inc"
#include "shared/runtime_probe_survey.inc"
#include "shared/runtime_companion.inc"
#include "shared/runtime_map_reader.inc"
#include "shared/runtime_map_dictionary.inc"
#include "shared/runtime_map_forts.inc"
#include "shared/runtime_probe_map_instance.inc"

// THROW_ASSIST owns throw binding/hooks. Keep this before observation because
// the shared observer drains module-owned throw diagnostics when armed.
#include "modules/encounter/throw_hooks.inc"
#include "shared/runtime_observation_policy.inc"
#include "shared/runtime_observation_senders.inc"
#define request_main_thread_map_snapshot request_main_thread_map_snapshot_throttled
#define refresh_runtime_encounter_owners refresh_runtime_encounter_owners_throttled
#include "shared/runtime_observation.inc"
#undef refresh_runtime_encounter_owners
#undef request_main_thread_map_snapshot

// catch_spin main-thread execution workers (dispatched by the shared bridge).
#include "modules/catch_spin/map_actions.inc"
#include "modules/catch_spin/direct_map_actions.inc"
#include "modules/catch_spin/encounter_actions.inc"
#include "shared/runtime_action_common.inc"

// Feature-owned command parsing/execution.
#include "modules/catch_spin/catch.inc"
#include "modules/catch_spin/spin.inc"
#include "modules/catch_spin/open_encounter.inc"
#include "modules/catch_spin/map_hooks.inc"
#include "modules/discard/inventory_reader.inc"
#include "modules/discard/parse.inc"
#include "modules/discard/execute.inc"
#include "modules/encounter/berry.inc"

// Host publication/routing comes after feature implementations so capability
// publication can describe the verified executors without owning them.
#include "host/runtime_capabilities.inc"
#include "shared/runtime_feature_modules.inc"
#include "shared/runtime_control.inc"
}  // namespace

#include "shared/runtime_module.inc"
