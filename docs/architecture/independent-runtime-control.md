# Independent runtime control and feature modules

## Goal

The controller UI is optional. Runtime attachment, orchestration, and gameplay feature groups have separate lifecycles so closing the UI does not tear down automation and toggling one feature does not implicitly activate the others.

The native side uses one Zygisk host inside the target process and independently controlled C++ feature modules. This avoids repeating Zygisk/process/bridge setup while preserving a clean module boundary that can later move behind separate shared libraries if required.

The core design rule for gameplay automation is:

> **Native decides whether an action can be performed safely; the Kotlin service decides whether the action should be performed.**

That rule keeps game-runtime mechanics in native code and keeps configurable automation policy in the service/core layer.

## Ownership

```text
Controller UI
  - settings/status/manual controls
        |
        v
HeadlessAutomationService
  RuntimeLifecycleCoordinator
  - desired host state
  - desired feature-module set
  - reconnect/recovery

  StructuredAutomationController / policy layer
  - consume observations
  - filter candidates
  - score/prioritize targets
  - decide catch/spin/discard/transfer intent
  - cooldown/retry/rate-limit policy
        |
        v
RuntimeBridgeClient
  - authenticated broker socket
  - runtime session identity
  - host START / STOP / DIAGNOSTIC
  - module ENABLE / DISABLE
  - gameplay command transport
        |
        v
Root companion / broker
  - stable process-to-controller relay
  - runtime session registry
        |
        v
Zygisk runtime host (inside target process)
  - attach during app specialization
  - wait for libil2cpp/libunity exports
  - remain ATTACHED_IDLE
  - prepare verified bindings after host START
  - host independent C++ feature modules
  - observe verified game-runtime state
  - validate and execute commands on the correct runtime/main thread
```

High-level gameplay policy remains in the controller/core side. Native modules only observe verified runtime state and execute guarded actions assigned to their feature group.

## Policy versus runtime mechanics

The service and native host intentionally do not share the same responsibility.

| Concern | Kotlin service/core | Native C++ module |
| --- | --- | --- |
| Read nearby Pokémon from game runtime | no | yes |
| Read nearby PokéStops/Forts from game runtime | no | yes |
| Normalize observations and publish them | no | yes |
| Filter by species/config/rules | yes | no |
| Score/prioritize Pokémon | yes | no |
| Decide whether to catch or spin | yes | no |
| Cooldown/rate-limit/retry policy | yes | no |
| Choose the target spawn/fort | yes | no |
| Resolve live IL2CPP object/binding | no | yes |
| Verify current runtime/lifecycle state | no | yes |
| Marshal work onto the required game/main thread | no | yes |
| Execute OpenEncounter/Catch/Spin | no | yes |
| Reject stale/unavailable commands | no | yes |

Native code must not contain configurable policy such as species whitelists, IV preference, target scoring, or "catch this type before that type" rules. Those rules change frequently and belong in Kotlin where they can be adjusted without rebuilding the injected native host.

Conversely, the service must not assume that a requested action is currently executable. Native code remains the final authority for live object existence, verified binding availability, lifecycle compatibility, and safe execution.

## Native source ownership

Feature implementation now lives under the module that owns it rather than being duplicated behind module facades:

```text
zygisk/jni/
  main.cpp
  runtime_feature_modules.inc          # registry + lifecycle router only
  runtime_control.inc                  # host command/control plane

  host/
    runtime_capabilities.inc           # shared host capability publication

  modules/
    catch_spin/
      module.inc
      catch.inc
      spin.inc
      open_encounter.inc

    discard/
      module.inc                       # executor not verified yet

    transfer/
      module.inc                       # executor not verified yet

    encounter/
      module.inc
      berry.inc
      throw_hooks.inc
      runtime_throw_trampoline.inc
```

The following legacy files were retired after their implementation was moved to the owning module:

```text
runtime_actions.inc
runtime_action_catch.inc
runtime_action_spin.inc
runtime_action_open_encounter.inc
runtime_throw_hooks.inc
runtime_throw_trampoline.inc
modules/runtime_module_catch_spin.inc
modules/runtime_module_discard.inc
modules/runtime_module_transfer.inc
modules/runtime_module_encounter.inc
```

