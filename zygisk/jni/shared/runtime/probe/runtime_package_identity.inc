bool clear_identity_jni_exception(JNIEnv *env, const char *reason) {
    if (env == nullptr || !env->ExceptionCheck()) return false;
    env->ExceptionClear();
    __android_log_print(
        ANDROID_LOG_WARN,
        kLogTag,
        "runtime identity: JNI exception reason=%s",
        reason == nullptr ? "unknown" : reason
    );
    return true;
}

jobject current_runtime_application(JNIEnv *env) {
    if (env == nullptr) return nullptr;
    jclass activity_thread = env->FindClass("android/app/ActivityThread");
    if (clear_identity_jni_exception(env, "activity_thread_class") ||
        activity_thread == nullptr) return nullptr;
    const jmethodID current_application = env->GetStaticMethodID(
        activity_thread,
        "currentApplication",
        "()Landroid/app/Application;"
    );
    if (clear_identity_jni_exception(env, "current_application_method") ||
        current_application == nullptr) return nullptr;
    jobject application = env->CallStaticObjectMethod(activity_thread, current_application);
    if (clear_identity_jni_exception(env, "current_application_call")) return nullptr;
    return application;
}

bool read_runtime_version_code(
    JNIEnv *env,
    jobject application,
    uint64_t *version_code,
    char *reason,
    size_t reason_size
) {
    if (env == nullptr || application == nullptr || version_code == nullptr) {
        copy_string(reason, reason_size, "version_application_unavailable");
        return false;
    }
    jclass application_class = env->GetObjectClass(application);
    const jmethodID get_package_name = application_class == nullptr ? nullptr :
        env->GetMethodID(application_class, "getPackageName", "()Ljava/lang/String;");
    const jmethodID get_package_manager = application_class == nullptr ? nullptr :
        env->GetMethodID(
            application_class,
            "getPackageManager",
            "()Landroid/content/pm/PackageManager;"
        );
    if (clear_identity_jni_exception(env, "application_package_methods") ||
        get_package_name == nullptr || get_package_manager == nullptr) {
        copy_string(reason, reason_size, "version_package_api_unavailable");
        return false;
    }
    jstring package_name = static_cast<jstring>(
        env->CallObjectMethod(application, get_package_name)
    );
    jobject package_manager = env->CallObjectMethod(application, get_package_manager);
    if (clear_identity_jni_exception(env, "application_package_values") ||
        package_name == nullptr || package_manager == nullptr) {
        copy_string(reason, reason_size, "version_package_values_unavailable");
        return false;
    }
    const char *package_chars = env->GetStringUTFChars(package_name, nullptr);
    const bool package_matches = package_chars != nullptr &&
        strcmp(package_chars, pogo_runtime_profile::kPackageName) == 0;
    if (package_chars != nullptr) env->ReleaseStringUTFChars(package_name, package_chars);
    if (!package_matches) {
        copy_string(reason, reason_size, "version_package_mismatch");
        return false;
    }
    jclass package_manager_class = env->GetObjectClass(package_manager);
    const jmethodID get_package_info = package_manager_class == nullptr ? nullptr :
        env->GetMethodID(
            package_manager_class,
            "getPackageInfo",
            "(Ljava/lang/String;I)Landroid/content/pm/PackageInfo;"
        );
    if (clear_identity_jni_exception(env, "package_info_method") ||
        get_package_info == nullptr) {
        copy_string(reason, reason_size, "version_package_info_api_unavailable");
        return false;
    }
    jobject package_info = env->CallObjectMethod(
        package_manager,
        get_package_info,
        package_name,
        0
    );
    if (clear_identity_jni_exception(env, "package_info_call") || package_info == nullptr) {
        copy_string(reason, reason_size, "version_package_info_unavailable");
        return false;
    }
    jclass package_info_class = env->GetObjectClass(package_info);
    const jmethodID get_long_version_code = package_info_class == nullptr ? nullptr :
        env->GetMethodID(package_info_class, "getLongVersionCode", "()J");
    if (clear_identity_jni_exception(env, "long_version_code_method") ||
        get_long_version_code == nullptr) {
        copy_string(reason, reason_size, "version_long_api_unavailable");
        return false;
    }
    const jlong observed = env->CallLongMethod(package_info, get_long_version_code);
    if (clear_identity_jni_exception(env, "long_version_code_call") || observed < 0) {
        copy_string(reason, reason_size, "version_value_unavailable");
        return false;
    }
    *version_code = static_cast<uint64_t>(observed);
    copy_string(
        reason,
        reason_size,
        *version_code == pogo_runtime_profile::kVersionCode
            ? "version_verified" : "version_code_mismatch"
    );
    return true;
}
