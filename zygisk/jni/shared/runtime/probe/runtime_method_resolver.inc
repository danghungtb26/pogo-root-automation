bool runtime_method_expectation_valid(
    const pogo_runtime_method::RuntimeMethodExpectation &expectation
) {
    if (expectation.image_name == nullptr || expectation.image_name[0] == '\0' ||
        expectation.name_space == nullptr || expectation.class_name == nullptr ||
        expectation.class_name[0] == '\0' || expectation.method_name == nullptr ||
        expectation.method_name[0] == '\0' || expectation.return_type == nullptr ||
        expectation.return_type[0] == '\0' || expectation.metadata_token == 0U) {
        return false;
    }
    return expectation.parameter_count == 0U || expectation.parameter_types != nullptr;
}

bool runtime_method_type_matches(
    const Il2CppApi &api,
    const Il2CppType *type,
    const char *expected,
    bool *name_unavailable
) {
    OwnedIl2CppTypeName actual = owned_il2cpp_type_name(api, type);
    if (actual.get() == nullptr) {
        if (name_unavailable != nullptr) *name_unavailable = true;
        return false;
    }
    return expected != nullptr && strcmp(actual.get(), expected) == 0;
}

bool runtime_method_signature_matches(
    const Il2CppApi &api,
    const void *method,
    const pogo_runtime_method::RuntimeMethodExpectation &expectation,
    bool *name_unavailable
) {
    if (api.method_is_instance(method) != expectation.is_instance ||
        api.method_get_param_count(method) != expectation.parameter_count) {
        return false;
    }
    const Il2CppType *return_type = api.method_get_return_type(method);
    if (!runtime_method_type_matches(
            api, return_type, expectation.return_type, name_unavailable)) {
        return false;
    }
    for (uint32_t index = 0U; index < expectation.parameter_count; ++index) {
        const Il2CppType *parameter_type = api.method_get_param(method, index);
        if (!runtime_method_type_matches(
                api,
                parameter_type,
                expectation.parameter_types[index],
                name_unavailable
            )) {
            return false;
        }
    }
    return true;
}

Il2CppClass *resolve_runtime_method_class(
    const Il2CppApi &api,
    const Il2CppAssembly **assemblies,
    size_t assembly_count,
    const pogo_runtime_method::RuntimeMethodExpectation &expectation
) {
    if (!runtime_method_expectation_valid(expectation) || assemblies == nullptr ||
        api.assembly_get_image == nullptr || api.image_get_name == nullptr ||
        api.class_from_name == nullptr) return nullptr;
    constexpr size_t kMaximumAssemblies = 4096U;
    const size_t count = std::min(assembly_count, kMaximumAssemblies);
    Il2CppClass *resolved = nullptr;
    for (size_t index = 0U; index < count; ++index) {
        const Il2CppImage *image = api.assembly_get_image(assemblies[index]);
        const char *image_name = image == nullptr ? nullptr : api.image_get_name(image);
        if (image_name == nullptr || strcmp(image_name, expectation.image_name) != 0) continue;
        Il2CppClass *candidate = api.class_from_name(
            image, expectation.name_space, expectation.class_name
        );
        if (candidate == nullptr) continue;
        if (resolved != nullptr && resolved != candidate) return nullptr;
        resolved = candidate;
    }
    return resolved;
}

[[maybe_unused]] pogo_runtime_method::VerifiedRuntimeMethod verify_runtime_method(
    const Il2CppApi &api,
    Il2CppClass *exact_class,
    const pogo_runtime_method::RuntimeMethodExpectation &expectation
) {
    using Reason = pogo_runtime_method::VerificationReason;
    pogo_runtime_method::VerifiedRuntimeMethod result;
    if (!runtime_method_expectation_valid(expectation)) {
        result.reason = Reason::kInvalidExpectation;
        return result;
    }
    if (!has_verified_method_api(api)) {
        result.reason = Reason::kApiUnavailable;
        return result;
    }
    if (exact_class == nullptr) {
        result.reason = Reason::kClassUnavailable;
        return result;
    }

    constexpr uint32_t kMaximumMethods = 2048U;
    void *iterator = nullptr;
    bool enumeration_complete = false;
    bool type_name_unavailable = false;
    for (; result.inspected_count < kMaximumMethods; ++result.inspected_count) {
        const void *method = api.class_get_methods(exact_class, &iterator);
        if (method == nullptr) {
            enumeration_complete = true;
            break;
        }
        const char *name = api.method_get_name(method);
        if (name == nullptr || strcmp(name, expectation.method_name) != 0) continue;
        if (!runtime_method_signature_matches(
                api, method, expectation, &type_name_unavailable)) continue;
        ++result.signature_match_count;
        const uint32_t token = api.method_get_token(method);
        if (token != expectation.metadata_token) continue;
        ++result.verified_match_count;
        result.method = method;
        result.metadata_token = token;
    }
    if (!enumeration_complete && api.class_get_methods(exact_class, &iterator) != nullptr) {
        result.method = nullptr;
        result.reason = Reason::kEnumerationLimit;
        return result;
    }
    if (result.verified_match_count > 1U) {
        result.method = nullptr;
        result.reason = Reason::kAmbiguous;
    } else if (result.verified_match_count == 1U) {
        result.reason = Reason::kVerified;
    } else if (result.signature_match_count > 0U) {
        result.reason = Reason::kTokenMismatch;
    } else if (type_name_unavailable) {
        result.reason = Reason::kTypeNameUnavailable;
    } else {
        result.reason = Reason::kNoMatch;
    }
    return result;
}
