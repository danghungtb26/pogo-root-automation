# Headless auto-catch and PokéStop spin

This mode runs the controller as an Android foreground service. The controller UI does not need to remain visible. Pokémon GO must remain the foreground game for the current root screen-driver implementation.

## Execution path

```text
Pokémon GO foreground
  -> root screencap
  -> lightweight screen-state analyzer
       -> encounter: throw ball with root input swipe
       -> PokéStop detail: spin disc with root input swipe
       -> overworld blue stop candidate: tap and verify detail screen
       -> optional encounter sweep: tap conservative map points and verify encounter
  -> repeat
```

The local API is a control API for this tool; it is not a direct Niantic/Pokémon GO server API. Direct server RPC would require the live game session/auth/signing stack and is intentionally not used by this implementation.

## Start

Open the controller once after installing the APK. `HeadlessAutomationService` starts and binds its API to loopback only:

```text
127.0.0.1:8765
```

The service is also registered for boot completed so it can come back after an emulator/device reboot. Automation itself stays at its last persisted enabled/disabled state.

## Control from the host

Forward the loopback API through ADB:

```bash
adb forward tcp:8765 tcp:8765
```

Then:

```bash
curl http://127.0.0.1:8765/v1/status
curl -X POST 'http://127.0.0.1:8765/v1/start?catch=true&spin=true&encounterSweep=true'
curl -X POST http://127.0.0.1:8765/v1/stop
```

Or use:

```bash
bash scripts/headless-control.sh status
bash scripts/headless-control.sh start
bash scripts/headless-control.sh stop
```

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

These are useful for calibrating a BlueStacks resolution before enabling the full loop.

## Current limitations

The current executor is resolution-independent by using normalized coordinates, but screen recognition is heuristic and needs real-device/BlueStacks calibration. The controller can stay in the background, but Pokémon GO must stay foreground because the executor currently uses root `screencap` plus Android `input tap/swipe`.

The longer-term runtime path is to replace this screen driver with the PogoEnhancer-style injected executor: observe game objects/RPC data and invoke encounter/spin/catch game methods directly. The local control API and automation policy can remain unchanged when that executor is introduced.
