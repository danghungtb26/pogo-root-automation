# M2 nearby binding boundary

M2 is split into two independent parts:

1. **Stable/testable nearby core** — raw observation mapping, validation, de-duplication, diffing, expiry confidence, and adapter selection.
2. **Version/runtime-specific game binding** — obtain the raw observation from the exact official Pokémon GO build running on the rooted device or emulator.

This separation is intentional: reverse-engineered bindings can change on every game update, while the nearby state machine should not.

## Raw observation contract

The version adapter must provide only structured facts it can actually observe:

- stable spawn identifier;
- species id;
- latitude / longitude;
- first-seen timestamp if known;
- expiry timestamp if known;
- expiry confidence: exact / estimated / unknown;
- observation timestamp and optional player position.

`PogoNearbyMapper` rejects malformed coordinates/ids and never upgrades unknown expiry to exact. `NearbySnapshotReducer` handles duplicate ids, expired entries, and add/update/remove diffs.

## Runtime binding probe

Before implementing a client binding, the root runtime performs read-only capability discovery:

1. observe the exact Pokémon GO process and package/version;
2. inspect `/proc/<pid>/maps` for `libunity.so`, `libil2cpp.so`, and translation libraries;
3. keep an exempt Zygisk companion socket across specialization;
4. from inside the target process, wait for `libil2cpp.so` to load;
5. attempt `RTLD_NOLOAD` + `dlsym` against a small set of IL2CPP C API exports;
6. persist the result through the root companion.

The controller/status script reports one of the relevant strategies:

- `il2cpp_exported_api` — preferred path. The runtime can resolve IL2CPP C API symbols and the next step is read-only IL2CPP metadata/type discovery.
- `il2cpp_mapped_only` — `libil2cpp.so` exists but exported symbols are unavailable from the injected runtime. A build-specific binding strategy is required.
- `unity_unknown_backend` — Unity loaded but IL2CPP has not been identified yet.
- `unavailable` — target process/native game modules are not ready.

On BlueStacks, the probe also reports `houdini` / `ndk_translation` when visible. A translated x86_64 environment must never reuse a native ARM binding just because the Pokémon GO version number matches.

## Build/runtime fingerprint

`GameBuild.fingerprint()` includes:

- package;
- version code/name;
- engine;
- binding strategy;
- Android primary ABI;
- kernel machine;
- translation layer.

Adapter resolution must fail closed when the observed runtime does not match a supported fingerprint.

## Device test flow

After installing the controller APK and multi-ABI Magisk module and rebooting:

```bash
./scripts/device-smoke-test.sh
./scripts/bluestacks-smoke-test.sh        # BlueStacks only
bash ./scripts/binding-probe-test.sh
./scripts/collect-binding-diagnostics.sh
```

`binding-probe-test.sh` waits for `probe_state=ready`. On timeout it automatically writes diagnostics so the next binding iteration has package/version/ABI/native-map evidence.

## Version fail-safe

`GameAdapterRegistry` resolves adapters against the observed package/version/runtime fingerprint. Zero matches is unsupported; multiple matches is ambiguous. Both cases fail closed instead of choosing a binding heuristically.

## What is still missing

No M2 code currently extracts Pokémon/spawn objects from Pokémon GO or calls game actions. The next implementation step depends on the probe result:

- exported IL2CPP API available → enumerate assemblies/types read-only and identify the map/spawn data path;
- mapped-only IL2CPP → analyze the exact client build and create a narrowly version-scoped adapter.

Binary/build-specific artifacts must not be committed to this repository; keep derived signatures/mappings small and version-scoped.
