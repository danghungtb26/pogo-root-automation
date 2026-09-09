# Live automation readiness

Updated: 2026-09-09

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

The initial lifecycle bridge capture passed its reconnect test and the Android
structured pipeline started successfully. At that point the runtime advertised
no capabilities, so that result was read-only. No teleport/map-target test and
no destructive item or Pokémon operation were run.

The active test target is now the BlueStacks instance `BlueStacks Air 1`,
normally connected as `127.0.0.1:5565`. Its native probe resolved the exact
build's exported IL2CPP symbols through the loaded ELF table after the Android
linker namespace hid them from `dlsym`:

```text
IL2CPP API resolved through loaded ELF exports
symbols=16
```

After Pokémon GO reached a stable overworld, the explicit post-init diagnostic
also resolved the managed domain and the client-owned Zenject services:

```text
expected libil2cpp build id match=1
IItemBag -> Niantic.Holoholo.Internal.ItemBagImpl
IPokemonBag -> Niantic.Holoholo.Internal.PokemonBagImpl
ILocationProvider -> Niantic.Holoholo.Map.NativeLocationProvider
UseItemOnPokemon(Holoholo.Rpc.Item, System.UInt64)
```

The broker then published a stronger `RuntimeReady` update with
`READ_LIFECYCLE`, `ENCOUNTER`, `OPEN_ENCOUNTER`, and `USE_BERRY`. This proves
binding identity, owner resolution, encounter field signatures, the dynamic
tappable contract, and the item method signature; it does not yet prove a live
berry Promise outcome or a live catch outcome.

Encounter owners are refreshed periodically after the diagnostic. This allows
the reader to discover the short-lived `EncounterPokemon` and
`EncounterInteractionState` instances when the operator enters an encounter,
without enumerating Unity objects from a worker thread. The refresh and the
observation/action readers share a binding lock so a transition cannot expose
half-updated managed pointers.

An earlier experimental probe called managed IL2CPP domain/assembly APIs from
the Zygisk thread during `il2cpp_init`; Air 1 reproduced a SIGSEGV/SIGABRT in
`libil2cpp.so` immediately after the export-discovery log. That survey path is
now disabled. The recovery module performs ELF export discovery during startup;
managed calls are deferred to the explicit post-initialization diagnostic after
the game is stable. After reinstall/restart, Pokémon GO remained alive and the
controller returned to `runtimeLifecycle=STARTING` with no new runtime error.

`autoEncounter`, `autoCatch`, `autoSpin`, `autoDiscard`, and `autoTransfer` in
`/v1/status` describe the requested policy, not confirmed execution. The
status response now also exposes:

- `runtimeCapabilities`: capabilities actually advertised by the target
  runtime;
- `runtimeMutationPermissionGranted`: identity plus build-allowlist gate;
- `runtimeLifecycle`, `observationSeq`, `lastAction`, and `lastError`.

For the current device after the diagnostic, the observed fields are:

```json
{
  "runtimeCapabilities": ["ENCOUNTER", "OPEN_ENCOUNTER", "READ_LIFECYCLE", "USE_BERRY"],
  "strongIdentityVerified": true,
  "runtimeMutationPermissionGranted": false,
  "runtimeLifecycle": "OVERWORLD"
}
```

Mutation permission is still false because the exact fingerprint is not
configured in the controller allowlist. No berry command was invoked: this
run had no structured encounter observation, so there was no valid encounter
ID to send to the game.

The direct map catch and PokéStop spin bindings were added after this baseline
capture. The native code compiles for both supported ABIs, but no new Air 1
live invocation has been recorded yet. `DIRECT_CATCH` remains absent until a
run verifies target lookup, the authoritative asynchronous result boundary,
and the complete-map synchronization postcondition.

## Action capability matrix

