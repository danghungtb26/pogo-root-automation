ProbeContext *g_main_thread_context = nullptr;

bool java_call_succeeded(JNIEnv *env, const char *label) {
    if (env == nullptr || !env->ExceptionCheck()) return true;
    __android_log_print(
        ANDROID_LOG_WARN,
        kLogTag,
        "main-thread bridge Java exception label=%s",
        label == nullptr ? "<unknown>" : label
    );
    env->ExceptionDescribe();
    env->ExceptionClear();
    return false;
}

jobject resolve_controller_application_info(
    JNIEnv *env,
    jobject package_manager,
    jstring controller_package
) {
    if (env == nullptr || package_manager == nullptr || controller_package == nullptr) return nullptr;
    jclass package_manager_class = env->GetObjectClass(package_manager);
    const jmethodID get_application_info = env->GetMethodID(
        package_manager_class,
        "getApplicationInfo",
        "(Ljava/lang/String;I)Landroid/content/pm/ApplicationInfo;"
    );
    if (!java_call_succeeded(env, "PackageManager methods") || get_application_info == nullptr) return nullptr;
    jobject application_info = env->CallObjectMethod(
        package_manager, get_application_info, controller_package, 0
    );
    if (!env->ExceptionCheck() && application_info != nullptr) return application_info;
    if (env->ExceptionCheck()) env->ExceptionClear();

    const jmethodID get_launch_intent = env->GetMethodID(
        package_manager_class,
        "getLaunchIntentForPackage",
        "(Ljava/lang/String;)Landroid/content/Intent;"
    );
    const jmethodID resolve_activity = env->GetMethodID(
        package_manager_class,
        "resolveActivity",
        "(Landroid/content/Intent;I)Landroid/content/pm/ResolveInfo;"
    );
    if (!java_call_succeeded(env, "PackageManager launch methods") ||
        get_launch_intent == nullptr || resolve_activity == nullptr) return nullptr;
    jobject launch_intent = env->CallObjectMethod(
        package_manager, get_launch_intent, controller_package
    );
    if (!java_call_succeeded(env, "getLaunchIntentForPackage") || launch_intent == nullptr) return nullptr;
    jobject resolve_info = env->CallObjectMethod(package_manager, resolve_activity, launch_intent, 0);
    if (!java_call_succeeded(env, "resolveActivity") || resolve_info == nullptr) return nullptr;
    jclass resolve_info_class = env->GetObjectClass(resolve_info);
    const jfieldID activity_info_field = env->GetFieldID(
        resolve_info_class, "activityInfo", "Landroid/content/pm/ActivityInfo;"
    );
    if (!java_call_succeeded(env, "ResolveInfo activityInfo") || activity_info_field == nullptr) return nullptr;
    jobject activity_info = env->GetObjectField(resolve_info, activity_info_field);
    if (!java_call_succeeded(env, "ResolveInfo activityInfo value") || activity_info == nullptr) return nullptr;
    jclass activity_info_class = env->GetObjectClass(activity_info);
    const jfieldID application_info_field = env->GetFieldID(
        activity_info_class, "applicationInfo", "Landroid/content/pm/ApplicationInfo;"
    );
    if (!java_call_succeeded(env, "ActivityInfo applicationInfo") || application_info_field == nullptr) return nullptr;
    return env->GetObjectField(activity_info, application_info_field);
}

jobject load_bridge_from_package_context(JNIEnv *env, jobject application) {
    if (env == nullptr || application == nullptr) return nullptr;
    jclass application_class = env->GetObjectClass(application);
    const jmethodID create_package_context = env->GetMethodID(
        application_class,
        "createPackageContext",
        "(Ljava/lang/String;I)Landroid/content/Context;"
    );
    jstring controller_package = env->NewStringUTF("dev.pogoroot.automation");
    if (create_package_context == nullptr || controller_package == nullptr) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return nullptr;
    }
    jobject controller_context = env->CallObjectMethod(
        application, create_package_context, controller_package, 1 | 2
    );
    if (env->ExceptionCheck() || controller_context == nullptr) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return nullptr;
    }
    jclass context_class = env->GetObjectClass(controller_context);
    const jmethodID get_class_loader = env->GetMethodID(
        context_class, "getClassLoader", "()Ljava/lang/ClassLoader;"
    );
    if (get_class_loader == nullptr) return nullptr;
    jobject class_loader = env->CallObjectMethod(controller_context, get_class_loader);
    if (env->ExceptionCheck() || class_loader == nullptr) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return nullptr;
    }
    jclass class_loader_class = env->GetObjectClass(class_loader);
    const jmethodID load_class = env->GetMethodID(
        class_loader_class, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;"
    );
    jstring bridge_name = env->NewStringUTF(
        "dev.pogoroot.automation.root.RuntimeMainThreadBridge"
    );
    if (load_class == nullptr || bridge_name == nullptr) return nullptr;
    jobject bridge_class = env->CallObjectMethod(class_loader, load_class, bridge_name);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return nullptr;
    }
    return bridge_class;
}

