// Keep the native runtime in focused units while preserving one translation
// unit for Zygisk's registration macros and the existing internal linkage.
#include <sys/mman.h>
#include "runtime_native_prelude.inc"
#include "runtime_feature_module_protocol.h"
#include "runtime_native_common.inc"
#include "runtime_bridge_protocol.inc"
#include "runtime_bridge_broker.inc"
#include "runtime_probe_elf.inc"
#include "runtime_probe_inspection.inc"
#include "runtime_probe_owners.inc"
#include "runtime_probe_discovery.inc"
#include "runtime_direct_map_bindings.inc"
#include "runtime_encounter_owners.inc"
#include "runtime_probe_candidates.inc"
#include "runtime_main_thread_bridge.inc"
#include "runtime_scene_owners.inc"
#include "runtime_main_thread_loader.inc"
#include "runtime_main_thread_unity.inc"
#include "runtime_main_thread_actions.inc"
#include "runtime_main_thread_direct_actions.inc"
#include "runtime_probe_gesture.inc"
#include "runtime_probe_diagnostic.inc"
#include "runtime_probe_inventory.inc"
#include "runtime_probe_survey.inc"
#include "runtime_companion.inc"
#include "runtime_map_reader.inc"
#include "runtime_map_dictionary.inc"
#include "runtime_map_forts.inc"
#include "runtime_probe_map_instance.inc"

// THROW_ASSIST owns throw binding/hooks. Keep this before observation because
// the shared observer drains module-owned throw diagnostics when armed.
#include "modules/throw_assist/throw_hooks.inc"
#include "runtime_observation_policy.inc"
#define request_main_thread_map_snapshot request_main_thread_map_snapshot_throttled
#define refresh_runtime_encounter_owners refresh_runtime_encounter_owners_throttled
#include "runtime_observation.inc"
#undef refresh_runtime_encounter_owners
#undef request_main_thread_map_snapshot

// Shared execution infrastructure.
#include "runtime_map_actions.inc"
#include "runtime_direct_map_actions.inc"
#include "runtime_encounter_actions.inc"
#include "runtime_action_common.inc"

// Feature-owned command parsing/execution.
#include "modules/catch_spin/catch.inc"
#include "modules/catch_spin/spin.inc"
#include "modules/catch_spin/open_encounter.inc"
#include "modules/catch_spin/map_hooks.inc"
#include "modules/discard/discard.inc"
#include "modules/throw_assist/berry.inc"

// Host publication/routing comes after feature implementations so capability
// publication can describe the verified executors without owning them.
#include "host/runtime_capabilities.inc"
#include "runtime_feature_modules.inc"
#include "runtime_control.inc"
}  // namespace

#include "runtime_module.inc"
