#pragma once

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <elf.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "core_sha256.h"

namespace pogo_core_handoff {

constexpr uint32_t kMagic = 0x50474346U;
constexpr uint32_t kProtocol = 1U;

enum class Result : uint32_t {
    kReady = 0U,
    kCoreMissing = 1U,
    kManifestInvalid = 2U,
    kCoreIntegrity = 3U,
    kCoreAbi = 4U,
};

struct Reply {
    uint32_t magic = kMagic;
    uint32_t protocol = kProtocol;
    uint32_t result = uint32_t(Result::kCoreMissing);
    uint32_t reserved = 0U;
    uint64_t size = 0U;
    char abi[16]{};
    char sha256[65]{};
};
static_assert(sizeof(Reply) == 112U);

inline const char *device_abi() {
#if defined(__aarch64__)
    return "arm64-v8a";
#elif defined(__x86_64__)
    return "x86_64";
#else
    return "unsupported";
#endif
}

inline bool elf_matches_abi(int fd) {
    Elf64_Ehdr header{};
    if (pread(fd, &header, sizeof(header), 0) != sizeof(header) ||
        std::memcmp(header.e_ident, ELFMAG, SELFMAG) != 0 ||
        header.e_ident[EI_CLASS] != ELFCLASS64 ||
        header.e_ident[EI_DATA] != ELFDATA2LSB ||
        header.e_type != ET_DYN) return false;
#if defined(__aarch64__)
    return header.e_machine == EM_AARCH64;
#elif defined(__x86_64__)
    return header.e_machine == EM_X86_64;
#else
    return false;
#endif
}

inline bool manifest(int fd, const char *abi, Reply *reply) {
    if (fd < 0 || abi == nullptr || reply == nullptr) return false;
    struct stat info{};
    if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) ||
        info.st_size <= 0 || info.st_size > 255) return false;
    char source[256]{};
    if (pread(fd, source, size_t(info.st_size), 0) != info.st_size) return false;
    unsigned protocol = 0U;
    unsigned long long size = 0U;
    char parsed_abi[16]{};
    char digest[65]{};
    if (std::sscanf(source, "protocol=%u\nabi=%15[^\n]\nsize=%llu\nsha256=%64s\n",
            &protocol, parsed_abi, &size, digest) != 4 ||
        protocol != kProtocol || std::strcmp(parsed_abi, abi) != 0 ||
        size == 0U || size > 64U * 1024U * 1024U ||
        std::strlen(digest) != 64U) return false;
    char canonical[256]{};
    const int length = std::snprintf(canonical, sizeof(canonical),
        "protocol=%u\nabi=%s\nsize=%llu\nsha256=%s\n",
        protocol, parsed_abi, size, digest);
    if (length != info.st_size || std::memcmp(source, canonical, size_t(length)) != 0)
        return false;
    reply->size = uint64_t(size);
    std::snprintf(reply->abi, sizeof(reply->abi), "%s", abi);
    std::memcpy(reply->sha256, digest, sizeof(digest));
    return true;
}

inline int open_verified_core(Reply *reply) {
    if (reply == nullptr) return -1;
    const char *abi = device_abi();
    char core_path[192]{};
    char manifest_path[192]{};
    std::snprintf(core_path, sizeof(core_path),
        "/data/adb/modules/pogo_root_automation/runtime-core/%s.so", abi);
    std::snprintf(manifest_path, sizeof(manifest_path),
        "/data/adb/modules/pogo_root_automation/runtime-core/%s.manifest", abi);
    const int core = open(core_path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (core < 0) return -1;
    const int meta = open(manifest_path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (meta < 0 || !manifest(meta, abi, reply)) {
        if (meta >= 0) close(meta);
        close(core);
        reply->result = uint32_t(Result::kManifestInvalid);
        return -1;
    }
    close(meta);
    if (!elf_matches_abi(core)) {
        close(core);
        reply->result = uint32_t(Result::kCoreAbi);
        return -1;
    }
    char actual[65]{};
    if (!pogo_core_sha256::file_hex(core, reply->size, actual) ||
        std::strcmp(actual, reply->sha256) != 0) {
        close(core);
        reply->result = uint32_t(Result::kCoreIntegrity);
        return -1;
    }
    reply->result = uint32_t(Result::kReady);
    return core;
}

inline bool send_reply(int socket_fd, const Reply &reply, int core_fd) {
    iovec bytes{const_cast<Reply *>(&reply), sizeof(reply)};
    msghdr message{};
    message.msg_iov = &bytes;
    message.msg_iovlen = 1U;
    char control[CMSG_SPACE(sizeof(int))]{};
    if (core_fd >= 0) {
        message.msg_control = control;
        message.msg_controllen = sizeof(control);
        cmsghdr *header = CMSG_FIRSTHDR(&message);
        header->cmsg_level = SOL_SOCKET;
        header->cmsg_type = SCM_RIGHTS;
        header->cmsg_len = CMSG_LEN(sizeof(int));
        std::memcpy(CMSG_DATA(header), &core_fd, sizeof(core_fd));
    }
    return sendmsg(socket_fd, &message, MSG_NOSIGNAL) == ssize_t(sizeof(reply));
}

inline bool receive_reply(int socket_fd, Reply *reply, int *core_fd) {
    if (reply == nullptr || core_fd == nullptr) return false;
    *core_fd = -1;
    iovec bytes{reply, sizeof(*reply)};
    char control[CMSG_SPACE(sizeof(int))]{};
    msghdr message{};
    message.msg_iov = &bytes;
    message.msg_iovlen = 1U;
    message.msg_control = control;
    message.msg_controllen = sizeof(control);
    const ssize_t received = recvmsg(socket_fd, &message,
        MSG_WAITALL | MSG_CMSG_CLOEXEC);
    bool ancillary_valid = true;
    for (cmsghdr *header = CMSG_FIRSTHDR(&message); header != nullptr;
         header = CMSG_NXTHDR(&message, header)) {
        if (header->cmsg_level == SOL_SOCKET && header->cmsg_type == SCM_RIGHTS &&
            header->cmsg_len >= CMSG_LEN(sizeof(int))) {
            const size_t count = (header->cmsg_len - CMSG_LEN(0)) / sizeof(int);
            for (size_t index = 0; index < count; ++index) {
                int passed_fd = -1;
                std::memcpy(&passed_fd,
                    CMSG_DATA(header) + index * sizeof(int), sizeof(int));
                if (*core_fd < 0) *core_fd = passed_fd;
                else { close(passed_fd); ancillary_valid = false; }
            }
            if (count != 1U) ancillary_valid = false;
        } else ancillary_valid = false;
    }
    return ancillary_valid && received == ssize_t(sizeof(*reply)) &&
        (message.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) == 0 &&
        reply->magic == kMagic && reply->protocol == kProtocol &&
        reply->reserved == 0U;
}

inline bool verify_reply(const Reply &reply, int core_fd) {
    if (reply.result != uint32_t(Result::kReady) || core_fd < 0 ||
        strnlen(reply.abi, sizeof(reply.abi)) >= sizeof(reply.abi) ||
        std::strcmp(reply.abi, device_abi()) != 0 ||
        strnlen(reply.sha256, sizeof(reply.sha256)) != 64U) return false;
    char actual[65]{};
    return elf_matches_abi(core_fd) &&
        pogo_core_sha256::file_hex(core_fd, reply.size, actual) &&
        std::strcmp(actual, reply.sha256) == 0;
}

}  // namespace pogo_core_handoff