Shared runtime infrastructure remains at the host level when it is not feature policy, for example bridge/protocol transport, IL2CPP inspection, main-thread dispatch, observation infrastructure, and common action/result helpers.

The registry only resolves module IDs, tracks enabled state, starts/stops shared observation, and gates gameplay routing. Feature-specific availability/init/cleanup belongs to the module folder.

```text
Runtime host
  |
  +-- CATCH_SPIN
  |     - nearby Pokémon observation
  |     - fort/PokéStop observation
  |     - encounter/open flow
  |     - catch execution
  |     - direct map catch
  |     - spin
  |     - encounter snapshot support
  |
  +-- DISCARD
  |     - item inventory observation/execution boundary
  |     - currently UNAVAILABLE until a verified native discard binding exists
  |
  +-- TRANSFER
  |     - Pokémon storage observation/execution boundary
  |     - currently UNAVAILABLE until a verified native transfer binding exists
  |
  +-- ENCOUNTER
        - berry action
        - throw-profile requests such as Excellent/curve
        - throw diagnostic hooks
        - no force-hit or guaranteed server-side capture-result flag
```

`CATCH_SPIN`, `DISCARD`, `TRANSFER`, and `ENCOUNTER` have independent enabled state. An unavailable module fails closed without preventing another verified module from running.

Current implementation status:

| Module | Independent service control | Native boundary | Verified executor in current build |
| --- | --- | --- | --- |
| `CATCH_SPIN` | yes | yes | existing catch/spin/open bindings where capability verification succeeds |
| `DISCARD` | yes | yes | no — reports `UNAVAILABLE` |
| `TRANSFER` | yes | yes | no — reports `UNAVAILABLE` |
| `ENCOUNTER` | yes | yes | existing berry/throw-profile/hook bindings where capability verification succeeds |

The architecture is therefore independent already, while discard/transfer still require their own verified native implementation before they can perform mutations.

## Module registration, availability, and enabled state

A feature module has several distinct lifecycle concepts:

```text
UNREGISTERED
    |
    | native host bootstrap
    v
REGISTERED / LOADED
    |
    | runtime binding check
    +------> UNAVAILABLE
    |
    | service desires the module
    v
DISABLED -> ENABLING -> ENABLED
               |
               +------> ERROR

ENABLED -> DISABLING -> DISABLED
```

`REGISTERED` or `LOADED` only means the C++ module exists in `RuntimeModuleRegistry`. It does **not** mean the feature is active.

At native bootstrap, the host explicitly registers:

```text
CATCH_SPIN
DISCARD
TRANSFER
ENCOUNTER
```

Registration status is sent back through the bridge and the service may show a bootstrap toast such as:

```text
CATCH_SPIN module loaded
```

or:

```text
CATCH_SPIN module load failed: runtime_module_load_failed
```

The service then independently decides whether the module should be enabled from persisted config. Native code decides whether the module can actually become enabled for the current runtime/build.

For example:

```text
DISCARD module loaded
    |
    | service requests ENABLE
    v
native available(binding) == false
    |
    v
DISCARD = UNAVAILABLE
```

Therefore `loaded`, `available`, and `enabled` must never be treated as synonyms.

## Settings to module mapping

The foreground service derives desired native modules from persisted settings:

```text
autoCatch || autoSpin || autoEncounter || autoSnapshot
    -> CATCH_SPIN

autoDiscard
    -> DISCARD

autoTransfer
    -> TRANSFER

berry != NONE || throwQuality != ANY || curve != ANY
    -> ENCOUNTER
```

A configured Excellent throw is therefore a `ENCOUNTER` feature. It is a client-side requested throw profile, not a guarantee that a remote/server capture result will succeed.

## CATCH_SPIN data and decision flow

`CATCH_SPIN` is intentionally split into two directions:

1. Native-to-service observations.
2. Service-to-native action commands.

The module must not autonomously select Pokémon or PokéStops inside C++.

```text
Pokémon GO runtime
    |
    | read verified runtime state
    v
CATCH_SPIN native module
    |
    +--> NearbyObservation
    |
    +--> FortsObservation
    |
    +--> EncounterObservation
    |
    v
Runtime bridge
    |
    v
Kotlin service / policy layer
    |
    +--> filter
    +--> score
    +--> prioritize
    +--> cooldown/rate-limit checks
    +--> choose target
    |
    v
AutomationAction
    |
    +--> OpenEncounter
    +--> Catch
    +--> Spin
    |
    v
CATCH_SPIN native module
    |
    +--> validate live target/runtime/binding
    +--> marshal to correct thread
    +--> execute or reject
```

