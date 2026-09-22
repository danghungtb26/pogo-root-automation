# M1 runtime bridge

M1 intentionally proves rooted process lifecycle before any game-state hooking.

## Flow

```text
Pokémon GO starts
  -> Zygisk preAppSpecialize sees target process
  -> module sends a small versioned RuntimeEvent to Zygisk companion
  -> companion (root) atomically writes /data/adb/pogo_root_automation/runtime.status
  -> controller executes runtime-status.sh through su
  -> script validates that the recorded PID still belongs to the same process
  -> script reports the package/version for the attached build
  -> controller parses key=value output into RuntimeSnapshot
```

This is deliberately a polling bridge for M1. It has two useful properties:

1. the injected library does not need to stay resident merely to report lifecycle;
2. a stale status file cannot report `connected`, because the root status script verifies `/proc/<pid>/cmdline` and process liveness.

M2 is now augmented by the persistent structured bridge below. The root status
file remains diagnostics only and is not a gameplay/config queue.

## Root state

The companion writes only:

- protocol version;
- PID;
- target process name;
- observation timestamp.

The controller-side root script derives current liveness and installed game version at read time. If both supported Pokémon GO packages are installed, the observed process is preferred so the reported version matches the actual attached client.

## Device smoke test

After installing the controller APK and the Magisk zip from CI, reboot the rooted device, connect ADB, then run:

```bash
./scripts/device-smoke-test.sh
```

The script verifies:

1. ADB can obtain root via `su`;
2. the module status command exists;
3. launching Pokémon GO reaches `connected` and reports a version;
4. force-stop reaches `disconnected`;
5. relaunch returns to `connected`.

It does not test any game-state hook because M1 intentionally has none.

## Current limitation

There is no explicit process-death callback from this M1 stub. A dead/reused PID is detected by checking both liveness and `/proc/<pid>/cmdline`. Runtime state therefore converges to disconnected on the controller's next poll.

## Structured bridge contract

The live controller path uses a persistent Unix-domain-socket contract with
binary framing (`length`, protocol version 3, message type and sequence).
Typed messages include `RuntimeReady`, `RuntimeUiStatus`, desired-state
requests, automation results/events, navigation, map target, diagnostics,
`BindingLost` and `RuntimeError`. The UI router consumes typed DTOs; it does
not decode POGO protobuf or construct a game adapter.

`RuntimeUiAutomationFacade` persists user settings and sends one full desired
snapshot with revision/expiry. Native owns readiness, build/capability guards,
config reconciliation and gameplay. `RuntimeUiStatus` separates desired,
applied and ready state. On reconnect the client resends the latest desired
snapshot; the broker caches only the latest UI status and never replays a
gameplay command. Unsupported actions remain rejected and there is no alternate
screen/input automation path.

## Explicit post-init diagnostic

For build-specific binding work, the controller exposes an explicit diagnostic
request:

```bash
ANDROID_SERIAL=127.0.0.1:5565 bash scripts/headless-control.sh diagnostic
```

Call it only after Pokémon GO has finished loading into a stable game screen.
The request travels through the existing authenticated bridge. Native performs
build-specific discovery only behind its lifecycle/main-thread guards and
publishes capabilities/status only after the corresponding checks pass. Missing
device evidence keeps the capability disabled; this document does not claim a
live Air 1 result when no Air 1 target is available.

## Action and location boundary

Gameplay action selection/execution remains native and must pass identity,
expiry, lifecycle, build and capability guards. Kotlin does not turn a desired
receipt, local arrival or UI event into a gameplay action. Native-issued map
target/navigation is handed to Kotlin only as a capability/freshness/lease
checked DTO; manual joystick, teleport and walk remain Kotlin-owned location
operations. Spin, discard, transfer and other bindings stay disabled unless
their exact binding and outcome evidence is advertised by native.
