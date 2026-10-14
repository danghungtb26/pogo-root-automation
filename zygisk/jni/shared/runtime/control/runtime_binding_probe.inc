void *binding_probe_thread(void *opaque_context) {
    auto *context = static_cast<ProbeContext *>(opaque_context);
    if (context == nullptr) return nullptr;

    RuntimeEvent event{};
    event.magic = kRuntimeEventMagic;
    event.protocol_version = kRuntimeProtocolVersion;
    event.event_type = static_cast<uint32_t>(RuntimeEventType::kBindingProbe);
    event.pid = context->pid;
    event.il2cpp_required_symbol_count = kRequiredIl2cppCoreSymbolCount;
    copy_string(event.process_name, sizeof(event.process_name), context->process_name);
    copy_string(event.translation_layer, sizeof(event.translation_layer), "none");

    // Mapping is diagnostic only. A mapped libil2cpp does not prove that
    // il2cpp_init returned, so do not resolve or call managed APIs here.
    if (find_mapping_path("libil2cpp.so", event.il2cpp_path,
            sizeof(event.il2cpp_path))) {
        event.probe_flags |= kIl2cppLoaded;
        copy_string(context->il2cpp_path, sizeof(context->il2cpp_path),
            event.il2cpp_path);
    }
    if (find_mapping_path("libunity.so", event.unity_path,
            sizeof(event.unity_path))) event.probe_flags |= kUnityLoaded;
    if (mapping_contains("libhoudini")) {
        event.probe_flags |= kHoudiniTranslation;
        copy_string(event.translation_layer, sizeof(event.translation_layer), "houdini");
    } else if (mapping_contains("libndk_translation")) {
        event.probe_flags |= kNdkTranslation;
        copy_string(event.translation_layer, sizeof(event.translation_layer), "ndk_translation");
    }

    event.probe_flags |= kProbeComplete;
    if (!write_full(context->fd, &event, sizeof(event))) {
        __android_log_print(
            ANDROID_LOG_ERROR,
            kLogTag,
            "failed to publish binding probe for pid=%d",
            event.pid
        );
    } else {
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "probe: il2cpp=%d symbols=%u core_required=%u assemblies=%u classes=%u candidates=%u translation=%s",
            (event.probe_flags & kIl2cppLoaded) != 0U,
            event.il2cpp_symbol_count,
            event.il2cpp_required_symbol_count,
            event.assembly_count,
            event.class_count,
            event.candidate_class_count,
            event.translation_layer
        );
    }

    const bool modules_registered = pogo_runtime_module::initialize_runtime_modules(context->fd);
    __android_log_print(
        modules_registered ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR,
        kLogTag,
        "runtime host module registration complete success=%d",
        modules_registered ? 1 : 0
    );
    if (!modules_registered) {
        pogo_runtime_bootstrap::Status failure;
        failure.phase = pogo_runtime_bootstrap::Phase::kFailed;
        failure.error_code = "runtime_module_registry_uninitialized";
        failure.message = "runtime feature module registry failed to initialize";
        if (!publish_runtime_bootstrap_status(*context, std::move(failure))) {
            __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                "failed to publish bootstrap module registry error");
        }
    }

    // Command receipt stays available while lifecycle and gameplay gates are
    // closed. A future verified lifecycle owner may advance bootstrap phase.
    runtime_command_channel_loop(*context);
    close(context->fd);
    delete context;
    return nullptr;
}
