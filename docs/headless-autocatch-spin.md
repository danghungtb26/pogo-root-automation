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

`start` enables auto-encounter, auto-catch, and auto-spin. `game` is only a
convenience for launching Pokémon GO; runtime readiness and observations are
reported by the bridge.

## API

### `GET /v1/status`

Returns worker/config state and the canonical structured runtime fields:
`runtimeSessionId`, `runtimeStrongIdentityVerified`, `runtimeLifecycle`,
`observationSeq`, `runtimeSuspended`, `lastAction`, and `lastError`.

### `POST /v1/start`

Optional query parameters:

- `autoEncounter=true|false`
- `catch=true|false` or `autoCatch=true|false`
- `autoCloseCatchPreview=true|false` — only active when the verified runtime
  advertises `CATCH_AND_CLOSE_PREVIEW`; otherwise normal catch remains enabled
- `spin=true|false` or `autoSpin=true|false`

Example:

```text
POST /v1/start?autoEncounter=true&catch=true&spin=true
```

### `POST /v1/stop`

Disables automation while leaving the service/API alive.

### `POST /v1/config`

Supported parameters include `autoEncounter`, `autoCatch`, `autoSpin`,
`autoCloseCatchPreview`,
`autoDiscard`, `autoTransfer`, `keepHundo`, `keepShiny`, `keepBackground`,
`keepFavorite`, `transferMinIv`, `berry`, `loopIntervalMs`,
`spinSettleDelayMs`, `catchSettleDelayMs`,
`buildFingerprints`, `throwQuality`, `curve`, `arPlus`, `autoSnapshot`,
`snapshotArPlus`, and `toasts`.

Throw settings are client-owned intents: `throwQuality` accepts `any`, `nice`,
`great`, or `excellent`; `curve` accepts `any`, `straight`, or `curve`.
`arPlus=true` requests Pokémon GO's AR+ encounter mode. `autoSnapshot=true`
requests a GO Snapshot while the encounter is active, and `snapshotArPlus=true`
requests that snapshot in AR+ mode. These settings are persisted and serialized
through the bridge, but the current probe-only runtime advertises none of the
required mutation capabilities, so it remains read-only and rejects them until
an exact build-scoped binding is installed.

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

When `autoCloseCatchPreview` is enabled, the structured `Catch` command carries
an explicit close-preview intent. The client-owned runtime must verify
`CAUGHT` before closing the preview and must advertise the
`CATCH_AND_CLOSE_PREVIEW` capability. The current probe-only runtime does not
advertise that capability, so it remains read-only/rejects mutation and never
uses a timer or screen-input fallback.
