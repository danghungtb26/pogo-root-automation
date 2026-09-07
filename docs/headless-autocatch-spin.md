# Headless auto-catch and PokéStop spin

This mode runs the controller as an Android foreground service. The service
defaults to the existing screen automation path, so current headless behavior
continues to work. The structured-runtime path is an explicit opt-in and does
not use screenshots or `input tap/swipe` to decide game state.

## Execution path

```text
Foreground service
  -> runtimeMode=screen (default) -> screencap + analyzer + root input
  -> runtimeMode=structured -> Zygisk runtime + companion broker
       -> persistent binary bridge
       -> RuntimeReady / structured observations
       -> PogoProtoDecoder + PogoGameAdapter
       -> AutomationRunner (one mutation, await outcome, resync)
       -> persistent companion command channel
```

The native probe-only build currently exposes liveness and a read-only ready
event; it rejects commands until a verified binding announces the required
capability. The structured mode is not selected automatically from readiness;
it must be explicitly configured after live observations and bindings are
verified.

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
- `runtimeMode=screen|structured` (hot-switches the running engine; structured bridge connects only while selected)
- `buildFingerprints=<comma-separated exact fingerprints>` for structured mutation allowlisting

Example:

```bash
curl -X POST 'http://127.0.0.1:8765/v1/config?loopIntervalMs=750&catchThrowDurationMs=300&spinOpenDelayMs=1400'
```

### Manual actions

```text
POST /v1/actions/catch
POST /v1/actions/spin
```

Manual actions are available in `screen` mode. They are rejected in
`structured` mode because that mode has no screen/input policy source.

## Current limitations

The structured path still needs live observation hooks in the injected runtime and
version-scoped client-owned invokers. Until those are verified, the native broker
announces no mutation capabilities and the controller remains read-only. Its
command channel is persistent end-to-end, but currently returns a safe rejection
for unimplemented bindings. Mutation additionally requires `strongIdentityVerified`
from the runtime, an exact allowlisted fingerprint, and the action capability.
The screen path remains the service default.
