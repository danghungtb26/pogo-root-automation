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

`HeadlessAutomationService` always runs the structured controller. The native
probe first publishes a conservative probe-only `RuntimeReady`. After the
explicit post-init diagnostic resolves client-owned services, the companion
can publish a stronger `RuntimeReady` update. On the verified Air 1 build, the
  current native binding advertises `READ_LIFECYCLE`, `ENCOUNTER`,
  `OPEN_ENCOUNTER`, and `USE_BERRY`; mutation still requires the
  `strongIdentityVerified` flag, the exact allowlist entry, and the action
  capability. Unsupported actions remain rejected and there is no alternate
  screen/input automation path.

## Explicit post-init diagnostic

For build-specific binding work, the controller exposes a read-only diagnostic
request:

```bash
ANDROID_SERIAL=127.0.0.1:5565 bash scripts/headless-control.sh diagnostic
```

Call it only after Pokémon GO has finished loading into a stable game screen.
The request travels through the existing authenticated bridge as a reserved
`Alert` marker; the injected side then inspects managed runtime class/method
signatures and resolves the client-owned encounter/item services through
Zenject. It publishes `ENCOUNTER`/`READ_LIFECYCLE` only when the structured
encounter fields are present, and publishes `USE_BERRY` only when the pinned
`libil2cpp` BuildID and exact
`ItemBagImpl.UseItemOnPokemon(Holoholo.Rpc.Item, UInt64)` signature/owner
checks pass. Encounter owners are refreshed periodically after the diagnostic
so short-lived encounter objects can be observed after the operator enters an
encounter. This explicit timing gate exists because managed IL2CPP calls
from the process-start probe previously crashed the exact BlueStacks Air 1
runtime during `il2cpp_init`.

## Current action binding

The implemented action path is deliberately narrow:

```text
AutomationCommand(UseBerry)
  -> bridge/session/process/build/lifecycle/expiry checks
  -> pinned ItemBagImpl instance from DiContainer.TryResolve(IItemBag)
  -> Unity main-thread post
  -> ItemBagImpl.UseItemOnPokemon(Item, encounterId)
  -> INDETERMINATE until the returned Promise has an observed outcome
```

Berry wire values map to the exact item IDs `701`, `703`, `705`, `706`, and
`708` for Razz, Nanab, Pinap, Golden Razz, and Silver Pinap. A duplicate
command ID, malformed encounter ID, stale command, wrong lifecycle, missing
owner, exception, or null Promise fails closed. The catch binding currently
resolves `PokeballService.Throw(GameObject, Vector3)` and validates the active
`EncounterPokemon`, selected `Pokeball`, and target position, but remains behind
a disabled capability gate until one live throw and its result transition are
observed. For `OPEN_ENCOUNTER`, the target resolver now tries the exact
`MapEntityCell.GetMapPokemon(UInt64)` and `MapEntityCell.GetMapTappable(UInt64)`
methods before the verified `WildMapPokemon.egwu` field path. The new resolver
is built for Air 1 but still needs an on-device action run after the controller
root permission is restored. Spin, discard, transfer, snapshot, and input/map
actions remain unavailable until their own bindings and outcome observations
are verified.