jobject load_bridge_from_apk_path(
    JNIEnv *env,
    jobject parent_loader,
    jobject code_cache,
    jstring apk_path
) {
    if (env == nullptr || parent_loader == nullptr || code_cache == nullptr || apk_path == nullptr) {
        return nullptr;
    }
    jclass file_class = env->GetObjectClass(code_cache);
    const jmethodID get_absolute_path = env->GetMethodID(
        file_class, "getAbsolutePath", "()Ljava/lang/String;"
    );
    if (!java_call_succeeded(env, "CodeCache methods") || get_absolute_path == nullptr) return nullptr;
    jstring optimized_directory = static_cast<jstring>(
        env->CallObjectMethod(code_cache, get_absolute_path)
    );
    if (!java_call_succeeded(env, "CodeCache path") || optimized_directory == nullptr) return nullptr;

    jclass dex_loader_class = env->FindClass("dalvik/system/DexClassLoader");
    if (!java_call_succeeded(env, "FindClass DexClassLoader") || dex_loader_class == nullptr) return nullptr;
    const jmethodID dex_loader_constructor = env->GetMethodID(
        dex_loader_class,
        "<init>",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/ClassLoader;)V"
    );
    if (!java_call_succeeded(env, "DexClassLoader methods") || dex_loader_constructor == nullptr) return nullptr;
    jobject dex_loader = env->NewObject(
        dex_loader_class,
        dex_loader_constructor,
        apk_path,
        optimized_directory,
        nullptr,
        parent_loader
    );
    if (!java_call_succeeded(env, "DexClassLoader constructor") || dex_loader == nullptr) return nullptr;

    jclass class_loader_class = env->GetObjectClass(dex_loader);
    const jmethodID load_class = env->GetMethodID(
        class_loader_class, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;"
    );
    jstring bridge_name = env->NewStringUTF(
        "dev.pogoroot.automation.root.RuntimeMainThreadBridge"
    );
    if (!java_call_succeeded(env, "DexClassLoader class methods") ||
        load_class == nullptr || bridge_name == nullptr) return nullptr;
    jobject bridge_class = env->CallObjectMethod(dex_loader, load_class, bridge_name);
    if (!java_call_succeeded(env, "DexClassLoader loadClass")) return nullptr;
    return bridge_class;
}

