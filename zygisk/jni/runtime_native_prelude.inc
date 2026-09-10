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

#include "zygisk.hpp"
#include "runtime_command_protocol.h"
#include "runtime_observation_protocol.h"
#include "runtime_throw_events.h"
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
constexpr uint16_t kBridgeProtocolVersion = 2U;
constexpr uint32_t kBridgeCommandType = pogo_runtime::kBridgeCommandType;
constexpr uint32_t kBridgeCommandResultType = 5U;
constexpr uint32_t kBridgeHardMessageBytes = 4U * 1024U * 1024U;
constexpr uint32_t kRuntimeCommandMagic = 0x504F4743U;
constexpr uint32_t kRuntimeResultMagic = 0x504F4752U;
constexpr uint32_t kRuntimeCapabilitiesMagic = 0x504F474BU;
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
};

std::mutex g_broker_mutex;
std::mutex g_internal_message_mutex;
bool g_broker_running = false;
std::string g_broker_session_id;

struct Il2CppDomain;
struct Il2CppAssembly;
struct Il2CppImage;
struct Il2CppThread;
struct Il2CppClass;
struct Il2CppType;
struct ManagedDiagnosticClass;

using Il2CppDomainGet = Il2CppDomain *(*)();
using Il2CppThreadCurrent = Il2CppThread *(*)();
using Il2CppThreadAttach = Il2CppThread *(*)(Il2CppDomain *);
using Il2CppThreadDetach = void (*)(Il2CppThread *);
using Il2CppDomainGetAssemblies = const Il2CppAssembly **(*)(const Il2CppDomain *, size_t *);
using Il2CppAssemblyGetImage = const Il2CppImage *(*)(const Il2CppAssembly *);
using Il2CppImageGetName = const char *(*)(const Il2CppImage *);
using Il2CppClassFromName = Il2CppClass *(*)(const Il2CppImage *, const char *, const char *);
using Il2CppClassGetFieldFromName = void *(*)(Il2CppClass *, const char *);
using Il2CppFieldGetValue = void (*)(void *, void *, void *);
using Il2CppObjectGetClass = Il2CppClass *(*)(void *);
using Il2CppObjectUnbox = void *(*)(void *);
using Il2CppArrayLength = size_t (*)(void *);
using Il2CppArrayGetByteLength = size_t (*)(void *);
using Il2CppImageGetClassCount = size_t (*)(const Il2CppImage *);
using Il2CppImageGetClass = Il2CppClass *(*)(const Il2CppImage *, size_t);
using Il2CppClassGetName = const char *(*)(Il2CppClass *);
using Il2CppClassGetNamespace = const char *(*)(Il2CppClass *);
using Il2CppClassGetMethods = const void *(*)(Il2CppClass *, void **);
using Il2CppMethodGetName = const char *(*)(const void *);
using Il2CppClassGetMethodFromName = const void *(*)(Il2CppClass *, const char *, int);
using Il2CppMethodGetParamCount = uint32_t (*)(const void *);
using Il2CppMethodGetParam = const Il2CppType *(*)(const void *, uint32_t);
using Il2CppMethodGetReturnType = const Il2CppType *(*)(const void *);
using Il2CppMethodIsInstance = bool (*)(const void *);
using Il2CppTypeGetName = const char *(*)(const Il2CppType *);
using Il2CppClassGetFields = const void *(*)(Il2CppClass *, void **);
using Il2CppFieldGetFlags = uint32_t (*)(const void *);
using Il2CppFieldGetName = const char *(*)(const void *);
using Il2CppFieldGetType = const Il2CppType *(*)(const void *);
using Il2CppFieldGetOffset = int32_t (*)(const void *);
using Il2CppFieldStaticGetValue = void (*)(const void *, void *);
using Il2CppTypeGetClassOrElementClass = Il2CppClass *(*)(const Il2CppType *);
using Il2CppClassGetType = const Il2CppType *(*)(Il2CppClass *);
using Il2CppTypeGetObject = void *(*)(const Il2CppType *);
using Il2CppObjectNew = void *(*)(Il2CppClass *);
using Il2CppRuntimeObjectInit = void (*)(void *);
using Il2CppRuntimeClassInit = void (*)(Il2CppClass *);
using Il2CppRuntimeInvoke = void *(*)(const void *, void *, void **, void **);
using Il2CppStringNew = void *(*)(const char *);

