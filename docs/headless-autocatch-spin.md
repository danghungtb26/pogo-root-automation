# Headless structured automation

The Android foreground service has one automation path: structured observations
from the runtime bridge are decoded by the POGO adapter, planned by the core,
and submitted through `AutomationRunner`. There is no visual-state fallback and
no direct device-input action API.

```text
Foreground service
  -> persistent runtime bridge
  -> RuntimeReady / structured observations
  -> PogoProtoDecoder + PogoGameAdapter
  -> AutomationRunner (one mutation, await outcome, resync)
  -> persistent companion command channel
```

Until live observations, a verified identity, an allowlisted build fingerprint,
and action capabilities are available, the service remains read-only and
fail-closed. It never substitutes another state or action source.

## Start

The service binds its API to loopback only:

```text
127.0.0.1:8765
```

Bootstrap the foreground service and ADB forward from the host:

```bash
bash scripts/headless-control.sh bootstrap
bash scripts/headless-control.sh status
bash scripts/headless-control.sh start
bash scripts/headless-control.sh game
bash scripts/headless-control.sh stop
```

`start` enables the persisted policy. For the direct map flow, use
`autoEncounter=false&autoCatch=true&autoSpin=true`; `game` is only a
convenience for launching Pokémon GO. Runtime readiness and observations are
reported by the bridge.

## API

### `GET /v1/status`

Returns worker/config state and the canonical structured runtime fields:
`runtimeSessionId`, `runtimeStrongIdentityVerified`, `runtimeCapabilities`,
`runtimeMutationPermissionGranted`, `runtimeLifecycle`, `observationSeq`,
`runtimeSuspended`, `lastAction`, and `lastError`. Policy booleans such as
`autoCatch=true` are requested intents; `runtimeCapabilities` is the source of
truth for what the target process can actually execute. See
[`LIVE_AUTOMATION_READINESS.md`](LIVE_AUTOMATION_READINESS.md) for the current
device result and action matrix.

### `POST /v1/start`

Optional query parameters:

- `autoEncounter=true|false`
- `catch=true|false` or `autoCatch=true|false`
- `catchAll=true|false` — direct-map catch-all filter
- `autoExcellent=true|false` — convenience alias for the encounter throw quality
- `autoCloseCatchPreview=true|false` — only active when the verified runtime
  advertises `CATCH_AND_CLOSE_PREVIEW`; otherwise normal catch remains enabled
- `spin=true|false` or `autoSpin=true|false`

Example for direct map catch + spin:

```text
POST /v1/start?autoEncounter=false&catch=true&spin=true
```

### `POST /v1/stop`

Disables automation while leaving the service/API alive.

### `POST /v1/config`

Supported parameters include `autoEncounter`, `autoCatch`, `autoSpin`,
`catchAll`,
`autoExcellent`,
`autoCloseCatchPreview`,
`autoDiscard`, `autoTransfer`, `keepHundo`, `keepShiny`, `keepBackground`,
`keepFavorite`, `transferMinIv`, `berry`, `loopIntervalMs`,
`spinSettleDelayMs`, `catchSettleDelayMs`,
`buildFingerprints`, `throwQuality`, `curve`, `arPlus`, `autoSnapshot`,
`snapshotArPlus`, and `toasts`.

`autoExcellent=true` is a convenience alias for
`throwQuality=excellent`; `GET /v1/status` and `POST /v1/config` expose the
resolved value as both `autoExcellent` and `throwQuality`.

Throw settings are encounter-flow intents: `throwQuality` accepts `any`, `nice`,
`great`, or `excellent`; `curve` accepts `any`, `straight`, or `curve`.
`arPlus=true` requests Pokémon GO's AR+ encounter mode. `autoSnapshot=true`
requests a GO Snapshot while the encounter is active, and `snapshotArPlus=true`
requests that snapshot in AR+ mode. These settings are persisted and serialized
through the bridge. Native catch-spin is separate from those encounter throw
settings: it reads nearby/forts/inventory on the observer tick, filters valid
catch-all targets, and invokes `MapPokemon.TryCapture` or the PokéStop spinner
directly on the main thread. It never opens the encounter UI. The native policy
is intentionally limited to `catchAll`, because nearby map state does not contain
IV/shiny metadata.

The persisted catch/spin settings carry a monotonic `configRevision`. Every
change from the overlay or loopback API is saved by the Android repository and
the complete snapshot is dispatched once to the enabled native `catch_spin`
module through `CONFIG_SET`. Native validates the session identity, exact build,
expiry, and revision before replacing its runtime config mirror and returning an
idempotent acknowledgement. The mirror is reset when the runtime session stops;
Android remains the durable source of truth across process restarts.

Selecting `excellent` does not fabricate an excellent result. The runner only
releases a completed encounter catch after the runtime supplies a catch outcome
and, for a structured throw profile, a real throw outcome. A `MISSED` outcome is
still a completed throw attempt and is not silently retried. The current native
runtime does not publish the required throw capabilities, so this setting stays
blocked until the exact-build user/throw binding is calibrated.

The native bindings are now implemented and compiled for the supported ABIs,
but capability publication still depends on the exact runtime resolver. A
successful method lookup is not the same as a live gameplay postcondition; the
BlueStacks Air 1 run must verify the target and invocation before treating
`DIRECT_CATCH` or `SPIN` as live-ready. If direct-catch outcome observation is
not verified, native suspends after an invocation rather than risking a second
throw.

`spinSettleDelayMs` is a native wait after a PokéStop invocation and before the
next native scan. `catchSettleDelayMs` remains available to the encounter flow.
They are independent from command timeout and default to `1000` and `3500`
milliseconds. Values are clamped to `0..60000`; `0` disables the corresponding
settle wait. Native does not use a timer to recover an indeterminate catch.

Legacy mode, visual-driver, and `encounterSweep` parameters are ignored. A
persisted `encounter_sweep` preference is migrated once to `auto_encounter`;
the old runtime selection preference is discarded and cannot activate another
automation path.

There are intentionally no `/v1/actions/catch` or `/v1/actions/spin` routes.
Catch-spin mutations are generated only by the enabled native observer; any
future manual action API must carry structured identity and observation context.

## Direct map catch and spin

When the native catch-spin module is armed and `autoCatch=true` with
`catchAll=true`, its observer scans on its own cadence, selects a valid nearby
spawn, verifies that the Poké Ball stack is known and non-zero, then resolves the
matching `WildMapPokemon` and calls `IMapPokemon.TryCapture` on Unity's main
thread. It never calls `OpenEncounter`, `OnTap`, or a screen-input fallback.

If the direct invocation is indeterminate, native suspends catch-spin and does
not retry or spin blindly. Target disappearance alone cannot resolve it. A
zero-ball read switches the native decision to spinning; when `autoSpin=true`,
native selects an available PokéStop, invokes `PoiItemSpinner.Spin`, and applies
the native settle delay before its next scan. Kotlin no longer owns a catch-spin
planner, map-scan request, direct-map action model, or mutation queue.

When `autoCloseCatchPreview` is enabled, the structured `Catch` command carries
an explicit close-preview intent. The client-owned runtime must verify
`CAUGHT` before closing the preview and must advertise the
`CATCH_AND_CLOSE_PREVIEW` capability. The current probe-only runtime does not
advertise that capability, so it remains read-only/rejects mutation and never
uses a timer or screen-input fallback.
