#include <cassert>
#include <cstdint>
#include <cstring>
#include <functional>

struct Il2CppClass {};
struct Il2CppDomain {};
struct Il2CppAssembly {};
struct Il2CppType { const char *name; };
using Il2CppGcHandle = uint32_t;
struct Il2CppApi {
    std::function<Il2CppDomain *()> domain_get;
    std::function<const Il2CppAssembly **(Il2CppDomain *, size_t *)> domain_get_assemblies;
    std::function<const Il2CppType *(const void *)> field_get_type;
    std::function<void *(const Il2CppType *)> type_get_object;
    std::function<const void *(Il2CppClass *, void **)> class_get_methods;
    std::function<const char *(const void *)> method_get_name;
    std::function<uint32_t(const void *)> method_get_param_count;
    std::function<const Il2CppType *(const void *, uint32_t)> method_get_param;
    std::function<const char *(const Il2CppType *)> type_get_name;
    std::function<void *(const void *, void *, void **, void **)> runtime_invoke;
    std::function<void *(const char *)> string_new;
    std::function<Il2CppGcHandle(void *, bool)> gchandle_new;
    std::function<void *(Il2CppGcHandle)> gchandle_get_target;
    std::function<const void *(Il2CppClass *, const char *)> class_get_field_from_name;
    std::function<void(void *, void *)> field_static_get_value;
};
Il2CppClass host, delegate_class;
Il2CppDomain domain;
Il2CppAssembly assembly;
const Il2CppAssembly *assemblies[] = {&assembly};
int singleton, singleton_field, filter_field, type_object, delegate_object;
int wrong_overload, factory_method;
Il2CppType parameter_types[] = {
    {"System.Type"}, {"System.Object"}, {"System.String"}, {"System.Boolean"}, {"System.Boolean"}
};
bool is_probable_managed_pointer(const void *value) { return value != nullptr; }
Il2CppClass *find_runtime_class(const Il2CppApi &, const Il2CppAssembly **, size_t,
                              const char *ns, const char *name, const void *) {
    assert(strcmp(ns, "System") == 0 && strcmp(name, "Delegate") == 0);
    return &delegate_class;
}
#include "modules/discard/inventory_filter_factory.inc"

int main() {
    Il2CppApi api;
    int invokes = 0;
    bool fail_binding = false;
    bool fail_retention = false;
    bool have_singleton = true;
    api.domain_get = [] { return &domain; };
    api.domain_get_assemblies = [](Il2CppDomain *, size_t *count) { *count = 1; return assemblies; };
    api.field_get_type = [](const void *field) {
        assert(field == &filter_field); return &parameter_types[0];
    };
    api.type_get_object = [](const Il2CppType *) -> void * { return &type_object; };
    api.class_get_field_from_name = [](Il2CppClass *klass, const char *name) -> const void * {
        assert(klass == &host && strcmp(name, "<>9") == 0); return &singleton_field;
    };
    api.field_static_get_value = [&](void *field, void *out) {
        assert(field == &singleton_field);
        *static_cast<void **>(out) = have_singleton ? &singleton : nullptr;
    };
    api.class_get_methods = [](Il2CppClass *, void **iterator) -> const void * {
        if (*iterator == nullptr) { *iterator = &wrong_overload; return &wrong_overload; }
        if (*iterator == &wrong_overload) { *iterator = &factory_method; return &factory_method; }
        return nullptr;
    };
    api.method_get_name = [](const void *) { return "CreateDelegate"; };
    api.method_get_param_count = [](const void *) { return 5U; };
    api.method_get_param = [](const void *method, uint32_t index) {
        // The Type-target overload must never be selected for an instance.
        return &parameter_types[method == &wrong_overload && index == 1 ? 0 : index];
    };
    api.type_get_name = [](const Il2CppType *type) { return type->name; };
    api.string_new = [](const char *value) -> void * { return const_cast<char *>(value); };
    api.runtime_invoke = [&](const void *method, void *target, void **args, void **exception) -> void * {
        ++invokes;
        assert(method == &factory_method && target == nullptr);
        assert(args[0] == &type_object && args[1] == &singleton);
        assert(strcmp(static_cast<const char *>(args[2]), "chvt") == 0);
        assert(*static_cast<uint8_t *>(args[3]) == 0 && *static_cast<uint8_t *>(args[4]) == 1);
        if (fail_binding) { *exception = &singleton; return nullptr; }
        return &delegate_object;
    };
    api.gchandle_new = [&](void *object, bool pinned) {
        assert(object == &delegate_object && !pinned); return fail_retention ? 0U : 7U;
    };
    api.gchandle_get_target = [](Il2CppGcHandle handle) -> void * {
        assert(handle == 7U); return &delegate_object;
    };
    have_singleton = false;
    assert(create_discard_inventory_filter(api, &host, &filter_field) == nullptr);
    assert(invokes == 0);
    have_singleton = true;
    fail_binding = true;
    assert(create_discard_inventory_filter(api, &host, &filter_field) == nullptr);
    assert(g_discard_owned_filter_handle == 0);
    fail_binding = false;
    fail_retention = true;
    assert(create_discard_inventory_filter(api, &host, &filter_field) == nullptr);
    fail_retention = false;
    assert(create_discard_inventory_filter(api, &host, &filter_field) == &delegate_object);
    assert(invokes == 3);
    assert(create_discard_inventory_filter(api, &host, &filter_field) == &delegate_object);
    assert(invokes == 3); // Reconnect reuses its rooted delegate without running UI/constructors.
}
