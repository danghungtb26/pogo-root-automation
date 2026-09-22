// Per-module observation (root orchestrates, modules trigger). The shared
// observer thread builds this once per tick and passes it to each enabled module's
// observe() hook, which reads its own concern and decides its own cadence.
struct ObserverTickContext {
    ProbeContext &context;
    const RuntimeBinding &binding;
    uint64_t tick = 0U;
};

namespace pogo_runtime_module {
void observe_enabled_modules(ObserverTickContext &tick);
void sync_auto_enabled_modules(ProbeContext &context);
bool map_binding_ready(const RuntimeBinding &binding);
bool map_runtime_ready(ProbeContext &context);
bool probe_map_live(ProbeContext &context);
void reset_map_live_verified(ProbeContext &context);
}  // namespace pogo_runtime_module

bool read_runtime_map_snapshot_on_main_thread(
    ProbeContext &context,
    pogo_runtime::RuntimeNearbyObservation *observation,
    pogo_runtime::RuntimeFortsObservation *forts
);

bool read_runtime_inventory(
    const Il2CppApi &api,
    const RuntimeBinding &binding,
    pogo_runtime::RuntimeInventoryObservation *observation
);

bool request_main_thread_player_position(
    ProbeContext &context,
    double *latitude,
    double *longitude
);

void *find_active_map_pokemon_by_id(ProbeContext &context, uint64_t encounter_id);

bool read_runtime_map_forts(
    const Il2CppApi &api,
    const RuntimeBinding &binding,
    pogo_runtime::RuntimeFortsObservation *observation
);

bool read_runtime_player_position(
    const Il2CppApi &api,
    const RuntimeBinding &binding,
    double *latitude,
    double *longitude
);

void discover_direct_map_bindings(
    const Il2CppApi &api,
    const Il2CppAssembly **assemblies,
    size_t assembly_count,
    RuntimeBinding *binding
);

bool read_encounter_from_wild_map_pokemon(
    const Il2CppApi &api,
    void *map_pokemon,
    pogo_runtime::RuntimeEncounterObservation *observation
);

MainThreadActionOutcome open_runtime_encounter_on_main_thread(
    ProbeContext &context,
    uint64_t encounter_id
);

MainThreadActionOutcome throw_runtime_ball_on_main_thread(
    ProbeContext &context,
    uint64_t encounter_id
);

MainThreadActionOutcome request_main_thread_catch(
    ProbeContext &context,
    uint64_t encounter_id
);

MainThreadActionOutcome request_main_thread_use_berry(
    ProbeContext &context,
    uint64_t encounter_id,
    int32_t item_id
);

MainThreadActionOutcome request_main_thread_direct_catch(
    ProbeContext &context,
    uint64_t encounter_id
);

MainThreadActionOutcome request_main_thread_poll_direct_catch(ProbeContext &context);

MainThreadActionOutcome request_main_thread_poll_spin(ProbeContext &context);

MainThreadActionOutcome request_main_thread_poll_encounter(
    ProbeContext &context,
    uint64_t encounter_id
);

uint64_t runtime_pending_encounter_id(ProbeContext &context);

MainThreadActionOutcome poll_runtime_encounter_on_main_thread(
    ProbeContext &context,
    uint64_t encounter_id
);

enum class RuntimeDirectCatchPromisePollResult : uint32_t {
    kNoPending = 0U,
    kWaiting = 1U,
    kCompleted = 2U,
    kFailed = 3U,
    kTimedOut = 4U,
};

enum class RuntimeTransferPromisePollResult : uint32_t {
    kNoPending = 0U,
    kWaiting = 1U,
    kCompleted = 2U,
    kFailed = 3U,
    kTimedOut = 4U,
};

enum class RuntimeDiscardPromisePollResult : uint32_t {
    kNoPending = 0U,
    kWaiting = 1U,
    kCompleted = 2U,
    kFailed = 3U,
    kTimedOut = 4U,
};

enum class RuntimeSpinPromisePollResult : uint32_t {
    kNoPending = 0U,
    kWaiting = 1U,
    kCompleted = 2U,
    kFailed = 3U,
    kTimedOut = 4U,
};

bool begin_pending_runtime_spin(
    ProbeContext &context,
    const std::string &fort_id,
    const char *command_id
);

bool runtime_spin_is_pending(ProbeContext &context);

void abandon_pending_runtime_spin(ProbeContext &context, const char *reason);

bool retain_pending_runtime_spin(
    ProbeContext &context,
    const std::string &fort_id,
    void *spinner,
    void *promise
);

