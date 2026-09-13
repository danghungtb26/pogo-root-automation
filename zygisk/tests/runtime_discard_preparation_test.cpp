#include <cassert>
#include <cstdint>
#include <cstring>
#include <functional>
#include <mutex>

constexpr int ANDROID_LOG_WARN = 1, ANDROID_LOG_INFO = 2;
constexpr const char *kLogTag = "test";
void __android_log_print(int, const char *, const char *, ...) {}
struct Il2CppClass {};
Il2CppClass row_class, other_class;
int row, bag, method, set, promise;
struct Il2CppApi {
    std::function<void *(const void *, void *, void **, void **)> runtime_invoke;
    std::function<Il2CppClass *(void *)> object_get_class;
};
struct RuntimeBinding {
    bool discard_verified = true;
    void *item_bag = &bag;
    const void *item_bag_recycle_item = &method;
    Il2CppClass *item_data_class = &row_class;
    void *hash_set_item_supplier = nullptr;
    Il2CppClass *hash_set_item_class = nullptr;
    const void *hash_set_item_ctor = nullptr;
};
struct ProbeContext { Il2CppApi il2cpp_api; RuntimeBinding binding; std::mutex binding_mutex; };
RuntimeBinding runtime_binding_snapshot(ProbeContext &context) { return context.binding; }
int32_t stack_count = 19, row_count = 19, row_item = 2;
uint8_t recyclable = 1;
bool row_present = true, set_present = true, fields_present = true;
bool is_probable_managed_pointer(const void *pointer) { return pointer != nullptr; }
int32_t read_runtime_item_count(const Il2CppApi &, const RuntimeBinding &, int32_t) { return stack_count; }
void *create_recycle_item_data_on_main_thread(ProbeContext &, int32_t, int32_t, void **) {
    return row_present ? &row : nullptr;
}
void *resolve_recycle_expiring_items_copy(const Il2CppApi &, const RuntimeBinding &, void **) {
    return set_present ? &set : nullptr;
}
template <class T> bool read_runtime_field(const Il2CppApi &, void *, const char *field, T *out) {
    if (!fields_present) return false;
    if (strcmp(field, "item") == 0) *out = static_cast<T>(row_item);
    else if (strcmp(field, "count") == 0) *out = static_cast<T>(row_count);
    else if (strcmp(field, "recyclable") == 0) *out = static_cast<T>(recyclable);
    else return false;
    return true;
}
#include "modules/discard/recycle_item_action.inc"

int main() {
    ProbeContext context;
    int calls = 0;
    bool throw_on_invoke = false, wrong_class = false;
    context.il2cpp_api.object_get_class = [&](void *) { return wrong_class ? &other_class : &row_class; };
    context.il2cpp_api.runtime_invoke = [&](const void *target_method, void *target,
                                           void **args, void **exception) -> void * {
        ++calls;
        assert(target_method == &method && target == &bag);
        assert(args[0] == &row && *static_cast<int32_t *>(args[1]) == 9 && args[2] == &set);
        if (throw_on_invoke) { *exception = &bag; return nullptr; }
        return &promise;
    };
    bool invoked = false;
    const char *failure = "";
    void *exception = nullptr;
    auto attempt = [&] {
        return recycle_runtime_item_on_main_thread(context, 2, 9, &exception, &invoked, &failure);
    };
    context.binding.discard_verified = false;
    assert(attempt() == nullptr && !invoked);
    context.binding.discard_verified = true;
    stack_count = 8;
    assert(attempt() == nullptr && !invoked);
    stack_count = 19;
    row_present = false; // Unopened inventory / missing filter: no mutation was attempted.
    assert(attempt() == nullptr && !invoked && strstr(failure, "filter") != nullptr);
    row_present = true;
    row_item = 1;
    assert(attempt() == nullptr && !invoked);
    row_item = 2;
    row_count = 18;
    assert(attempt() == nullptr && !invoked);
    row_count = 19;
    recyclable = 0;
    assert(attempt() == nullptr && !invoked);
    recyclable = 1;
    fields_present = false;
    assert(attempt() == nullptr && !invoked);
    fields_present = true;
    wrong_class = true;
    assert(attempt() == nullptr && !invoked);
    wrong_class = false;
    set_present = false;
    assert(attempt() == nullptr && !invoked);
    set_present = true;
    assert(calls == 0);
    assert(attempt() == &promise && invoked && calls == 1);
    throw_on_invoke = true;
    assert(attempt() == nullptr && invoked && calls == 2 && exception != nullptr);
    assert(strstr(failure, "result unknown") != nullptr); // Must not blindly retry this mutation.
}
