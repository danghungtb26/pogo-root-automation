struct HookPokeballThrow {
    int32_t ball_type = 0;
    float reticle_size = 0.0F;
    uint8_t hit_bullseye = 0U;
    uint8_t spinning = 0U;
    uint8_t missed = 0U;
    uint8_t reserved = 0U;
};

static_assert(offsetof(HookPokeballThrow, ball_type) == 0U);
static_assert(offsetof(HookPokeballThrow, reticle_size) == 4U);
static_assert(offsetof(HookPokeballThrow, hit_bullseye) == 8U);
static_assert(offsetof(HookPokeballThrow, spinning) == 9U);
static_assert(offsetof(HookPokeballThrow, missed) == 10U);
static_assert(sizeof(HookPokeballThrow) == 12U);

struct ThrowHookMethodInfoView {
    const void *method_pointer = nullptr;
};

struct RuntimeInlineHook {
    void *target = nullptr;
    void *trampoline = nullptr;
    size_t trampoline_bytes = 0U;
    uint8_t original[16]{};
    bool installed = false;
};

using PokeballLaunchFunction = void (*)(void *, float, float, const void *);
using PokeballBsuzFunction = void (*)(void *, HookPokeballThrow, const void *);
using AttemptCaptureFunction = void *(*)(void *, HookPokeballThrow, const void *);
using ThrowMissedFunction = void (*)(void *, const void *);

ProbeContext *g_throw_hook_context = nullptr;
RuntimeInlineHook g_pokeball_launch_hook;
RuntimeInlineHook g_pokeball_bsuz_hook;
RuntimeInlineHook g_attempt_capture_hook;
RuntimeInlineHook g_throw_missed_hook;
PokeballLaunchFunction g_original_pokeball_launch = nullptr;
PokeballBsuzFunction g_original_pokeball_bsuz = nullptr;
AttemptCaptureFunction g_original_attempt_capture = nullptr;
ThrowMissedFunction g_original_throw_missed = nullptr;

const void *throw_method_code_pointer(const void *method) {
    if (method == nullptr) return nullptr;
    return reinterpret_cast<const ThrowHookMethodInfoView *>(method)->method_pointer;
}

const void *find_throw_method(
    const Il2CppApi &api,
    Il2CppClass *klass,
    const char *method_name
) {
    const void *method = find_runtime_method_with_parameter(
        api, klass, method_name, "Niantic.Holoholo.Encounter.PokeballThrow"
    );
    if (method == nullptr) {
        method = find_runtime_method_with_parameter(api, klass, method_name, "PokeballThrow");
    }
    return method;
}

bool verify_throw_method_shape(
    const Il2CppApi &api,
    const void *method,
    const char *label
) {
    const bool valid = method != nullptr && api.method_is_instance != nullptr &&
        api.method_is_instance(method) && api.method_get_param_count != nullptr &&
        api.method_get_param_count(method) == 1U;
    __android_log_print(
        valid ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "runtime throw binding method=%s method=%p code=%p instance=%d params=%u valid=%d",
        label == nullptr ? "<unknown>" : label,
        method,
        throw_method_code_pointer(method),
        valid && api.method_is_instance(method) ? 1 : 0,
        valid ? api.method_get_param_count(method) : 0U,
        valid ? 1 : 0
    );
    return valid;
}

bool verify_throw_method_rva(const void *method, uintptr_t expected_rva, const char *label) {
    const void *code = throw_method_code_pointer(method);
    Dl_info info{};
    if (code == nullptr || dladdr(code, &info) == 0 || info.dli_fbase == nullptr) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "runtime throw binding rva unavailable method=%s code=%p",
            label == nullptr ? "<unknown>" : label,
            code
        );
        return false;
    }
    const uintptr_t rva = reinterpret_cast<uintptr_t>(code) -
        reinterpret_cast<uintptr_t>(info.dli_fbase);
    __android_log_print(
        rva == expected_rva ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "runtime throw binding rva method=%s code=%p image=%s rva=0x%lx expected=0x%lx valid=%d",
        label == nullptr ? "<unknown>" : label,
        code,
        info.dli_fname == nullptr ? "<unknown>" : info.dli_fname,
        static_cast<unsigned long>(rva),
        static_cast<unsigned long>(expected_rva),
        rva == expected_rva ? 1 : 0
    );
    return rva == expected_rva;
}

