# AGENTS.md

## Project overview

`pogo-root-automation` is a rooted Android automation framework for
Pokémon GO. Keep the stable automation/domain layer independent from
version-specific Pokémon GO and Unity/IL2CPP bindings.

The repository contains:

- `core/`: pure domain models, geo math, movement planning and reusable policy helpers.
- `bridge/protocol/`: versioned bridge frames, runtime events and payload codecs.
- `game-adapter/api/`: capability contracts for game adapters.
- `game-adapter/pogo/`: Pokémon GO runtime source, protobuf decoding and bridge adapter.
- `game-adapter/fake/`: deterministic fake adapter used by tests.
- `app/`: Android controller, config persistence, headless service, overlay and mock-location provider.
- `zygisk/`: native gameplay automation, version-specific bindings and root companion bridge.
- `scripts/`: device diagnostics, smoke tests and packaging helpers.
- `docs/`: architecture, binding boundaries and feature design notes.

## Project structure and ownership rules

Read [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) before placing new code or
changing native/Kotlin ownership, persistence, or the bridge contract. It is the
current structure reference; older milestone documents may describe retired
Kotlin orchestration paths.

### Native (`zygisk/`)

- Put live gameplay scheduling and execution in `zygisk/jni/modules/<feature>/`.
  Native owns catch/spin/discard/transfer decisions from current game state,
  pending actions, outcome observation, and gameplay cooldowns. Do not add a
  second Kotlin loop that plans or dispatches the same live actions.
- Put reusable process/IL2CPP services in `shared/runtime/`, bootstrap and
  low-level helpers in `shared/core/`, controller IPC in `shared/bridge_kotlin/`,
  and the target-process/companion channel in `shared/bridge_appproc/`.
  `host/` publishes capabilities; it must not absorb feature policy.
- Keep `main.cpp` as composition/include wiring. Preserve the current single
  translation unit and its include dependencies when splitting `.inc` files.
- The injected runtime owns game objects and verified bindings. Marshal
  main-thread-required Unity/IL2CPP work through the existing main-thread bridge.
  The root companion owns IPC forwarding, peer authorization and diagnostics.

### Kotlin (`app/`, `core/`, `bridge/`, `game-adapter/`)

- Target required by the user: Kotlin owns overlay/UI and Android fake-location
  control. It accepts user input, forwards gameplay intent through a thin
  client and renders backend-provided state. It must not own Pokemon GO runtime
  orchestration, gameplay decisions or raw game-state interpretation.
- Explicit location exception: Kotlin may own joystick, teleport, walk-to-location,
  coordinate validation, speed/bearing/step calculation, local arrival/stop state,
  location input arbitration, Android mock-provider lifecycle and location UI
  cooldown estimates. `JoystickLocationController`, `WalkPlanner`/`GeoMath` and
  `RootMockLocationProvider` may remain in Kotlin; keep this logic in the
  location/domain layer and let overlay views delegate to it.
- A user may choose a coordinate or favorite in Kotlin. Native owns target
  selection based on live game state (such as the next fort), gameplay pauses
  and game action eligibility. Kotlin executes native walk/stop intents with
  session/freshness/lease checks; local geometric arrival does not authorize a
  catch/spin action or prove game-side arrival. Do not add PoGo hooks, object
  reads or game-state-based target selection to the location controller.
- The live controller path is UI-only: `RuntimeUiAutomationFacade` persists and
  submits a full desired snapshot, while `RuntimeUiEventRouter` renders native
  status/events and forwards location handoffs. `HeadlessAutomationEngine`,
  `RuntimeLifecycleCoordinator`, and the old raw-observation path are no longer
  production entry points; compatibility implementations remain only where
  existing tests still consume them. See the evidence and boundary in [the
  Kotlin UI-only review](docs/issues/2026-09-22/kotlin-ui-only-boundary/brainstorm.md)
  and [the refactor plan](docs/issues/2026-09-22/kotlin-ui-only-boundary/checklists/00-overview.md).
- Keep Android UI/service lifecycle, input validation, presentation and thin
  IPC separate from game logic. Settings/UI persistence remains app-owned under
  the persistence rules below; it does not authorize runtime decision logic.
- The Android mock-location provider is an OS adapter, not a PoGo binding.
  Preserve provider cleanup and freshness for native-issued navigation. Manual
  joystick/teleport/walk does not require a new PoGo binding. Do not move a
  gameplay state machine to Java and call that UI-only.
