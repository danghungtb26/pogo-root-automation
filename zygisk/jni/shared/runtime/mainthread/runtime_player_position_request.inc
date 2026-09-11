bool request_main_thread_player_position(
    ProbeContext &context,
    double *latitude,
    double *longitude
) {
    if (latitude == nullptr || longitude == nullptr || !context.main_thread_bridge.ready ||
        context.main_thread_bridge.java_vm == nullptr) return false;
    std::unique_lock<std::mutex> lock(context.main_thread_bridge.mutex);
    if (context.main_thread_bridge.pending_token != 0U) return false;
    const uint64_t token = context.main_thread_bridge.next_token++;
    context.main_thread_bridge.pending_token = token;
    context.main_thread_bridge.pending_type = MainThreadTaskType::kReadPlayerPosition;
    context.main_thread_bridge.pending_namespace.clear();
    context.main_thread_bridge.pending_class_name.clear();
    context.main_thread_bridge.pending_result = nullptr;
    context.main_thread_bridge.pending_player_latitude = 0.0;
    context.main_thread_bridge.pending_player_longitude = 0.0;
    context.main_thread_bridge.pending_has_player_position = false;
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
    const bool post_ok = java_call_succeeded(env, "post player position main-thread task") &&
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
        !context.main_thread_bridge.pending_success ||
        !context.main_thread_bridge.pending_has_player_position) {
        context.main_thread_bridge.pending_token = 0U;
        return false;
    }
    *latitude = context.main_thread_bridge.pending_player_latitude;
    *longitude = context.main_thread_bridge.pending_player_longitude;
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "main-thread player position completed lat=%.7f lng=%.7f",
        *latitude,
        *longitude
    );
    context.main_thread_bridge.pending_token = 0U;
    context.main_thread_bridge.pending_done = false;
    context.main_thread_bridge.pending_has_player_position = false;
    return true;
}
