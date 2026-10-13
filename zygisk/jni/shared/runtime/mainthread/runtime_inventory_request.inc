bool request_main_thread_inventory(
    ProbeContext &context,
    pogo_runtime::RuntimeInventoryObservation *observation
) {
    if (observation == nullptr || !context.main_thread_bridge.ready ||
        context.main_thread_bridge.java_vm == nullptr) return false;
    std::unique_lock<std::mutex> lock(context.main_thread_bridge.mutex);
    if (context.main_thread_bridge.pending_token != 0U) return false;
    const uint64_t dirty_generation = pogo_runtime_world_state::mark_dirty(
        pogo_runtime_world_state::kInventorySource
    );
    const uint64_t token = context.main_thread_bridge.next_token++;
    context.main_thread_bridge.pending_token = token;
    context.main_thread_bridge.pending_type = MainThreadTaskType::kReadInventory;
    context.main_thread_bridge.pending_namespace.clear();
    context.main_thread_bridge.pending_class_name.clear();
    context.main_thread_bridge.pending_result = nullptr;
    context.main_thread_bridge.pending_inventory.items.clear();
    context.main_thread_bridge.pending_done = false;
    context.main_thread_bridge.pending_success = false;

    JNIEnv *env = nullptr;
    bool attached = false;
    if (context.main_thread_bridge.java_vm->GetEnv(
            reinterpret_cast<void **>(&env), JNI_VERSION_1_6
        ) != JNI_OK) {
        if (context.main_thread_bridge.java_vm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
            context.main_thread_bridge.pending_token = 0U;
            return false;
        }
        attached = true;
    }
    const jboolean posted = env->CallStaticBooleanMethod(
        static_cast<jclass>(context.main_thread_bridge.bridge_class),
        context.main_thread_bridge.post_method,
        static_cast<jlong>(token)
    );
    const bool post_ok = java_call_succeeded(env, "post inventory main-thread task") &&
        posted == JNI_TRUE;
    if (attached) context.main_thread_bridge.java_vm->DetachCurrentThread();
    if (!post_ok) {
        context.main_thread_bridge.pending_token = 0U;
        return false;
    }
    const bool completed = context.main_thread_bridge.condition.wait_for(
        lock,
        std::chrono::seconds(5),
        [&context, token]() {
            return context.main_thread_bridge.pending_token != token ||
                context.main_thread_bridge.pending_done;
        }
    );
    if (!completed || context.main_thread_bridge.pending_token != token ||
        !context.main_thread_bridge.pending_success) {
        const RuntimeBinding binding = runtime_binding_snapshot(context);
        (void)pogo_runtime_world_state::commit_inventory(
            binding.owner_generation,
            dirty_generation,
            static_cast<uint64_t>(now_elapsed_nanos()),
            pogo_runtime_world_state::Availability::kError,
            pogo_runtime::RuntimeInventoryObservation{},
            "inventory_snapshot_failed"
        );
        context.main_thread_bridge.pending_token = 0U;
        return false;
    }
    *observation = context.main_thread_bridge.pending_inventory;
    const RuntimeInventoryBallCounts balls = runtime_inventory_ball_counts(*observation);
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "main-thread inventory completed stacks=%zu curated_total=%d "
        "poke_ball_count=%d great_ball_count=%d ultra_ball_count=%d master_ball_count=%d",
        observation->items.size(),
        observation->used_slots,
        balls.poke_ball,
        balls.great_ball,
        balls.ultra_ball,
        balls.master_ball
    );
    const RuntimeBinding binding = runtime_binding_snapshot(context);
    (void)pogo_runtime_world_state::commit_inventory(
        binding.owner_generation,
        dirty_generation,
        static_cast<uint64_t>(now_elapsed_nanos()),
        observation->items.empty()
            ? pogo_runtime_world_state::Availability::kEmpty
            : pogo_runtime_world_state::Availability::kReady,
        *observation,
        nullptr
    );
    context.main_thread_bridge.pending_token = 0U;
    context.main_thread_bridge.pending_done = false;
    return true;
}
