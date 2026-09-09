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

Throw settings are client-owned intents: `throwQuality` accepts `any`, `nice`,
`great`, or `excellent`; `curve` accepts `any`, `straight`, or `curve`.
`arPlus=true` requests Pokémon GO's AR+ encounter mode. `autoSnapshot=true`
requests a GO Snapshot while the encounter is active, and `snapshotArPlus=true`
requests that snapshot in AR+ mode. These settings are persisted and serialized
through the bridge. The direct map branch is separate from those encounter
throw settings: it uses `MapPokemon.TryCapture` with an ordinary Poké Ball and
does not open the encounter UI. It is intentionally limited to `catchAll`,
because nearby map state does not contain IV/shiny metadata. The spin branch
reads active PokéStops and invokes the client-owned spinner method.

Selecting `excellent` does not fabricate an excellent result. The runner only
releases a completed encounter catch after the runtime supplies a catch outcome
and, for a structured throw profile, a real throw outcome. A `MISSED` outcome is
still a completed throw attempt and is not silently retried. The current native
runtime does not publish the required throw capabilities, so this setting stays
blocked until the exact-build user/throw binding is calibrated.

The native bindings are now implemented and compiled for the supported ABIs,
but capability publication still depends on the exact runtime resolver. A
successful method lookup is not the same as a live gameplay postcondition;
the BlueStacks Air 1 run must verify the target and result before treating
`DIRECT_CATCH` or `SPIN` as live-ready.

`spinSettleDelayMs` and `catchSettleDelayMs` are controller-side waits after a
definitive `Spin`/`Catch` result and before planning another mutation. They are
independent from the command timeout and default to `1000` and `3500`
milliseconds. Values are clamped to `0..60000`; `0` disables that action's
settle wait. The runner still requires a fresh observation after the wait and
does not use the delay to recover an indeterminate command.

Legacy mode, visual-driver, and `encounterSweep` parameters are ignored. A
persisted `encounter_sweep` preference is migrated once to `auto_encounter`;
the old runtime selection preference is discarded and cannot activate another
automation path.

There are intentionally no `/v1/actions/catch` or `/v1/actions/spin` routes.
Any future manual action API must carry structured identity and observation
context and pass through `AutomationRunner`.

## Direct map catch and spin

When `autoEncounter=false`, `autoCatch=true`, and `catchAll=true`, the planner
creates a `DIRECT_MAP` catch from a fresh nearby spawn. The runtime resolves the
matching `WildMapPokemon` and calls `IMapPokemon.TryCapture` on Unity's main
thread. It never calls `OpenEncounter`, `OnTap`, or a screen-input fallback.
The native side does not advertise `DIRECT_CATCH` until it can observe an
authoritative catch result. If an invocation ever reaches `INDETERMINATE`, the
runner stays suspended; target disappearance alone cannot resolve it. After a
verified `CAUGHT`/`FLED` result, it waits for a complete fresh nearby snapshot
to confirm map synchronization before planning another mutation.

When direct map catch and spin are enabled together, `AutomationRunner` keeps a
FIFO mutation queue. The queue dispatches one action at a time, waits for its
runtime result, applies the action-specific settle delay, and requires a newer
observation before dispatching the next action. Direct map catch is placed
before the spin candidates so an expiring spawn is handled first. An
`INDETERMINATE` catch pauses the entire queue until its result is
authoritatively reconciled; spin is never sent while that catch is unresolved.

When `autoSpin=true`, the planner consumes a fresh `FORTS` observation, skips
cooling-down forts, and submits one `Spin` action at a time. The runtime resolves
the exact PokéStop ID, starts its interactive mode, obtains `ItemSpinner`, and
calls `PoiItemSpinner.Spin`. The next fort observation controls whether that
fort may be considered again.

When `autoCloseCatchPreview` is enabled, the structured `Catch` command carries
an explicit close-preview intent. The client-owned runtime must verify
`CAUGHT` before closing the preview and must advertise the
`CATCH_AND_CLOSE_PREVIEW` capability. The current probe-only runtime does not
advertise that capability, so it remains read-only/rejects mutation and never
uses a timer or screen-input fallback.
