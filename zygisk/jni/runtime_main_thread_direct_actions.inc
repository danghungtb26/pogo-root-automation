MainThreadActionOutcome request_main_thread_direct_action(
    ProbeContext &context,
    MainThreadTaskType task_type,
    uint64_t encounter_id,
    const std::string &fort_id
) {
    if (!context.main_thread_bridge.ready || context.main_thread_bridge.java_vm == nullptr ||
        (task_type == MainThreadTaskType::kDirectCatch && encounter_id == 0U) ||
        (task_type == MainThreadTaskType::kSpin && fort_id.empty())) {
        return MainThreadActionOutcome::kRejected;
    }
    std::unique_lock<std::mutex> lock(context.main_thread_bridge.mutex);
    if (context.main_thread_bridge.pending_token != 0U) return MainThreadActionOutcome::kRejected;
    const uint64_t token = context.main_thread_bridge.next_token++;
    context.main_thread_bridge.pending_token = token;
    context.main_thread_bridge.pending_type = task_type;
    context.main_thread_bridge.pending_namespace.clear();
    context.main_thread_bridge.pending_class_name.clear();
    context.main_thread_bridge.pending_encounter_id = encounter_id;
    context.main_thread_bridge.pending_fort_id = fort_id;
    context.main_thread_bridge.pending_result = nullptr;
    context.main_thread_bridge.pending_done = false;
    context.main_thread_bridge.pending_success = false;
    context.main_thread_bridge.pending_action_outcome = MainThreadActionOutcome::kRejected;

    JNIEnv *env = nullptr;
    bool attached = false;
    if (context.main_thread_bridge.java_vm->GetEnv(
            reinterpret_cast<void **>(&env), JNI_VERSION_1_6
        ) != JNI_OK) {
        if (context.main_thread_bridge.java_vm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
            context.main_thread_bridge.pending_token = 0U;
            return MainThreadActionOutcome::kRejected;
        }
        attached = true;
    }
    const jboolean posted = env->CallStaticBooleanMethod(
        static_cast<jclass>(context.main_thread_bridge.bridge_class),
        context.main_thread_bridge.post_method,
        static_cast<jlong>(token)
    );
    const bool post_ok = java_call_succeeded(env, "post direct map action") && posted == JNI_TRUE;
    if (attached) context.main_thread_bridge.java_vm->DetachCurrentThread();
    if (!post_ok) {
        context.main_thread_bridge.pending_token = 0U;
        return MainThreadActionOutcome::kRejected;
    }
    const bool completed = context.main_thread_bridge.condition.wait_for(
        lock,
        std::chrono::seconds(5),
        [&context, token]() {
            return context.main_thread_bridge.pending_token != token ||
                context.main_thread_bridge.pending_done;
        }
    );
    const MainThreadActionOutcome outcome = completed &&
        context.main_thread_bridge.pending_token == token
        ? context.main_thread_bridge.pending_action_outcome
        : MainThreadActionOutcome::kIndeterminate;
    context.main_thread_bridge.pending_token = 0U;
    context.main_thread_bridge.pending_done = false;
    context.main_thread_bridge.pending_encounter_id = 0U;
    context.main_thread_bridge.pending_fort_id.clear();
    __android_log_print(
        outcome == MainThreadActionOutcome::kInvoked ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "main-thread direct map action type=%u encounter=%llu fort=%s outcome=%u",
        static_cast<unsigned>(task_type),
        static_cast<unsigned long long>(encounter_id),
        fort_id.c_str(),
        static_cast<unsigned>(outcome)
    );
    return outcome;
}

MainThreadActionOutcome request_main_thread_direct_catch(
    ProbeContext &context,
    uint64_t encounter_id
) {
    return request_main_thread_direct_action(
        context, MainThreadTaskType::kDirectCatch, encounter_id, ""
    );
}

MainThreadActionOutcome request_main_thread_spin(
    ProbeContext &context,
    const std::string &fort_id
) {
    return request_main_thread_direct_action(
        context, MainThreadTaskType::kSpin, 0U, fort_id
    );
}
