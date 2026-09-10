bool contains_ignore_case(const char *value, const char *needle) {
    if (value == nullptr || needle == nullptr || needle[0] == '\0') return false;
    for (const char *start = value; *start != '\0'; ++start) {
        const char *left = start;
        const char *right = needle;
        while (*left != '\0' && *right != '\0' && ascii_lower(*left) == ascii_lower(*right)) {
            ++left;
            ++right;
        }
        if (*right == '\0') return true;
    }
    return false;
}

bool is_candidate_class(const char *name, const char *name_space) {
    const char *keywords[] = {
        "map", "camera", "projection", "coordinate", "location", "world",
        "input", "touch", "pointer", "pokemon", "spawn", "encounter",
        "fort", "nearby", "wild", "inventory"
    };
    for (const char *keyword : keywords) {
        if (contains_ignore_case(name, keyword) || contains_ignore_case(name_space, keyword)) return true;
    }
    return false;
}

void append_candidate(RuntimeEvent *event, const char *name_space, const char *name) {
    if (event == nullptr || name == nullptr || event->candidate_class_count >= kMaxCandidateClasses) return;
    char qualified[256]{};
    if (name_space != nullptr && name_space[0] != '\0') {
        snprintf(qualified, sizeof(qualified), "%s.%s", name_space, name);
    } else {
        snprintf(qualified, sizeof(qualified), "%s", name);
    }

    const size_t used = strnlen(event->candidate_classes, sizeof(event->candidate_classes));
    if (used >= sizeof(event->candidate_classes) - 1U) return;
    const char *separator = used == 0U ? "" : ";";
    const int written = snprintf(
        event->candidate_classes + used,
        sizeof(event->candidate_classes) - used,
        "%s%s",
        separator,
        qualified
    );
    if (written > 0 && static_cast<size_t>(written) < sizeof(event->candidate_classes) - used) {
        event->candidate_class_count += 1U;
    }
}

void append_candidate_method(
    RuntimeEvent *event,
    const char *name_space,
    const char *class_name,
    const char *method_name
) {
    if (event == nullptr || class_name == nullptr || method_name == nullptr ||
        event->candidate_class_count >= kMaxCandidateClasses) return;
    char qualified[256]{};
    if (name_space != nullptr && name_space[0] != '\0') {
        snprintf(qualified, sizeof(qualified), "%s.%s::%s", name_space, class_name, method_name);
    } else {
        snprintf(qualified, sizeof(qualified), "%s::%s", class_name, method_name);
    }

    const size_t used = strnlen(event->candidate_classes, sizeof(event->candidate_classes));
    if (used >= sizeof(event->candidate_classes) - 1U) return;
    const char *separator = used == 0U ? "" : ";";
    const int written = snprintf(
        event->candidate_classes + used,
        sizeof(event->candidate_classes) - used,
        "%s%s",
        separator,
        qualified
    );
    if (written > 0 && static_cast<size_t>(written) < sizeof(event->candidate_classes) - used) {
        event->candidate_class_count += 1U;
    }
}

bool is_candidate_method(const char *name) {
    const char *keywords[] = {
        "touch", "pointer", "input", "screen", "camera", "project",
        "world", "geo", "coordinate", "map", "ray", "gesture"
    };
    for (const char *keyword : keywords) {
        if (contains_ignore_case(name, keyword)) return true;
    }
    return false;
}

void survey_candidate_methods(
    Il2CppApi *api,
    Il2CppClass *klass,
    const char *name_space,
    const char *class_name,
    RuntimeEvent *event
) {
    if (api == nullptr || klass == nullptr || class_name == nullptr || event == nullptr ||
        !has_method_survey_api(*api)) return;

    void *iterator = nullptr;
    size_t inspected_methods = 0U;
    constexpr size_t kMaxMethodsPerClass = 256U;
    while (event->candidate_class_count < kMaxCandidateClasses &&
        inspected_methods < kMaxMethodsPerClass) {
        const void *method = api->class_get_methods(klass, &iterator);
        if (method == nullptr) break;
        ++inspected_methods;
        const char *method_name = api->method_get_name(method);
        if (method_name != nullptr && is_candidate_method(method_name)) {
            append_candidate_method(event, name_space, class_name, method_name);
        }
    }
}

