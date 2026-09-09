// Keep the native runtime in focused units while preserving one translation
// unit for Zygisk's registration macros and the existing internal linkage.
#include <sys/mman.h>
#include "runtime_native_prelude.inc"
#include "runtime_native_common.inc"
#include "runtime_bridge_protocol.inc"
#include "runtime_bridge_broker.inc"
#include "runtime_probe_elf.inc"
#include "runtime_probe_inspection.inc"
#include "runtime_soft_catch_bindings.inc"
#include "runtime_probe_discovery.inc"
#include "runtime_direct_map_bindings.inc"
#include "runtime_encounter_owners.inc"
#include "runtime_main_thread_bridge.inc"
#include "runtime_main_thread_loader.inc"
#include "runtime_main_thread_unity.inc"
#include "runtime_main_thread_actions.inc"
#include "runtime_main_thread_direct_actions.inc"
#include "runtime_probe_diagnostic.inc"
#include "runtime_probe_survey.inc"
#include "runtime_companion.inc"
#include "runtime_map_reader.inc"
#include "runtime_map_dictionary.inc"
#include "runtime_map_forts.inc"
#include "runtime_throw_hooks.inc"
#include "runtime_observation_policy.inc"
// Keep throw-event dispatch responsive while rate-limiting only the expensive
// Unity-main-thread map walk and owner rediscovery performed by the observer.
#define request_main_thread_map_snapshot request_main_thread_map_snapshot_throttled
#define refresh_runtime_encounter_owners refresh_runtime_encounter_owners_throttled
#include "runtime_observation.inc"
#undef refresh_runtime_encounter_owners
#undef request_main_thread_map_snapshot
#include "runtime_action_common.inc"
#include "runtime_soft_catch.inc"
#include "runtime_map_actions.inc"
#include "runtime_direct_map_actions.inc"
#include "runtime_encounter_actions.inc"
#include "runtime_action_catch.inc"
#include "runtime_action_spin.inc"
#include "runtime_action_open_encounter.inc"
#include "runtime_actions.inc"
}  // namespace

#include "runtime_module.inc"
