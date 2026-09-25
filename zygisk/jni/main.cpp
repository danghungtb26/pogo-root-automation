// Keep the native runtime in focused units while preserving one translation
// unit for Zygisk's registration macros and the existing internal linkage.
#include <sys/mman.h>
#include "shared/bridge_kotlin/runtime_catch_spin_protocol.h"
#include "shared/bridge_kotlin/runtime_catch_spin_config_protocol.h"
#include "shared/bridge_kotlin/runtime_automation_event_protocol.h"
#include "shared/bridge_kotlin/runtime_walk_candidate_protocol.h"
#include "shared/runtime/names/automation_subject_names.h"
#include "modules/catch_spin/auto_fort_navigation.h"
#include "modules/catch_spin/fort_refresh_schedule.h"
#include "shared/bridge_kotlin/runtime_discard_config_protocol.h"
#include "shared/bridge_kotlin/runtime_transfer_config_protocol.h"
#include "shared/bridge_kotlin/runtime_desired_state_protocol.h"
#include "shared/bridge_kotlin/runtime_ui_status_protocol.h"
#include "shared/core/runtime_native_prelude.inc"
#include "shared/core/runtime_native_declarations.inc"
#include "shared/runtime/inventory/runtime_inventory_common.inc"
#include "shared/bridge_kotlin/runtime_feature_module_protocol.h"
#include "shared/core/runtime_native_common.inc"
#include "shared/bridge_kotlin/runtime_bridge_protocol.inc"
#include "shared/bridge_kotlin/runtime_bridge_observation_protocol.inc"
#include "shared/bridge_kotlin/runtime_catch_spin_bridge.inc"
#include "shared/bridge_kotlin/runtime_automation_event_bridge.inc"
#include "shared/bridge_kotlin/runtime_navigation_bridge.inc"
#include "shared/bridge_kotlin/runtime_walk_candidate_bridge.inc"
#include "shared/bridge_kotlin/runtime_bridge_broker.inc"
#include "shared/core/runtime_probe_elf.inc"
#include "shared/runtime/probe/runtime_probe_inspection.inc"
#include "shared/runtime/probe/runtime_probe_owners.inc"
#include "modules/discard/hash_set_binding.inc"
#include "modules/discard/inventory_filter_binding.inc"
#include "modules/discard/inventory_binding_contract.inc"
#include "modules/catch_spin/spin_item_bubble_bindings.inc"
#include "modules/catch_spin/direct_map_bindings.inc"
#include "shared/runtime/probe/runtime_probe_discovery.inc"
#include "shared/runtime/probe/runtime_encounter_owners.inc"
#include "shared/runtime/probe/runtime_probe_candidates.inc"
#include "shared/runtime/mainthread/runtime_main_thread_bridge.inc"
#include "shared/runtime/mainthread/runtime_main_thread_bridge_lifecycle.inc"
#include "shared/runtime/probe/runtime_scene_owners.inc"
#include "shared/runtime/mainthread/runtime_main_thread_loader.inc"
#include "shared/runtime/mainthread/runtime_main_thread_unity.inc"
#include "shared/runtime/mainthread/runtime_main_thread_actions.inc"
#include "modules/catch_spin/main_thread_direct_actions.inc"
#include "shared/runtime/probe/runtime_probe_gesture.inc"
#include "shared/runtime/probe/runtime_probe_diagnostic.inc"
#include "shared/runtime/probe/runtime_probe_inventory.inc"
#include "shared/runtime/probe/runtime_probe_survey.inc"
#include "shared/bridge_appproc/runtime_companion.inc"
#include "shared/runtime/map/runtime_map_reader.inc"
#include "shared/runtime/map/runtime_map_dictionary.inc"
#include "shared/runtime/map/runtime_map_forts.inc"
#include "shared/runtime/probe/runtime_probe_map_instance.inc"