| Policy action | Required runtime capability | Additional evidence |
|---|---|---|
| auto-encounter | `OPEN_ENCOUNTER` | fresh nearby observation and target id |
| catch | `CATCH` | fresh encounter observation and definitive outcome |
| direct map catch | `DIRECT_CATCH` | fresh nearby observation, live `WildMapPokemon`, verified authoritative catch result observer, and complete-map synchronization; no encounter UI |
| close catch preview | `CATCH_AND_CLOSE_PREVIEW` | `CAUGHT` confirmed before close |
| throw quality/curve | `THROW_CONTROL`, `OBSERVE_THROW_OUTCOME` | client-owned throw result |
| berry | `USE_BERRY` | fresh encounter and item result; current binding returns `INDETERMINATE` until Promise outcome observation is added |
| snapshot | `SNAPSHOT_DURING_ENCOUNTER` | encounter-active snapshot result |
| AR+ mode | `AR_ENCOUNTER` | AR+ mode validated by the client |
| spin | `SPIN` | fresh `FORTS` observation, non-cooling PokéStop, and spin result |
| discard | `DISCARD_ITEM` | inventory revision and item result |
| transfer | `TRANSFER_POKEMON` | storage revision and transfer result |

The core already plans and serializes these actions. Direct map catch and spin
now have version-scoped client-owned bindings inside the target process. Spin
has its own live verification gate; direct map catch additionally requires the
authoritative result observer before the capability can be exposed.

## Current action boundary

The injected native component now has one version-scoped client-owned invoker:
`ItemBagImpl.UseItemOnPokemon(Item, UInt64)`. It is posted to the Unity main
thread and is reachable only through the authenticated bridge after the exact
BuildID/owner/signature checks pass. The invoker reports `INDETERMINATE` after a
non-null Promise because the current runtime does not yet observe the
asynchronous server/result transition. It must not claim `COMPLETED` or retry
automatically in that state.

It also has a separate direct-map path for `MapPokemon.TryCapture` and a
PokéStop path for `MapPlaceDirectoryService.GetPokestop` →
`MapPokestop.StartInteractiveMode` → `PoiItemSpinner.Spin`. The map reader
publishes nearby spawns and structured fort IDs/cooldown state as independent
observations. `DIRECT_CATCH` is only planned for catch-all map spawns, and the
path does not call the encounter opener. Both action gates require the exact
build identity, an overworld lifecycle, a fresh observation, and idempotency;
direct catch remains indeterminate until its asynchronous result is observed.

The encounter reader also resolves `IEncounterPokemon` and
`IEncounterState` dynamically and emits a structured encounter observation when
their `MapPokemon` backing object is valid. A `PokeballService.Throw` contract
has been identified and is implemented behind a disabled gate, but `CATCH` is
not advertised: no live throw postcondition has been verified yet. The earlier
`MapPokemon.OnTap` probe invoked without opening an encounter and remains
disabled for direct catch. The open-encounter resolver now uses the build's exact
`MapEntityCell.GetMapPokemon(UInt64)`/`GetMapTappable(UInt64)` methods before
reading the verified `WildMapPokemon.egwu` tappable field. The code is compiled,
but the updated route still needs a live Air 1 run after the controller's root
permission is restored. This keeps `autoCatch` from issuing unsupported
commands while the runtime binding is still being calibrated.

The bridge/native parser now accepts the structured throw payload, including an
`EXCELLENT` quality intent, and preserves it through the exact command boundary.
That parser change does not publish `THROW_CONTROL` or enable encounter catch:
the runtime still lacks a verified user-throw hook and authoritative catch
outcome. A completed catch without `CatchOutcome`, or a structured throw without
`ThrowOutcome`, is converted to `INDETERMINATE` by the core runner.

All other actions still return `binding_not_implemented` or a capability
rejection. Configuring a berry mode or allowlist entry alone cannot create an
encounter observation or prove the item result. The structured controller may
plan an action, but the runtime capability and strong identity remain the
authority for whether it can be sent.

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
5. Verify outcomes on-device, then add Promise/state-transition observation and
   publish only capabilities that passed those tests. Add the exact fingerprint
   to the mutation allowlist only for an intentional live test.

Until the new direct paths have passed the Air 1 live run, the correct behavior
is capability-gated execution with explicit indeterminate handling; enabling
the policy flags alone must not bypass identity, freshness, or binding checks.

## Verification commands

```bash
ANDROID_SERIAL=127.0.0.1:5565 bash scripts/binding-probe-test.sh
ANDROID_SERIAL=127.0.0.1:5565 bash scripts/headless-control.sh status
ANDROID_SERIAL=127.0.0.1:5565 bash scripts/bluestacks-smoke-test.sh
```

The last command is only applicable when the target reports BlueStacks. These
checks do not test teleport or perform destructive inventory/storage actions.