void main_thread_native_run(JNIEnv *, jobject, jlong token) {
    ProbeContext *context = g_main_thread_context;
    if (context == nullptr) return;
    Il2CppThread *attached_thread = nullptr;
    if (context->il2cpp_api.domain_get != nullptr && context->il2cpp_api.thread_attach != nullptr) {
        Il2CppDomain *domain = context->il2cpp_api.domain_get();
        if (domain != nullptr) attached_thread = context->il2cpp_api.thread_attach(domain);
    }
    const Il2CppThread *current_thread = context->il2cpp_api.thread_current == nullptr
        ? nullptr : context->il2cpp_api.thread_current();
    std::string name_space;
    std::string class_name;
    MainThreadTaskType task_type = MainThreadTaskType::kFindObject;
    {
        std::lock_guard<std::mutex> lock(context->main_thread_bridge.mutex);
        if (context->main_thread_bridge.pending_token != static_cast<uint64_t>(token)) {
            if (attached_thread != nullptr && context->il2cpp_api.thread_detach != nullptr) {
                context->il2cpp_api.thread_detach(attached_thread);
            }
            return;
        }
        task_type = context->main_thread_bridge.pending_type;
        name_space = context->main_thread_bridge.pending_namespace;
        class_name = context->main_thread_bridge.pending_class_name;
    }
    void *result = nullptr;
    pogo_runtime::RuntimeNearbyObservation nearby;
    pogo_runtime::RuntimeFortsObservation forts;
    bool task_success = false;
    MainThreadActionOutcome action_outcome = MainThreadActionOutcome::kRejected;
    uint64_t encounter_id = 0U;
    std::string fort_id;
    {
        std::lock_guard<std::mutex> lock(context->main_thread_bridge.mutex);
        encounter_id = context->main_thread_bridge.pending_encounter_id;
        fort_id = context->main_thread_bridge.pending_fort_id;
    }
    if (task_type == MainThreadTaskType::kFindObject) {
        result = find_unity_object_on_main_thread(*context, name_space.c_str(), class_name.c_str());
        task_success = result != nullptr;
    } else if (task_type == MainThreadTaskType::kReadMap) {
        task_success = read_runtime_map_snapshot_on_main_thread(*context, &nearby, &forts);
    } else if (task_type == MainThreadTaskType::kOpenEncounter) {
        action_outcome = open_runtime_encounter_on_main_thread(*context, encounter_id);
        task_success = action_outcome == MainThreadActionOutcome::kInvoked;
    } else if (task_type == MainThreadTaskType::kCatch) {
        action_outcome = throw_runtime_ball_on_main_thread(*context, encounter_id);
        task_success = action_outcome == MainThreadActionOutcome::kInvoked;
    } else if (task_type == MainThreadTaskType::kUseBerry) {
        int32_t item_id = 0;
        {
            std::lock_guard<std::mutex> lock(context->main_thread_bridge.mutex);
            item_id = context->main_thread_bridge.pending_item_id;
        }
        action_outcome = use_runtime_berry_on_main_thread(*context, encounter_id, item_id);
        task_success = action_outcome == MainThreadActionOutcome::kInvoked;
    } else if (task_type == MainThreadTaskType::kDirectCatch) {
        action_outcome = direct_catch_runtime_on_main_thread(*context, encounter_id);
        task_success = action_outcome == MainThreadActionOutcome::kInvoked;
    } else if (task_type == MainThreadTaskType::kSpin) {
        action_outcome = spin_runtime_pokestop_on_main_thread(*context, fort_id);
        task_success = action_outcome == MainThreadActionOutcome::kInvoked;
    }
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "main-thread task type=%u target=%s.%s success=%d result=%p nearby=%zu forts=%zu",
        static_cast<unsigned>(task_type),
        name_space.c_str(),
        class_name.c_str(),
        task_success ? 1 : 0,
        result,
        nearby.spawns.size(),
        forts.forts.size()
    );
    __android_log_print(
        current_thread == nullptr ? ANDROID_LOG_WARN : ANDROID_LOG_INFO,
        kLogTag,
        "main-thread IL2CPP registration present=%d thread=%p",
        current_thread == nullptr ? 0 : 1,
        current_thread
    );
    if (attached_thread != nullptr && context->il2cpp_api.thread_detach != nullptr) {
        context->il2cpp_api.thread_detach(attached_thread);
    }
    {
        std::lock_guard<std::mutex> lock(context->main_thread_bridge.mutex);
        if (context->main_thread_bridge.pending_token != static_cast<uint64_t>(token)) return;
        context->main_thread_bridge.pending_result = result;
        if (task_type == MainThreadTaskType::kReadMap) {
            context->main_thread_bridge.pending_nearby = std::move(nearby);
            context->main_thread_bridge.pending_forts = std::move(forts);
        }
        context->main_thread_bridge.pending_success = task_success;
        context->main_thread_bridge.pending_action_outcome = action_outcome;
        context->main_thread_bridge.pending_done = true;
    }
    context->main_thread_bridge.condition.notify_all();
}

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
        context.main_thread_bridge.pending_forts.forts.clear();
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
    if (!completed || context.main_thread_bridge.pending_token != token ||
        !context.main_thread_bridge.pending_success) {
        context.main_thread_bridge.pending_token = 0U;
        return false;
    }
    *observation = context.main_thread_bridge.pending_nearby;
    *forts = context.main_thread_bridge.pending_forts;
    context.main_thread_bridge.pending_token = 0U;
    context.main_thread_bridge.pending_done = false;
    return true;
}
