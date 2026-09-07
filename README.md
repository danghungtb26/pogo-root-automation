# pogo-root-automation

Rooted Android automation framework for experimenting with structured game-state observation, built-in location control and deterministic automation around the official Pokémon GO client.

## Scope

- Built-in root-backed location control is supported through a floating joystick overlay.
- External GPS Joystick remains an optional fallback; the automation core is not coupled to either location implementation.
- The project is split into a stable automation core and version-specific Pokémon GO/runtime adapters.
- POGOProtos is vendored for decoding known Pokémon GO protobuf payloads into stable internal observation models.
- Catch, spin, discard and transfer rules live in the automation core; live mutation executors are kept separate from decision logic.
- No Play Integrity bypass, root hiding, anti-detection or account-safety evasion is implemented.

## Target architecture

```text
                         Official Pokémon GO
                          ^              |
                          |              | runtime/protobuf data
                          |              v
Built-in Joystick -> RootMockLocation   Zygisk/Runtime Bridge
       |                  ^              |
       |                  |              v
       +-> LocationController       POGOProtos decoder
                  ^                      |
                  |                      v
                  +---------- Automation Core
                              |- Nearby/countdown
                              |- Hundo/Shundo filters
                              |- Catch planner
                              |- Spin planner
                              |- Inventory/discard rules
                              `- Transfer rules

External GPS Joystick can replace the built-in location path when desired.
```

## Built-in joystick

The controller APK now includes a PogoEnhancer-style floating joystick:

- draggable overlay
- root-granted mock-location app-op
- GPS + network test providers
- 20 Hz movement updates
- proportional joystick strength
- speed presets
- coordinate display
- teleport dialog
- persisted last location

From the controller app, tap **Start built-in joystick**. Android will ask for **Display over other apps** permission the first time. The overlay then starts a foreground service and requests the mock-location app-op through `su`.

The location provider uses Android test providers. It intentionally does not contain mock-location hiding or anti-detection logic.

## Vendored PogoEnhancer dependencies

The following pinned binaries live in `app/libs`:

```text
POGOProtos-2.60.8.jar
bcpkix-jdk15on-1.60.jar
bcprov-jdk15on-1.60.jar
parser-1.6.0.aar
virtualjoystick-1.10.1.aar
```

`scripts/vendor-pogoenhancer-libs.sh` can reproduce them from the public `Map-A-Droid/PogoEnhancer` source dump and verifies both byte size and the exact Git blob SHA before replacing a file. See `THIRD_PARTY_NOTICES.md` for provenance.

`game-adapter:pogo` contains `PogoProtoDecoder`, which already compiles against the vendored POGOProtos and decodes known encounter and map-object payloads into `RawEncounterObservation` / `RawNearbyObservation`.

## Runtime targets

### Physical Android

Primary production/debug target: rooted ARM64 Android with Magisk/Zygisk.

### BlueStacks 5

BlueStacks is supported as an emulator runtime target when the instance has working `su` plus a Zygisk-compatible injector. The Magisk module is packaged with both:

- `zygisk/arm64-v8a.so`
- `zygisk/x86_64.so`

BlueStacks may expose x86_64 as its Android/Zygote ABI while translating ARM application libraries. Game bindings must therefore use collected runtime diagnostics rather than assuming ARM64 offsets/signatures apply to BlueStacks.

After installing the controller APK and multi-ABI Magisk ZIP and rebooting the instance, run:

```bash
./scripts/bluestacks-smoke-test.sh
./scripts/collect-binding-diagnostics.sh
```

## Milestones

1. **M0 Foundation** — Android project, domain contracts and tests.
2. **M1 Root runtime** — Zygisk lifecycle targeting and controller bridge boundary.
3. **M1.1 BlueStacks runtime** — x86_64/multi-ABI packaging and emulator diagnostics.
4. **M2 Nearby read-only** — nearby snapshot and despawn countdown contracts.
5. **M3 Automation core** — encounter/IV/Shundo, catch, spin, discard and transfer planners.
6. **M4 Built-in location** — PogoEnhancer-style joystick, teleport, speed control and root mock provider.
7. **M5 POGOProtos/live binding** — connect intercepted runtime RPC payloads to `PogoProtoDecoder` and the live `PogoRuntimeSource`.
8. **M6 Executors** — live catch/spin/discard/transfer/movement action execution.
9. **M7 Hardening** — version adapters, recovery, diagnostics and long-run tests.

> Gameplay automation can violate Pokémon GO's terms of service. Use test accounts and devices you control.
