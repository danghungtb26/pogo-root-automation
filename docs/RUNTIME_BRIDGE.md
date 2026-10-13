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

## Manual device handoff

The debug APK contains the validated Magisk ZIP. The user installs the APK via
the BlueStacks Air 1 UI, grants root on first foreground open, confirms reboot,
then observes connect/disconnect manually. Host device-smoke/deploy scripts are
not part of the canonical workflow.

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

## Diagnostics

The app UI is the only control surface. After a failed manual test, use the
target-guarded `scripts/collect-binding-diagnostics.sh` and
`scripts/logcat-full.sh`; missing device evidence keeps every affected
capability disabled.

## Action and location boundary

Gameplay action selection/execution remains native and must pass identity,
expiry, lifecycle, build and capability guards. Kotlin does not turn a desired
receipt, local arrival or UI event into a gameplay action. Native-issued map
target/navigation is handed to Kotlin only as a capability/freshness/lease
checked DTO; manual joystick, teleport and walk remain Kotlin-owned location
operations. Spin, discard, transfer and other bindings stay disabled unless
their exact binding and outcome evidence is advertised by native.