RuntimeSpinPromisePollResult poll_pending_runtime_spin_on_main_thread(
    ProbeContext &context
);

bool consume_terminal_runtime_spin(
    ProbeContext &context,
    std::string *fort_id,
    int32_t *server_result,
    int64_t *cooldown_complete_ms
);

void reset_runtime_spin_state(ProbeContext &context);

bool begin_pending_direct_catch(
    ProbeContext &context,
    const std::string &command_id,
    uint64_t encounter_id
);

bool runtime_direct_catch_is_pending(ProbeContext &context);

void abandon_pending_direct_catch(ProbeContext &context, const char *reason);

bool retain_pending_direct_catch(
    ProbeContext &context,
    uint64_t encounter_id,
    void *promise
);


RuntimeDirectCatchPromisePollResult poll_pending_direct_catch_on_main_thread(
    ProbeContext &context
);

bool consume_terminal_pending_direct_catch(
    ProbeContext &context,
    PendingDirectCatch *terminal
);

// Transfer module entry points. The implementations are included after the
// catch Promise observer so catch-spin can emit a native success event without
// owning transfer policy or Promise lifecycle.
void on_runtime_catch_succeeded_on_main_thread(
    ProbeContext &context,
    uint64_t captured_pokemon_id
);

RuntimeTransferPromisePollResult poll_pending_runtime_transfer_on_main_thread(
    ProbeContext &context
);

MainThreadActionOutcome start_runtime_transfer_on_main_thread(
    ProbeContext &context,
    uint64_t pokemon_id,
    const char *command_id
);

MainThreadActionOutcome request_main_thread_poll_runtime_transfer(ProbeContext &context);

MainThreadActionOutcome request_main_thread_start_runtime_transfer(
    ProbeContext &context,
    uint64_t pokemon_id,
    const char *command_id
);

bool runtime_transfer_blocks_catch_spin();
bool runtime_transfer_has_pending();
void reset_runtime_transfer_state(ProbeContext &context);

RuntimeDiscardPromisePollResult poll_pending_runtime_discard_on_main_thread(
    ProbeContext &context
);

MainThreadActionOutcome start_runtime_discard_on_main_thread(
    ProbeContext &context,
    int32_t item_id,
    int32_t amount,
    const char *command_id
);

MainThreadActionOutcome request_main_thread_poll_runtime_discard(ProbeContext &context);

MainThreadActionOutcome request_main_thread_start_runtime_discard(
    ProbeContext &context,
    int32_t item_id,
    int32_t amount,
    const char *command_id
);

bool runtime_discard_blocks_catch_spin();
bool runtime_discard_has_pending();
void reset_runtime_discard_state(ProbeContext &context);

MainThreadActionOutcome request_main_thread_spin(
    ProbeContext &context,
    const std::string &fort_id
);

MainThreadActionOutcome use_runtime_berry_on_main_thread(
    ProbeContext &context,
    uint64_t encounter_id,
    int32_t item_id
);

MainThreadActionOutcome direct_catch_runtime_on_main_thread(
    ProbeContext &context,
    uint64_t encounter_id
);

MainThreadActionOutcome spin_runtime_pokestop_on_main_thread(
    ProbeContext &context,
    const std::string &fort_id
);

bool refresh_runtime_encounter_owners(ProbeContext &context);

bool bind_runtime_throw_methods(
    const Il2CppApi &api,
    const Il2CppAssembly **assemblies,
    size_t assembly_count,
    RuntimeBinding *binding
);

bool install_runtime_throw_hooks(ProbeContext &context);

jobject load_controller_bridge_class(JNIEnv *env);
void *request_main_thread_object(ProbeContext &, const char *, const char *);

void *find_unity_object_on_main_thread(
    ProbeContext &context,
    const char *name_space,
    const char *class_name
);

void diagnose_map_pokemon_instance(ProbeContext &context);

bool verify_tap_event_contract(
    const Il2CppApi &api,
    const Il2CppAssembly **assemblies,
    size_t assembly_count,
    RuntimeBinding *binding
);

void diagnose_tap_gesture_class(
    const Il2CppApi &api,
    const Il2CppAssembly **assemblies,
    size_t assembly_count
);

void diagnose_managed_runtime_candidates(
    const Il2CppApi &api,
    const Il2CppAssembly **assemblies,
    size_t assembly_count,
    const ManagedDiagnosticClass *candidates,
    size_t candidate_count
);
