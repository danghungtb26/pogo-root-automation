struct RuntimeSha256 {
    uint32_t state[8]{
        0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
        0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U,
    };
    uint8_t block[64]{};
    uint64_t total_bytes = 0U;
    size_t block_bytes = 0U;
};

uint32_t runtime_sha256_rotate(uint32_t value, uint32_t count) {
    return (value >> count) | (value << (32U - count));
}

void runtime_sha256_transform(RuntimeSha256 *sha, const uint8_t *block) {
    static constexpr uint32_t kRound[64] = {
        0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
        0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
        0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
        0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
        0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
        0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
        0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
        0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
        0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
        0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
        0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
        0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
        0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
        0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
        0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
        0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
    };
    uint32_t words[64]{};
    for (size_t index = 0U; index < 16U; ++index) {
        const size_t offset = index * 4U;
        words[index] = static_cast<uint32_t>(block[offset]) << 24U |
            static_cast<uint32_t>(block[offset + 1U]) << 16U |
            static_cast<uint32_t>(block[offset + 2U]) << 8U |
            static_cast<uint32_t>(block[offset + 3U]);
    }
    for (size_t index = 16U; index < 64U; ++index) {
        const uint32_t s0 = runtime_sha256_rotate(words[index - 15U], 7U) ^
            runtime_sha256_rotate(words[index - 15U], 18U) ^
            (words[index - 15U] >> 3U);
        const uint32_t s1 = runtime_sha256_rotate(words[index - 2U], 17U) ^
            runtime_sha256_rotate(words[index - 2U], 19U) ^
            (words[index - 2U] >> 10U);
        words[index] = words[index - 16U] + s0 + words[index - 7U] + s1;
    }
    uint32_t a = sha->state[0];
    uint32_t b = sha->state[1];
    uint32_t c = sha->state[2];
    uint32_t d = sha->state[3];
    uint32_t e = sha->state[4];
    uint32_t f = sha->state[5];
    uint32_t g = sha->state[6];
    uint32_t h = sha->state[7];
    for (size_t index = 0U; index < 64U; ++index) {
        const uint32_t s1 = runtime_sha256_rotate(e, 6U) ^
            runtime_sha256_rotate(e, 11U) ^ runtime_sha256_rotate(e, 25U);
        const uint32_t choice = (e & f) ^ (~e & g);
        const uint32_t temp1 = h + s1 + choice + kRound[index] + words[index];
        const uint32_t s0 = runtime_sha256_rotate(a, 2U) ^
            runtime_sha256_rotate(a, 13U) ^ runtime_sha256_rotate(a, 22U);
        const uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        const uint32_t temp2 = s0 + majority;
        h = g; g = f; f = e; e = d + temp1;
        d = c; c = b; b = a; a = temp1 + temp2;
    }
    sha->state[0] += a; sha->state[1] += b; sha->state[2] += c; sha->state[3] += d;
    sha->state[4] += e; sha->state[5] += f; sha->state[6] += g; sha->state[7] += h;
}

void runtime_sha256_update(RuntimeSha256 *sha, const uint8_t *data, size_t size) {
    if (sha == nullptr || data == nullptr) return;
    sha->total_bytes += size;
    while (size > 0U) {
        const size_t available = sizeof(sha->block) - sha->block_bytes;
        const size_t count = std::min(size, available);
        memcpy(sha->block + sha->block_bytes, data, count);
        sha->block_bytes += count;
        data += count;
        size -= count;
        if (sha->block_bytes == sizeof(sha->block)) {
            runtime_sha256_transform(sha, sha->block);
            sha->block_bytes = 0U;
        }
    }
}