- Keep `core/` Android-free and independent of storage, sockets, JNI, hooks,
  offsets and game classes. Existing generic planners/runners do not establish
  ownership of the current live gameplay loop.
- Keep transport contracts/codecs in `bridge/protocol/`, adapter contracts in
  `game-adapter/api/`, POGO payload interpretation in `game-adapter/pogo/`, and
  deterministic test behavior in `game-adapter/fake/`. The current POGO adapter
  dependency is not a target dependency of the UI-only layer.

### Persistence

- Kotlin repositories/stores own durable user data in app-private storage.
  Use the existing SharedPreferences owners described in `docs/ARCHITECTURE.md`;
  do not read/write their XML files directly from native or root shell.
- Android config is the durable source of truth. Native receives versioned,
  revisioned config snapshots over IPC and keeps an in-memory session mirror.
  Reapply config after a new ready session; never restore live game pointers,
  pending mutations, or readiness from persisted user settings.
- Reserve `/data/adb/pogo_root_automation/` for root bridge metadata such as
  `runtime.status` and `controller.uids`. Status files are diagnostics, not
  gameplay state or a config/message queue.
- Keep observations and runtime state session-scoped. The existing `map_target`
  preference is a short-lived handoff with a 30-second default TTL, not a
  durable route. New persisted data must specify its owner and retention rules.

### Native ↔ Kotlin communication

- Use the existing path: `RuntimeBridgeClient` → abstract Unix socket
  `pogo_root_automation_runtime` → root companion broker → injected runtime.
  Do not introduce direct cross-process JNI, shared preference polling, or
  filesystem command queues as a second gameplay transport.
- Change Kotlin codecs and native wire definitions together. Preserve protocol
  versions, size limits, session/identity checks, sequence/freshness validation,
  request correlation, capability checks and fail-closed behavior.
- A connection, START acknowledgement, or CONFIG_SET acknowledgement does not
  prove a gameplay action completed. Native owns readiness/module activation
  and reports actual action outcomes; Kotlin consumes structured results/events.
- `RuntimeMainThreadBridge.java` is an in-process JNI scheduling helper loaded
  inside the game process, not the IPC link to the controller APK.

## Test emulator

The active manual and harness test target is the BlueStacks instance named
`BlueStacks Air 1`. Do not assume the other `BlueStacks Air` instance or the
Android Studio AVD is the test target. Use the repository scripts to select and
validate the configured emulator target; do not inspect or select it with raw
ADB commands.

## Device and emulator command policy

Use the existing `scripts/*.sh` entry point for each corresponding device or
emulator operation, including reset/reconnect, build, upload, installation,
diagnostics, smoke tests and log collection. Do not run equivalent ad hoc ADB
commands directly.

Do not run `adb kill-server`, `adb start-server`, `adb devices`, or
`adb devices -l`. For complete logcat capture, use
`./scripts/logcat-full.sh` (and its documented options) rather than invoking
logcat directly. If an operation succeeds, do not read log files; inspect the
relevant log file only when the operation fails.

## Reverse-engineered APK workflow

For version-specific Pokémon GO class, method, field, or RVA questions, read
the local reverse output in `reverse/pogo-0.427.0/classes/` first. Use the
full compressed dump files in that directory only when the curated class
extract is insufficient. Do not begin by enumerating the live process to
discover names that are already present in the reverse output.

Use the live process only after the reverse pass, to verify instance state,
dependency owners, object lifetimes, ABI/layout assumptions, lifecycle
postconditions, and device-specific behavior. Keep the process survey
read-only unless an exact reverse-derived binding has passed its guards.

Regenerate the local reverse artifacts with:

```bash
./scripts/reverse-pogo-apk.sh
```

The inputs are the exact APKs under `pogo-apkm/`; they are read-only and must
not be modified or committed. Reverse output and the local Il2CppDumper tool
are generated, ignored artifacts. The current output is pinned to Pokémon GO
`0.427.0`, version code `2026082702`, arm64-v8a. Keep readable generated text
files at or below the 500-line project limit; retain any complete oversized
dump only in compressed form.

## Gradle usage

The repository includes the Gradle Wrapper. Use it instead of requiring a
global Gradle installation:

```bash
./gradlew test
./gradlew assembleDebug
```

For a full verification run:

```bash
./gradlew test assembleDebug
```

The wrapper is pinned to Gradle `9.7.1` in
`gradle/wrapper/gradle-wrapper.properties`. On a new machine, the first run
downloads that distribution into the user's Gradle cache.

Useful focused tasks:

```bash
./gradlew :core:test
./gradlew :bridge:protocol:test
./gradlew :game-adapter:api:test :game-adapter:fake:test :game-adapter:pogo:test
```

Use `--rerun-tasks` when verifying source changes that may otherwise be hidden
by an up-to-date build cache.

## Native protocol checks

The Zygisk C++ code is not built by the Android Gradle tasks. The host-side
protocol tests can be compiled directly:

```bash
c++ -std=c++17 -Wall -Wextra -Werror -Izygisk/jni \
  zygisk/jni/runtime_command_protocol_test.cpp \
  -o /tmp/runtime_command_protocol_test
/tmp/runtime_command_protocol_test

c++ -std=c++17 -Wall -Wextra -Werror -Izygisk/jni \
  zygisk/jni/runtime_observation_protocol_test.cpp \
  -o /tmp/runtime_observation_protocol_test
/tmp/runtime_observation_protocol_test
```

Building the injected Zygisk library requires the Android NDK, CMake/Ninja and
the canonical `zygisk.hpp`; see `zygisk/README.md` and the CI workflow for the
ABI-specific configuration.

## Build the Magisk ZIP

The APK build and the Zygisk runtime build are separate. Use the existing
workflow scripts below for native builds, packaging, emulator uploads,
module installation and logcat collection. These scripts replace ad hoc
commands for those operations.

```bash
./scripts/build-magisk.sh
./scripts/push-emulator.sh
./scripts/install-magisk-module.sh
./scripts/logcat-full.sh
```

User preference: do not create replacement scripts, temporary scripts,
inline shell/Python workflows, or manually reproduce these steps unless the
user explicitly requests it. Use the existing scripts' options and environment
variables. If a script is insufficient or fails, diagnose the problem and
report the needed change; do not silently bypass it or change its behavior
without the user's request.

The build script defaults to NDK `28.2.13676358` and CMake `3.22.1`. On the
current Mac, use the installed NDK through its supported override:

```bash
ANDROID_NDK="$HOME/Library/Android/sdk/ndk/27.1.12297006" ./scripts/build-magisk.sh
```

The install script uses Magisk already installed on the emulator. Use
`--reboot` when a reboot is requested or authorized, and `--apk <path>` to
also install the controller APK. Logcat supports `--follow` for continuous
capture; its default output directory is `build/logs/`.

The build script calls `scripts/package-magisk.sh` internally and validates
that the ZIP contains both `zygisk/arm64-v8a.so` and `zygisk/x86_64.so`.
The resulting artifacts are:

- `app/build/outputs/apk/debug/app-debug.apk`
- `build/pogo-root-automation-magisk-multiabi.zip`

The full reference is `.github/workflows/ci.yml`; it also runs shell syntax
checks and uploads both artifacts.

## Direct map-tap walk

The map-walk path is split into two parts and is permitted by the explicit
Kotlin fake-location exception:

1. A verified, version-scoped Pokémon GO Unity/IL2CPP binding observes a real
   map tap, resolves it to a `GeoPoint` using same-frame camera/map state and
   emits `ObservationType.MAP_TARGET`.
2. The app validates the observation, stores it briefly in
   `MapTargetRepository`, and the built-in mock-location controller walks to
   the target through `WalkPlanner`.

The native bridge provides the observation envelope and forwarder in
`zygisk/jni/runtime_observation_protocol.h` and `zygisk/jni/main.cpp`. It does
not guess class names, method addresses, camera projection or input hooks.
`READ_MAP_TARGET` must stay disabled until an exact package/version/ABI/build
binding has passed calibration and device verification.

Do not add screenshot-based coordinate inference or `input tap`/`input swipe`
fallbacks. If the runtime binding or camera state is unavailable, fail closed.

## Change and verification rules

- Do not write tests: do not create test files, add test cases, or modify test
  code as part of feature implementation or bug fixes. Running existing tests
  is still allowed and required by the verification rules below.
- Keep every non-Markdown source file at or below 500 lines, regardless of
  language. Split cohesive responsibilities into smaller files and use the
  language's import/include mechanism when sharing code.
- Use the existing `scripts/*.sh` entry point for device/emulator operations;
  do not recover manually with raw ADB commands.
- Preserve capability checks, runtime identity checks, freshness and fail-closed
  behavior.
- Keep game-build-specific code behind the adapter/native binding boundary.
- Do not run destructive Git commands or overwrite unrelated user changes.
- Before handing off a code change, run the focused tests relevant to the change
  and `./gradlew test assembleDebug` when the environment permits it.
- `git diff --check` should be clean.
