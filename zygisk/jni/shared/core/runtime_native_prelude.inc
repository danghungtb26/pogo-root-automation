#include <android/log.h>
#include <dlfcn.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <algorithm>
#include <utility>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "zygisk.hpp"
#include "../bridge_kotlin/runtime_command_protocol.h"
#include "../bridge_kotlin/runtime_observation_protocol.h"
#include "../bridge_appproc/runtime_throw_events.h"
namespace {
constexpr const char *kLogTag = "PogoRootAutomation";
constexpr const char *kGooglePlayProcess = "com.nianticlabs.pokemongo";
constexpr const char *kGalaxyProcess = "com.nianticlabs.pokemongo.ares";
constexpr const char *kStateDirectory = "/data/adb/pogo_root_automation";
constexpr const char *kStatePath = "/data/adb/pogo_root_automation/runtime.status";
constexpr const char *kTempStatePrefix = "/data/adb/pogo_root_automation/runtime.status.tmp";
constexpr const char *kBridgeSocketName = "pogo_root_automation_runtime";
constexpr const char *kControllerUidPath = "/data/adb/pogo_root_automation/controller.uids";
constexpr uint32_t kRuntimeEventMagic = 0x504F474FU;
constexpr uint32_t kRuntimeProtocolVersion = 4U;
constexpr uint16_t kBridgeProtocolVersion = 3U;
constexpr uint32_t kBridgeCommandType = pogo_runtime::kBridgeCommandType;
constexpr uint32_t kBridgeCommandResultType = 5U;
constexpr uint32_t kBridgeHardMessageBytes = 4U * 1024U * 1024U;
constexpr uint32_t kRuntimeCommandMagic = 0x504F4743U;
constexpr uint32_t kRuntimeResultMagic = 0x504F4752U;
constexpr uint32_t kRuntimeCapabilitiesMagic = 0x504F474BU;
constexpr uint32_t kRuntimeUiStatusMagic = 0x504F4753U;
// This binding is intentionally pinned to the APK/libil2cpp evidence captured
// on BlueStacks Air 1. Any other runtime stays probe-only.
constexpr const char *kVerifiedBuildFingerprint =
    "com.nianticlabs.pokemongo|2026082702|il2cpp|il2cpp_runtime_invoke_v1|"
    "arm64-v8a|aarch64|native|il2cpp=84ed21009899c84abca01e0b8c6f4b0b054f264e|"
    "metadata=2c7c88033dbdff96ba35cab5bc2d44943b00e8c20874cd1ee1bd8e768e780212|"
    "apk=405a351ce2f89be9b03c0cf7d2ec364ccad619bbc24b1ca064022f18b75674c2";
// Pokémon GO can load Unity before the IL2CPP shared object on a cold
// BlueStacks start. Keep the probe alive long enough to observe the same
// process after that delayed load instead of publishing a false mapped-only
// result.
// Air 1 can finish the first account/map bootstrap after two minutes on a
// cold start. Keep the probe alive through that window so the controller does
// not attach to a permanently probe-only session.
constexpr int kProbeAttempts = 600;
constexpr useconds_t kProbeDelayUs = 500000U;
constexpr uint32_t kRequiredIl2cppCoreSymbolCount = 10U;
constexpr size_t kMaxAssembliesToInspect = 4096U;
constexpr size_t kMaxClassesToInspect = 50000U;
constexpr uint32_t kMaxCandidateClasses = 32U;
uint64_t g_session_counter = 0U;

enum class RuntimeEventType : uint32_t {
    kTargetAttached = 1U,
    kBindingProbe = 2U,
};

enum RuntimeProbeFlags : uint32_t {
    kProbeComplete = 1U << 0U,
    kIl2cppLoaded = 1U << 1U,
    kUnityLoaded = 1U << 2U,
    kIl2cppApiAvailable = 1U << 3U,
    kHoudiniTranslation = 1U << 4U,
    kNdkTranslation = 1U << 5U,
    kAssemblySurveyComplete = 1U << 6U,
    kAssemblyCSharpFound = 1U << 7U,
    kClassSurveyComplete = 1U << 8U,
};

struct RuntimeEvent {
    uint32_t magic;
    uint32_t protocol_version;
    uint32_t event_type;
    int32_t pid;
    uint32_t probe_flags;
    uint32_t il2cpp_symbol_count;
    uint32_t il2cpp_required_symbol_count;
    uint32_t assembly_count;
    uint32_t class_count;
    uint32_t candidate_class_count;
    char process_name[128];
    char il2cpp_path[512];
    char unity_path[512];
    char translation_layer[32];
    char assembly_csharp_name[64];
    char candidate_classes[2048];
};

struct RuntimeState {
    uint32_t protocol_version = kRuntimeProtocolVersion;
    int32_t pid = 0;
    uint32_t probe_flags = 0U;
    uint32_t il2cpp_symbol_count = 0U;
    uint32_t il2cpp_required_symbol_count = kRequiredIl2cppCoreSymbolCount;
    uint32_t assembly_count = 0U;
    uint32_t class_count = 0U;
    uint32_t candidate_class_count = 0U;
    uint64_t runtime_message_seq = 0U;
    char process_name[128]{};
    char runtime_session_id[64]{};
    char il2cpp_path[512]{};
    char unity_path[512]{};
    char translation_layer[32]{};
    char assembly_csharp_name[64]{};
    char candidate_classes[2048]{};
};

struct BrokerContext {
    int32_t pid;
    int runtime_fd;
    char process_name[128];
    char runtime_session_id[64];
    uint64_t next_message_seq = 1U;
    // Only the latest UI status is retained for reconnect replay. Runtime
    // gameplay/config commands are never queued while a controller is absent.
    std::vector<uint8_t> latest_runtime_ui_status;
};

std::mutex g_broker_mutex;
std::mutex g_internal_message_mutex;
bool g_broker_running = false;
std::string g_broker_session_id;
std::atomic<bool> g_runtime_explicit_snapshot_mode{false};

#include "runtime_il2cpp_api.inc"
struct RuntimeBinding {
    void *item_bag = nullptr;
    const void *use_item_on_pokemon = nullptr;
    void *encounter_pokemon = nullptr;
    void *encounter_state = nullptr;
    void *encounter_interaction = nullptr;
    const void *encounter_interaction_pokeball_in_play = nullptr;
    const void *pokeball_launch = nullptr, *pokeball_bsuz = nullptr;
    const void *encounter_attempt_capture = nullptr, *encounter_throw_missed = nullptr;
    bool throw_hook_binding_verified = false;
    bool throw_hooks_installed = false;
    void *encounter_gui = nullptr;
    void *pokeball_service = nullptr;
    const void *pokeball_service_throw = nullptr;
    void *map_entity_service = nullptr;
    void *map_entity_cells = nullptr;
    const void *map_entity_get_cells = nullptr;
    void *map_place_directory_service = nullptr;
    void *map_place_directory_pokestops = nullptr;
    const void *map_place_directory_get_active_pokestops = nullptr;
    const void *map_place_directory_get_pokestop = nullptr;
    const void *map_pokestop_get_id = nullptr;
    const void *map_pokestop_get_location = nullptr;
    const void *map_pokestop_get_is_cooling_down = nullptr;
    const void *map_pokestop_set_cooldown_time = nullptr;
    const void *map_pokestop_start_interactive = nullptr;
    const void *map_pokestop_interactive_item_spinner = nullptr;
    const void *poi_item_spinner_spin = nullptr;
    // PoiItemSpinner.ckko() -> IPromise<FortSearchOutProto> (build 0.427.0).
    // This is the actual FortSearch sender; Spin(float) only starts physics.
    const void *poi_item_spinner_request_items = nullptr;
    // PoiItemSpinner.ckkq(FortSearchOutProto), the game's result callback used
    // to update spinner/UI state after a successful FortSearch response.
    const void *poi_item_spinner_result_callback = nullptr;
    // Native spin item bubble VFX via IAwardItemBubbleService (build 0.427.0).
    void *award_item_bubble_service = nullptr;
    Il2CppClass *award_item_bubble_service_class = nullptr;
    Il2CppClass *award_bubble_data_list_class = nullptr;
    const void *award_bubble_gather_fort_search = nullptr;
    const void *award_bubble_spawn_bubbles = nullptr;
    const void *award_bubble_world_to_canvas = nullptr;
    const void *award_bubble_add_child = nullptr;
    const void *award_bubble_set_visibility = nullptr;
    const void *award_bubble_gather_candy = nullptr;
    const void *award_bubble_gather_xl_candy = nullptr;
    Il2CppClass *loot_proto_class = nullptr;
    Il2CppClass *loot_item_proto_class = nullptr;
    const void *map_avatar_get_position = nullptr;
    const void *map_place_get_transform = nullptr;
    const void *unity_transform_get_position = nullptr;
    bool spin_item_bubbles_verified = false;
    bool transfer_candy_bubbles_verified = false;
    const void *map_pokemon_try_capture = nullptr;
    const void *map_pokemon_get_spawn_point_id = nullptr;
    // WildMapPokemon.SendEncounterRequest (RPC Encounter=102). Fired in the
    // background before a direct catch so the server has an open encounter; it is
    // a pure RPC sender and does not open the encounter UI.
    const void *map_pokemon_send_encounter_request = nullptr;
    bool background_encounter_verified = false;
    // MapContentHandler.RegisterPokemonCaughtOrFled(ulong) — map synchronization
    // after a terminal catch outcome (does NOT itself distinguish caught vs fled).
    const void *map_content_handler_register_caught_or_fled = nullptr;
    Il2CppClass *ar_plus_encounter_values_class = nullptr;
    Il2CppClass *catch_pokemon_proto_class = nullptr;
    void *soft_sfida_rpc_handler = nullptr;
    const void *soft_sfida_capture_rpc = nullptr;
    Il2CppClass *soft_sfida_capture_out_proto_class = nullptr;
    bool soft_sfida_capture_verified = false;
    bool soft_sfida_outcome_observer_verified = false;
    const void *map_pokemon_on_tapped = nullptr;
    const void *unity_find_objects_of_type = nullptr;
    void *map_scene_view_service = nullptr;
    void *map_scene_context = nullptr;
    void *map_content_handler = nullptr;
    void *dynamic_tappables_service = nullptr;
    const void *dynamic_tappables_tap = nullptr;
    bool use_berry_verified = false;
    bool map_pokemon_tap_event_verified = false;
    bool map_entity_read_verified = false;
    bool encounter_read_verified = false;
    bool catch_throw_verified = false;
    bool direct_catch_verified = false;
    bool direct_catch_outcome_verified = false;
    bool forts_read_verified = false;
    bool spin_verified = false;
    bool exact_build_verified = false;
    // Player position (Niantic.Holoholo.ILocationProvider -> NativeLocationProvider).
    // The concrete provider exposes get_Location() -> LatLng and
    // get_HasValidLocation(); discovery resolves both through the concrete type
    // and its parents/explicit interface methods.
    void *location_provider = nullptr;
    const void *location_get_location = nullptr;
    const void *location_get_has_valid = nullptr;
    bool location_read_verified = false;
    // Inventory read (Niantic.Holoholo.IItemBag -> ItemBagImpl.GetItemCount(Item)).
    // item_bag is resolved in the IItemBag service block. Enables READ_INVENTORY
    // and populates AutomationSnapshot.inventory for auto-discard / ball gating.
    const void *item_bag_get_item_count = nullptr;
    const void *item_bag_get_total_items = nullptr;
    bool inventory_read_verified = false;
    // InventoryCache is the freshness boundary for discard. GetItemCount alone
    // is a local read and cannot authorize a mutation after a reconnect.
    void *inventory_cache = nullptr;
    const void *inventory_cache_get_latest_timestamp = nullptr;
    const void *inventory_cache_update_inventory = nullptr;
    bool inventory_cache_read_verified = false;
    uint64_t owner_generation = 0U;
    void *player_service = nullptr;
    const void *player_get_bag_is_full = nullptr;
    const void *player_get_max_items = nullptr;
    bool inventory_full_read_verified = false;
    // Discard executor (IItemBag.RecycleItem(ItemData, int, ISet<Item>)). ItemData
    // is populated on the main thread; expiringItemsCopy comes from IItemInventoryService.
    const void *item_bag_recycle_item = nullptr;
    Il2CppClass *item_data_class = nullptr;
    const void *item_data_ctor = nullptr;
    Il2CppClass *medicine_data_class = nullptr;
    const void *medicine_data_ctor = nullptr;
    Il2CppClass *project_context_class = nullptr;
    Il2CppClass *di_container_class = nullptr;
    const void *di_try_resolve = nullptr;
    Il2CppClass *item_inventory_service_class = nullptr;
    Il2CppClass *item_inventory_service_impl_class = nullptr;
    const void *item_inventory_list_sorted = nullptr;
    void *item_inventory_filter_delegate = nullptr;
    void *item_inventory_service = nullptr;
    const void *item_inventory_get_expiring_items = nullptr;
    Il2CppClass *hash_set_item_class = nullptr;
    const void *hash_set_item_ctor = nullptr;
    const void *hash_set_item_supplier = nullptr;
    bool discard_verified = false;
    // Transfer executor. Per the 0.427.0 dump, Niantic.Holoholo.Internal
    // .PokemonBagImpl (IPokemonBag) exposes:
    //   PokemonProto GetPokemon(ulong id)
    //   IPromise<ReleasePokemonOutProto> ReleasePokemon(PokemonProto pokemon)
    // Release is UI-independent, so the executor resolves the proto by storage id
    // then releases it. Execution stays gated behind kTransferExecutionEnabled
    // until the promise post-condition is verified on device, like the discard
    // path.
    void *pokemon_inventory = nullptr;
    const void *pokemon_inventory_get_pokemon = nullptr;
    const void *pokemon_inventory_release = nullptr;
    bool transfer_verified = false;
    void *game_master_data = nullptr;
    const void *game_master_get_item_settings = nullptr;
    const void *pokemon_proto_is_legendary = nullptr;
    const void *pokemon_proto_is_mythic = nullptr;
};

enum class MainThreadTaskType : uint32_t {
    kFindObject = 1U,
    kReadMap = 2U,
    kOpenEncounter = 3U,
    kCatch = 4U,
    kUseBerry = 5U,
    kDirectCatch = 6U,
    kSpin = 7U,
    kReadInventory = 8U,
    kReadPlayerPosition = 9U,
    kPollDirectCatch = 10U,
    kPollTransfer = 11U,
    kStartTransfer = 12U,
    kPollEncounter = 13U,
    kPollSpin = 14U,
    kPollDiscard = 15U,
    kStartDiscard = 16U,
};

enum class MainThreadActionOutcome : uint32_t {
    kRejected = 0U,
    kInvoked = 1U,
    kIndeterminate = 2U,
    kOutOfBalls = 3U,
    // Direct map catch needs a server-side encounter before CatchPokemon (103) is
    // accepted. When the target has no EncounterOutProto yet, the background
    // SendEncounterRequest (102) is fired and this non-terminal outcome asks the
    // controller to retry the same target on a later cycle (no ball is thrown).
    kEncounterRequested = 4U,
};

struct MainThreadBridge {
    JavaVM *java_vm = nullptr;
    jobject bridge_class = nullptr;
    jmethodID post_method = nullptr;
    std::mutex mutex;
    std::condition_variable condition;
    uint64_t next_token = 1U;
    uint64_t pending_token = 0U;
    MainThreadTaskType pending_type = MainThreadTaskType::kFindObject;
    std::string pending_namespace;
    std::string pending_class_name;
    void *pending_result = nullptr;
    uint64_t pending_encounter_id = 0U;
    std::string pending_fort_id;
    uint64_t pending_pokemon_id = 0U;
    std::string pending_transfer_command_id;
    int32_t pending_item_id = 0;
    int32_t pending_discard_amount = 0;
    std::string pending_discard_command_id;
    pogo_runtime::RuntimeNearbyObservation pending_nearby;
    pogo_runtime::RuntimeFortsObservation pending_forts;
    pogo_runtime::RuntimeInventoryObservation pending_inventory;
    double pending_player_latitude = 0.0;
    double pending_player_longitude = 0.0;
    bool pending_has_player_position = false;
    bool pending_done = false;
    bool pending_success = false;
    MainThreadActionOutcome pending_action_outcome = MainThreadActionOutcome::kRejected;
    bool ready = false;
};

struct PendingDirectCatch {
    bool active = false;
    bool initial_result_published = false;
    bool terminal = false;
    uint64_t encounter_id = 0U;
    std::string command_id;
    Il2CppGcHandle promise_gc_handle = 0U;
    uint64_t requested_at_ns = 0U;
    uint32_t poll_count = 0U;
    uint32_t phase = 0U;
    uint32_t catch_outcome_wire = 0U;
    std::string captured_pokemon_id;
    std::string error_code;
    std::string message;
};

struct PendingEncounterRequest {
    bool active = false;
    bool promise_completed = false;
    uint64_t encounter_id = 0U;
    Il2CppGcHandle promise_gc_handle = 0U;
    void *completed_value = nullptr;
    uint64_t requested_at_ns = 0U;
    uint32_t poll_count = 0U;
};

struct MapDictionaryEntry {
    uint64_t key = 0U;
    void *key_object = nullptr;
    void *value = nullptr;
};

bool enumerate_map_dictionary(
    const Il2CppApi &api,
    void *dictionary,
    std::vector<MapDictionaryEntry> *entries
);

bool runtime_object_is_exact_class(
    const Il2CppApi &api,
    void *object,
    const char *expected_namespace,
    const char *expected_name
);

struct ProbeContext {
    int fd;
    int32_t pid;
    char process_name[128];
    char il2cpp_path[512];
    Il2CppApi il2cpp_api;
    RuntimeBinding binding;
    std::mutex binding_mutex;
    RuntimeThrowEventQueue throw_events;
    std::atomic<uint64_t> active_encounter_id{0U};
    bool il2cpp_api_ready = false;
    std::string last_action_command_id;
    MainThreadBridge main_thread_bridge;
    std::mutex pending_direct_catch_mutex;
    PendingDirectCatch pending_direct_catch;
    std::mutex pending_encounter_request_mutex;
    PendingEncounterRequest pending_encounter_request;
    std::atomic<bool> observation_running{false};
    pthread_t observation_thread{};
    bool observation_thread_started = false;
    // Dirty-flag for the legacy push-observer path: set by the periodic fallback
    // tick so the observer re-reads the map only when needed (plus a rare
    // reconcile). Unused in native module-owned snapshot mode. Starts true so the first
    // cycle always reads.
    std::atomic<bool> world_dirty{true};
    // Set only after a successful main-thread map snapshot with a valid player
    // position. Binding verification alone is not enough for module start or
    // automation-event toasts — MapEntityService can exist before overworld.
    std::atomic<bool> map_live_verified{false};
};

RuntimeBinding runtime_binding_snapshot(ProbeContext &context) {
    std::lock_guard<std::mutex> lock(context.binding_mutex);
    return context.binding;
}
