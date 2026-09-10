jobject load_controller_bridge_class(JNIEnv *env) {
    if (env == nullptr) return nullptr;
    jclass activity_thread = env->FindClass("android/app/ActivityThread");
    if (!java_call_succeeded(env, "FindClass ActivityThread") || activity_thread == nullptr) return nullptr;
    const jmethodID current_application = env->GetStaticMethodID(
        activity_thread, "currentApplication", "()Landroid/app/Application;"
    );
    if (!java_call_succeeded(env, "GetStaticMethodID currentApplication") || current_application == nullptr) {
        return nullptr;
    }
    jobject application = env->CallStaticObjectMethod(activity_thread, current_application);
    if (!java_call_succeeded(env, "currentApplication") || application == nullptr) return nullptr;
    jobject package_context = load_bridge_from_package_context(env, application);
    if (package_context != nullptr) return package_context;

    jclass application_class = env->GetObjectClass(application);
    const jmethodID get_package_manager = env->GetMethodID(
        application_class, "getPackageManager", "()Landroid/content/pm/PackageManager;"
    );
    const jmethodID get_code_cache_dir = env->GetMethodID(
        application_class, "getCodeCacheDir", "()Ljava/io/File;"
    );
    const jmethodID get_class_loader = env->GetMethodID(
        application_class, "getClassLoader", "()Ljava/lang/ClassLoader;"
    );
    if (!java_call_succeeded(env, "Application methods") || get_package_manager == nullptr ||
        get_code_cache_dir == nullptr || get_class_loader == nullptr) return nullptr;
    jobject package_manager = env->CallObjectMethod(application, get_package_manager);
    jobject code_cache = env->CallObjectMethod(application, get_code_cache_dir);
    jobject parent_loader = env->CallObjectMethod(application, get_class_loader);
    if (!java_call_succeeded(env, "Application paths") || package_manager == nullptr ||
        code_cache == nullptr || parent_loader == nullptr) return nullptr;

    jstring controller_package = env->NewStringUTF("dev.pogoroot.automation");
    if (!java_call_succeeded(env, "NewString controller package") || controller_package == nullptr) return nullptr;
    jobject application_info = resolve_controller_application_info(
        env, package_manager, controller_package
    );
    jstring apk_path = nullptr;
    if (application_info != nullptr) {
        jclass application_info_class = env->GetObjectClass(application_info);
        const jfieldID source_dir = env->GetFieldID(
            application_info_class, "sourceDir", "Ljava/lang/String;"
        );
        if (java_call_succeeded(env, "ApplicationInfo sourceDir") && source_dir != nullptr) {
            apk_path = static_cast<jstring>(env->GetObjectField(application_info, source_dir));
        }
    }
    if (apk_path == nullptr) {
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "controller APK path unavailable through PackageManager; trying shared bridge APK"
        );
        apk_path = env->NewStringUTF("/data/local/tmp/pogo_root_automation_bridge.apk");
    }
    return load_bridge_from_apk_path(env, parent_loader, code_cache, apk_path);
}
