// Main-thread bridge lifecycle and read request helpers. Kept separate from
// JNI dispatch so the bridge wiring file stays below the project line limit.

bool initialize_main_thread_bridge(ProbeContext &context, JNIEnv *env) {
    if (context.main_thread_bridge.ready) return true;
    if (env == nullptr) {
        __android_log_print(ANDROID_LOG_WARN, kLogTag, "main-thread bridge unavailable: no JNIEnv");
        return false;
    }
    JavaVM *java_vm = nullptr;
    if (env->GetJavaVM(&java_vm) != JNI_OK || java_vm == nullptr) {
        __android_log_print(ANDROID_LOG_WARN, kLogTag, "main-thread bridge unavailable: no JavaVM");
        return false;
    }
    context.main_thread_bridge.java_vm = java_vm;
    jobject local_class = load_controller_bridge_class(env);
    if (!java_call_succeeded(env, "load controller bridge") || local_class == nullptr) {
        __android_log_print(ANDROID_LOG_WARN, kLogTag, "main-thread bridge class load deferred");
        return false;
    }
    jclass bridge_class = static_cast<jclass>(local_class);
    const JNINativeMethod native_method = {
        const_cast<char *>("nativeRun"),
        const_cast<char *>("(J)V"),
        reinterpret_cast<void *>(&main_thread_native_run),
    };
    if (env->RegisterNatives(bridge_class, &native_method, 1) != JNI_OK ||
        !java_call_succeeded(env, "RegisterNatives")) return false;
    const jmethodID post_method = env->GetStaticMethodID(
        bridge_class, "post", "(J)Z"
    );
    if (!java_call_succeeded(env, "GetStaticMethodID post") || post_method == nullptr) return false;
    context.main_thread_bridge.java_vm = java_vm;
    context.main_thread_bridge.bridge_class = env->NewGlobalRef(bridge_class);
    context.main_thread_bridge.post_method = post_method;
    context.main_thread_bridge.ready = context.main_thread_bridge.bridge_class != nullptr;
    if (context.main_thread_bridge.ready) g_main_thread_context = &context;
    __android_log_print(
        context.main_thread_bridge.ready ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        kLogTag,
        "main-thread bridge initialized=%d",
        context.main_thread_bridge.ready ? 1 : 0
    );
    return context.main_thread_bridge.ready;
}

bool ensure_main_thread_bridge(ProbeContext &context) {
    if (context.main_thread_bridge.ready) return true;
    JavaVM *java_vm = context.main_thread_bridge.java_vm;
    if (java_vm == nullptr) return false;
    JNIEnv *env = nullptr;
    bool attached = false;
    if (java_vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
        if (java_vm->AttachCurrentThread(&env, nullptr) != JNI_OK || env == nullptr) return false;
        attached = true;
    }
    const bool ready = initialize_main_thread_bridge(context, env);
    if (attached) java_vm->DetachCurrentThread();
    return ready;
}

void shutdown_main_thread_bridge(ProbeContext &context) {
    {
        std::lock_guard<std::mutex> lock(context.main_thread_bridge.mutex);
        context.main_thread_bridge.pending_token = 0U;
        context.main_thread_bridge.pending_done = true;
        context.main_thread_bridge.pending_item_id = 0;
        context.main_thread_bridge.pending_discard_amount = 0;
        context.main_thread_bridge.pending_discard_command_id.clear();
        context.main_thread_bridge.pending_pokemon_id = 0U;
        context.main_thread_bridge.pending_transfer_command_id.clear();
        context.main_thread_bridge.pending_forts.forts.clear();
        context.main_thread_bridge.pending_inventory.items.clear();
    }
    context.main_thread_bridge.condition.notify_all();
    if (g_main_thread_context == &context) g_main_thread_context = nullptr;
    if (context.main_thread_bridge.java_vm != nullptr && context.main_thread_bridge.bridge_class != nullptr) {
        JNIEnv *env = nullptr;
        if (context.main_thread_bridge.java_vm->GetEnv(
                reinterpret_cast<void **>(&env), JNI_VERSION_1_6
            ) == JNI_OK && env != nullptr) {
            env->DeleteGlobalRef(context.main_thread_bridge.bridge_class);
        }
    }
    context.main_thread_bridge.bridge_class = nullptr;
    context.main_thread_bridge.post_method = nullptr;
    context.main_thread_bridge.ready = false;
}

