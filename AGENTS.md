# AGENTS.md

## Project overview

`pogo-root-automation` is a rooted Android automation framework for
Pokémon GO. Keep the stable automation/domain layer independent from
version-specific Pokémon GO and Unity/IL2CPP bindings.

The repository contains:

- `core/`: pure domain models, geo math, movement planning and automation rules.
- `bridge/protocol/`: versioned bridge frames, runtime events and payload codecs.
- `game-adapter/api/`: capability contracts for game adapters.
- `game-adapter/pogo/`: Pokémon GO runtime source, protobuf decoding and bridge adapter.
- `game-adapter/fake/`: deterministic fake adapter used by tests.
- `app/`: Android controller, headless service, overlay and mock-location provider.
- `zygisk/`: native target-process probe and companion bridge.
- `scripts/`: device diagnostics, smoke tests and packaging helpers.
- `docs/`: architecture, binding boundaries and feature design notes.

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

The APK build and the Zygisk runtime build are separate. To reproduce the
multi-ABI artifact produced by CI, install/configure Android SDK components
for platform 36, build-tools 36.0.0, NDK `28.2.13676358` and CMake 3.22.1,
then fetch the canonical `zygisk.hpp` into a temporary include directory.

Build both native ABIs with the Android toolchain:

```bash
ANDROID_CMAKE="$ANDROID_HOME/cmake/3.22.1/bin/cmake"
ANDROID_NDK="$ANDROID_HOME/ndk/28.2.13676358"
ZYGISK_API_DIR="$RUNNER_TEMP/zygisk-api"

"$ANDROID_CMAKE" -S zygisk/jni -B build/zygisk-arm64 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-28 \
  -DZYGISK_API_DIR="$ZYGISK_API_DIR"
"$ANDROID_CMAKE" --build build/zygisk-arm64

"$ANDROID_CMAKE" -S zygisk/jni -B build/zygisk-x86_64 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=x86_64 -DANDROID_PLATFORM=android-28 \
  -DZYGISK_API_DIR="$ZYGISK_API_DIR"
"$ANDROID_CMAKE" --build build/zygisk-x86_64
```

Package the two `.so` files into the installable Magisk module:

```bash
./scripts/package-magisk.sh \
  build/zygisk-arm64/libpogo_root_automation.so \
  build/zygisk-x86_64/libpogo_root_automation.so \
  build/pogo-root-automation-magisk-multiabi.zip
```

The script validates that the ZIP contains both
`zygisk/arm64-v8a.so` and `zygisk/x86_64.so`. The resulting artifacts are:

- `app/build/outputs/apk/debug/app-debug.apk`
- `build/pogo-root-automation-magisk-multiabi.zip`

The full reference is `.github/workflows/ci.yml`; it also runs shell syntax
checks and uploads both artifacts.

## Direct map-tap walk

The map-walk path is intentionally split into two parts:

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

- Preserve capability checks, runtime identity checks, freshness and fail-closed
  behavior.
- Keep game-build-specific code behind the adapter/native binding boundary.
- Do not run destructive Git commands or overwrite unrelated user changes.
- Before handing off a code change, run the focused tests relevant to the change
  and `./gradlew test assembleDebug` when the environment permits it.
- `git diff --check` should be clean.
