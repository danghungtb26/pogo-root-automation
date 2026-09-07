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

M2 may replace or augment this transport when high-frequency nearby snapshots are required.

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

The controller-side structured path now uses a persistent Unix-domain-socket
contract with binary framing (`length`, protocol version, message type, and
sequence). Typed messages include `RuntimeReady`, `ObservationEvent`,
`AutomationCommand`, `AutomationCommandResult`, `BindingLost`, and
`RuntimeError`. Raw observation payloads remain opaque to the bridge and are
decoded by `game-adapter:pogo`.

`HeadlessAutomationService` runs the structured controller and no longer uses
screen recognition as its policy source. Mutation remains disabled until an
exact, strong build fingerprint is added to the local allowlist. The native
probe currently publishes diagnostics and a read-only `RuntimeReady`; it
rejects commands until a verified runtime binding/capability is installed.
