# Zygisk runtime

This directory is the root/instrumentation boundary.

The runtime loads in app specialization, detects the supported Pokémon GO
processes, probes IL2CPP, and starts a companion-owned persistent bridge after
the probe completes. The bridge publishes a versioned `RuntimeReady` message
and safely rejects commands while no verified client-owned binding/capability
is installed. It does **not** invent game method offsets or invoke gameplay
methods yet.

The current target build can therefore expose lifecycle/probe status only. In
`il2cpp_mapped_only` mode, without a verified build-specific binding, command
payloads are rejected as `binding_not_implemented`; setting an automation flag
in the controller cannot turn this into a live game action. The binding must
publish each capability only after its structured observation, client-owned
invoker, and definitive outcome hook have passed device verification. See
[`docs/LIVE_AUTOMATION_READINESS.md`](../docs/LIVE_AUTOMATION_READINESS.md).

## Why this is separate

The automation core must not depend on offsets, symbols, hook frameworks, or a particular Pokémon GO build. Version-specific work belongs behind `GameAdapter`.

## Building the native stub

1. Install Android NDK.
2. Download the published API 4 `zygisk.hpp` from the official
   `topjohnwu/zygisk-module-sample` repository when targeting Magisk v27.x
   (commit `7bb941ac8edfcffd1d23761e401c45ca95409dc1`). A newer API header can
   cause Magisk to create `zygisk/unloaded` and skip the native library.
3. Configure CMake with `-DZYGISK_API_DIR=/path/containing/zygisk.hpp`.
4. Build the shared library for the desired ABI.
5. Package the resulting ABI library under the Magisk module's `zygisk/` directory using the ABI filename expected by Zygisk.

The official Zygisk sample is the source of truth for API compatibility and packaging conventions.

## Map-target observation seam

`runtime_observation_protocol.h` defines the versioned private envelope used by
a verified Pokémon GO binding to publish a resolved map target. The companion
forwards it as the normal bridge `ObservationEvent`; it does not perform the
Unity touch hook or screen-to-geo projection. Those operations remain
version-scoped and must be installed only after the exact package/version/ABI
binding has been calibrated.

## Bridge peer authorization

The companion socket uses the abstract Unix socket name
`pogo_root_automation_runtime`; it is not a filesystem path. This avoids the
`/data/adb` parent directory's `0700` DAC boundary while keeping the endpoint
local to the device. The controller registers its Android UID in
`controller.uids` through root before connecting. The companion checks that
file and the peer's `SO_PEERCRED`; an unregistered app is rejected. The socket
is a transport endpoint only: raw observation payloads remain opaque and are
decoded in the controller's `game-adapter:pogo` module.
