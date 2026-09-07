# Headless auto-catch and PokéStop spin

This mode runs the controller as an Android foreground service. The service's
automation policy path is structured-runtime first and does not use screenshots
or `input tap/swipe` to decide game state. The legacy screen driver remains in
the source only for calibration/backward-compatible direct engine construction.

## Execution path

```text
Pokémon GO process
  -> Zygisk runtime + companion broker
  -> persistent binary bridge
  -> RuntimeReady / structured observations
  -> PogoProtoDecoder + PogoGameAdapter
  -> AutomationCoordinator
  -> AutomationRunner (one mutation, await outcome, resync)
  -> client-owned runtime command
```

The native probe-only build currently exposes liveness and a read-only ready
event; it rejects commands until a verified binding announces the required
capability. Screen capture/input helpers are retained only as legacy code and
are not used by the service policy loop.

The local API is a control API for this tool; it is not a direct Niantic/Pokémon GO server API. Direct server RPC would require the live game session/auth/signing stack and is intentionally not used by this implementation.

## Start

The service binds its API to loopback only:

```text
127.0.0.1:8765
```

For first install, the host helper can briefly launch the controller activity to bootstrap the foreground service and then control it entirely over ADB-forwarded HTTP:

```bash
bash scripts/headless-control.sh bootstrap
```

The service is also registered for `BOOT_COMPLETED`, so after it has been installed it can come back after a normal emulator/device reboot. Automation itself stays at its last persisted enabled/disabled state.

## Control from the host

The helper automatically establishes:

```bash
adb forward tcp:8765 tcp:8765
```

Common commands:

```bash
bash scripts/headless-control.sh bootstrap
bash scripts/headless-control.sh status
bash scripts/headless-control.sh start
bash scripts/headless-control.sh game
bash scripts/headless-control.sh stop
```

`start` enables auto-catch, auto-spin and the encounter sweep. `game` brings Pokémon GO to foreground. After that the controller UI can remain hidden/backgrounded.

Manual executor smoke tests:

```bash
bash scripts/headless-control.sh catch
bash scripts/headless-control.sh spin
```

## API

### `GET /v1/status`

Returns worker state, whether Pokémon GO is foreground, current detected screen state and counters.

### `POST /v1/start`

Optional query parameters:

- `catch=true|false`
- `spin=true|false`
- `encounterSweep=true|false`

Example:

```text
POST /v1/start?catch=true&spin=true&encounterSweep=true
```

### `POST /v1/stop`

Disables automation while leaving the service/API alive.

### `POST /v1/config`

Supported parameters:

- `autoCatch`
- `autoSpin`
- `encounterSweep`
- `loopIntervalMs`
- `catchThrowDurationMs`
- `catchResultDelayMs`
- `spinOpenDelayMs`
- `spinSwipeDurationMs`
- `spinResultDelayMs`
- `actionCooldownMs`

Example:

```bash
curl -X POST 'http://127.0.0.1:8765/v1/config?loopIntervalMs=750&catchThrowDurationMs=300&spinOpenDelayMs=1400'
```

### Manual actions

```text
POST /v1/actions/catch
POST /v1/actions/spin
```

These are retained for legacy screen-driver calibration only; the production
structured service rejects manual screen actions.

## Current limitations

The structured path still needs live observation hooks in the injected runtime and
version-scoped client-owned invokers. Until those are verified, the native broker
announces no mutation capabilities and the controller remains read-only. The
legacy normalized-coordinate executor is retained for calibration/direct use,
but is not a source of state or decisions for the foreground service.
