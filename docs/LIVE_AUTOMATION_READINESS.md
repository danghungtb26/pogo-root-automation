# Live automation readiness

Updated: 2026-09-08

This document records the boundary between the automation pipeline and live
Pokémon GO gameplay actions. Configuration flags are policy intents. A live
action is permitted only when the exact game runtime publishes the matching
capability, a strong identity, and an allowlisted build fingerprint.

## Current device result

The baseline non-teleport harness evidence was captured on the earlier
`emulator-5564` connection with:

```text
package=com.nianticlabs.pokemongo
version_name=0.427.0
version_code=2026082702
device_primary_abi=arm64-v8a
kernel_machine=aarch64
translation_layer=none
binding_strategy=il2cpp_mapped_only
native_il2cpp_api_available=0
native_il2cpp_symbol_count=0/10
native_assembly_survey_state=unavailable
native_class_survey_state=unavailable
base_apk_sha256=405a351ce2f89be9b03c0cf7d2ec364ccad619bbc24b1ca064022f18b75674c2
arm64_split_apk_sha256=8ae4f91212727b4709ad40e41c811783f62fcc06b429557b23d11f6ac24c3e7e
global_metadata_sha256=2c7c88033dbdff96ba35cab5bc2d44943b00e8c20874cd1ee1bd8e768e780212
global_metadata_bytes=38678636
libil2cpp_bytes=197376152
```

The APK contains `assets/bin/Data/Managed/Metadata/global-metadata.dat`, but the
current runtime cannot enumerate its assemblies/classes and the repository has
no verified metadata/code-registration resolver for this build. Presence of the
metadata file is evidence for a future binding analysis; it is not evidence
that a method can be invoked safely.

The lifecycle bridge passed its reconnect test and the Android structured
pipeline started successfully. The runtime advertised no capabilities, so the
result is read-only. No teleport/map-target test and no destructive item or
Pokémon operation were run.

The active test target is now the BlueStacks instance `BlueStacks Air 1`,
normally connected as `127.0.0.1:5565`. Its read-only native probe resolved the
exact build's exported IL2CPP symbols through the loaded ELF table after the
Android linker namespace hid them from `dlsym`:

```text
IL2CPP API resolved through loaded ELF exports
symbols=16
```

This only proves export discovery. It does not prove game-class ownership,
method signatures, object lifetime, action outcomes, or mutation permission.
The runtime therefore remains capability-empty and fail-closed.

An earlier experimental probe called managed IL2CPP domain/assembly APIs from
the Zygisk thread during `il2cpp_init`; Air 1 reproduced a SIGSEGV/SIGABRT in
`libil2cpp.so` immediately after the export-discovery log. That survey path is
now disabled. The recovery module performs ELF export discovery only and
defers all managed calls until a verified post-initialization lifecycle hook
exists. After reinstall/restart, Pokémon GO remained alive and the controller
returned to `runtimeLifecycle=STARTING` with no new runtime error.

`autoEncounter`, `autoCatch`, `autoSpin`, `autoDiscard`, and `autoTransfer` in
`/v1/status` describe the requested policy, not confirmed execution. The
status response now also exposes:

- `runtimeCapabilities`: capabilities actually advertised by the target
  runtime;
- `runtimeMutationPermissionGranted`: identity plus build-allowlist gate;
- `runtimeLifecycle`, `observationSeq`, `lastAction`, and `lastError`.

For the current device, the expected live-action fields are:

```json
{
  "runtimeCapabilities": [],
  "runtimeMutationPermissionGranted": false,
  "runtimeLifecycle": "STARTING"
}
```

## Action capability matrix

| Policy action | Required runtime capability | Additional evidence |
|---|---|---|
| auto-encounter | `OPEN_ENCOUNTER` | fresh nearby observation and target id |
| catch | `CATCH` | fresh encounter observation and definitive outcome |
| close catch preview | `CATCH_AND_CLOSE_PREVIEW` | `CAUGHT` confirmed before close |
| throw quality/curve | `THROW_CONTROL`, `OBSERVE_THROW_OUTCOME` | client-owned throw result |
| berry | `USE_BERRY` | fresh encounter and item result |
| snapshot | `SNAPSHOT_DURING_ENCOUNTER` | encounter-active snapshot result |
| AR+ mode | `AR_ENCOUNTER` | AR+ mode validated by the client |
| spin | `SPIN` | fresh fort observation and spin result |
| discard | `DISCARD_ITEM` | inventory revision and item result |
| transfer | `TRANSFER_POKEMON` | storage revision and transfer result |

The core already plans and serializes these actions. The missing component is
the version-scoped client-owned binding inside the target process that reads
the relevant game objects, invokes the client method, and reports the outcome.

## Why a config-only change cannot enable this build

The injected native component currently performs process/lifecycle detection,
IL2CPP probing, bridge transport, and safe command rejection. It does not know
the Pokémon GO classes, method signatures, object ownership, or outcome hooks
for build `0.427.0`. In `il2cpp_mapped_only` mode the required IL2CPP exports are
not available either. Guessing offsets or calling methods by name would break
the fail-closed boundary and can crash the game or produce an unverified
action. Therefore the native runtime deliberately returns
`binding_not_implemented` until a verified binding is installed.

## Required implementation sequence

1. Capture read-only evidence for the exact package, version, ABI, native
   library build id, metadata hash, and runtime process.
2. Create a small binding selected by the complete build fingerprint. It must
   resolve game-owned objects and methods without a screen/input fallback.
3. Add structured observations for lifecycle, nearby, encounter, fort,
   inventory, and storage as each path is verified.
4. Add client-owned invokers and outcome hooks for one action at a time. Each
   command must validate session, observation freshness, capability, and
   idempotency before invocation.
5. Verify outcomes on-device, then publish only the capabilities that passed
   those tests and add the exact fingerprint to the mutation allowlist.

Until these steps are complete, the correct behavior is read-only plus an
explicit rejection; enabling the policy flags alone must not trigger gameplay.

## Verification commands

```bash
ANDROID_SERIAL=127.0.0.1:5565 bash scripts/binding-probe-test.sh
ANDROID_SERIAL=127.0.0.1:5565 bash scripts/headless-control.sh status
ANDROID_SERIAL=127.0.0.1:5565 bash scripts/bluestacks-smoke-test.sh
```

The last command is only applicable when the target reports BlueStacks. These
checks do not test teleport or perform destructive inventory/storage actions.
