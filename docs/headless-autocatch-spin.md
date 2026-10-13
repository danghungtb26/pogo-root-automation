# Native catch/spin control

The foreground controller is UI-only. MainActivity and the floating overlay
persist policy and submit a full desired snapshot; native owns runtime
readiness, target selection, action execution, pending outcomes and cooldowns.
There is no loopback HTTP API, host arm command, visual-state fallback or
device-input gameplay path.

## Manual arm

Auto Catch starts OFF on every controller process and Pokémon GO runtime
session. A user toggle may arm only the exact current runtime session. Runtime
disconnect, PID/session change, STOP/error or service shutdown disarms it and
prevents reconnect replay. Durable filter/feature policy remains stored, but it
does not grant mutation authority.

## Capability boundary

The 0.433.0 build profile pins package, versionCode, ABI, `libil2cpp.so` build
ID, metadata hash and base APK hash. A policy is executable only when native
independently verifies the full identity, advertises the feature capability,
has fresh state, and owns no conflicting pending mutation.

Current live status is `UNVERIFIED_DEVICE`. REA provider gaps leave direct
catch, Virtual GO Plus fallback, transfer/discard storage contracts and the
full PGS-style map-source registry unresolved, so those mutation capabilities
remain unavailable. See
[`LIVE_AUTOMATION_READINESS.md`](LIVE_AUTOMATION_READINESS.md) and the 0.433.0
evidence ledger for exact blockers.

## User verification

Install the self-contained APK through the BlueStacks Air 1 UI, complete the
first-open root/module flow, confirm reboot, open Pokémon GO, then arm from the
app/overlay. If the manual flow fails, collect target-guarded evidence with
`scripts/collect-binding-diagnostics.sh` and `scripts/logcat-full.sh`.
