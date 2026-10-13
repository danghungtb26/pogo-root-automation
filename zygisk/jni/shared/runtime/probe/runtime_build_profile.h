#pragma once

#include <stddef.h>
#include <stdint.h>

namespace pogo_runtime_profile {

constexpr const char *kPackageName = "com.nianticlabs.pokemongo";
constexpr const char *kVersionName = "0.433.0";
constexpr uint64_t kVersionCode = 2026100702ULL;
constexpr const char *kAbi = "arm64-v8a";
constexpr const char *kIl2cppBuildIdHex =
    "da59ce3024c4ccfe16fa6616281a4d111de67aa2";
constexpr const char *kMetadataSha256 =
    "f5961a8548256da287b4f79466a169c302b90ec59be7f6461e8e58b482c0fb0d";
constexpr const char *kBaseApkSha256 =
    "1c8f198cd212e7303e5a5e38e34857f4ced42eb432ac62b07807bfd286503690";
constexpr uint8_t kIl2cppBuildId[] = {
    0xdaU, 0x59U, 0xceU, 0x30U, 0x24U, 0xc4U, 0xccU, 0xfeU,
    0x16U, 0xfaU, 0x66U, 0x16U, 0x28U, 0x1aU, 0x4dU, 0x11U,
    0x1dU, 0xe6U, 0x7aU, 0xa2U,
};
constexpr size_t kIl2cppBuildIdSize = sizeof(kIl2cppBuildId);

// A controller fingerprint is an expected identity, not proof of the running
// package. The runtime must independently verify every strong-identity field.
constexpr const char *kFingerprint =
    "com.nianticlabs.pokemongo|2026100702|0.433.0|arm64-v8a|"
    "il2cpp=da59ce3024c4ccfe16fa6616281a4d111de67aa2|"
    "metadata=f5961a8548256da287b4f79466a169c302b90ec59be7f6461e8e58b482c0fb0d|"
    "apk=1c8f198cd212e7303e5a5e38e34857f4ced42eb432ac62b07807bfd286503690";

struct Verification {
    bool package_name = false;
    bool version_code = false;
    bool il2cpp_build_id = false;
    bool metadata_sha256 = false;

    bool mutation_allowed() const {
        return package_name && version_code && il2cpp_build_id && metadata_sha256;
    }

    const char *reason() const {
        if (!package_name) return "package_mismatch";
        if (!version_code) return "version_code_unverified";
        if (!il2cpp_build_id) return "il2cpp_build_id_mismatch";
        if (!metadata_sha256) return "metadata_hash_unverified";
        return "verified";
    }
};

}  // namespace pogo_runtime_profile