struct ManagedDiagnosticClass {
    const char *name_space;
    const char *name;
    const char *const *methods;
    size_t method_count;
};

void log_runtime_method_signature(
    const Il2CppApi &api,
    Il2CppClass *klass,
    const char *class_name,
    const char *method_name
) {
    if (!has_runtime_inspection_api(api) || klass == nullptr || class_name == nullptr ||
        method_name == nullptr) return;

    const void *method = api.class_get_method_from_name(klass, method_name, -1);
    if (method == nullptr) {
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "managed diagnostic: method missing class=%s method=%s",
            class_name,
            method_name
        );
        return;
    }

    const uint32_t parameter_count = api.method_get_param_count(method);
    const Il2CppType *return_type = api.method_get_return_type(method);
    const char *return_name = (return_type == nullptr || api.type_get_name == nullptr)
        ? "<unknown>"
        : api.type_get_name(return_type);
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
            "managed diagnostic: method class=%s method=%s ptr=%p instance=%d params=%u return=%s",
            class_name,
            method_name,
            method,
            api.method_is_instance(method) ? 1 : 0,
        parameter_count,
        return_name == nullptr ? "<unknown>" : return_name
    );
    for (uint32_t index = 0U; index < parameter_count && index < 16U; ++index) {
        const Il2CppType *parameter_type = api.method_get_param(method, index);
        const char *parameter_name = (parameter_type == nullptr || api.type_get_name == nullptr)
            ? "<unknown>"
            : api.type_get_name(parameter_type);
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "managed diagnostic: parameter class=%s method=%s index=%u type=%s",
            class_name,
            method_name,
            index,
            parameter_name == nullptr ? "<unknown>" : parameter_name
        );
    }
}

void log_runtime_method_overloads(
    const Il2CppApi &api,
    Il2CppClass *klass,
    const char *class_name,
    const char *const *method_names,
    size_t method_name_count
) {
    if (!has_runtime_inspection_api(api) || api.class_get_methods == nullptr ||
        api.method_get_name == nullptr || klass == nullptr || class_name == nullptr ||
        method_names == nullptr) return;

    void *iterator = nullptr;
    uint32_t inspected = 0U;
    while (inspected < 512U) {
        const void *method = api.class_get_methods(klass, &iterator);
        if (method == nullptr) break;
        ++inspected;
        const char *method_name = api.method_get_name(method);
        bool wanted = false;
        for (size_t index = 0U; index < method_name_count; ++index) {
            if (method_name != nullptr && strcmp(method_name, method_names[index]) == 0) {
                wanted = true;
                break;
            }
        }
        if (!wanted) continue;
        const uint32_t parameter_count = api.method_get_param_count(method);
        const Il2CppType *return_type = api.method_get_return_type(method);
        const char *return_name = (return_type == nullptr || api.type_get_name == nullptr)
            ? "<unknown>"
            : api.type_get_name(return_type);
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "managed diagnostic: overload class=%s method=%s instance=%d params=%u return=%s",
            class_name,
            method_name == nullptr ? "<unknown>" : method_name,
            api.method_is_instance(method) ? 1 : 0,
            parameter_count,
            return_name == nullptr ? "<unknown>" : return_name
        );
        for (uint32_t index = 0U; index < parameter_count && index < 16U; ++index) {
            const Il2CppType *parameter_type = api.method_get_param(method, index);
            const char *parameter_name = (parameter_type == nullptr || api.type_get_name == nullptr)
                ? "<unknown>"
                : api.type_get_name(parameter_type);
            __android_log_print(
                ANDROID_LOG_INFO,
                kLogTag,
                "managed diagnostic: overload parameter class=%s method=%s index=%u type=%s",
                class_name,
                method_name == nullptr ? "<unknown>" : method_name,
                index,
                parameter_name == nullptr ? "<unknown>" : parameter_name
            );
        }
    }
}