void runtime_sha256_finish(RuntimeSha256 *sha, char output[65]) {
    const uint64_t bit_count = sha->total_bytes * 8U;
    sha->block[sha->block_bytes++] = 0x80U;
    if (sha->block_bytes > 56U) {
        memset(sha->block + sha->block_bytes, 0, sizeof(sha->block) - sha->block_bytes);
        runtime_sha256_transform(sha, sha->block);
        sha->block_bytes = 0U;
    }
    memset(sha->block + sha->block_bytes, 0, 56U - sha->block_bytes);
    for (size_t index = 0U; index < 8U; ++index) {
        sha->block[63U - index] = static_cast<uint8_t>(bit_count >> (index * 8U));
    }
    runtime_sha256_transform(sha, sha->block);
    static constexpr char kHex[] = "0123456789abcdef";
    for (size_t index = 0U; index < 32U; ++index) {
        const uint8_t value = static_cast<uint8_t>(
            sha->state[index / 4U] >> (24U - (index % 4U) * 8U)
        );
        output[index * 2U] = kHex[value >> 4U];
        output[index * 2U + 1U] = kHex[value & 0x0fU];
    }
    output[64] = '\0';
}

bool read_runtime_metadata_sha256(
    JNIEnv *env,
    jobject application,
    char output[65],
    uint64_t *bytes_read,
    char *reason,
    size_t reason_size
) {
    constexpr uint64_t kMaximumMetadataBytes = 64ULL * 1024ULL * 1024ULL;
    constexpr jsize kReadBufferBytes = 64 * 1024;
    if (env == nullptr || application == nullptr || output == nullptr || bytes_read == nullptr) {
        copy_string(reason, reason_size, "metadata_application_unavailable");
        return false;
    }
    jclass application_class = env->GetObjectClass(application);
    const jmethodID get_assets = application_class == nullptr ? nullptr : env->GetMethodID(
        application_class,
        "getAssets",
        "()Landroid/content/res/AssetManager;"
    );
    if (clear_identity_jni_exception(env, "asset_manager_method") || get_assets == nullptr) {
        copy_string(reason, reason_size, "metadata_asset_api_unavailable");
        return false;
    }
    jobject assets = env->CallObjectMethod(application, get_assets);
    jclass assets_class = assets == nullptr ? nullptr : env->GetObjectClass(assets);
    const jmethodID open = assets_class == nullptr ? nullptr : env->GetMethodID(
        assets_class,
        "open",
        "(Ljava/lang/String;I)Ljava/io/InputStream;"
    );
    jstring asset_name = env->NewStringUTF("bin/Data/Managed/Metadata/global-metadata.dat");
    if (clear_identity_jni_exception(env, "metadata_asset_open_method") ||
        open == nullptr || asset_name == nullptr) {
        copy_string(reason, reason_size, "metadata_asset_open_unavailable");
        return false;
    }
    jobject stream = env->CallObjectMethod(assets, open, asset_name, 2);
    if (clear_identity_jni_exception(env, "metadata_asset_open") || stream == nullptr) {
        copy_string(reason, reason_size, "metadata_asset_missing");
        return false;
    }
    jclass stream_class = env->GetObjectClass(stream);
    const jmethodID read = stream_class == nullptr ? nullptr :
        env->GetMethodID(stream_class, "read", "([B)I");
    const jmethodID close = stream_class == nullptr ? nullptr :
        env->GetMethodID(stream_class, "close", "()V");
    jbyteArray buffer = env->NewByteArray(kReadBufferBytes);
    if (clear_identity_jni_exception(env, "metadata_stream_methods") ||
        read == nullptr || close == nullptr || buffer == nullptr) {
        copy_string(reason, reason_size, "metadata_stream_unavailable");
        return false;
    }
    RuntimeSha256 sha;
    std::vector<uint8_t> native_buffer(static_cast<size_t>(kReadBufferBytes));
    uint64_t total = 0U;
    bool succeeded = true;
    for (;;) {
        const jint count = env->CallIntMethod(stream, read, buffer);
        if (clear_identity_jni_exception(env, "metadata_stream_read")) {
            succeeded = false;
            copy_string(reason, reason_size, "metadata_read_failed");
            break;
        }
        if (count < 0) break;
        if (count == 0) {
            succeeded = false;
            copy_string(reason, reason_size, "metadata_zero_length_read");
            break;
        }
        total += static_cast<uint64_t>(count);
        if (total > kMaximumMetadataBytes) {
            succeeded = false;
            copy_string(reason, reason_size, "metadata_size_limit");
            break;
        }
        env->GetByteArrayRegion(buffer, 0, count, reinterpret_cast<jbyte *>(native_buffer.data()));
        if (clear_identity_jni_exception(env, "metadata_buffer_copy")) {
            succeeded = false;
            copy_string(reason, reason_size, "metadata_copy_failed");
            break;
        }
        runtime_sha256_update(&sha, native_buffer.data(), static_cast<size_t>(count));
    }
    env->CallVoidMethod(stream, close);
    if (clear_identity_jni_exception(env, "metadata_stream_close")) {
        succeeded = false;
        copy_string(reason, reason_size, "metadata_close_failed");
    }
    *bytes_read = total;
    if (!succeeded || total == 0U) return false;
    runtime_sha256_finish(&sha, output);
    const bool matches = total == pogo_runtime_profile::kMetadataBytes &&
        strcmp(output, pogo_runtime_profile::kMetadataSha256) == 0;
    copy_string(reason, reason_size, matches ? "metadata_verified" :
        total == pogo_runtime_profile::kMetadataBytes
            ? "metadata_hash_mismatch" : "metadata_size_mismatch");
    return true;
}

