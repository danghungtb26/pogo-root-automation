#include "maintenance_runtime.h"

#include <android/log.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <map>
#include <string>
#include <vector>

namespace {
constexpr const char *kLogTag = "PogoMaintenance";
constexpr int kProtocol = 1;
constexpr int kBindAttempts = 120;
constexpr useconds_t kBindDelayUs = 500000U;
constexpr useconds_t kLoopDelayUs = 250000U;
constexpr int kMaxPokemon = 5000;

const int kTrackedItems[] = {
    1, 2, 3,
    101, 102, 103, 104,
    201, 202,
    301,
    401,
    501, 502, 503, 504,
    701, 703, 705, 706, 708, 709,
    1101, 1102, 1103, 1104, 1105, 1106, 1107,
    1201, 1202, 1203, 1204,
    1301, 1404,
};

struct Il2CppDomain;
struct Il2CppAssembly;
struct Il2CppImage;
struct Il2CppThread;
struct Il2CppClass;
struct Il2CppObject;
struct Il2CppException;
struct Il2CppType;
struct MethodInfo;
struct FieldInfo;

using DomainGet = Il2CppDomain *(*)();
using ThreadAttach = Il2CppThread *(*)(Il2CppDomain *);
using ThreadDetach = void (*)(Il2CppThread *);
using DomainGetAssemblies = const Il2CppAssembly **(*)(const Il2CppDomain *, size_t *);
using AssemblyGetImage = const Il2CppImage *(*)(const Il2CppAssembly *);
using ImageGetName = const char *(*)(const Il2CppImage *);
using ImageGetClassCount = size_t (*)(const Il2CppImage *);
using ImageGetClass = Il2CppClass *(*)(const Il2CppImage *, size_t);
using ClassGetName = const char *(*)(Il2CppClass *);
using ClassGetNamespace = const char *(*)(Il2CppClass *);
using ClassGetMethodFromName = const MethodInfo *(*)(Il2CppClass *, const char *, int);
using ClassGetMethods = const MethodInfo *(*)(Il2CppClass *, void **);
using MethodGetName = const char *(*)(const MethodInfo *);
using MethodGetParamCount = uint32_t (*)(const MethodInfo *);
using MethodGetParam = const Il2CppType *(*)(const MethodInfo *, uint32_t);
using MethodGetReturnType = const Il2CppType *(*)(const MethodInfo *);
using TypeGetName = char *(*)(const Il2CppType *);
using RuntimeInvoke = Il2CppObject *(*)(const MethodInfo *, void *, void **, Il2CppException **);
using ObjectUnbox = void *(*)(Il2CppObject *);
using ObjectGetClass = Il2CppClass *(*)(Il2CppObject *);
using ClassGetFieldFromName = FieldInfo *(*)(Il2CppClass *, const char *);
using ClassGetFields = FieldInfo *(*)(Il2CppClass *, void **);
using FieldGetName = const char *(*)(FieldInfo *);
using FieldGetType = const Il2CppType *(*)(FieldInfo *);
using FieldGetValue = void (*)(Il2CppObject *, FieldInfo *, void *);
using Il2CppFree = void (*)(void *);

struct Api {
    void *handle = nullptr;
    DomainGet domain_get = nullptr;
    ThreadAttach thread_attach = nullptr;
    ThreadDetach thread_detach = nullptr;
    DomainGetAssemblies domain_get_assemblies = nullptr;
    AssemblyGetImage assembly_get_image = nullptr;
    ImageGetName image_get_name = nullptr;
    ImageGetClassCount image_get_class_count = nullptr;
    ImageGetClass image_get_class = nullptr;
    ClassGetName class_get_name = nullptr;
    ClassGetNamespace class_get_namespace = nullptr;
    ClassGetMethodFromName class_get_method_from_name = nullptr;
    ClassGetMethods class_get_methods = nullptr;
    MethodGetName method_get_name = nullptr;
    MethodGetParamCount method_get_param_count = nullptr;
    MethodGetParam method_get_param = nullptr;
    MethodGetReturnType method_get_return_type = nullptr;
    TypeGetName type_get_name = nullptr;
    RuntimeInvoke runtime_invoke = nullptr;
    ObjectUnbox object_unbox = nullptr;
    ObjectGetClass object_get_class = nullptr;
    ClassGetFieldFromName class_get_field_from_name = nullptr;
    ClassGetFields class_get_fields = nullptr;
    FieldGetName field_get_name = nullptr;
    FieldGetType field_get_type = nullptr;
    FieldGetValue field_get_value = nullptr;
    Il2CppFree free_fn = nullptr;
};

struct Bindings {
    Il2CppObject *player_service = nullptr;
    Il2CppObject *item_bag = nullptr;
    Il2CppObject *pokemon_bag = nullptr;
    Il2CppClass *pokemon_proto_class = nullptr;
    Il2CppClass *pokemon_display_class = nullptr;
    const MethodInfo *item_count = nullptr;
    const MethodInfo *recycle_item = nullptr;
    const MethodInfo *pokemon_get_by_id = nullptr;
    const MethodInfo *release_pokemon = nullptr;
    const MethodInfo *pokemon_collection = nullptr;
    FieldInfo *pokemon_collection_field = nullptr;
    bool background_metadata_known = false;
};

struct PokemonMeta {
    uint64_t id = 0U;
    int32_t species = 0;
    int32_t attack = -1;
    int32_t defense = -1;
    int32_t stamina = -1;
    bool shiny = false;
    bool favorite = false;
    bool background = false;
    bool legendary = false;
    bool mythical = false;
    bool complete = false;
};

struct RuntimeContext {
    char process_name[128]{};
};

std::atomic<bool> g_started(false);

template <typename T>
T resolve(void *handle, const char *name) {
    return reinterpret_cast<T>(dlsym(handle, name));
}

bool resolve_api(Api *api) {
    if (api == nullptr) return false;
    api->handle = dlopen("libil2cpp.so", RTLD_NOW | RTLD_NOLOAD);
    if (api->handle == nullptr) return false;
#define R(field, type, symbol) api->field = resolve<type>(api->handle, symbol)
    R(domain_get, DomainGet, "il2cpp_domain_get");
    R(thread_attach, ThreadAttach, "il2cpp_thread_attach");
    R(thread_detach, ThreadDetach, "il2cpp_thread_detach");
    R(domain_get_assemblies, DomainGetAssemblies, "il2cpp_domain_get_assemblies");
    R(assembly_get_image, AssemblyGetImage, "il2cpp_assembly_get_image");
    R(image_get_name, ImageGetName, "il2cpp_image_get_name");
    R(image_get_class_count, ImageGetClassCount, "il2cpp_image_get_class_count");
    R(image_get_class, ImageGetClass, "il2cpp_image_get_class");
    R(class_get_name, ClassGetName, "il2cpp_class_get_name");
    R(class_get_namespace, ClassGetNamespace, "il2cpp_class_get_namespace");
    R(class_get_method_from_name, ClassGetMethodFromName, "il2cpp_class_get_method_from_name");
    R(class_get_methods, ClassGetMethods, "il2cpp_class_get_methods");
    R(method_get_name, MethodGetName, "il2cpp_method_get_name");
    R(method_get_param_count, MethodGetParamCount, "il2cpp_method_get_param_count");
    R(method_get_param, MethodGetParam, "il2cpp_method_get_param");
    R(method_get_return_type, MethodGetReturnType, "il2cpp_method_get_return_type");
    R(type_get_name, TypeGetName, "il2cpp_type_get_name");
    R(runtime_invoke, RuntimeInvoke, "il2cpp_runtime_invoke");
    R(object_unbox, ObjectUnbox, "il2cpp_object_unbox");
    R(object_get_class, ObjectGetClass, "il2cpp_object_get_class");
    R(class_get_field_from_name, ClassGetFieldFromName, "il2cpp_class_get_field_from_name");
    R(class_get_fields, ClassGetFields, "il2cpp_class_get_fields");
    R(field_get_name, FieldGetName, "il2cpp_field_get_name");
    R(field_get_type, FieldGetType, "il2cpp_field_get_type");
    R(field_get_value, FieldGetValue, "il2cpp_field_get_value");
    R(free_fn, Il2CppFree, "il2cpp_free");
#undef R
    return api->domain_get != nullptr && api->thread_attach != nullptr && api->thread_detach != nullptr &&
        api->domain_get_assemblies != nullptr && api->assembly_get_image != nullptr && api->image_get_name != nullptr &&
        api->image_get_class_count != nullptr && api->image_get_class != nullptr && api->class_get_name != nullptr &&
        api->class_get_method_from_name != nullptr && api->class_get_methods != nullptr && api->method_get_name != nullptr &&
        api->method_get_param_count != nullptr && api->method_get_param != nullptr && api->method_get_return_type != nullptr &&
        api->type_get_name != nullptr && api->runtime_invoke != nullptr && api->object_unbox != nullptr &&
        api->object_get_class != nullptr && api->class_get_field_from_name != nullptr && api->class_get_fields != nullptr &&
        api->field_get_name != nullptr && api->field_get_type != nullptr && api->field_get_value != nullptr;
}

char lower_ascii(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
}

bool contains_ci(const std::string &value, const std::string &needle) {
    if (needle.empty()) return false;
    for (size_t start = 0U; start + needle.size() <= value.size(); ++start) {
        bool same = true;
        for (size_t index = 0U; index < needle.size(); ++index) {
            if (lower_ascii(value[start + index]) != lower_ascii(needle[index])) {
                same = false;
                break;
            }
        }
        if (same) return true;
    }
    return false;
}

std::string type_name(Api *api, const Il2CppType *type) {
    if (api == nullptr || type == nullptr || api->type_get_name == nullptr) return std::string();
    char *raw = api->type_get_name(type);
    if (raw == nullptr) return std::string();
    std::string value(raw);
    if (api->free_fn != nullptr) api->free_fn(raw);
    return value;
}

const Il2CppImage *find_image(Api *api, Il2CppDomain *domain, const std::vector<std::string> &names) {
    if (api == nullptr || domain == nullptr) return nullptr;
    size_t count = 0U;
    const Il2CppAssembly **assemblies = api->domain_get_assemblies(domain, &count);
    if (assemblies == nullptr) return nullptr;
    for (size_t index = 0U; index < count; ++index) {
        const Il2CppImage *image = api->assembly_get_image(assemblies[index]);
        if (image == nullptr) continue;
        const char *raw_name = api->image_get_name(image);
        if (raw_name == nullptr) continue;
        const std::string image_name(raw_name);
        for (const std::string &wanted : names) {
            if (image_name == wanted || contains_ci(image_name, wanted)) return image;
        }
    }
    return nullptr;
}

Il2CppClass *find_class(Api *api, const Il2CppImage *image, const char *simple_name) {
    if (api == nullptr || image == nullptr || simple_name == nullptr) return nullptr;
    const size_t count = api->image_get_class_count(image);
    for (size_t index = 0U; index < count; ++index) {
        Il2CppClass *klass = api->image_get_class(image, index);
        if (klass == nullptr) continue;
        const char *name = api->class_get_name(klass);
        if (name != nullptr && strcmp(name, simple_name) == 0) return klass;
    }
    return nullptr;
}

Il2CppObject *invoke(Api *api, const MethodInfo *method, void *instance, void **args, bool *ok) {
    if (ok != nullptr) *ok = false;
    if (api == nullptr || method == nullptr) return nullptr;
    Il2CppException *exception = nullptr;
    Il2CppObject *result = api->runtime_invoke(method, instance, args, &exception);
    if (exception != nullptr) return nullptr;
    if (ok != nullptr) *ok = true;
    return result;
}

bool unbox_i32(Api *api, Il2CppObject *object, int32_t *value) {
    if (api == nullptr || object == nullptr || value == nullptr) return false;
    void *raw = api->object_unbox(object);
    if (raw == nullptr) return false;
    *value = *reinterpret_cast<int32_t *>(raw);
    return true;
}

bool unbox_bool(Api *api, Il2CppObject *object, bool *value) {
    if (api == nullptr || object == nullptr || value == nullptr) return false;
    void *raw = api->object_unbox(object);
    if (raw == nullptr) return false;
    *value = *reinterpret_cast<uint8_t *>(raw) != 0U;
    return true;
}

bool method_param_contains(Api *api, const MethodInfo *method, uint32_t index, const char *needle) {
    if (api == nullptr || method == nullptr || needle == nullptr || index >= api->method_get_param_count(method)) return false;
    return contains_ci(type_name(api, api->method_get_param(method, index)), needle);
}

bool method_return_contains(Api *api, const MethodInfo *method, const char *needle) {
    if (api == nullptr || method == nullptr || needle == nullptr) return false;
    return contains_ci(type_name(api, api->method_get_return_type(method)), needle);
}

const MethodInfo *best_method(Api *api, Il2CppClass *klass, const char *kind) {
    if (api == nullptr || klass == nullptr || kind == nullptr) return nullptr;
    const MethodInfo *best = nullptr;
    int best_score = -1;
    bool tied = false;
    void *iterator = nullptr;
    const MethodInfo *method = nullptr;
    while ((method = api->class_get_methods(klass, &iterator)) != nullptr) {
        const uint32_t params = api->method_get_param_count(method);
        const char *raw_name = api->method_get_name(method);
        const std::string name = raw_name == nullptr ? std::string() : std::string(raw_name);
        int score = -1;
        if (strcmp(kind, "item_count") == 0) {
            if (params == 1U && method_return_contains(api, method, "Int32") && method_param_contains(api, method, 0U, "Item")) {
                score = 10;
                if (contains_ci(name, "count")) score += 100;
                if (contains_ci(name, "item")) score += 20;
            }
        } else if (strcmp(kind, "recycle") == 0) {
            if (params == 2U && method_param_contains(api, method, 0U, "Item") && method_param_contains(api, method, 1U, "Int32")) {
                score = 10;
                if (contains_ci(name, "recycle") || contains_ci(name, "discard")) score += 100;
                if (method_return_contains(api, method, "RecycleItem")) score += 80;
            }
        } else if (strcmp(kind, "get_by_id") == 0) {
            if (params == 1U && method_param_contains(api, method, 0U, "UInt64") && method_return_contains(api, method, "PokemonProto")) {
                score = 10;
                if (contains_ci(name, "id")) score += 50;
                if (contains_ci(name, "pokemon")) score += 20;
            }
        } else if (strcmp(kind, "release") == 0) {
            if (params == 1U && method_param_contains(api, method, 0U, "PokemonProto")) {
                score = 10;
                if (contains_ci(name, "release") || contains_ci(name, "transfer")) score += 100;
                if (method_return_contains(api, method, "ReleasePokemon")) score += 80;
            }
        } else if (strcmp(kind, "collection") == 0) {
            const std::string return_name = type_name(api, api->method_get_return_type(method));
            if (params == 0U && contains_ci(return_name, "PokemonProto") &&
                (contains_ci(return_name, "IEnumerable") || contains_ci(return_name, "List") ||
                 contains_ci(return_name, "Collection") || contains_ci(return_name, "ReadOnly"))) {
                score = 10;
                if (contains_ci(name, "pokemon")) score += 30;
                if (contains_ci(name, "all")) score += 20;
            }
        }
        if (score < 0) continue;
        if (score > best_score) {
            best = method;
            best_score = score;
            tied = false;
        } else if (score == best_score) {
            tied = true;
        }
    }
    return tied ? nullptr : best;
}

FieldInfo *find_collection_field(Api *api, Il2CppClass *klass) {
    if (api == nullptr || klass == nullptr) return nullptr;
    FieldInfo *best = nullptr;
    void *iterator = nullptr;
    FieldInfo *field = nullptr;
    while ((field = api->class_get_fields(klass, &iterator)) != nullptr) {
        const std::string name = type_name(api, api->field_get_type(field));
        if (!contains_ci(name, "PokemonProto")) continue;
        if (!(contains_ci(name, "List") || contains_ci(name, "Collection") || contains_ci(name, "Dictionary") ||
              contains_ci(name, "Enumerable"))) continue;
        if (best != nullptr) return nullptr;
        best = field;
    }
    return best;
}

template <typename T>
bool read_field(Api *api, Il2CppObject *object, Il2CppClass *klass, const std::vector<const char *> &names, T *value) {
    if (api == nullptr || object == nullptr || klass == nullptr || value == nullptr) return false;
    for (const char *name : names) {
        FieldInfo *field = api->class_get_field_from_name(klass, name);
        if (field == nullptr) continue;
        T result{};
        api->field_get_value(object, field, &result);
        *value = result;
        return true;
    }
    return false;
}

bool background_value(Api *api, Il2CppObject *object, Il2CppClass *klass, bool *known, bool *present) {
    if (known != nullptr) *known = false;
    if (present != nullptr) *present = false;
    if (api == nullptr || object == nullptr || klass == nullptr) return false;
    void *iterator = nullptr;
    FieldInfo *field = nullptr;
    bool found = false;
    bool any_value = false;
    while ((field = api->class_get_fields(klass, &iterator)) != nullptr) {
        const char *raw_name = api->field_get_name(field);
        if (raw_name == nullptr) continue;
        const std::string name(raw_name);
        if (!(contains_ci(name, "background") || contains_ci(name, "locationcard") || contains_ci(name, "location_card"))) continue;
        found = true;
        uint64_t raw_value = 0U;
        api->field_get_value(object, field, &raw_value);
        if (raw_value != 0U) any_value = true;
    }
    if (known != nullptr) *known = found;
    if (present != nullptr) *present = any_value;
    return found;
}

bool is_legendary(int species) {
    switch (species) {
        case 144: case 145: case 146: case 150:
        case 243: case 244: case 245: case 249: case 250:
        case 377: case 378: case 379: case 380: case 381: case 382: case 383: case 384:
        case 480: case 481: case 482: case 483: case 484: case 485: case 486: case 487: case 488:
        case 638: case 639: case 640: case 641: case 642: case 643: case 644: case 645: case 646:
        case 716: case 717: case 718:
        case 772: case 773: case 785: case 786: case 787: case 788: case 789: case 790: case 791: case 792:
        case 793: case 794: case 795: case 796: case 797: case 798: case 799: case 800:
        case 803: case 804: case 805: case 806:
        case 888: case 889: case 890: case 891: case 892: case 894: case 895: case 896: case 897: case 898:
        case 905:
        case 1001: case 1002: case 1003: case 1004: case 1007: case 1008:
        case 1014: case 1015: case 1016: case 1017: case 1024:
            return true;
        default:
            return false;
    }
}

bool is_mythical(int species) {
    switch (species) {
        case 151: case 251: case 385: case 386: case 489: case 490: case 491: case 492: case 493: case 494:
        case 647: case 648: case 649:
        case 719: case 720: case 721:
        case 801: case 802: case 807: case 808: case 809:
        case 893: case 1025:
            return true;
        default:
            return false;
    }
}

bool inspect_pokemon(Api *api, Bindings *bindings, Il2CppObject *pokemon, PokemonMeta *meta) {
    if (api == nullptr || bindings == nullptr || pokemon == nullptr || meta == nullptr) return false;
    Il2CppClass *klass = api->object_get_class(pokemon);
    if (klass == nullptr) return false;
    uint64_t id = 0U;
    int32_t species = 0;
    int32_t attack = -1;
    int32_t defense = -1;
    int32_t stamina = -1;
    int32_t favorite = 0;
    Il2CppObject *display = nullptr;
    const bool id_ok = read_field<uint64_t>(api, pokemon, klass, {"id_", "Id_", "id"}, &id);
    const bool species_ok = read_field<int32_t>(api, pokemon, klass, {"pokemonId_", "PokemonId_", "pokemon_id_"}, &species);
    const bool attack_ok = read_field<int32_t>(api, pokemon, klass, {"individualAttack_", "IndividualAttack_", "individual_attack_"}, &attack);
    const bool defense_ok = read_field<int32_t>(api, pokemon, klass, {"individualDefense_", "IndividualDefense_", "individual_defense_"}, &defense);
    const bool stamina_ok = read_field<int32_t>(api, pokemon, klass, {"individualStamina_", "IndividualStamina_", "individual_stamina_"}, &stamina);
    const bool favorite_ok = read_field<int32_t>(api, pokemon, klass, {"favorite_", "Favorite_", "favorite"}, &favorite);
    const bool display_ok = read_field<Il2CppObject *>(api, pokemon, klass, {"pokemonDisplay_", "PokemonDisplay_", "pokemon_display_"}, &display);

    bool shiny = false;
    bool shiny_ok = false;
    bool background_known = false;
    bool background = false;
    if (display_ok && display != nullptr) {
        Il2CppClass *display_class = api->object_get_class(display);
        uint8_t shiny_raw = 0U;
        shiny_ok = read_field<uint8_t>(api, display, display_class, {"shiny_", "Shiny_", "shiny"}, &shiny_raw);
        shiny = shiny_raw != 0U;
        background_value(api, display, display_class, &background_known, &background);
    }
    bool pokemon_background_known = false;
    bool pokemon_background = false;
    background_value(api, pokemon, klass, &pokemon_background_known, &pokemon_background);
    background_known = background_known || pokemon_background_known;
    background = background || pokemon_background;

    meta->id = id;
    meta->species = species;
    meta->attack = attack;
    meta->defense = defense;
    meta->stamina = stamina;
    meta->shiny = shiny;
    meta->favorite = favorite != 0;
    meta->background = background;
    meta->legendary = is_legendary(species);
    meta->mythical = is_mythical(species);
    meta->complete = id_ok && species_ok && attack_ok && defense_ok && stamina_ok && favorite_ok && shiny_ok && background_known &&
        id != 0U && species > 0 && attack >= 0 && attack <= 15 && defense >= 0 && defense <= 15 && stamina >= 0 && stamina <= 15;
    return id_ok && id != 0U;
}

Il2CppObject *collection_from_bindings(Api *api, Bindings *bindings) {
    if (api == nullptr || bindings == nullptr || bindings->pokemon_bag == nullptr) return nullptr;
    bool ok = false;
    if (bindings->pokemon_collection != nullptr) {
        return invoke(api, bindings->pokemon_collection, bindings->pokemon_bag, nullptr, &ok);
    }
    if (bindings->pokemon_collection_field != nullptr) {
        Il2CppObject *collection = nullptr;
        api->field_get_value(bindings->pokemon_bag, bindings->pokemon_collection_field, &collection);
        return collection;
    }
    return nullptr;
}

std::vector<Il2CppObject *> enumerate_collection(Api *api, Il2CppObject *collection) {
    std::vector<Il2CppObject *> result;
    if (api == nullptr || collection == nullptr) return result;
    Il2CppClass *klass = api->object_get_class(collection);
    if (klass == nullptr) return result;

    const MethodInfo *get_count = api->class_get_method_from_name(klass, "get_Count", 0);
    const MethodInfo *get_item = api->class_get_method_from_name(klass, "get_Item", 1);
    if (get_count != nullptr && get_item != nullptr) {
        bool ok = false;
        Il2CppObject *boxed_count = invoke(api, get_count, collection, nullptr, &ok);
        int32_t count = 0;
        if (ok && unbox_i32(api, boxed_count, &count) && count >= 0) {
            count = std::min(count, kMaxPokemon);
            result.reserve(static_cast<size_t>(count));
            for (int32_t index = 0; index < count; ++index) {
                int32_t arg = index;
                void *args[] = {&arg};
                Il2CppObject *item = invoke(api, get_item, collection, args, &ok);
                if (ok && item != nullptr) result.push_back(item);
            }
            return result;
        }
    }

    const MethodInfo *get_enumerator = api->class_get_method_from_name(klass, "GetEnumerator", 0);
    if (get_enumerator == nullptr) return result;
    bool ok = false;
    Il2CppObject *enumerator = invoke(api, get_enumerator, collection, nullptr, &ok);
    if (!ok || enumerator == nullptr) return result;
    Il2CppClass *enumerator_class = api->object_get_class(enumerator);
    if (enumerator_class == nullptr) return result;
    const MethodInfo *move_next = api->class_get_method_from_name(enumerator_class, "MoveNext", 0);
    const MethodInfo *get_current = api->class_get_method_from_name(enumerator_class, "get_Current", 0);
    if (move_next == nullptr || get_current == nullptr) return result;
    for (int index = 0; index < kMaxPokemon; ++index) {
        Il2CppObject *boxed_next = invoke(api, move_next, enumerator, nullptr, &ok);
        bool has_next = false;
        if (!ok || !unbox_bool(api, boxed_next, &has_next) || !has_next) break;
        Il2CppObject *item = invoke(api, get_current, enumerator, nullptr, &ok);
        if (ok && item != nullptr) result.push_back(item);
    }
    return result;
}

Il2CppObject *find_zero_arg_return(Api *api, Il2CppClass *klass, Il2CppObject *instance, const char *return_fragment) {
    if (api == nullptr || klass == nullptr || return_fragment == nullptr) return nullptr;
    const MethodInfo *candidate = nullptr;
    void *iterator = nullptr;
    const MethodInfo *method = nullptr;
    while ((method = api->class_get_methods(klass, &iterator)) != nullptr) {
        if (api->method_get_param_count(method) != 0U || !method_return_contains(api, method, return_fragment)) continue;
        if (candidate != nullptr) return nullptr;
        candidate = method;
    }
    if (candidate == nullptr) return nullptr;
    bool ok = false;
    return invoke(api, candidate, instance, nullptr, &ok);
}

bool bind_runtime(Api *api, Il2CppDomain *domain, Bindings *bindings) {
    if (api == nullptr || domain == nullptr || bindings == nullptr) return false;
    const Il2CppImage *game_image = find_image(api, domain, {"holo-game.dll", "Assembly-CSharp.dll", "Assembly-CSharp"});
    const Il2CppImage *proto_image = find_image(api, domain, {"holo-protos.dll", "holo-protos"});
    if (game_image == nullptr) return false;

    Il2CppClass *player_class = find_class(api, game_image, "PlayerService");
    if (player_class == nullptr) return false;
    const MethodInfo *get_instance = api->class_get_method_from_name(player_class, "get_Instance", 0);
    if (get_instance == nullptr) return false;
    bool ok = false;
    bindings->player_service = invoke(api, get_instance, nullptr, nullptr, &ok);
    if (!ok || bindings->player_service == nullptr) return false;

    const MethodInfo *get_item_bag = api->class_get_method_from_name(player_class, "get_ItemBag", 0);
    if (get_item_bag != nullptr) bindings->item_bag = invoke(api, get_item_bag, bindings->player_service, nullptr, &ok);
    if (bindings->item_bag != nullptr) {
        Il2CppClass *item_class = api->object_get_class(bindings->item_bag);
        bindings->item_count = best_method(api, item_class, "item_count");
        bindings->recycle_item = best_method(api, item_class, "recycle");
    }

    bindings->pokemon_bag = find_zero_arg_return(api, player_class, bindings->player_service, "PokemonBag");
    if (bindings->pokemon_bag != nullptr) {
        Il2CppClass *pokemon_bag_class = api->object_get_class(bindings->pokemon_bag);
        bindings->pokemon_get_by_id = best_method(api, pokemon_bag_class, "get_by_id");
        bindings->release_pokemon = best_method(api, pokemon_bag_class, "release");
        bindings->pokemon_collection = best_method(api, pokemon_bag_class, "collection");
        if (bindings->pokemon_collection == nullptr) {
            bindings->pokemon_collection_field = find_collection_field(api, pokemon_bag_class);
        }
    }

    if (proto_image != nullptr) {
        bindings->pokemon_proto_class = find_class(api, proto_image, "PokemonProto");
        bindings->pokemon_display_class = find_class(api, proto_image, "PokemonDisplay");
        bool known_proto = false;
        bool ignored_proto = false;
        bool known_display = false;
        bool ignored_display = false;
        if (bindings->pokemon_proto_class != nullptr) {
            // The class layout can be checked with a real object later. Presence of a named background field is enough here.
            void *iterator = nullptr;
            FieldInfo *field = nullptr;
            while ((field = api->class_get_fields(bindings->pokemon_proto_class, &iterator)) != nullptr) {
                const char *name = api->field_get_name(field);
                if (name != nullptr && (contains_ci(name, "background") || contains_ci(name, "locationcard"))) {
                    known_proto = true;
                    break;
                }
            }
        }
        if (bindings->pokemon_display_class != nullptr) {
            void *iterator = nullptr;
            FieldInfo *field = nullptr;
            while ((field = api->class_get_fields(bindings->pokemon_display_class, &iterator)) != nullptr) {
                const char *name = api->field_get_name(field);
                if (name != nullptr && (contains_ci(name, "background") || contains_ci(name, "locationcard"))) {
                    known_display = true;
                    break;
                }
            }
        }
        ignored_proto = false;
        ignored_display = false;
        bindings->background_metadata_known = known_proto || known_display || ignored_proto || ignored_display;
    }

    return true;
}

bool discard_ready(const Bindings &bindings) {
    return bindings.item_bag != nullptr && bindings.item_count != nullptr && bindings.recycle_item != nullptr;
}

bool transfer_ready(const Bindings &bindings) {
    return bindings.pokemon_bag != nullptr && bindings.pokemon_get_by_id != nullptr && bindings.release_pokemon != nullptr &&
        (bindings.pokemon_collection != nullptr || bindings.pokemon_collection_field != nullptr) &&
        bindings.pokemon_proto_class != nullptr && bindings.background_metadata_known;
}

int item_count(Api *api, Bindings *bindings, int32_t item_id, bool *ok) {
    if (ok != nullptr) *ok = false;
    if (!discard_ready(*bindings)) return 0;
    void *args[] = {&item_id};
    bool invoke_ok = false;
    Il2CppObject *boxed = invoke(api, bindings->item_count, bindings->item_bag, args, &invoke_ok);
    int32_t count = 0;
    if (!invoke_ok || !unbox_i32(api, boxed, &count) || count < 0) return 0;
    if (ok != nullptr) *ok = true;
    return count;
}

std::map<std::string, std::string> read_kv_file(const std::string &path) {
    std::map<std::string, std::string> values;
    FILE *file = fopen(path.c_str(), "re");
    if (file == nullptr) return values;
    char line[1024]{};
    while (fgets(line, sizeof(line), file) != nullptr) {
        char *newline = strpbrk(line, "\r\n");
        if (newline != nullptr) *newline = '\0';
        char *separator = strchr(line, '=');
        if (separator == nullptr) continue;
        *separator = '\0';
        values[std::string(line)] = std::string(separator + 1);
    }
    fclose(file);
    return values;
}

long long parse_ll(const std::map<std::string, std::string> &values, const char *key, long long fallback) {
    const auto iterator = values.find(key);
    if (iterator == values.end()) return fallback;
    char *end = nullptr;
    errno = 0;
    const long long parsed = strtoll(iterator->second.c_str(), &end, 10);
    return errno == 0 && end != iterator->second.c_str() && *end == '\0' ? parsed : fallback;
}

double parse_double(const std::map<std::string, std::string> &values, const char *key, double fallback) {
    const auto iterator = values.find(key);
    if (iterator == values.end()) return fallback;
    char *end = nullptr;
    errno = 0;
    const double parsed = strtod(iterator->second.c_str(), &end);
    return errno == 0 && end != iterator->second.c_str() && *end == '\0' ? parsed : fallback;
}

bool parse_flag(const std::map<std::string, std::string> &values, const char *key, bool fallback) {
    const auto iterator = values.find(key);
    if (iterator == values.end()) return fallback;
    return iterator->second == "1" || iterator->second == "true";
}

std::string sanitize_message(std::string value) {
    std::replace(value.begin(), value.end(), '\n', ' ');
    std::replace(value.begin(), value.end(), '\r', ' ');
    return value;
}

void write_result(const std::string &directory, long long command_id, bool success, const std::string &message) {
    const std::string temp = directory + "/maintenance.result.tmp";
    const std::string target = directory + "/maintenance.result";
    FILE *file = fopen(temp.c_str(), "we");
    if (file == nullptr) return;
    fprintf(file, "command_id=%lld\n", command_id);
    fprintf(file, "success=%d\n", success ? 1 : 0);
    fprintf(file, "message=%s\n", sanitize_message(message).c_str());
    fflush(file);
    fsync(fileno(file));
    fclose(file);
    rename(temp.c_str(), target.c_str());
}

bool revalidate_transfer(const PokemonMeta &meta, const std::map<std::string, std::string> &command, std::string *reason) {
    if (!meta.complete) {
        if (reason != nullptr) *reason = "pokemon metadata incomplete";
        return false;
    }
    const int total = meta.attack + meta.defense + meta.stamina;
    const double iv_percent = static_cast<double>(total) * 100.0 / 45.0;
    if (parse_flag(command, "protect_hundo", true) && total == 45) {
        if (reason != nullptr) *reason = "hundo protected";
        return false;
    }
    if (parse_flag(command, "protect_shiny", true) && meta.shiny) {
        if (reason != nullptr) *reason = "shiny protected";
        return false;
    }
    if (parse_flag(command, "protect_background", true) && meta.background) {
        if (reason != nullptr) *reason = "background protected";
        return false;
    }
    if (parse_flag(command, "protect_favorite", true) && meta.favorite) {
        if (reason != nullptr) *reason = "favorite protected";
        return false;
    }
    if (parse_flag(command, "protect_legendary", true) && meta.legendary) {
        if (reason != nullptr) *reason = "legendary protected";
        return false;
    }
    if (parse_flag(command, "protect_mythical", true) && meta.mythical) {
        if (reason != nullptr) *reason = "mythical protected";
        return false;
    }
    const double minimum = parse_double(command, "minimum_iv_percent", 80.0);
    if (iv_percent >= minimum) {
        if (reason != nullptr) *reason = "IV threshold protected";
        return false;
    }
    return true;
}

void process_command(Api *api, Bindings *bindings, const std::string &directory, long long *last_command_id) {
    if (api == nullptr || bindings == nullptr || last_command_id == nullptr) return;
    const std::string path = directory + "/maintenance.command";
    const auto command = read_kv_file(path);
    if (command.empty()) return;
    if (parse_ll(command, "maintenance_command_protocol", 0) != kProtocol) return;
    const long long command_id = parse_ll(command, "command_id", 0);
    if (command_id <= *last_command_id) return;
    *last_command_id = command_id;
    const auto action_iterator = command.find("action");
    if (action_iterator == command.end()) {
        write_result(directory, command_id, false, "missing action");
        return;
    }

    if (action_iterator->second == "discard") {
        if (!discard_ready(*bindings)) {
            write_result(directory, command_id, false, "discard bindings unavailable");
            return;
        }
        const long long item_id_raw = parse_ll(command, "item_id", -1);
        const long long amount_raw = parse_ll(command, "amount", -1);
        if (item_id_raw <= 0 || item_id_raw > INT32_MAX || amount_raw <= 0 || amount_raw > INT32_MAX) {
            write_result(directory, command_id, false, "invalid discard command");
            return;
        }
        const int32_t item_id = static_cast<int32_t>(item_id_raw);
        const int32_t amount = static_cast<int32_t>(amount_raw);
        bool count_ok = false;
        const int before = item_count(api, bindings, item_id, &count_ok);
        if (!count_ok || before < amount) {
            write_result(directory, command_id, false, "discard amount exceeds live count");
            return;
        }
        int32_t item_arg = item_id;
        int32_t amount_arg = amount;
        void *args[] = {&item_arg, &amount_arg};
        bool ok = false;
        invoke(api, bindings->recycle_item, bindings->item_bag, args, &ok);
        write_result(directory, command_id, ok, ok ? "discard submitted" : "recycle invocation failed");
        return;
    }

    if (action_iterator->second == "transfer") {
        if (!transfer_ready(*bindings)) {
            write_result(directory, command_id, false, "transfer bindings unavailable");
            return;
        }
        const long long raw_id = parse_ll(command, "pokemon_id", -1);
        if (raw_id <= 0) {
            write_result(directory, command_id, false, "invalid pokemon id");
            return;
        }
        uint64_t pokemon_id = static_cast<uint64_t>(raw_id);
        void *args[] = {&pokemon_id};
        bool ok = false;
        Il2CppObject *pokemon = invoke(api, bindings->pokemon_get_by_id, bindings->pokemon_bag, args, &ok);
        if (!ok || pokemon == nullptr) {
            write_result(directory, command_id, false, "pokemon no longer exists");
            return;
        }
        PokemonMeta meta{};
        inspect_pokemon(api, bindings, pokemon, &meta);
        std::string reason;
        if (!revalidate_transfer(meta, command, &reason)) {
            write_result(directory, command_id, false, reason);
            return;
        }
        void *release_args[] = {pokemon};
        invoke(api, bindings->release_pokemon, bindings->pokemon_bag, release_args, &ok);
        write_result(directory, command_id, ok, ok ? "transfer submitted" : "release invocation failed");
        return;
    }

    write_result(directory, command_id, false, "unsupported action");
}

void write_snapshot(Api *api, Bindings *bindings, const std::string &directory) {
    if (api == nullptr || bindings == nullptr) return;
    const std::string temp = directory + "/maintenance.snapshot.tmp";
    const std::string target = directory + "/maintenance.snapshot";
    FILE *file = fopen(temp.c_str(), "we");
    if (file == nullptr) return;
    const long long now_ms = static_cast<long long>(time(nullptr)) * 1000LL;
    fprintf(file, "maintenance_protocol=%d\n", kProtocol);
    fprintf(file, "maintenance_seen_at_epoch_ms=%lld\n", now_ms);
    fprintf(file, "discard_bindings_ready=%d\n", discard_ready(*bindings) ? 1 : 0);
    fprintf(file, "transfer_bindings_ready=%d\n", transfer_ready(*bindings) ? 1 : 0);

    long long total_items = 0LL;
    if (discard_ready(*bindings)) {
        for (int item_id : kTrackedItems) {
            bool ok = false;
            const int count = item_count(api, bindings, item_id, &ok);
            if (!ok) continue;
            fprintf(file, "item_%d=%d\n", item_id, count);
            total_items += count;
        }
        fprintf(file, "inventory_used_slots=%lld\n", total_items);
        fprintf(file, "inventory_capacity=%lld\n", total_items);
    }

    if (transfer_ready(*bindings)) {
        Il2CppObject *collection = collection_from_bindings(api, bindings);
        const std::vector<Il2CppObject *> pokemon = enumerate_collection(api, collection);
        fprintf(file, "pokemon_used_slots=%zu\n", pokemon.size());
        fprintf(file, "pokemon_capacity=%zu\n", pokemon.size());
        fputs("pokemon_storage=", file);
        bool first = true;
        for (Il2CppObject *object : pokemon) {
            PokemonMeta meta{};
            if (!inspect_pokemon(api, bindings, object, &meta) || meta.id == 0U || meta.species <= 0) continue;
            if (!first) fputc(';', file);
            first = false;
            fprintf(
                file,
                "%llu|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d",
                static_cast<unsigned long long>(meta.id),
                meta.species,
                meta.attack,
                meta.defense,
                meta.stamina,
                meta.shiny ? 1 : 0,
                meta.favorite ? 1 : 0,
                meta.legendary ? 1 : 0,
                meta.mythical ? 1 : 0,
                meta.background ? 1 : 0,
                meta.complete ? 1 : 0
            );
        }
        fputc('\n', file);
    }

    fflush(file);
    fsync(fileno(file));
    fclose(file);
    rename(temp.c_str(), target.c_str());
}

void *maintenance_thread(void *opaque) {
    RuntimeContext *context = static_cast<RuntimeContext *>(opaque);
    if (context == nullptr) return nullptr;
    char directory_buffer[512]{};
    snprintf(
        directory_buffer,
        sizeof(directory_buffer),
        "/data/user/0/%s/files/pogo_root_automation",
        context->process_name
    );
    const std::string directory(directory_buffer);
    delete context;

    if (mkdir(directory.c_str(), 0700) != 0 && errno != EEXIST) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "cannot create runtime directory errno=%d", errno);
        return nullptr;
    }

    Api api{};
    for (int attempt = 0; attempt < kBindAttempts; ++attempt) {
        if (resolve_api(&api)) break;
        usleep(kBindDelayUs);
    }
    if (api.handle == nullptr || api.domain_get == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "IL2CPP API unavailable");
        return nullptr;
    }

    Il2CppDomain *domain = api.domain_get();
    if (domain == nullptr) return nullptr;
    Il2CppThread *thread = api.thread_attach(domain);
    if (thread == nullptr) return nullptr;

    Bindings bindings{};
    long long last_command_id = 0LL;
    int bind_tick = 0;
    int snapshot_tick = 0;
    for (;;) {
        if (bind_tick <= 0 || !discard_ready(bindings) || !transfer_ready(bindings)) {
            Bindings refreshed{};
            if (bind_runtime(&api, domain, &refreshed)) bindings = refreshed;
            bind_tick = 20;
        }
        --bind_tick;

        process_command(&api, &bindings, directory, &last_command_id);
        if (snapshot_tick <= 0) {
            write_snapshot(&api, &bindings, directory);
            snapshot_tick = 4;
        }
        --snapshot_tick;
        usleep(kLoopDelayUs);
    }

    // Unreachable while the target process is alive; kept for ownership clarity.
    api.thread_detach(thread);
    return nullptr;
}
}  // namespace

void start_maintenance_runtime(const char *process_name) {
    if (process_name == nullptr || process_name[0] == '\0') return;
    bool expected = false;
    if (!g_started.compare_exchange_strong(expected, true)) return;
    RuntimeContext *context = new RuntimeContext{};
    snprintf(context->process_name, sizeof(context->process_name), "%s", process_name);
    pthread_t thread{};
    const int result = pthread_create(&thread, nullptr, maintenance_thread, context);
    if (result != 0) {
        delete context;
        g_started.store(false);
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "pthread_create failed=%d", result);
        return;
    }
    pthread_detach(thread);
}
