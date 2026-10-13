# Repository baseline

Captured: 2026-10-13, before implementation source changes.

## Worktree and revision

- HEAD: `47800bada0dc8cd97910b4d7a1bf7f034a2bd719`.
- `git status --short`: empty.
- `git diff --stat`: empty.
- No user-owned dirty files existed at baseline.

## Current ownership and call path

The production Kotlin path is `MainActivity`/overlay intents →
`HeadlessAutomationService` → `RuntimeUiAutomationFacade` → `RuntimeUiClient` →
`RuntimeBridgeClient` → abstract socket `pogo_root_automation_runtime`. The root
companion brokers frames to the injected runtime; native desired-state reconcile
activates modules and emits status/events back through the same broker. Kotlin
also owns the mock-location controller and consumes native walk candidates.

At baseline, `HeadlessAutomationService` also starts `AutomationControlServer`
on loopback port 8765. `MainActivity` and `scripts/headless-control.sh` expose that
legacy surface. This is explicitly scheduled for removal in Phase 8.

## Packaging and native guard

- `scripts/build-magisk.sh` builds arm64-v8a and x86_64, then delegates once to
  `scripts/package-magisk.sh`.
- `scripts/package-magisk.sh` copies `zygisk/module/`, injects both ABI libraries,
  creates `build/pogo-root-automation-magisk-multiabi.zip`, and checks both ZIP
  entries. It does not yet emit a machine-readable manifest.
- `app/build.gradle.kts` does not yet depend on the native build and does not
  bundle the module ZIP.
- Native `discover_runtime_owners` calls `verify_expected_il2cpp_build_id`; the
  UI capability builder returns no capabilities when `exact_build_verified` is
  false. Package/version/metadata hash are not yet a unified build profile.
- Mutation feature gates are additionally controlled by compile-time flags and
  resolved binding predicates. A socket connection/receipt is not an action
  outcome.
- No version/update network path is in the requested implementation. The only
  baseline network-related build behavior is fetching the pinned Zygisk header
  when it is absent from `build/zygisk-api-4`; there is no runtime updater.

## Persistence/lifecycle gaps observed

- `AutomationConfigRepository` persists `catch_spin_armed` and reads it with
  default `true`; manual arm is therefore not session-scoped at baseline.
- Service start/boot is not gated by a durable module-install state.
- No foreground-only bundled module installer exists.

## File-size baseline

No text source file under `app/`, `core/`, `bridge/`, `game-adapter/`, `zygisk/`
or `scripts/` exceeds 500 lines. Files closest to the limit were:

| Lines | File |
|---:|---|
| 485 | `app/.../root/RuntimeBridgeClient.kt` |
| 482 | `game-adapter/pogo/.../BridgePogoRuntime.kt` |
| 476 | `zygisk/jni/shared/bridge_kotlin/runtime_observation_protocol.h` |
| 474 | `zygisk/jni/modules/catch_spin/coordinator.inc` |
| 464 | `app/.../overlay/JoystickOverlayService.kt` |
| 463 | `zygisk/jni/shared/runtime/map/runtime_map_reader.inc` |
| 458 | `zygisk/jni/modules/catch_spin/spin_promise_observer.inc` |
| 451 | `zygisk/jni/modules/discard/item_inventory_lookup.inc` |

Binary jars/AARs and reverse artifacts were excluded from source line limits.

## Baseline verification

| Command | Result |
|---|---|
| Six existing host C++ checks from `.github/workflows/ci.yml` | PASS, exit 0 |
| `ANDROID_NDK=/Users/admin/Library/Android/sdk/ndk/28.2.13676358 ./scripts/build-magisk.sh` | PASS, exit 0; ZIP `build/pogo-root-automation-magisk-multiabi.zip` |
| `./gradlew test assembleDebug` | PASS, exit 0; 51 tasks up-to-date |
| `shasum -a 256 build/pogo-root-automation-magisk-multiabi.zip` | `942667336fbac5be2d7caef25ad0b3c4da4676d36ada6e4d56e473d207973bbd` |
| `shasum -a 256 app/build/outputs/apk/debug/app-debug.apk` | `3b3e6a03f949a2ce5dd73fecdfd91675514aa8785cef8adf584978cd351d94c4` |
| `git status --short` after baseline build | empty |

The plan's documented NDK 27 override is not installed on this machine. The
script default NDK 28.2.13676358 is installed and was used successfully.