This gives the service ownership of **intent** and native ownership of **mechanism**.

### Observation mechanism (mode B: dirty-flag)

The native observer produces `NearbyObservation`/`FortsObservation` on a background
thread (`runtime_observation_thread`, ~100 ms tick). It uses a **dirty-flag**
model rather than an unconditional per-tick full scan:

```text
map-change signal  --marks-->  ProbeContext.world_dirty = true
observer tick:
    if world_dirty (or reconcile due) and an observe source is verified:
        read map snapshot on the Unity main thread
        publish NearbyObservation / FortsObservation
        clear world_dirty
```

Design intent and rationale:

- **Mode C (unconditional poll) is the prior model and remains the fallback.**
  Until the map-query batch callback is inline-hooked, a periodic tick
  (`kFallbackPollTicks`) marks the world dirty, so observation runs at the old
  cadence with no regression.
- **Mode B (target)** installs an inline hook on the game's map-query batch
  callback (`OnMapQueryResponseReceived` / `ProcessCellsFromResponse`, in
  `Niantic.Platform.GameMapObject`). The hook does the minimum — call
  `mark_world_dirty()` and return — so a server map update triggers exactly one
  bounded read instead of continuous polling. This is preferred over raw
  per-entity (mode A) hooks, which fire in bursts (dozens of entity callbacks in
  one frame) and risk a visible hitch.
- **Reconcile** (`kReconcileTicks`, ~30 s) forces a periodic full re-read so a
  missed dirty signal cannot leave the cache permanently stale.
- **Player position is a separate concern.** "In range of a stationary fort"
  changes when the player moves, with no map-change signal, so player location
  (`ILocationProvider`) is observed/published independently of `world_dirty`.

### Per-module observers

The observer thread orchestrates; modules observe their own concern. Each tick it
builds an `ObserverTickContext {context, binding, tick}` and calls
`pogo_runtime_module::observe_enabled_modules`, which dispatches to every enabled
module's `observe(ObserverTickContext&)` hook on the observer's attached il2cpp
thread. Each module self-gates on its own binding and decides its own cadence via
`tick`. Root keeps shared/coupled concerns (world snapshot, encounter, lifecycle,
throw-event drain); genuinely private observations live in a module:
`DiscardModule::observe()` reads inventory. Build: the Zygisk API header is
vendored at `zygisk/jni/third_party/zygisk.hpp` and is the CMake default, so
`cmake -S zygisk/jni -B <dir> -DCMAKE_TOOLCHAIN_FILE=<ndk>/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a` builds without extra flags.

Seams already in place (`refactor/independent-runtime-control`):

| Seam | Location | State |
| --- | --- | --- |
| `ProbeContext.world_dirty` (atomic) | `runtime_native_prelude.inc` | done |
| `mark_world_dirty()` | `runtime_observation.inc` | done (called by fallback; call site for the hook is TODO) |
| `RuntimeBinding.map_query_hook_installed` | `runtime_native_prelude.inc` | done (always false until the hook lands) |
| dirty-driven observer loop + reconcile | `runtime_observation.inc` | done |
| `RuntimeBinding.map_query_on_response` + `map_query_hook_binding_verified` (resolved in probe) | `runtime_probe_discovery.inc` | done |
| map-query batch callback inline hook + prologue diagnostic | `modules/catch_spin/map_hooks.inc` (`install_runtime_map_query_hook`, `log_map_query_prologue`, hook → `mark_world_dirty`) | scaffolded, **gated off until the prologue constant is filled** |
| hook install/detach wiring | `CatchSpinModule::on_enable/on_disable` | done (best-effort install; enabling still succeeds via poll fallback) |
| player position wired to Kotlin | `ILocationProvider` binding + `read_runtime_player_position` + nearby payload v2 + `RuntimeNearbyPayloadCodec` | done (needs device verify of the value-struct invoke) |

Target (resolved from the full IL2CPP dump, build 0.427.0):