void log_runtime_class_fields(
    const Il2CppApi &api,
    Il2CppClass *klass,
    const char *class_name
) {
    if (!has_field_inspection_api(api) || klass == nullptr || class_name == nullptr) return;

    void *iterator = nullptr;
    uint32_t field_count = 0U;
    while (field_count < 128U) {
        const void *field = api.class_get_fields(klass, &iterator);
        if (field == nullptr) break;
        ++field_count;
        const char *field_name = api.field_get_name(field);
        const uint32_t flags = api.field_get_flags(field);
        const Il2CppType *field_type = api.field_get_type == nullptr
            ? nullptr
            : api.field_get_type(field);
        const char *type_name = (field_type == nullptr || api.type_get_name == nullptr)
            ? "<unknown>"
            : api.type_get_name(field_type);
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "managed diagnostic: field class=%s name=%s flags=0x%x static=%d type=%s",
            class_name,
            field_name == nullptr ? "<unknown>" : field_name,
            flags,
            (flags & 0x0010U) != 0U ? 1 : 0,
            type_name == nullptr ? "<unknown>" : type_name
        );
    }
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "managed diagnostic: fields class=%s count=%u truncated=%d",
        class_name,
        field_count,
        field_count >= 128U ? 1 : 0
    );
    const bool dump_method_inventory = strcmp(class_name, "EncounterGestureHandler") == 0 ||
        strcmp(class_name, "EncounterPokemon") == 0 ||
        strcmp(class_name, "EncounterState") == 0 ||
        strcmp(class_name, "EncounterInteractionState") == 0 ||
        strcmp(class_name, "EncounterGuiController") == 0 ||
        strcmp(class_name, "PokeballThrow") == 0 ||
        strcmp(class_name, "PokeballService") == 0 ||
        strcmp(class_name, "MapPokemon") == 0 ||
        strcmp(class_name, "WildMapPokemon") == 0 ||
        strcmp(class_name, "MapPokestop") == 0 ||
        strcmp(class_name, "MapPokestopInteractive") == 0 ||
        strcmp(class_name, "Pokeball") == 0;
    if (!dump_method_inventory || !has_method_survey_api(api) ||
        api.class_get_methods == nullptr || api.method_get_name == nullptr) return;

    void *method_iterator = nullptr;
    uint32_t method_count = 0U;
    while (method_count < 512U) {
        const void *method = api.class_get_methods(klass, &method_iterator);
        if (method == nullptr) break;
        ++method_count;
        const char *method_name = api.method_get_name(method);
        const uint32_t parameter_count = api.method_get_param_count(method);
        const Il2CppType *return_type = api.method_get_return_type(method);
        const char *return_name = return_type == nullptr || api.type_get_name == nullptr
            ? "<unknown>"
            : api.type_get_name(return_type);
        __android_log_print(
            ANDROID_LOG_INFO,
            kLogTag,
            "managed diagnostic: method inventory class=%s method=%s instance=%d params=%u return=%s",
            class_name,
            method_name == nullptr ? "<unknown>" : method_name,
            api.method_is_instance(method) ? 1 : 0,
            parameter_count,
            return_name == nullptr ? "<unknown>" : return_name
        );
        for (uint32_t index = 0U; index < parameter_count && index < 16U; ++index) {
            const Il2CppType *parameter_type = api.method_get_param(method, index);
            const char *parameter_name = parameter_type == nullptr || api.type_get_name == nullptr
                ? "<unknown>"
                : api.type_get_name(parameter_type);
            __android_log_print(
                ANDROID_LOG_INFO,
                kLogTag,
                "managed diagnostic: method inventory parameter class=%s method=%s index=%u type=%s",
                class_name,
                method_name == nullptr ? "<unknown>" : method_name,
                index,
                parameter_name == nullptr ? "<unknown>" : parameter_name
            );
        }
    }
    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "managed diagnostic: method inventory class=%s count=%u truncated=%d",
        class_name,
        method_count,
        method_count >= 512U ? 1 : 0
    );
}
