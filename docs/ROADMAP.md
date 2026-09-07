# Implementation roadmap

## M0 — Foundation

- [x] Android controller project
- [x] Core domain models
- [x] `GameAdapter` capability contract
- [x] Fake read-only adapter
- [x] Countdown service + unit tests
- [x] CI build/test workflow

## M1 — Root runtime

- [x] Zygisk target-process lifecycle stub
- [x] Companion-backed runtime attach event
- [x] Root status query with PID/process validation
- [x] Controller runtime polling + status parsing
- [x] Installed Pokémon GO package/version detection
- [x] Build/package arm64 Zygisk module in CI
- [ ] Rooted-device smoke test: install → launch → kill → relaunch → uninstall

**Acceptance:** controller can show `runtime connected`, process name, package version, and lifecycle without game-specific memory bindings.

## M2 — Nearby read-only

- [ ] Define raw map-observation boundary in version adapter
- [ ] Map raw spawn state to `NearbySpawn`
- [ ] Stable spawn identity / de-duplication
- [ ] Exact/estimated/unknown expiry mapping
- [ ] Snapshot cache and add/update/remove diff
- [ ] Real nearby UI

**Acceptance:** changing location with GPS Joystick causes Pokémon GO to load the new map and the controller displays the same nearby spawns as structured data with honest countdown confidence.

## M3 — Catch

- [ ] Manual encounter action
- [ ] Manual catch action
- [ ] Normalize catch outcomes
- [ ] Catch filter
- [ ] Serialized catch queue
- [ ] Auto-catch scheduler

## M4 — Spin

- [ ] Fort reader
- [ ] Spin eligibility state
- [ ] Manual spin action
- [ ] Stop cooldown tracking
- [ ] Auto-spin scheduler

## M5 — Inventory

- [ ] Inventory reader
- [ ] Keep/min/max item rules
- [ ] Discard planner
- [ ] Dry-run output
- [ ] Discard executor

## M6 — Transfer

- [ ] Pokémon storage reader
- [ ] Protected flags: shiny/favorite/legendary/mythical/shadow/costume
- [ ] IV/species keep rules
- [ ] Transfer dry-run
- [ ] Transfer executor

## M7 — Hardening

- [ ] Version adapter registry
- [ ] Unsupported-version fail-safe
- [ ] Game restart / hook-loss recovery
- [ ] Structured diagnostics export
- [ ] Long-run state-machine tests
- [ ] Release packaging

## Explicitly out of scope

- GPS spoof implementation (GPS Joystick owns location)
- Root hiding / Play Integrity bypass
- Anti-cheat or detection bypass
- Direct server-protocol bot