void *request_main_thread_object(
    ProbeContext &context,
    const char *name_space,
    const char *class_name
) {
    if (!context.main_thread_bridge.ready || context.main_thread_bridge.java_vm == nullptr ||
        name_space == nullptr || class_name == nullptr) return nullptr;
    std::unique_lock<std::mutex> lock(context.main_thread_bridge.mutex);
    if (context.main_thread_bridge.pending_token != 0U) return nullptr;
    const uint64_t token = context.main_thread_bridge.next_token++;
    context.main_thread_bridge.pending_token = token;
    context.main_thread_bridge.pending_type = MainThreadTaskType::kFindObject;
    context.main_thread_bridge.pending_namespace = name_space;
    context.main_thread_bridge.pending_class_name = class_name;
    context.main_thread_bridge.pending_result = nullptr;
    context.main_thread_bridge.pending_done = false;
    context.main_thread_bridge.pending_success = false;
    JNIEnv *env = nullptr;
    bool attached = false;
    if (context.main_thread_bridge.java_vm->GetEnv(
            reinterpret_cast<void **>(&env), JNI_VERSION_1_6
        ) != JNI_OK) {
        if (context.main_thread_bridge.java_vm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
            context.main_thread_bridge.pending_token = 0U;
            return nullptr;
        }
        attached = true;
    }
    const jboolean posted = env->CallStaticBooleanMethod(
        static_cast<jclass>(context.main_thread_bridge.bridge_class),
        context.main_thread_bridge.post_method,
        static_cast<jlong>(token)
    );
    const bool post_ok = java_call_succeeded(env, "post main-thread task") && posted == JNI_TRUE;
    if (attached) context.main_thread_bridge.java_vm->DetachCurrentThread();
    if (!post_ok) {
        context.main_thread_bridge.pending_token = 0U;
        return nullptr;
    }
    const bool completed = context.main_thread_bridge.condition.wait_for(
        lock,
        std::chrono::seconds(5),
        [&context, token]() {
            return context.main_thread_bridge.pending_token != token ||
                context.main_thread_bridge.pending_done;
        }
    );
    void *result = completed && context.main_thread_bridge.pending_token == token &&
        context.main_thread_bridge.pending_success
        ? context.main_thread_bridge.pending_result
        : nullptr;
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "main-thread task completed target=%s.%s success=%d result=%p",
        name_space,
        class_name,
        result != nullptr ? 1 : 0,
        result
    );
    context.main_thread_bridge.pending_token = 0U;
    context.main_thread_bridge.pending_done = false;
    context.main_thread_bridge.pending_encounter_id = 0U;
    context.main_thread_bridge.pending_item_id = 0;
    context.main_thread_bridge.pending_namespace.clear();
    context.main_thread_bridge.pending_class_name.clear();
    return result;
}

bool request_main_thread_map_snapshot(
    ProbeContext &context,
    pogo_runtime::RuntimeNearbyObservation *observation,
    pogo_runtime::RuntimeFortsObservation *forts
) {
    if (observation == nullptr || forts == nullptr || !context.main_thread_bridge.ready ||
        context.main_thread_bridge.java_vm == nullptr) return false;
    std::unique_lock<std::mutex> lock(context.main_thread_bridge.mutex);
    if (context.main_thread_bridge.pending_token != 0U) return false;
    const uint64_t token = context.main_thread_bridge.next_token++;
    context.main_thread_bridge.pending_token = token;
    context.main_thread_bridge.pending_type = MainThreadTaskType::kReadMap;
    context.main_thread_bridge.pending_namespace.clear();
    context.main_thread_bridge.pending_class_name.clear();
    context.main_thread_bridge.pending_result = nullptr;
    context.main_thread_bridge.pending_nearby.spawns.clear();
    context.main_thread_bridge.pending_forts.forts.clear();
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
    const bool post_ok = java_call_succeeded(env, "post map main-thread task") && posted == JNI_TRUE;
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
    const bool committed = completed &&
        context.main_thread_bridge.pending_token == token &&
        context.main_thread_bridge.pending_done;
    if (committed) {
        // Forts and player location are independent of the fragile nearby walk.
        *observation = context.main_thread_bridge.pending_nearby;
        *forts = context.main_thread_bridge.pending_forts;
    }
    const bool success = committed && context.main_thread_bridge.pending_success;
    context.main_thread_bridge.pending_token = 0U;
    context.main_thread_bridge.pending_done = false;
    return success;
}
