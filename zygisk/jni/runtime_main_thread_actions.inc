MainThreadActionOutcome request_main_thread_open_encounter(
    ProbeContext &context,
    uint64_t encounter_id
) {
    if (!context.main_thread_bridge.ready || context.main_thread_bridge.java_vm == nullptr ||
        encounter_id == 0U) return MainThreadActionOutcome::kRejected;
    std::unique_lock<std::mutex> lock(context.main_thread_bridge.mutex);
    if (context.main_thread_bridge.pending_token != 0U) return MainThreadActionOutcome::kRejected;
    const uint64_t token = context.main_thread_bridge.next_token++;
    context.main_thread_bridge.pending_token = token;
    context.main_thread_bridge.pending_type = MainThreadTaskType::kOpenEncounter;
    context.main_thread_bridge.pending_namespace.clear();
    context.main_thread_bridge.pending_class_name.clear();
    context.main_thread_bridge.pending_encounter_id = encounter_id;
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
    const bool post_ok = java_call_succeeded(env, "post open encounter task") && posted == JNI_TRUE;
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
    __android_log_print(
        outcome == MainThreadActionOutcome::kInvoked ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "main-thread open encounter completed id=%llu outcome=%u",
        static_cast<unsigned long long>(encounter_id),
        static_cast<unsigned>(outcome)
    );
    return outcome;
}

MainThreadActionOutcome request_main_thread_catch(
    ProbeContext &context,
    uint64_t encounter_id
) {
    if (!context.main_thread_bridge.ready || context.main_thread_bridge.java_vm == nullptr ||
        encounter_id == 0U) return MainThreadActionOutcome::kRejected;
    std::unique_lock<std::mutex> lock(context.main_thread_bridge.mutex);
    if (context.main_thread_bridge.pending_token != 0U) return MainThreadActionOutcome::kRejected;
    const uint64_t token = context.main_thread_bridge.next_token++;
    context.main_thread_bridge.pending_token = token;
    context.main_thread_bridge.pending_type = MainThreadTaskType::kCatch;
    context.main_thread_bridge.pending_namespace.clear();
    context.main_thread_bridge.pending_class_name.clear();
    context.main_thread_bridge.pending_encounter_id = encounter_id;
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
    const bool post_ok = java_call_succeeded(env, "post catch task") && posted == JNI_TRUE;
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
    __android_log_print(
        outcome == MainThreadActionOutcome::kInvoked ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "main-thread catch completed id=%llu outcome=%u",
        static_cast<unsigned long long>(encounter_id),
        static_cast<unsigned>(outcome)
    );
    return outcome;
}

MainThreadActionOutcome request_main_thread_use_berry(
    ProbeContext &context,
    uint64_t encounter_id,
    int32_t item_id
) {
    if (!context.main_thread_bridge.ready || context.main_thread_bridge.java_vm == nullptr ||
        encounter_id == 0U || item_id <= 0) return MainThreadActionOutcome::kRejected;
    std::unique_lock<std::mutex> lock(context.main_thread_bridge.mutex);
    if (context.main_thread_bridge.pending_token != 0U) return MainThreadActionOutcome::kRejected;
    const uint64_t token = context.main_thread_bridge.next_token++;
    context.main_thread_bridge.pending_token = token;
    context.main_thread_bridge.pending_type = MainThreadTaskType::kUseBerry;
    context.main_thread_bridge.pending_namespace.clear();
    context.main_thread_bridge.pending_class_name.clear();
    context.main_thread_bridge.pending_encounter_id = encounter_id;
    context.main_thread_bridge.pending_item_id = item_id;
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
    const bool post_ok = java_call_succeeded(env, "post berry task") && posted == JNI_TRUE;
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
    context.main_thread_bridge.pending_item_id = 0;
    __android_log_print(
        outcome == MainThreadActionOutcome::kInvoked ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "main-thread berry completed encounter=%llu item=%d outcome=%u",
        static_cast<unsigned long long>(encounter_id),
        item_id,
        static_cast<unsigned>(outcome)
    );
    return outcome;
}