bool bind_runtime_throw_methods(
    const Il2CppApi &api,
    const Il2CppAssembly **assemblies,
    size_t assembly_count,
    RuntimeBinding *binding
) {
    if (binding == nullptr || assemblies == nullptr || !binding->exact_build_verified ||
        !has_runtime_inspection_api(api)) return false;

    const bool hooks_installed = binding->throw_hooks_installed;
    binding->pokeball_launch = nullptr;
    binding->pokeball_bsuz = nullptr;
    binding->encounter_attempt_capture = nullptr;
    binding->encounter_throw_missed = nullptr;
    binding->throw_hook_binding_verified = false;
    binding->throw_hooks_installed = hooks_installed;

    Il2CppClass *pokeball_class = find_runtime_class(
        api, assemblies, assembly_count, "Niantic.Holoholo.Encounter", "Pokeball", nullptr
    );
    Il2CppClass *interaction_class = find_runtime_class(
        api,
        assemblies,
        assembly_count,
        "Niantic.Holoholo.Encounter",
        "EncounterInteractionState",
        nullptr
    );
    binding->pokeball_launch = pokeball_class == nullptr ? nullptr :
        find_runtime_method_with_parameter(api, pokeball_class, "Launch", "UnityEngine.Vector2");
    binding->pokeball_bsuz = find_throw_method(api, pokeball_class, "bsuz");
    binding->encounter_attempt_capture = find_throw_method(
        api, interaction_class, "AttemptCapture"
    );
    binding->encounter_throw_missed = interaction_class == nullptr ? nullptr :
        api.class_get_method_from_name(interaction_class, "ThrowMissed", 0);

    const bool methods_valid = verify_throw_method_shape(
            api, binding->pokeball_launch, "Pokeball.Launch"
        ) && verify_throw_method_shape(api, binding->pokeball_bsuz, "Pokeball.bsuz") &&
        verify_throw_method_shape(api, binding->encounter_attempt_capture, "EncounterInteractionState.AttemptCapture") &&
        binding->encounter_throw_missed != nullptr && api.method_is_instance != nullptr &&
        api.method_is_instance(binding->encounter_throw_missed) &&
        api.method_get_param_count(binding->encounter_throw_missed) == 0U;
    const bool rvas_valid = methods_valid &&
        verify_throw_method_rva(binding->pokeball_launch, 0x89E77F8U, "Pokeball.Launch") &&
        verify_throw_method_rva(binding->pokeball_bsuz, 0x89E9A74U, "Pokeball.bsuz") &&
        verify_throw_method_rva(
            binding->encounter_attempt_capture,
            0x89A74F8U,
            "EncounterInteractionState.AttemptCapture"
        ) && verify_throw_method_rva(
            binding->encounter_throw_missed,
            0x89AC69CU,
            "EncounterInteractionState.ThrowMissed"
        );
    binding->throw_hook_binding_verified = methods_valid && rvas_valid;
    __android_log_print(
        binding->throw_hook_binding_verified ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "runtime throw binding verified=%d launch=%p bsuz=%p attempt=%p missed=%p",
        binding->throw_hook_binding_verified ? 1 : 0,
        binding->pokeball_launch,
        binding->pokeball_bsuz,
        binding->encounter_attempt_capture,
        binding->encounter_throw_missed
    );
    return binding->throw_hook_binding_verified;
}

#include "runtime_throw_trampoline.inc"

#if defined(__aarch64__)
void queue_throw_hook_event(
    ProbeContext *context,
    uint32_t stage,
    uintptr_t object_instance,
    const HookPokeballThrow *throw_data
) {
    if (context == nullptr) return;
    pogo_runtime::RuntimeThrowDiagnosticObservation diagnostic;
    diagnostic.encounter_id = context->active_encounter_id.load();
    diagnostic.stage = stage;
    if (throw_data != nullptr) {
        diagnostic.ball_type = throw_data->ball_type < 0 ? 0 : throw_data->ball_type;
        if (throw_data->hit_bullseye != 0U) diagnostic.flags |= pogo_runtime::kThrowHitBullseye;
        if (throw_data->spinning != 0U) diagnostic.flags |= pogo_runtime::kThrowSpinning;
        if (throw_data->missed != 0U) diagnostic.flags |= pogo_runtime::kThrowMissedData;
    }
    if (stage == pogo_runtime::kThrowUserLaunch) {
        diagnostic.flags |= pogo_runtime::kThrowBallInPlay;
    } else if (stage == pogo_runtime::kThrowCaptureAttempt) {
        diagnostic.flags |= pogo_runtime::kThrowInteractionInPlay;
    } else if (stage == pogo_runtime::kThrowMissed) {
        diagnostic.flags |= pogo_runtime::kThrowMissedData;
    }
    context->throw_events.push(diagnostic, object_instance);
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "runtime throw hook queued stage=%u encounter=%llu object=%p ball=%d flags=0x%x",
        diagnostic.stage,
        static_cast<unsigned long long>(diagnostic.encounter_id),
        reinterpret_cast<void *>(object_instance),
        diagnostic.ball_type,
        diagnostic.flags
    );
}

void hooked_pokeball_launch(void *self, float x, float y, const void *method_info) {
    queue_throw_hook_event(g_throw_hook_context, pogo_runtime::kThrowUserLaunch,
        reinterpret_cast<uintptr_t>(self), nullptr);
    if (g_original_pokeball_launch != nullptr) g_original_pokeball_launch(self, x, y, method_info);
}

void hooked_pokeball_bsuz(
    void *self,
    HookPokeballThrow throw_data,
    const void *method_info
) {
    queue_throw_hook_event(g_throw_hook_context, pogo_runtime::kThrowDataReceived,
        reinterpret_cast<uintptr_t>(self), &throw_data);
    if (g_original_pokeball_bsuz != nullptr) {
        g_original_pokeball_bsuz(self, throw_data, method_info);
    }
}