- Class `Niantic.Platform.GameMapObject.Map.S2CellManager`.
- Method `OnMapQueryResponse(int rpcId, byte[] response)` — RVA `0x9994F4C`
  (alternative: `ProcessCellsFromResponse(IEnumerable<MapS2Cell>)` RVA `0x9995900`).

Remaining work to complete mode B (steps 1 and 3 are done; only the device-only
prologue capture and on-device verification remain):

1. ~~Resolve the method + take its code pointer.~~ Done: `map_query_on_response`
   is resolved in `runtime_probe_discovery.inc` and the code pointer is taken via
   `throw_method_code_pointer` in `map_hooks.inc`.
2. **Obtain the 16-byte ARM64 prologue (device-only).** `libil2cpp.so` is not
   available offline, but it is mapped in the game process at runtime.
   `install_runtime_map_query_hook` already calls `log_map_query_prologue`, which
   logs the live 16 bytes at the resolved code pointer on every enable. Run once
   on device, read the logged `bytes={0x..,0x..,0x..,0x..}`, and replace the
   placeholder `on_map_query_response_prologue[4]` in `map_hooks.inc`. The
   exact-build gate still rejects drift.
3. ~~Add the hook callback and install/detach it from the module.~~ Done:
   `hooked_on_map_query_response` marks the world dirty and calls the original;
   `CatchSpinModule::on_enable` installs best-effort and sets
   `map_query_hook_installed` on success (which makes the observer skip the poll
   fallback).
4. Runtime-verify on device that the callback fires once per server map update
   and not per frame, then the poll fallback can be retired.

Until step 2 is done, `install_inline_hook`'s prologue `memcmp` fails (placeholder
is all-zero), the hook safely does not install, and observation keeps running on
the poll fallback — no regression.

Constraints: inline hooks are aarch64-only and pinned to the exact build; a
prologue/address mismatch must fall back to the poll, never crash.

## Pokémon target selection

Nearby Pokémon observations are used as candidate discovery data. The service may apply policy such as:

```text
species allow/block list
priority species
nearest target
spawn expiry
configured distance
recently attempted target
cooldown state
user automation settings
```

A future selector may be organized as:

```text
CatchSpinCoordinator
  |
  +-- PokemonFilter
  +-- PokemonScorer
  +-- FortSelector
  +-- CatchSpinPolicy
```

The exact class names are implementation details, but this ownership boundary should remain stable.

### Two-phase catch decision

Nearby data may not contain all information required for the final catch decision. The catch flow therefore supports two policy phases:

```text
NearbyObservation
    |
    | phase 1: choose candidate
    v
service filter + score
    |
    v
OpenEncounter(target)
    |
    v
native resolves encounter
    |
    v
EncounterObservation
    |
    | phase 2: final policy check
    v
service evaluates richer encounter data
    |
    +--> reject/skip candidate
    |
    +--> Catch(encounterId, throwProfile)
              |
              v
          native executor
```

Phase 1 should use only data known from nearby/map observations. Phase 2 can use richer encounter-derived information when available.

This avoids putting rules such as IV/shiny/species preference into C++ and avoids making a final decision from incomplete nearby data.

## PokéStop/Fort selection

Fort observations follow the same pattern:

```text
FortsObservation
    |
    v
service policy
    |
    +-- autoSpin enabled?
    +-- fort currently available?
    +-- distance acceptable?
    +-- cooldown/retry state acceptable?
    +-- higher-priority catch currently active?
    |
    v
select fort
    |
    v
Spin(fortId)
    |
    v
native validates live runtime state and executes/rejects
```

Native code reports what is currently observable and executable. Kotlin decides whether spinning the fort is desirable at that moment.

## Command validation rule

A command from the service is an **intent**, not permission to blindly mutate the game runtime.

For example:

```text
Service:
  "Open/catch spawn 123"

Native CATCH_SPIN:
  - is CATCH_SPIN enabled?
  - is the exact build/binding verified?
  - does the live target/encounter still exist?
  - is the lifecycle state compatible?
  - can execution be dispatched safely?

  yes -> execute
  no  -> reject with a structured failure
```

This protects against stale nearby snapshots, despawned targets, scene transitions, reconnect races, and runtime binding loss.

## Runtime host lifecycle

