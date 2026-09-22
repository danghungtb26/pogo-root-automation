#include "runtime_readiness_retry.h"
namespace {
RuntimeReadinessRetry g_runtime_readiness_retry;
constexpr int kMapReadinessPollTimeoutMs = 500;

void maybe_probe_pending_map_readiness(ProbeContext &context) {
    if (!g_runtime_active.load()) return;

    if (g_runtime_readiness_retry.due(
            static_cast<uint64_t>(now_elapsed_nanos()), pogo_runtime_module::any_enabled())) {
        if (run_managed_runtime_diagnostic(context)) {
            g_runtime_readiness_retry.completed();
            pogo_runtime_module::probe_map_live(context);
            send_runtime_capability_update(context.fd, runtime_binding_snapshot(context),
                context.map_live_verified.load(std::memory_order_acquire));
        }
    }

    RuntimeBinding binding = runtime_binding_snapshot(context);
    const bool live_before = context.map_live_verified.load(std::memory_order_relaxed);

    if (!pogo_runtime_module::map_binding_ready(binding)) {
        static std::atomic<uint64_t> last_discovery_ns{0U};
        const uint64_t now = static_cast<uint64_t>(now_elapsed_nanos());
        constexpr uint64_t kIntervalNs = 500'000'000ULL;
        const uint64_t last = last_discovery_ns.load(std::memory_order_relaxed);
        if (last == 0U || now - last >= kIntervalNs) {
            last_discovery_ns.store(now, std::memory_order_relaxed);
            discover_scene_runtime_owners(context);
            binding = runtime_binding_snapshot(context);
        }
    } else if (!live_before) {
        static std::atomic<uint64_t> last_live_probe_ns{0U};
        const uint64_t now = static_cast<uint64_t>(now_elapsed_nanos());
        constexpr uint64_t kIntervalNs = 500'000'000ULL;
        const uint64_t last = last_live_probe_ns.load(std::memory_order_relaxed);
        if (last == 0U || now - last >= kIntervalNs) {
            last_live_probe_ns.store(now, std::memory_order_relaxed);
            pogo_runtime_module::probe_map_live(context);
        }
    }

    const bool live_now = context.map_live_verified.load(std::memory_order_relaxed);
    if (!live_before && live_now) {
        binding = runtime_binding_snapshot(context);
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "map instance live; publishing capabilities for native module autostart"
        );
        send_runtime_capability_update(
            context.fd,
            binding,
            true
        );
        (void)send_runtime_ui_status(context);
    }

    reconcile_runtime_desired_state(context);
}
}  // namespace
