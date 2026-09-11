static const char *const kMapEntityCellMethods[] = {
    "get_MapPlacesList", "get_NearbyPokemon", "get_AllCellWildPokemon",
    "get_AllCellWildTappable", "GetMapPokemon", "GetMapTappable",
};
static const char *const kMapEntityServiceMethods[] = {
    "get_Cells", "get_PlayerMapRoot", "get_OverheadMapRoot", "get_HoldingPoolRoot",
    "get_NearbyPokemon", "UpdatePois",
};
static const char *const kDynamicTappableMethods[] = {
    "get_Position", "get_Id", "get_EncounterId", "get_AssetKey", "get_TypeKey",
    "get_Location", "get_TappableLocation", "get_VisibleOnMap", "get_DespawnTimeMs",
    "get_IsEncounter", "get_CellId", "AnimateInteraction", "Remove", "Refresh",
};
static const char *const kDynamicTappablesServiceMethods[] = {
    "EndSession", "get_TappablesEnabled", "GetTappables", "TapTappable",
    "get_TappableTapped", "set_TappableTapped", "get_TappableAdded", "set_TappableAdded",
    "get_TappableRemoved", "set_TappableRemoved",
};
static const char *const kDynamicTappablesSessionMethods[] = {
    "GetTappable", "AddTappables", "AddTappable",
};
static const char *const kMapSceneContextMethods[] = {
    "RunInternal", "get_Instance", "get_MapEntityService", "get_MapScene", "Initialize", "Start", "Update",
};
static const char *const kMapSceneMethods[] = {
    "get_MapObjectsParent", "get_SkydomeParent", "get_SkydomeComponent", "SetMoonVisibility",
    "Initialize", "Start", "Update",
};
static const char *const kMapSceneViewServiceMethods[] = {
    "get_CurrentMapMode", "SwitchMapView",
};
static const char *const kMapContentHandlerMethods[] = {
    "get_MapEntityService", "get_MapPokemon", "get_NearbyPokemon",
};

[[maybe_unused]] void diagnose_map_runtime_classes(
    const Il2CppApi &api,
    const Il2CppAssembly **assemblies,
    size_t assembly_count
) {
    __android_log_print(ANDROID_LOG_INFO, kLogTag, "managed diagnostic: map diagnostics entered");
    static const ManagedDiagnosticClass candidates[] = {
        {"Niantic.Holoholo.Map", "MapEntityCell", kMapEntityCellMethods, sizeof(kMapEntityCellMethods) / sizeof(kMapEntityCellMethods[0])},
        {"Niantic.Holoholo.Map", "MapEntityService", kMapEntityServiceMethods, sizeof(kMapEntityServiceMethods) / sizeof(kMapEntityServiceMethods[0])},
        {"", "DynamicTappableController", kDynamicTappableMethods, sizeof(kDynamicTappableMethods) / sizeof(kDynamicTappableMethods[0])},
        {"Niantic.Holoholo.Services.DynamicTappables", "IDynamicTappablesService", kDynamicTappablesServiceMethods, sizeof(kDynamicTappablesServiceMethods) / sizeof(kDynamicTappablesServiceMethods[0])},
        {"Niantic.Holoholo.Services.DynamicTappables", "DynamicTappablesService", kDynamicTappablesServiceMethods, sizeof(kDynamicTappablesServiceMethods) / sizeof(kDynamicTappablesServiceMethods[0])},
        {"Niantic.Holoholo.Services.DynamicTappables", "IDynamicTappablesSession", kDynamicTappablesSessionMethods, sizeof(kDynamicTappablesSessionMethods) / sizeof(kDynamicTappablesSessionMethods[0])},
        {"Niantic.Holoholo.Services.DynamicTappables", "DynamicTappablesSession", kDynamicTappablesSessionMethods, sizeof(kDynamicTappablesSessionMethods) / sizeof(kDynamicTappablesSessionMethods[0])},
        {"Niantic.Holoholo", "MapSceneContext", kMapSceneContextMethods, sizeof(kMapSceneContextMethods) / sizeof(kMapSceneContextMethods[0])},
        {"Niantic.Holoholo.Map", "MapScene", kMapSceneMethods, sizeof(kMapSceneMethods) / sizeof(kMapSceneMethods[0])},
        {"Niantic.Holoholo.Map", "MapSceneViewService", kMapSceneViewServiceMethods, sizeof(kMapSceneViewServiceMethods) / sizeof(kMapSceneViewServiceMethods[0])},
        {"Niantic.Holoholo.Map", "MapContentHandler", kMapContentHandlerMethods, sizeof(kMapContentHandlerMethods) / sizeof(kMapContentHandlerMethods[0])},
    };
    for (const ManagedDiagnosticClass &candidate : candidates) {
        const Il2CppImage *image = nullptr;
        Il2CppClass *klass = find_runtime_class(
            api, assemblies, assembly_count, candidate.name_space, candidate.name, &image
        );
        if (klass == nullptr) {
            __android_log_print(
                ANDROID_LOG_INFO,
                kLogTag,
                "managed diagnostic: map class missing %s.%s",
                candidate.name_space,
                candidate.name
            );
            continue;
        }
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "managed diagnostic: map class found %s.%s image=%s ptr=%p",
            candidate.name_space,
            candidate.name,
            image == nullptr || api.image_get_name == nullptr
                ? "<unknown>"
                : api.image_get_name(image),
            klass
        );
        log_runtime_class_fields(api, klass, candidate.name);
        for (size_t index = 0U; index < candidate.method_count; ++index) {
            log_runtime_method_signature(api, klass, candidate.name, candidate.methods[index]);
        }
    }
}

[[maybe_unused]] void scan_runtime_map_class_names(
    const Il2CppApi &api,
    const Il2CppAssembly **assemblies,
    size_t assembly_count
) {
    if (assemblies == nullptr || api.assembly_get_image == nullptr ||
        api.image_get_class_count == nullptr || api.image_get_class == nullptr ||
        api.class_get_name == nullptr || api.class_get_namespace == nullptr) return;
    size_t matches = 0U;
    constexpr size_t kMaxMatches = 128U;
    for (size_t assembly_index = 0U; assembly_index < assembly_count; ++assembly_index) {
        const Il2CppAssembly *assembly = assemblies[assembly_index];
        const Il2CppImage *image = assembly == nullptr ? nullptr : api.assembly_get_image(assembly);
        if (image == nullptr) continue;
        const size_t class_count = api.image_get_class_count(image);
        for (size_t class_index = 0U; class_index < class_count && matches < kMaxMatches; ++class_index) {
            Il2CppClass *klass = api.image_get_class(image, class_index);
            const char *name = klass == nullptr ? nullptr : api.class_get_name(klass);
            const char *name_space = klass == nullptr ? nullptr : api.class_get_namespace(klass);
            if (!contains_ignore_case(name, "MapEntity") &&
                !contains_ignore_case(name, "DynamicTappable") &&
                !contains_ignore_case(name_space, "DynamicTappable")) continue;
            ++matches;
            __android_log_print(
                ANDROID_LOG_INFO,
                kLogTag,
                "managed diagnostic: map class scan image=%s namespace=%s name=%s ptr=%p",
                api.image_get_name == nullptr ? "<unknown>" : api.image_get_name(image),
                name_space == nullptr ? "" : name_space,
                name == nullptr ? "<unknown>" : name,
                klass
            );
        }
        if (matches >= kMaxMatches) break;
    }
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "managed diagnostic: map class scan matches=%zu truncated=%d",
        matches,
        matches >= kMaxMatches ? 1 : 0
    );
}
