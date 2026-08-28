# Zygisk runtime

This directory is the root/instrumentation boundary.

Current implementation intentionally does only one thing: load in app specialization, detect the supported Pokémon GO process names, and log that the target process was seen. It does **not** hook game functions yet.

## Why this is separate

The automation core must not depend on offsets, symbols, hook frameworks, or a particular Pokémon GO build. Version-specific work belongs behind `GameAdapter`.

## Building the native stub

1. Install Android NDK.
2. Download the canonical `zygisk.hpp` from the official `topjohnwu/zygisk-module-sample` repository.
3. Configure CMake with `-DZYGISK_API_DIR=/path/containing/zygisk.hpp`.
4. Build the shared library for the desired ABI.
5. Package the resulting ABI library under the Magisk module's `zygisk/` directory using the ABI filename expected by Zygisk.

The official Zygisk sample is the source of truth for API compatibility and packaging conventions.
