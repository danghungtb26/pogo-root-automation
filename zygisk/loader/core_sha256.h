#pragma once

#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

namespace pogo_core_sha256 {

constexpr uint32_t kRound[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

inline uint32_t rotate(uint32_t value, unsigned bits) {
    return (value >> bits) | (value << (32U - bits));
}

inline void block(const uint8_t *bytes, uint32_t state[8]) {
    uint32_t words[64]{};
    for (unsigned i = 0; i < 16; ++i) {
        const unsigned offset = i * 4U;
        words[i] = (uint32_t(bytes[offset]) << 24U) |
            (uint32_t(bytes[offset + 1U]) << 16U) |
            (uint32_t(bytes[offset + 2U]) << 8U) | bytes[offset + 3U];
    }
    for (unsigned i = 16; i < 64; ++i) {
        const uint32_t a = words[i - 15U], b = words[i - 2U];
        words[i] = words[i - 16U] +
            (rotate(a, 7U) ^ rotate(a, 18U) ^ (a >> 3U)) +
            words[i - 7U] +
            (rotate(b, 17U) ^ rotate(b, 19U) ^ (b >> 10U));
    }
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
    for (unsigned i = 0; i < 64; ++i) {
        const uint32_t choice = (e & f) ^ (~e & g);
        const uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        const uint32_t t1 = h + (rotate(e, 6U) ^ rotate(e, 11U) ^ rotate(e, 25U)) +
            choice + kRound[i] + words[i];
        const uint32_t t2 = (rotate(a, 2U) ^ rotate(a, 13U) ^ rotate(a, 22U)) +
            majority;
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

inline bool file_hex(int fd, uint64_t expected_size, char output[65]) {
    if (fd < 0 || output == nullptr || expected_size == 0U ||
        expected_size > 64U * 1024U * 1024U) return false;
    struct stat info{};
    if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) ||
        info.st_size < 0 || uint64_t(info.st_size) != expected_size) return false;
    uint32_t state[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    std::array<uint8_t, 4096> buffer{};
    uint8_t tail[128]{};
    uint64_t offset = 0U;
    size_t used = 0U;
    while (offset < expected_size) {
        const size_t wanted = size_t(std::min<uint64_t>(buffer.size(),
            expected_size - offset));
        const ssize_t count = pread(fd, buffer.data(), wanted, off_t(offset));
        if (count <= 0) return false;
        for (ssize_t i = 0; i < count; ++i) {
            tail[used++] = buffer[size_t(i)];
            if (used == 64U) { block(tail, state); used = 0U; }
        }
        offset += uint64_t(count);
    }
    tail[used++] = 0x80U;
    if (used > 56U) {
        std::memset(tail + used, 0, 64U - used);
        block(tail, state);
        used = 0U;
    }
    std::memset(tail + used, 0, 56U - used);
    const uint64_t bits = expected_size * 8U;
    for (unsigned i = 0; i < 8U; ++i) tail[56U + i] =
        uint8_t(bits >> (56U - i * 8U));
    block(tail, state);
    constexpr char digits[] = "0123456789abcdef";
    for (unsigned i = 0; i < 8U; ++i) {
        for (unsigned j = 0; j < 4U; ++j) {
            const uint8_t value = uint8_t(state[i] >> (24U - j * 8U));
            output[i * 8U + j * 2U] = digits[value >> 4U];
            output[i * 8U + j * 2U + 1U] = digits[value & 15U];
        }
    }
    output[64] = '\0';
    return true;
}

}  // namespace pogo_core_sha256
