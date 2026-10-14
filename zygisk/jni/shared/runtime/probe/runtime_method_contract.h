#pragma once

#include <stddef.h>
#include <stdint.h>

namespace pogo_runtime_method {

enum class VerificationReason : uint8_t {
    kVerified = 0U,
    kApiUnavailable,
    kInvalidExpectation,
    kClassUnavailable,
    kEnumerationLimit,
    kNoMatch,
    kAmbiguous,
    kTokenMismatch,
    kTypeNameUnavailable,
};

struct RuntimeMethodExpectation {
    const char *image_name = nullptr;
    const char *name_space = nullptr;
    const char *class_name = nullptr;
    const char *method_name = nullptr;
    bool is_instance = false;
    const char *const *parameter_types = nullptr;
    uint32_t parameter_count = 0U;
    const char *return_type = nullptr;
    uint32_t metadata_token = 0U;
};

struct VerifiedRuntimeMethod {
    const void *method = nullptr;
    VerificationReason reason = VerificationReason::kInvalidExpectation;
    uint32_t metadata_token = 0U;
    uint32_t inspected_count = 0U;
    uint32_t signature_match_count = 0U;
    uint32_t verified_match_count = 0U;

    bool verified() const {
        return reason == VerificationReason::kVerified && method != nullptr &&
            verified_match_count == 1U;
    }
};

inline const char *verification_reason_name(VerificationReason reason) {
    switch (reason) {
        case VerificationReason::kVerified: return "verified";
        case VerificationReason::kApiUnavailable: return "api_unavailable";
        case VerificationReason::kInvalidExpectation: return "invalid_expectation";
        case VerificationReason::kClassUnavailable: return "class_unavailable";
        case VerificationReason::kEnumerationLimit: return "enumeration_limit";
        case VerificationReason::kNoMatch: return "no_match";
        case VerificationReason::kAmbiguous: return "ambiguous";
        case VerificationReason::kTokenMismatch: return "token_mismatch";
        case VerificationReason::kTypeNameUnavailable: return "type_name_unavailable";
    }
    return "unknown";
}

}  // namespace pogo_runtime_method
