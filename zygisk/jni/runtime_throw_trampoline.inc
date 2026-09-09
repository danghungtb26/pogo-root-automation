#if defined(__aarch64__)
constexpr size_t kInlineHookPatchBytes = 16U;
constexpr size_t kInlineHookTrampolineBytes = 128U;

bool make_code_range_writable(void *address, size_t size, int protection) {
    if (address == nullptr || size == 0U) return false;
    const long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0L) return false;
    const uintptr_t page_mask = static_cast<uintptr_t>(page_size) - 1U;
    const uintptr_t start = reinterpret_cast<uintptr_t>(address) & ~page_mask;
    const uintptr_t end = (reinterpret_cast<uintptr_t>(address) + size - 1U) & ~page_mask;
    const size_t length = static_cast<size_t>(end - start + static_cast<uintptr_t>(page_size));
    return mprotect(reinterpret_cast<void *>(start), length, protection) == 0;
}

void write_absolute_branch(uint8_t *destination, const void *target) {
    const uint32_t instructions[] = {0x58000050U, 0xD61F0200U};
    memcpy(destination, instructions, sizeof(instructions));
    const uintptr_t target_address = reinterpret_cast<uintptr_t>(target);
    memcpy(destination + sizeof(instructions), &target_address, sizeof(target_address));
}

bool is_adrp(uint32_t instruction) {
    return (instruction & 0x9F000000U) == 0x90000000U;
}

uintptr_t adrp_target(uint32_t instruction, uintptr_t source_pc) {
    int64_t immediate = static_cast<int64_t>((instruction >> 5U) & 0x7FFFFU) << 2U;
    immediate |= static_cast<int64_t>((instruction >> 29U) & 0x3U);
    if ((immediate & (1LL << 20U)) != 0) immediate |= ~((1LL << 21U) - 1LL);
    return (source_pc & ~static_cast<uintptr_t>(0xFFFU)) +
        static_cast<uintptr_t>(immediate << 12U);
}

size_t emit_relocated_instruction(
    uint8_t *destination,
    uint32_t instruction,
    uintptr_t source_pc
) {
    if (!is_adrp(instruction)) {
        memcpy(destination, &instruction, sizeof(instruction));
        return sizeof(instruction);
    }
    const uint32_t register_number = instruction & 0x1FU;
    const uint32_t relocated[] = {0x58000000U | (2U << 5U) | register_number, 0x14000003U};
    memcpy(destination, relocated, sizeof(relocated));
    const uintptr_t target = adrp_target(instruction, source_pc);
    memcpy(destination + sizeof(relocated), &target, sizeof(target));
    return sizeof(relocated) + sizeof(target);
}

bool patch_inline_hook(RuntimeInlineHook *hook, const uint8_t *bytes) {
    if (hook == nullptr || hook->target == nullptr || bytes == nullptr ||
        !make_code_range_writable(
            hook->target, sizeof(hook->original), PROT_READ | PROT_WRITE | PROT_EXEC
        )) return false;
    memcpy(hook->target, bytes, sizeof(hook->original));
    __builtin___clear_cache(
        static_cast<char *>(hook->target),
        static_cast<char *>(hook->target) + sizeof(hook->original)
    );
    return make_code_range_writable(hook->target, sizeof(hook->original), PROT_READ | PROT_EXEC);
}

bool install_inline_hook(
    RuntimeInlineHook *hook,
    const void *target,
    const uint32_t expected_prologue[4],
    const void *replacement
) {
    if (hook == nullptr || target == nullptr || expected_prologue == nullptr ||
        replacement == nullptr || reinterpret_cast<uintptr_t>(target) % 4U != 0U) return false;
    uint8_t original[kInlineHookPatchBytes]{};
    memcpy(original, target, sizeof(original));
    if (memcmp(original, expected_prologue, sizeof(original)) != 0) {
        __android_log_print(
            ANDROID_LOG_WARN,
            kLogTag,
            "runtime throw hook prologue mismatch target=%p",
            target
        );
        return false;
    }
    void *trampoline = mmap(
        nullptr,
        kInlineHookTrampolineBytes,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0
    );
    if (trampoline == MAP_FAILED) return false;
    size_t trampoline_offset = 0U;
    for (size_t index = 0U; index < sizeof(original); index += sizeof(uint32_t)) {
        uint32_t instruction = 0U;
        memcpy(&instruction, original + index, sizeof(instruction));
        const size_t emitted = emit_relocated_instruction(
            static_cast<uint8_t *>(trampoline) + trampoline_offset,
            instruction,
            reinterpret_cast<uintptr_t>(target) + index
        );
        if (emitted == 0U || trampoline_offset + emitted + kInlineHookPatchBytes >
            kInlineHookTrampolineBytes) {
            munmap(trampoline, kInlineHookTrampolineBytes);
            return false;
        }
        trampoline_offset += emitted;
    }
    write_absolute_branch(
        static_cast<uint8_t *>(trampoline) + trampoline_offset,
        static_cast<const uint8_t *>(target) + kInlineHookPatchBytes
    );
    trampoline_offset += kInlineHookPatchBytes;
    __builtin___clear_cache(
        static_cast<char *>(trampoline),
        static_cast<char *>(trampoline) + trampoline_offset
    );
    if (mprotect(trampoline, kInlineHookTrampolineBytes, PROT_READ | PROT_EXEC) != 0) {
        munmap(trampoline, kInlineHookTrampolineBytes);
        return false;
    }
    uint8_t jump[kInlineHookPatchBytes]{};
    write_absolute_branch(jump, replacement);
    hook->target = const_cast<void *>(target);
    hook->trampoline = trampoline;
    hook->trampoline_bytes = kInlineHookTrampolineBytes;
    memcpy(hook->original, original, sizeof(hook->original));
    if (!patch_inline_hook(hook, jump)) {
        munmap(trampoline, kInlineHookTrampolineBytes);
        *hook = RuntimeInlineHook{};
        return false;
    }
    hook->installed = true;
    return true;
}

void uninstall_inline_hook(RuntimeInlineHook *hook) {
    if (hook == nullptr || !hook->installed) return;
    if (!patch_inline_hook(hook, hook->original)) {
        __android_log_print(
            ANDROID_LOG_ERROR,
            kLogTag,
            "runtime throw hook restore failed target=%p",
            hook->target
        );
    }
    munmap(hook->trampoline, hook->trampoline_bytes);
    *hook = RuntimeInlineHook{};
}
#endif