void *hooked_attempt_capture(
    void *self,
    HookPokeballThrow throw_data,
    const void *method_info
) {
    queue_throw_hook_event(g_throw_hook_context, pogo_runtime::kThrowCaptureAttempt,
        reinterpret_cast<uintptr_t>(self), &throw_data);
    void *promise = g_original_attempt_capture == nullptr
        ? nullptr
        : g_original_attempt_capture(self, throw_data, method_info);
    if (promise != nullptr) {
        queue_throw_hook_event(g_throw_hook_context, pogo_runtime::kThrowCapturePromiseCreated,
            reinterpret_cast<uintptr_t>(self), &throw_data);
    }
    return promise;
}

void hooked_throw_missed(void *self, const void *method_info) {
    queue_throw_hook_event(g_throw_hook_context, pogo_runtime::kThrowMissed,
        reinterpret_cast<uintptr_t>(self), nullptr);
    if (g_original_throw_missed != nullptr) g_original_throw_missed(self, method_info);
}
#endif

bool install_runtime_throw_hooks(ProbeContext &context) {
#if defined(__aarch64__)
    RuntimeBinding binding = runtime_binding_snapshot(context);
    if (binding.throw_hooks_installed) return true;
    if (!binding.exact_build_verified || !binding.throw_hook_binding_verified) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "runtime throw hooks refused exact_build=%d binding_verified=%d",
            binding.exact_build_verified ? 1 : 0,
            binding.throw_hook_binding_verified ? 1 : 0
        );
        return false;
    }
    g_throw_hook_context = &context;
    const uint32_t launch_prologue[] = {0xD10203FFU, 0x6D013BEFU, 0x6D0233EDU, 0x6D032BEBU};
    const uint32_t bsuz_prologue[] = {0xF81D0FFEU, 0xA90157F6U, 0xA9024FF4U, 0xF0019376U};
    const uint32_t attempt_prologue[] = {0xA9BB6FFEU, 0xA90167FAU, 0xA9025FF8U, 0xA90357F6U};
    const uint32_t missed_prologue[] = {0xF9400008U, 0xF9416101U, 0xF9415D02U, 0xD61F0040U};
    const bool launch_installed = install_inline_hook(
        &g_pokeball_launch_hook,
        throw_method_code_pointer(binding.pokeball_launch),
        launch_prologue,
        reinterpret_cast<const void *>(&hooked_pokeball_launch)
    );
    if (launch_installed) {
        g_original_pokeball_launch = reinterpret_cast<PokeballLaunchFunction>(
            g_pokeball_launch_hook.trampoline
        );
    }
    const bool bsuz_installed = install_inline_hook(
        &g_pokeball_bsuz_hook,
        throw_method_code_pointer(binding.pokeball_bsuz),
        bsuz_prologue,
        reinterpret_cast<const void *>(&hooked_pokeball_bsuz)
    );
    if (bsuz_installed) {
        g_original_pokeball_bsuz = reinterpret_cast<PokeballBsuzFunction>(
            g_pokeball_bsuz_hook.trampoline
        );
    }
    const bool attempt_installed = install_inline_hook(
        &g_attempt_capture_hook,
        throw_method_code_pointer(binding.encounter_attempt_capture),
        attempt_prologue,
        reinterpret_cast<const void *>(&hooked_attempt_capture)
    );
    if (attempt_installed) {
        g_original_attempt_capture = reinterpret_cast<AttemptCaptureFunction>(
            g_attempt_capture_hook.trampoline
        );
    }
    const bool missed_installed = install_inline_hook(
        &g_throw_missed_hook,
        throw_method_code_pointer(binding.encounter_throw_missed),
        missed_prologue,
        reinterpret_cast<const void *>(&hooked_throw_missed)
    );
    if (missed_installed) {
        g_original_throw_missed = reinterpret_cast<ThrowMissedFunction>(
            g_throw_missed_hook.trampoline
        );
    }
    const bool installed = launch_installed && bsuz_installed && attempt_installed && missed_installed;
    if (!installed) {
        uninstall_inline_hook(&g_throw_missed_hook);
        uninstall_inline_hook(&g_attempt_capture_hook);
        uninstall_inline_hook(&g_pokeball_bsuz_hook);
        uninstall_inline_hook(&g_pokeball_launch_hook);
        g_original_throw_missed = nullptr;
        g_original_attempt_capture = nullptr;
        g_original_pokeball_bsuz = nullptr;
        g_original_pokeball_launch = nullptr;
        g_throw_hook_context = nullptr;
        __android_log_print(ANDROID_LOG_WARN, kLogTag, "runtime throw hooks installed=0");
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(context.binding_mutex);
        context.binding.throw_hooks_installed = true;
    }
    __android_log_print(ANDROID_LOG_INFO, kLogTag, "runtime throw hooks installed=1");
    return true;
#else
    (void)context;
    __android_log_print(ANDROID_LOG_WARN, kLogTag, "runtime throw hooks unsupported ABI");
    return false;
#endif
}
