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
    pogo_runtime::RuntimeInventoryObservation inventory;
    bool task_success = false;
    MainThreadActionOutcome action_outcome = MainThreadActionOutcome::kRejected;
    uint64_t encounter_id = 0U;
    std::string fort_id;
    uint64_t pokemon_id = 0U;
    std::string transfer_command_id;
    int32_t discard_item_id = 0;
    int32_t discard_amount = 0;
    std::string discard_command_id;
    double player_latitude = 0.0;
    double player_longitude = 0.0;
    {
        std::lock_guard<std::mutex> lock(context->main_thread_bridge.mutex);
        encounter_id = context->main_thread_bridge.pending_encounter_id;
        fort_id = context->main_thread_bridge.pending_fort_id;
        pokemon_id = context->main_thread_bridge.pending_pokemon_id;
        transfer_command_id = context->main_thread_bridge.pending_transfer_command_id;
        discard_item_id = context->main_thread_bridge.pending_item_id;
        discard_amount = context->main_thread_bridge.pending_discard_amount;
        discard_command_id = context->main_thread_bridge.pending_discard_command_id;
    }
    if (task_type == MainThreadTaskType::kFindObject) {
        result = find_unity_object_on_main_thread(*context, name_space.c_str(), class_name.c_str());
        task_success = result != nullptr;
        if (task_success && name_space == "Niantic.Holoholo" && class_name == "MapSceneContext") {
            std::lock_guard<std::mutex> binding_lock(context->binding_mutex);
            context->binding.map_scene_context = result;
        }
    } else if (task_type == MainThreadTaskType::kReadMap) {
        task_success = read_runtime_map_snapshot_on_main_thread(*context, &nearby, &forts);
    } else if (task_type == MainThreadTaskType::kReadInventory) {
        const RuntimeBinding binding = runtime_binding_snapshot(*context);
        task_success = read_runtime_inventory(context->il2cpp_api, binding, &inventory);
    } else if (task_type == MainThreadTaskType::kReadPlayerPosition) {
        const RuntimeBinding binding = runtime_binding_snapshot(*context);
        task_success = read_runtime_player_position(
            context->il2cpp_api,
            binding,
            &player_latitude,
            &player_longitude
        );
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
    } else if (task_type == MainThreadTaskType::kPollDirectCatch) {
        const RuntimeDirectCatchPromisePollResult poll_result =
            poll_pending_direct_catch_on_main_thread(*context);
        task_success = poll_result != RuntimeDirectCatchPromisePollResult::kNoPending;
        action_outcome = poll_result == RuntimeDirectCatchPromisePollResult::kWaiting ||
                poll_result == RuntimeDirectCatchPromisePollResult::kCompleted
            ? MainThreadActionOutcome::kInvoked
            : poll_result == RuntimeDirectCatchPromisePollResult::kNoPending
                ? MainThreadActionOutcome::kRejected
                : MainThreadActionOutcome::kIndeterminate;
    } else if (task_type == MainThreadTaskType::kPollSpin) {
        const RuntimeSpinPromisePollResult poll_result =
            poll_pending_runtime_spin_on_main_thread(*context);
        task_success = poll_result != RuntimeSpinPromisePollResult::kNoPending;
        action_outcome = poll_result == RuntimeSpinPromisePollResult::kWaiting ||
                poll_result == RuntimeSpinPromisePollResult::kCompleted
            ? MainThreadActionOutcome::kInvoked
            : poll_result == RuntimeSpinPromisePollResult::kNoPending
                ? MainThreadActionOutcome::kRejected
                : MainThreadActionOutcome::kIndeterminate;
    } else if (task_type == MainThreadTaskType::kPollEncounter) {
        action_outcome = poll_runtime_encounter_on_main_thread(*context, encounter_id);
        task_success = action_outcome != MainThreadActionOutcome::kRejected;
    } else if (task_type == MainThreadTaskType::kPollTransfer) {
        const RuntimeTransferPromisePollResult poll_result =
            poll_pending_runtime_transfer_on_main_thread(*context);
        task_success = poll_result != RuntimeTransferPromisePollResult::kNoPending;
        action_outcome = poll_result == RuntimeTransferPromisePollResult::kWaiting ||
                poll_result == RuntimeTransferPromisePollResult::kCompleted
            ? MainThreadActionOutcome::kInvoked
            : poll_result == RuntimeTransferPromisePollResult::kNoPending
                ? MainThreadActionOutcome::kRejected
                : MainThreadActionOutcome::kIndeterminate;
    } else if (task_type == MainThreadTaskType::kStartTransfer) {
        action_outcome = start_runtime_transfer_on_main_thread(
            *context,
            pokemon_id,
            transfer_command_id.c_str()
        );
        task_success = action_outcome == MainThreadActionOutcome::kInvoked;
    } else if (task_type == MainThreadTaskType::kPollDiscard) {
        const RuntimeDiscardPromisePollResult poll_result =
            poll_pending_runtime_discard_on_main_thread(*context);
        task_success = poll_result != RuntimeDiscardPromisePollResult::kNoPending;
        action_outcome = poll_result == RuntimeDiscardPromisePollResult::kWaiting ||
                poll_result == RuntimeDiscardPromisePollResult::kCompleted
            ? MainThreadActionOutcome::kInvoked
            : poll_result == RuntimeDiscardPromisePollResult::kNoPending
                ? MainThreadActionOutcome::kRejected
                : MainThreadActionOutcome::kIndeterminate;
    } else if (task_type == MainThreadTaskType::kStartDiscard) {
        action_outcome = start_runtime_discard_on_main_thread(
            *context,
            discard_item_id,
            discard_amount,
            discard_command_id.c_str()
        );
        task_success = action_outcome == MainThreadActionOutcome::kInvoked;
    } else if (task_type == MainThreadTaskType::kSpin) {
        action_outcome = spin_runtime_pokestop_on_main_thread(*context, fort_id);
        task_success = action_outcome == MainThreadActionOutcome::kInvoked;
    }
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "main-thread task type=%u target=%s.%s success=%d result=%p nearby=%zu forts=%zu inventory_stacks=%zu "
        "discard_item=%d discard_amount=%d",
        static_cast<unsigned>(task_type),
        name_space.c_str(),
        class_name.c_str(),
        task_success ? 1 : 0,
        result,
        nearby.spawns.size(),
        forts.forts.size(),
        inventory.items.size(),
        discard_item_id,
        discard_amount
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
        } else if (task_type == MainThreadTaskType::kReadInventory) {
            context->main_thread_bridge.pending_inventory = std::move(inventory);
        } else if (task_type == MainThreadTaskType::kReadPlayerPosition) {
            context->main_thread_bridge.pending_has_player_position = task_success;
            context->main_thread_bridge.pending_player_latitude = player_latitude;
            context->main_thread_bridge.pending_player_longitude = player_longitude;
        }
        context->main_thread_bridge.pending_success = task_success;
        context->main_thread_bridge.pending_action_outcome = action_outcome;
        context->main_thread_bridge.pending_done = true;
    }
    context->main_thread_bridge.condition.notify_all();
}
