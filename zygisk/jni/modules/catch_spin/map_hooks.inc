// Mode B map-query batch-callback hook.
//
// Hooks Niantic.Platform.GameMapObject.Map.MapQueryManager.OnMapQueryResponse
// (int rpcId, byte[] response) so a server map update marks the observed world
// dirty exactly once per batch, instead of the observer polling every tick. The
// hook does the minimum (mark dirty, call original) and returns immediately.
//
// This reuses the throw-assist inline-hook machinery (RuntimeInlineHook /
// install_inline_hook / uninstall_inline_hook), so it must be included after
// modules/throw_assist/throw_hooks.inc and after runtime_observation.inc
// (for mark_world_dirty), and before runtime_feature_modules.inc.

// Instance native ABI: (this, int rpcId, byte[] response, MethodInfo*).
using OnMapQueryResponseFunction = void (*)(void *, int32_t, void *, const void *);

ProbeContext *g_map_query_hook_context = nullptr;
RuntimeInlineHook g_map_query_hook;
OnMapQueryResponseFunction g_original_on_map_query_response = nullptr;

#if defined(__aarch64__)
void hooked_on_map_query_response(
    void *self,
    int32_t rpc_id,
    void *response,
    const void *method_info
) {
    if (g_map_query_hook_context != nullptr) {
        mark_world_dirty(*g_map_query_hook_context);
    }
    if (g_original_on_map_query_response != nullptr) {
        g_original_on_map_query_response(self, rpc_id, response, method_info);
    }
}
#endif

// Read-only diagnostic: log the first 16 bytes at the resolved code pointer so
// the prologue constant above can be filled for the pinned build. Safe to call
// without installing anything.
void log_map_query_prologue(const void *code) {
    if (code == nullptr) return;
    uint32_t words[4]{};
    memcpy(words, code, sizeof(words));
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "map query prologue target=%p bytes={0x%08X, 0x%08X, 0x%08X, 0x%08X}",
        code, words[0], words[1], words[2], words[3]
    );
}

bool install_runtime_map_query_hook(ProbeContext &context) {
#if defined(__aarch64__)
    RuntimeBinding binding = runtime_binding_snapshot(context);
    if (binding.map_query_hook_installed) {
        g_map_query_hook_context = &context;
        return true;
    }
    if (!binding.exact_build_verified || !binding.map_query_hook_binding_verified) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "runtime map query hook refused exact_build=%d binding_verified=%d",
            binding.exact_build_verified ? 1 : 0,
            binding.map_query_hook_binding_verified ? 1 : 0
        );
        return false;
    }
    const void *code = throw_method_code_pointer(binding.map_query_on_response);
    if (code == nullptr) return false;
    // Always log the live prologue so it can be captured even on the first run
    // (when the constant is still the placeholder and the install will refuse).
    log_map_query_prologue(code);
    // TODO(device): replace the placeholder with the exact 16-byte (4x uint32
    // little-endian) ARM64 prologue of OnMapQueryResponse read from live memory
    // (see the value logged by log_map_query_prologue above). Until then the
    // prologue memcmp in install_inline_hook fails and the hook safely refuses,
    // so the observer keeps running on the mode-C poll fallback.
    const uint32_t on_map_query_response_prologue[4] = {0U, 0U, 0U, 0U};
    g_map_query_hook_context = &context;
    const bool installed = install_inline_hook(
        &g_map_query_hook,
        code,
        on_map_query_response_prologue,
        reinterpret_cast<const void *>(&hooked_on_map_query_response)
    );
    if (!installed) {
        g_map_query_hook_context = nullptr;
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "runtime map query hook installed=0 (fill kOnMapQueryResponsePrologue)"
        );
        return false;
    }
    g_original_on_map_query_response = reinterpret_cast<OnMapQueryResponseFunction>(
        g_map_query_hook.trampoline
    );
    {
        std::lock_guard<std::mutex> lock(context.binding_mutex);
        context.binding.map_query_hook_installed = true;
    }
    __android_log_print(ANDROID_LOG_INFO, kLogTag, "runtime map query hook installed=1");
    return true;
#else
    (void)context;
    __android_log_print(ANDROID_LOG_WARN, kLogTag, "runtime map query hook unsupported ABI");
    return false;
#endif
}