// THROW_ASSIST owns throw binding/hooks. Keep this before observation because
// the shared observer drains module-owned throw diagnostics when armed.
#include "modules/encounter/throw_hooks.inc"
#include "shared/runtime/observation/runtime_observation_policy.inc"
#include "shared/runtime/observation/runtime_observation_senders.inc"
#define request_main_thread_map_snapshot request_main_thread_map_snapshot_throttled
#define refresh_runtime_encounter_owners refresh_runtime_encounter_owners_throttled
#include "shared/runtime/observation/runtime_observation.inc"
#undef refresh_runtime_encounter_owners
#undef request_main_thread_map_snapshot

// catch_spin main-thread execution workers (dispatched by the shared bridge).
#include "modules/catch_spin/map_actions.inc"
#include "modules/catch_spin/map_post_action_sync.inc"
#include "modules/catch_spin/encounter_promise_observer.inc"
#include "modules/catch_spin/direct_map_actions.inc"
#include "modules/catch_spin/fort_cooldown.inc"
#include "modules/catch_spin/encounter_actions.inc"
#include "shared/runtime/module/runtime_action_common.inc"
// Serializes the three applied-config mirrors as one native revision. Feature
// observers take this lock through their snapshot helpers, so reconcile never
// publishes a partially applied desired snapshot.
std::recursive_mutex g_runtime_applied_config_mutex;
#include "modules/catch_spin/spin_item_bubbles.inc"
#include "modules/catch_spin/spin_interaction_cleanup.inc"
#include "modules/catch_spin/spin_promise_observer.inc"
#include "modules/catch_spin/spin_server_action.inc"
#include "modules/catch_spin/direct_catch_promise_observer.inc"
#include "modules/catch_spin/direct_catch_map_sync.inc"

// Feature-owned command parsing/execution.
#include "modules/catch_spin/catch.inc"
#include "modules/catch_spin/spin.inc"
#include "modules/catch_spin/open_encounter.inc"
#include "modules/catch_spin/config.inc"
#include "modules/discard/config.inc"
#include "modules/discard/inventory_readiness.inc"
#include "modules/discard/inventory_reader.inc"
#include "shared/runtime/mainthread/runtime_inventory_request.inc"
#include "shared/runtime/mainthread/runtime_player_position_request.inc"
#include "modules/discard/parse.inc"
#include "modules/discard/discard_scene_container.inc"
#include "modules/discard/item_inventory_lookup.inc"
#include "modules/discard/item_data_factory.inc"
#include "modules/discard/execute.inc"
#include "modules/discard/reconcile.inc"
#include "modules/transfer/parse.inc"
#include "modules/encounter/berry.inc"

#include "modules/transfer/config.inc"
#include "modules/transfer/metadata.inc"
#include "modules/transfer/promise_observer.inc"
#include "modules/transfer/transfer_candy_bubbles.inc"
#include "modules/transfer/release_promise_observer.inc"
#include "modules/transfer/execute.inc"
#include "modules/transfer/coordinator.inc"

// Host publication/routing comes after feature implementations so capability
// publication can describe the verified executors without owning them.
#include "host/runtime_capabilities.inc"
#include "modules/catch_spin/coordinator.inc"
// The runtime-core module owns START/STOP/DIAGNOSTIC; the lifecycle handlers are
// defined in runtime_control.inc (below). Forward-declare them so the module can
// reference them from runtime_feature_modules.inc.
bool activate_runtime(ProbeContext &context, const char *request_id);
bool stop_runtime(ProbeContext &context, const char *request_id);
bool run_runtime_control_diagnostic(ProbeContext &context, const char *request_id);
#include "shared/runtime/control/runtime_map_readiness.inc"
#include "shared/runtime/control/runtime_desired_state.inc"
#include "shared/runtime/module/runtime_feature_modules.inc"
#include "shared/runtime/control/runtime_desired_state_reconcile.inc"
#include "shared/bridge_kotlin/runtime_ui_status_bridge.inc"
#include "shared/runtime/control/runtime_control.inc"
}  // namespace

#include "shared/core/runtime_module.inc"