pogo_runtime_profile::IdentityObservation runtime_identity_snapshot(ProbeContext &context) {
    std::lock_guard<std::mutex> lock(context.identity_mutex);
    if (context.identity.attempted) return context.identity;
    context.identity.attempted = true;
    copy_string(
        context.identity.version_reason,
        sizeof(context.identity.version_reason),
        "version_not_observed"
    );
    copy_string(
        context.identity.metadata_reason,
        sizeof(context.identity.metadata_reason),
        "metadata_not_observed"
    );
    JavaVM *java_vm = context.main_thread_bridge.java_vm;
    JNIEnv *env = nullptr;
    bool attached_here = false;
    const jint get_env_result = java_vm == nullptr
        ? JNI_ERR
        : java_vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
    if (get_env_result == JNI_EDETACHED) {
        attached_here = java_vm->AttachCurrentThread(&env, nullptr) == JNI_OK;
    }
    if (java_vm == nullptr ||
        (get_env_result != JNI_OK && !attached_here) || env == nullptr) {
        copy_string(
            context.identity.version_reason,
            sizeof(context.identity.version_reason),
            "identity_jvm_unavailable"
        );
        copy_string(
            context.identity.metadata_reason,
            sizeof(context.identity.metadata_reason),
            "identity_jvm_unavailable"
        );
        return context.identity;
    }
    jobject application = current_runtime_application(env);
    context.identity.version_code_read = read_runtime_version_code(
        env,
        application,
        &context.identity.observed_version_code,
        context.identity.version_reason,
        sizeof(context.identity.version_reason)
    );
    context.identity.version_code_verified = context.identity.version_code_read &&
        context.identity.observed_version_code == pogo_runtime_profile::kVersionCode;
    context.identity.metadata_read = read_runtime_metadata_sha256(
        env,
        application,
        context.identity.observed_metadata_sha256,
        &context.identity.observed_metadata_bytes,
        context.identity.metadata_reason,
        sizeof(context.identity.metadata_reason)
    );
    context.identity.metadata_sha256_verified = context.identity.metadata_read &&
        context.identity.observed_metadata_bytes == pogo_runtime_profile::kMetadataBytes &&
        strcmp(
            context.identity.observed_metadata_sha256,
            pogo_runtime_profile::kMetadataSha256
        ) == 0;
    if (attached_here) java_vm->DetachCurrentThread();
    return context.identity;
}