struct Il2CppApi {
    void *handle = nullptr;
    bool owns_handle = false;
    uint32_t symbol_count = 0U;
    Il2CppDomainGet domain_get = nullptr;
    Il2CppThreadCurrent thread_current = nullptr;
    Il2CppThreadAttach thread_attach = nullptr;
    Il2CppThreadDetach thread_detach = nullptr;
    Il2CppDomainGetAssemblies domain_get_assemblies = nullptr;
    Il2CppAssemblyGetImage assembly_get_image = nullptr;
    Il2CppImageGetName image_get_name = nullptr;
    Il2CppClassFromName class_from_name = nullptr;
    Il2CppClassGetFieldFromName class_get_field_from_name = nullptr;
    Il2CppFieldGetValue field_get_value = nullptr;
    Il2CppObjectGetClass object_get_class = nullptr;
    Il2CppObjectUnbox object_unbox = nullptr;
    Il2CppArrayLength array_length = nullptr;
    Il2CppArrayGetByteLength array_get_byte_length = nullptr;
    Il2CppImageGetClassCount image_get_class_count = nullptr;
    Il2CppImageGetClass image_get_class = nullptr;
    Il2CppClassGetName class_get_name = nullptr;
    Il2CppClassGetNamespace class_get_namespace = nullptr;
    Il2CppClassGetMethods class_get_methods = nullptr;
    Il2CppMethodGetName method_get_name = nullptr;
    Il2CppClassGetMethodFromName class_get_method_from_name = nullptr;
    Il2CppMethodGetParamCount method_get_param_count = nullptr;
    Il2CppMethodGetParam method_get_param = nullptr;
    Il2CppMethodGetReturnType method_get_return_type = nullptr;
    Il2CppMethodIsInstance method_is_instance = nullptr;
    Il2CppTypeGetName type_get_name = nullptr;
    Il2CppClassGetFields class_get_fields = nullptr;
    Il2CppFieldGetFlags field_get_flags = nullptr;
    Il2CppFieldGetName field_get_name = nullptr;
    Il2CppFieldGetType field_get_type = nullptr;
    Il2CppFieldGetOffset field_get_offset = nullptr;
    Il2CppFieldStaticGetValue field_static_get_value = nullptr;
    Il2CppTypeGetClassOrElementClass type_get_class_or_element_class = nullptr;
    Il2CppClassGetType class_get_type = nullptr;
    Il2CppTypeGetObject type_get_object = nullptr;
    Il2CppObjectNew object_new = nullptr;
    Il2CppRuntimeObjectInit runtime_object_init = nullptr;
    Il2CppRuntimeClassInit runtime_class_init = nullptr;
    Il2CppRuntimeInvoke runtime_invoke = nullptr;
    Il2CppStringNew string_new = nullptr;
};

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
    const void *map_pokestop_start_interactive = nullptr;
    const void *map_pokestop_interactive_item_spinner = nullptr;
    const void *poi_item_spinner_spin = nullptr;
    const void *map_pokemon_try_capture = nullptr;
    const void *map_pokemon_get_spawn_point_id = nullptr;
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
    // Mode B: true once the map-query batch callback is inline-hooked so it marks
    // the world dirty. While false, the observer uses a periodic dirty-tick
    // fallback (mode-C cadence). Installing the hook needs a build-specific
    // prologue captured from the live libil2cpp.so; see docs/architecture.
    bool map_query_hook_installed = false;
    // Player position (Niantic.Holoholo.ILocationProvider -> LocationProviderAdapter).
    // get_CurrentLocation returns a Niantic.Platform.Ditto.Geo.Location value
    // struct exposing LatitudeDeg/LongitudeDeg (double).
    void *location_provider = nullptr;
    const void *location_get_current = nullptr;
    const void *location_get_latitude = nullptr;
    const void *location_get_longitude = nullptr;
    bool location_read_verified = false;
    // Map-query batch callback (Niantic.Platform.GameMapObject.Map.MapQueryManager
    // .OnMapQueryResponse) used as the mode-B dirty trigger. Resolved to a method
    // for inline hooking; see modules/catch_spin/map_hooks.inc.
    const void *map_query_on_response = nullptr;
    bool map_query_hook_binding_verified = false;
    // Inventory read (Niantic.Holoholo.IItemBag -> ItemBagImpl.GetItemCount(Item)).
    // item_bag is resolved in the IItemBag service block. Enables READ_INVENTORY
    // and populates AutomationSnapshot.inventory for auto-discard / ball gating.
    const void *item_bag_get_item_count = nullptr;
    bool inventory_read_verified = false;
    // Discard executor (IItemBag.RecycleItem(ItemData, int, ISet<Item>)). ItemData
    // is a plain data class constructed by object_new + field writes. Execution is
    // gated behind kDiscardExecutionEnabled until verified on device.
    const void *item_bag_recycle_item = nullptr;
    Il2CppClass *item_data_class = nullptr;
    bool discard_verified = false;
};

enum class MainThreadTaskType : uint32_t {
    kFindObject = 1U,
    kReadMap = 2U,
    kOpenEncounter = 3U,
    kCatch = 4U,
    kUseBerry = 5U,
    kDirectCatch = 6U,
    kSpin = 7U,
};

enum class MainThreadActionOutcome : uint32_t {
    kRejected = 0U,
    kInvoked = 1U,
    kIndeterminate = 2U,
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
    int32_t pending_item_id = 0;
    pogo_runtime::RuntimeNearbyObservation pending_nearby;
    pogo_runtime::RuntimeFortsObservation pending_forts;
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
    uint32_t phase = 0U;
    uint32_t catch_outcome_wire = 0U;
    std::string error_code;
    std::string message;
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
    std::atomic<bool> observation_running{false};
    pthread_t observation_thread{};
    bool observation_thread_started = false;
    // Mode B (dirty-flag) observation: set when a map-change signal indicates the
    // world may have changed. The observer re-reads the map only when this is set
    // (plus a rare reconcile). Starts true so the first cycle always reads.
    std::atomic<bool> world_dirty{true};
};

RuntimeBinding runtime_binding_snapshot(ProbeContext &context) {
    std::lock_guard<std::mutex> lock(context.binding_mutex);
    return context.binding;
}

bool read_runtime_map_snapshot_on_main_thread(
    ProbeContext &context,
    pogo_runtime::RuntimeNearbyObservation *observation,
    pogo_runtime::RuntimeFortsObservation *forts
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
