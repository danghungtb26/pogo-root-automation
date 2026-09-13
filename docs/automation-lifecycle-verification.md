# Automation lifecycle and action feedback

The headless service restores the persisted `enabled` and `catchSpinArmed`
settings when the controller process is recreated. It connects to the injected
game process independently of foreground/overlay permission. START remains
probe-only: native schedules managed discovery and retries; config is sent only
after managed identity verification, and native
modules wait for matching config revisions and a verified live map.

Closing or killing the game destroys its native workers. The controller resets
navigation on disconnect and retries the bridge while enabled. A new runtime
session gets all three config snapshots again. Explicitly disabling the service
or catch-spin arm persists that choice; opening the overlay does not re-enable it.
The observer is joined before native module state and Promise handles are reset.

Catch-spin starts after transfer/discard initialization. Auto-discard does not
start while catch, encounter, spin, or transfer is in progress. Catch breakout
and missed results can retry; uncertain mutation outcomes retain the existing
fail-closed gates. Catch settling now uses the configured delay.

Native auto-walk pauses for Pokémon and unresolved actions, and resumes only
after a complete empty scan. A Pokémon disappearing
without a catch event does not latch the walk indefinitely. The target fort is
revalidated on each scan. Missing map/fort data, a disarmed cluster, and a lost
session stop walking. Kotlin receives native walk/stop/arrival instructions and
only executes destinations; a five-second lease prevents stale walks. Walking
still requires the existing mock-location provider
and joystick service to be ready.

Spin start/result and catch failure events extend the existing telemetry wire
enum with values 10–14. Transfer/discard retain their start/result events.
Auto-walk emits start/stop/arrival feedback. The `showActionToasts` preference and
overlay permission still apply to the app's custom toast surface.

On successful FortSearch, the game bubble service runs on Unity's main thread
independently of the spinner's result callback. `AwardBubbleConfig.Scope` uses
the service's own `dwwf` RecyclableScope; bubbles are additive. The reference is
the local 0.427.0 dump: `AwardItemBubbleService` TypeDefIndex 10886 and
`AwardBubbleConfig` TypeDefIndex 10888. A missing binding/scope fails closed and
emits a separate bubble error, rather than claiming the spin failed or repeating
the server request. Visual behavior still requires device verification.

## Local verification

```sh
./gradlew test assembleDebug --rerun-tasks
ANDROID_NDK="$HOME/Library/Android/sdk/ndk/27.1.12297006" ./scripts/build-magisk.sh
c++ -std=c++17 -Wall -Wextra -Werror -Izygisk/jni/shared/bridge_kotlin \
  zygisk/tests/runtime_automation_event_protocol_test.cpp \
  -o /tmp/runtime_automation_event_protocol_test
/tmp/runtime_automation_event_protocol_test
c++ -std=c++17 -Wall -Wextra -Werror -Izygisk/jni \
  zygisk/tests/runtime_auto_fort_navigation_test.cpp \
  -o /tmp/runtime_auto_fort_navigation_test
/tmp/runtime_auto_fort_navigation_test
c++ -std=c++17 -Wall -Wextra -Werror -Izygisk/jni \
  zygisk/tests/runtime_readiness_retry_test.cpp \
  -o /tmp/runtime_readiness_retry_test
/tmp/runtime_readiness_retry_test
git diff --check
```

Regression coverage includes fresh-session config gating, game death/reconnect,
same-process stop/resume, config rejection, disarmed restoration, telemetry wire
decoding, native readiness retries, despawns, action pauses, invalidated forts,
navigation reset and stale instruction rejection. Native navigation tests replace
the old Kotlin planner tests. CI runs these host C++ checks in addition to Gradle.

The current ownership contract is documented in
[native runtime ownership](architecture/independent-runtime-control.md).

## Device verification still required

The upload attempt on 2026-09-13 failed because BlueStacks Air 1 refused the
connection at `127.0.0.1:5565`. No updated module/APK was installed by that attempt.
Once that instance is available, use the existing push/install scripts and
restart it to load the new Zygisk library. The existing device smoke test covers
bridge connect → kill → reconnect, but does not prove gameplay results.

On the pinned game build, verify saved config restores without opening settings;
catch resumes after relaunch; transfer obeys keep filters; discard obeys limits;
walking stops when game dies; and each successful spin displays the game's reward
bubbles. Check action start/result toasts including failed requests. Verify a
manually disabled master/arm stays disabled across controller and game restarts.