```text
DETACHED
   |
   | target app process starts
   v
Zygisk pre/postAppSpecialize
   |
   | lightweight mapping/export probe
   v
ATTACHED_IDLE
   |
   | module registry bootstrap
   | modules become REGISTERED/LOADED but remain disabled
   v
ATTACHED_IDLE
   |
   | START_RUNTIME from HeadlessAutomationService
   v
STARTING
   |
   | managed diagnostic / exact-build binding
   | publish capabilities
   v
RUNNING HOST
   |
   | module ENABLE/DISABLE requests derived from service config
   v
independent feature-module state
   |
   | STOP_RUNTIME
   v
STOPPING
   |
   | disable every feature module
   | stop observer
   | keep process attachment alive
   v
ATTACHED_IDLE
```

Host `RUNNING` means verified native bindings are prepared. It no longer means every automation feature is active.

If the target process exits, the broker connection dies and the service returns to `DETACHED`. When automation remains enabled, the service reconnects to the next runtime session, starts the host, and re-synchronizes desired modules.

## Host control protocol

Bridge v2 keeps the outer `COMMAND` frame for compatibility. Host control uses its own sub-protocol:

```text
payload version
runtime session id
request id
marker = 0x52544354 ("RTCT")
action = START | STOP | DIAGNOSTIC
expiry
pid
process name
package name
```

## Feature-module control protocol

Feature activation uses a separate marker:

```text
payload version
runtime session id
request id
marker = 0x52544D44 ("RTMD")
module = CATCH_SPIN | DISCARD | TRANSFER | ENCOUNTER
action = ENABLE | DISABLE
expiry
pid
process name
package name
```

The broker validates the common runtime-session prefix before forwarding either control sub-protocol.

## Gameplay routing

Native gameplay commands are first gated by host state, then by module ownership:

```text
OpenEncounter / Catch / Spin / Snapshot / DirectCatch
    -> CATCH_SPIN

Catch with non-default throw profile
    -> CATCH_SPIN + ENCOUNTER

UseBerry
    -> ENCOUNTER

DiscardItem
    -> DISCARD

TransferPokemon
    -> TRANSFER
```

If the host is idle, commands fail with `runtime_inactive`. If the required module is disabled, commands fail with `runtime_module_disabled`. If a module is enabled but the concrete native executor is not verified/implemented, the command continues to the fail-closed `binding_not_implemented` path.

## Host START behavior

`START` is idempotent and only prepares the runtime host:

1. Confirm that the runtime module registry initialized successfully.
2. Run the bounded managed-runtime diagnostic/binding step.
3. Verify the supported build and read bindings.
4. Publish strong runtime identity/capabilities.
5. Mark the runtime host ready.
6. Leave every feature module disabled until the service explicitly enables it.

The native bootstrap performed from `postAppSpecialize()` still avoids managed operations during the early Unity/IL2CPP initialization window.

## Module ENABLE/DISABLE behavior

Module control is idempotent.

- Enable checks that the host is ready and the module has the required verified bindings.
- A module starts shared observation only when it needs it.
- Disabling one module does not disable another module.
- The shared observer stops when no enabled module needs it.
- `ENCOUNTER` installs throw hooks at most once; later disable operations disarm the hook context instead of physically unhooking live code.
- Re-enabling `ENCOUNTER` re-arms the existing hooks and clears stale diagnostic events.

Keeping already-installed trampolines in place avoids unsafe live unhook/re-hook races while still making the feature module logically inactive.

## Host STOP behavior

`STOP` is also idempotent.

- Disable all native feature modules.
- Stop/join shared observation.
- Clear active encounter/module-owned transient state.
- Keep the command channel, companion FD, main-thread bridge, and Zygisk process attachment alive.

## Service behavior

`HeadlessAutomationEngine`, `RuntimeLifecycleCoordinator`, and the automation policy layer own desired state and gameplay intent:

```text
config.enabled = false
    -> host STOP
    -> all modules DISABLED
    -> worker remains alive

config.enabled = true
    -> host START if needed
    -> derive desired modules from config
    -> ENABLE/DISABLE modules independently
    -> receive observations
    -> filter/score/select targets in Kotlin
    -> emit guarded AutomationAction commands
```

Changing a persisted setting while the service stays alive changes only the affected native module on the next synchronization cycle.

Examples:

```text
autoCatch=true, autoSpin=true, berry=NONE
    CATCH_SPIN   = ENABLED
    ENCOUNTER = DISABLED

then berry=GOLDEN_RAZZ
    CATCH_SPIN   = stays ENABLED
    ENCOUNTER = ENABLED

then autoDiscard=true
    DISCARD = UNAVAILABLE on the current build
    CATCH_SPIN/ENCOUNTER keep their own state
```

## Recommended CATCH_SPIN service structure

The policy side should remain replaceable and testable without touching native bindings. A recommended direction is:

```text
app/.../automation/catchspin/
  CatchSpinCoordinator.kt
  PokemonTargetSelector.kt
  PokemonFilter.kt
  PokemonScorer.kt
  FortSelector.kt
  CatchSpinPolicy.kt
```

Native remains focused on runtime integration:

```text
zygisk/jni/modules/catch_spin/
  module.inc
  catch.inc
  spin.inc
  open_encounter.inc
  [observation-specific implementation as it is further separated]
```

The exact file layout may evolve, but policy must stay above the bridge and game-runtime mechanics must stay below it.

## Recovery rules

- Controller UI closes: no effect; foreground service continues.
- Service restarts while target process survives: reconnect, START host idempotently, then re-sync modules.
- Target process restarts: drop stale session, connect the new session, then re-sync modules.
- Lost ENABLE result: retry is safe because module enable is idempotent.
- Lost DISABLE result: service can send DISABLE again.
- Module unavailable: record `UNAVAILABLE`; do not fail unrelated modules.
- Stale gameplay target: native rejects it; service selects again from a newer observation.
- Runtime identity mismatch/expired request: fail closed.

## Debugging checkpoints

```text
TARGET_ATTACHED
PROBE_COMPLETE
MODULE_REGISTERED <module>
MODULE_REGISTRATION_FAILED <module>
ATTACHED_IDLE
HOST_START_REQUEST
MANAGED_BINDING_READY
HOST_READY
MODULE_ENABLE_REQUEST <module>
MODULE_ENABLED <module>
MODULE_UNAVAILABLE <module>
NEARBY_OBSERVATION
FORTS_OBSERVATION
TARGET_SELECTED <target>
OPEN_ENCOUNTER_REQUEST <target>
ENCOUNTER_OBSERVATION <encounter>
CATCH_REQUEST <encounter>
SPIN_REQUEST <fort>
MODULE_DISABLE_REQUEST <module>
MODULE_DISABLED <module>
HOST_STOP_REQUEST
ATTACHED_IDLE
```

`GET /v1/status` exposes `runtimeControlState` plus `runtimeModules`, so these module states can be checked without depending on logcat alone.

## Device validation checklist

Before merge, validate the independent module behavior on the target rooted runtime:

1. Launch the target app with automation disabled: modules register/load, host reaches `ATTACHED_IDLE`, and no module is active.
2. Confirm the service receives one registration load/fail status per module for the runtime session.
3. Enable only catch/spin: `CATCH_SPIN=ENABLED`, `ENCOUNTER=DISABLED`.
4. Confirm nearby Pokémon and fort observations reach Kotlin while `CATCH_SPIN` is enabled.
5. Confirm Kotlin selects a candidate and native does not autonomously choose a Pokémon.
6. Open an encounter from a selected nearby candidate and confirm the service can perform a second policy check from `EncounterObservation` before sending Catch.
7. Turn berry or non-default throw quality on: `ENCOUNTER` becomes enabled without restarting `CATCH_SPIN`.
8. Turn throw assist off while catch/spin stays on: hooks remain safe pass-through and no throw-assist diagnostics are emitted.
9. Enable discard: current build reports `DISCARD=UNAVAILABLE` while catch/spin continues.
10. Enable transfer: current build reports `TRANSFER=UNAVAILABLE` while other modules continue.
11. Disable master automation: all modules become disabled and the process remains attached-idle.
12. Re-enable master automation: host and desired available modules recover idempotently.
13. Kill/relaunch the target process: service binds a new runtime session and re-synchronizes module intent.
14. Verify a stale/despawned Pokémon command is rejected by native and policy can select a new target.
15. Stress host START during cold boot to validate the existing managed-runtime readiness boundary.

Diagnostics/flight-recorder work should store host transitions separately from per-module transitions, observation/policy decisions, and gameplay action logs.
