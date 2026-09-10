# Pokémon GO 0.427.0 observation reverse notes

## Purpose

This file is a durable notebook for runtime-observation reverse engineering on the Pokémon GO 0.427.0 dump stored under `reverse/pogo-0.427.0/DummyDll`.

The immediate goal is to find the cheapest stable runtime boundary for observing nearby Pokémon, PokéStops/Forts, and encounter lifecycle without continuously traversing the full Unity/IL2CPP object graph on the game main thread.

Architecture rule used by the automation runtime:

> Native decides whether an action can be performed safely; the Kotlin service decides whether the action should be performed.

Native observation should therefore publish game state/deltas, while filtering, scoring, target selection, cooldown policy, and catch/spin intent remain in Kotlin.

## Provenance

- Pokémon GO dump version: `0.427.0`
- Dump source: `reverse/pogo-0.427.0/DummyDll`
- Main commit that introduced/tracked the generated DummyDll set: `4cd30c65f548e2d7836ca9a142331e00b7325325`
- Assemblies inspected: `Niantic.Platform.GameMapObject.dll`, `holo-game.dll`, `holo-game-interfaces.dll`, `holo-game-shared-types.dll`
- Primary source of concrete runtime types is `holo-game.dll` (the layer the existing runtime reader already binds against), not the lower-level `GameMapObject` assembly.
- Blob inspected for `GameMapObject`: `87d620036c690ae4f529f772ee672f87bbb5a010`
- Full Il2CppDumper output is now available (gzip): `reverse/pogo-0.427.0/{dump.cs,script.json,il2cpp.h,stringliteral.json}.gz`. These give RVAs, exact field offsets, and full signatures. They are large (dump.cs ~86 MB, script.json ~225 MB, il2cpp.h ~213 MB uncompressed) — prefer git-lfs / `.gitignore` over committing raw.
- `libil2cpp.so` is **not** available offline (not in the reversed APK). This is fine: it is mapped in the game process at runtime, where the Zygisk module runs, so raw bytes (e.g. inline-hook prologues) can be captured from live memory. The dump's RVAs are for cross-checking; runtime binding still resolves methods via IL2CPP metadata.

## Resolved from full IL2CPP dump (0.427.0)

RVAs are version-scoped to this exact build. Re-verify if the build changes.

| Purpose | Symbol | Namespace / type | RVA |
| --- | --- | --- | --- |
| Mode B hook (preferred) | `MapQueryManager.OnMapQueryResponse(int, byte[])` | `Niantic.Platform.GameMapObject.Map` | `0x9994F4C` |
| Mode B hook (per-batch cells) | `MapQueryManager.ProcessCellsFromResponse(IEnumerable<MapS2Cell>)` | same | `0x9995900` |
| Entity delta classifier (mode A ref) | `MapQueryManager.Process(visible/updated/hidden/deleted/disposed/translate)` | same | `0x9995FEC` |
| Per-cell merge | `MapQueryManager.MergeCellUpdate(MapS2Cell)` | same | `0x998D684` |
| Player position | `LocationProviderAdapter.get_CurrentLocation()` → `Location` | impl of `Niantic.Holoholo.ILocationProvider` | `0x8AA6D38` |
| Player lat/lng | `Location.get_LatitudeDeg()` / `get_LongitudeDeg()` (double) | `Niantic.Platform.Ditto.Geo.Location` (struct) | `0x9964C24` / `0x9964C34` |
| Direct catch | `MapPokemon/WildMapPokemon.TryCapture(PokeballThrow, ARPlusEncounterValuesProto)` | `Niantic.Holoholo.Map` | `0x83A3CC8` / `0x8A1EDCC` (base `0x7F91ECC`), Slot 42 |

`PokeballThrow` is a **value struct** (`Item BallType @0x0`, `float ReticleSize @0x4`, `bool HitBullseye/Spinning/Missed @0x8/0x9/0xA`) — this matches the native `DirectMapPokeballThrow` layout exactly, confirming the by-value ABI used by the direct-catch invocation is correct.

`Location` is a value struct in `Niantic.Platform.Ditto.Geo` exposing `LatitudeDeg`/`LongitudeDeg` (double) getters; `ILocationProvider.get_CurrentLocation` returns it by value.

## Important DummyDll limitation

The generated DLLs are metadata-oriented IL2CPP dummy assemblies. They are useful for:

- namespaces and type names;
- method and property names;
- signatures and relationships visible in metadata;
- fields and interfaces;
- generated address/RVA attributes when present in the dump.

They are **not** a trustworthy copy of the original managed method bodies. Dummy method bodies may be stubs/default returns. Do not infer real control flow from a dummy IL body.

To prove that a candidate is a real observation source, correlate it with runtime behavior or inspect the corresponding native `libil2cpp.so` implementation at its recovered address.

## Confidence labels

Use these labels in later notes so static discovery is not confused with runtime proof.

| Label | Meaning |
| --- | --- |
| `STATIC_NAME` | A relevant type/method/field name exists in DummyDll metadata. |
| `STATIC_SIGNATURE` | Signature/ownership information has also been confirmed from metadata. |
| `STATIC_ADDRESS` | A native address/RVA has been recovered for the candidate. |
| `SOURCE_CORRELATED` | Static candidate has been correlated with another independent source/path. |
| `RUNTIME_VERIFIED` | A read-only runtime probe showed the candidate fires with the expected state change. |

Status as of this pass: the concrete map-object types below are `SOURCE_CORRELATED` (a native reader in `zygisk/jni/` already enumerates them at runtime). Nothing is `RUNTIME_VERIFIED` for the event-driven (mode A) boundary yet, because the current reader is snapshot-based (mode C), not callback-driven.

## Confirmed concrete types & current implementation (0.427.0)

The unresolved questions from earlier passes are now answered by the shipped runtime reader. Record these as `SOURCE_CORRELATED`.

| Concern | Concrete type / entry point | Where used today |
| --- | --- | --- |
| Wild map Pokémon | `Niantic.Holoholo.Map.WildMapPokemon` (per `MapEntityCell.eghw` wild dictionary, reached from `MapEntityService.GetCells()`) | `zygisk/jni/runtime_map_reader.inc` |
| PokéStop | `Niantic.Holoholo.Pokestop.MapPokestop` (`get_Id`, `get_Location`, `get_IsCoolingDown`) | `zygisk/jni/runtime_map_forts.inc` |
| PokéStop directory | `Niantic.Holoholo.Map.MapPlaceDirectoryService` → pokestops dictionary | `zygisk/jni/runtime_map_forts.inc` |
| Spin execution | `PoiItemSpinner.Spin` (main-thread invoke) | `zygisk/jni/modules/catch_spin/spin.inc` |

**Observer control model is now mode B (dirty-flag), falling back to mode C.** `runtime_observation_thread` re-reads the map only when `ProbeContext.world_dirty` is set (or a rare reconcile), instead of every tick. Until the map-query batch callback is inline-hooked, a periodic tick marks it dirty, so it still behaves like the old mode-C poll with no regression. The underlying read is still `read_runtime_map_snapshot_on_main_thread` walking `GetCells()` + the PokéStop dictionary (`runtime_map_reader.inc`, `kMapReaderStage = 5`). Seams `world_dirty`, `mark_world_dirty()`, and `RuntimeBinding.map_query_hook_installed` are in place; the remaining mode-B work (resolve + inline-hook the batch callback, capture its ARM64 prologue from the live `libil2cpp.so`) is device-dependent. See `docs/architecture/independent-runtime-control.md` → "Observation mechanism (mode B: dirty-flag)". Raw per-entity mode A is intentionally skipped (burst lag).

### Observer gating: sources are now independent

What each verification flag actually covers (do not be misled by the names):

| Flag | Really means | Source |
| --- | --- | --- |
| `map_entity_read_verified` | The map-entity **cell traversal** works (`MapEntityService.GetCells`, `MapEntityCell` field read) | `runtime_probe_discovery.inc:119` |
| `encounter_read_verified` | The shared **wild/encounter proto field schema** is present (`WildMapPokemon.egws`, `WildPokemonProto.{EncounterId,Latitude,Longitude,Pokemon}`, `PokemonProto.*`, `PokemonDisplayProto.Shiny`) — used by BOTH the nearby reader and the encounter reader | `runtime_probe_discovery.inc:465-480` |
| `forts_read_verified` | The PokéStop directory reader works (`MapPlaceDirectoryService`, `MapPokestop.get_Id/Location/IsCoolingDown`) | `runtime_direct_map_bindings.inc:166` |

Real dependency matrix:

- Nearby pokemon (with species/IV/shiny) needs `map_entity_read_verified` **and** `encounter_read_verified`.
- **Forts need only `forts_read_verified`** — independent of the other two.
- Active-encounter read needs `encounter_read_verified`.

Previously both `CatchSpinModule::available()` and `start_runtime_observation` gated solely on `encounter_read_verified`, and the loop nested the fort read inside the `map_entity_read_verified` block — so a failure to bind pokemon proto fields killed **all** observation, including forts/spin that do not need them. This was decoupled:

- `start_runtime_observation` now starts when `encounter_read_verified || forts_read_verified` (`runtime_observation.inc`).
- `CatchSpinModule::available()` now returns `exact_build_verified && (encounter_read_verified || forts_read_verified)` (`modules/catch_spin/module.inc`).
- The observer loop polls the snapshot when `map_entity_read_verified || forts_read_verified`, and publishes each payload only under its own flag (`runtime_observation.inc`).
- `read_runtime_map_snapshot_on_main_thread` takes a forts-only fast path when the cell reader is unavailable but `forts_read_verified` holds (`runtime_map_reader.inc`).

Each loop section self-gates on its own flag, so the loosened outer gates are safe: a missing binding just disables that one source. This keeps fort/spin observation (and the out-of-balls → spin path) alive when the pokemon proto fields fail to bind, and matches the independent-path goal for the mode A/B refactor. Native changes are edits only; they were not cross-compiled here (the build needs the Zygisk API header via `-DZYGISK_API_DIR`; see `zygisk/README.md`).

## `Niantic.Platform.GameMapObject.dll` findings

### Map query / cell ingestion pipeline

The following symbols are present in the assembly metadata (`STATIC_NAME`):

```text
OnMapQueryResponse_RpcResponse
OnMapQueryResponse
OnMapQueryResponseReceived
ProcessCellsFromResponse
SetCellDataFromResponse
SetGameEntitiesFromResponse
PruneRecentUpdatedCells
MergeCellUpdate
AddGameEntityUpdate
ClearEntityFromUpdateQueue
QueueEntityForDeletion
```

Related type/area names visible in the same assembly include:

```text
MapQueryManager
GameEntityManager
GameEntityUpdateListener
MapS2Cell
S2CellCache
GameEntityDataUpdate
```

This is strong static evidence for a map-response -> cell-processing -> entity-update pipeline. A likely conceptual flow is:

```text
map query response
    -> OnMapQueryResponseReceived
    -> ProcessCellsFromResponse
    -> SetCellDataFromResponse / SetGameEntitiesFromResponse
    -> MergeCellUpdate / AddGameEntityUpdate
    -> entity lifecycle notifications
```

The arrows above are an architectural hypothesis based on names and ownership, not yet verified call edges.

### Entity lifecycle candidates

The following symbols are particularly interesting (`STATIC_NAME`):

```text
OnGameEntityVisible
OnGameEntityHidden
OnGameEntityUpdate
OnGameEntityDelete
OnGameEntityTranslate
SetGameEntitiesDeleted
IsEntityDeleted
IsEntityInCache
ClearCache
```

These are currently the strongest candidates for avoiding periodic full-map scans.

If runtime correlation proves that wild Pokémon and Fort entities pass through these callbacks, the preferred observer architecture becomes:

```text
GameEntity callback
    -> copy minimal immutable identity/state
    -> bounded native queue
    -> return immediately to game

native worker
    -> drain queue
    -> update NativeWorldCache
    -> dedupe/batch deltas
    -> publish Nearby/Forts observation

Kotlin service
    -> filter / score / select
    -> issue guarded OpenEncounter/Catch/Spin command
```

Do not perform filtering, socket I/O, large serialization, collection enumeration, or long locking inside a game callback hook.

## Lowest-lag design (preferred over raw mode A)

Raw mode A — hooking every per-entity lifecycle call (`SpawnOrUpdateMapPokemon`, `AddMapPokemon`, `RemoveMapPokemon`, `AddPokestop`/`RemovePokestop`) — is event-driven, but it is **not** automatically the cheapest. When a map RPC batch lands, dozens of entities are added in a single frame, so the game fires the per-entity method dozens of times in that frame. Any non-trivial work multiplied by that burst is exactly what causes a visible hitch. Never hook the visual `OnSpawned`/`OnDespawned` callbacks either — those run on the render/animation path and are the highest-lag surface.

Two independent facts drive an auto catch-spin decision, and they change at very different rates:

1. **The target set** (which Pokémon/Forts exist and where) — changes only when a server map update lands. Coarse, low frequency (seconds).
2. **In-range state** (is the player close enough now) — changes as the player *moves*, with no map data change at all. Walking into range of an already-known Fort fires zero map events.

So the low-lag architecture separates them:

```text
coarse map-data signal (mode B trigger)
    -> hook ONE batch boundary, not each entity:
       OnMapQueryResponseReceived / ProcessCellsFromResponse (network/RPC layer)
    -> set world_dirty = true; return immediately (no read in the hook)

player-location tick (~1 Hz GPS cadence)
    -> hook UpdatePlayerLocation (cheap, low frequency) OR read player LatLng in the pump
    -> cache player position

rate-limited main-thread pump (e.g. every 250-500 ms)
    -> if world_dirty: do ONE bounded read of the concrete types, refresh target cache, clear dirty
    -> always: recompute distance(player, cached targets)  // pure math, no IL2CPP graph walk
    -> emit in-range / spinnable deltas to Kotlin
```

Why this beats raw mode A for lag:

- Map reads are **coalesced to one per server batch**, instead of one hook invocation per entity in a burst frame.
- The frequently-running part (in-range recompute) is **pure arithmetic over a small cached list** — no managed calls, no object-graph traversal, no allocation.
- Nothing runs per render frame; the pump is rate-limited and skips work when neither dirty nor moved.
- It captures the case raw entity hooks miss entirely: player walking into range of a stationary, already-known Fort.

Cheaper still, if a per-cell version/dirty marker can be found (the pipeline has `PruneRecentUpdatedCells`, `MergeCellUpdate`, `S2CellCache`): re-read only the cells whose update marker changed, instead of all cells, on each dirty read.

Hook hygiene (applies to any hook on the game thread): atomic flag / fixed-size ring buffer only, no allocation, no logging, no locking, no socket I/O inside the hook — copy the minimum and return. Keep the existing 512-entry bounded read cap.

## Current observation hypothesis

Prefer the following hierarchy rather than a fixed 1-2 second full snapshot loop.

### A. Best case: entity-level incremental callbacks

If `OnGameEntityVisible/Update/Delete/Hidden` reliably represent the lifecycle of Pokémon/Fort entities:

- hook them read-only;
- capture only enough primitive identity/state to update a cache;
- push to a bounded queue;
- publish batched deltas off-thread;
- use a rare reconciliation snapshot only for recovery.

### B. Acceptable case: broad map/cell dirty trigger

If entity callbacks are incomplete but `OnMapQueryResponseReceived`, `ProcessCellsFromResponse`, `MergeCellUpdate`, or a similar callback reliably indicates world-state change:

```text
map/cell callback
    -> set world_dirty = true
    -> return

observer worker
    -> when dirty, schedule one bounded/narrow read
    -> diff against cache
    -> publish only changes
```

This still avoids blind continuous polling.

### C. Fallback: adaptive reconciliation polling

Only if no stable callback can be runtime-verified:

- take an initial snapshot when `CATCH_SPIN` becomes enabled;
- poll at a conservative/adaptive interval;
- avoid per-frame or very short intervals;
- skip reads while scene/runtime state is not safe;
- emit only when the normalized snapshot changes.

## Near-player triggering (the actual auto catch-spin gate)

This is the part the reader does not compute yet. Publishing a target's `lat/lng` is not enough; the trigger needs distance-to-player and a range threshold.

Mechanism 1 — **player position (now wired, needs device verify).** Implemented via `ILocationProvider` (impl `LocationProviderAdapter`): resolved in `runtime_probe_discovery.inc` (`location_get_current` + `Location.get_LatitudeDeg/get_LongitudeDeg`), read by `read_runtime_player_position` (`runtime_map_forts.inc`), attached to `RuntimeNearbyObservation` and serialized in nearby payload **v2**, decoded by `RuntimeNearbyPayloadCodec` into `RawNearbyObservation.playerLatitude/Longitude`. `Location` is a value struct returned by value, so the getters are invoked with the unboxed interior pointer — this value-type invoke ABI is the part to confirm on device. (The old `append_optional_absent` at `runtime_bridge_protocol.inc:349-350` belongs to the separate `map_target` message path, not the nearby payload.)

Preferred source — `Niantic.Holoholo.ILocationProvider` (impl `LocationProviderAdapter`), holo-game.dll. Chosen because the runtime's service dispatcher **already lists it** as a resolvable candidate (`zygisk/jni/runtime_probe_discovery.inc:265`), so no new resolution machinery is needed. Members (`STATIC_NAME`):

- `get_CurrentLocation` — on-demand player position for the rate-limited pump.
- `add_OnLocationChanged` / `remove_OnLocationChanged` — the cheap, low-frequency player-movement event that drives the in-range recompute in the low-lag design above (this is the mode-A boundary for *position*, distinct from the map-data boundary).
- `get_LocationLatLng`.

Other candidates seen in the dump if `ILocationProvider` proves unsuitable at runtime: `Niantic.Titan.Core` `ITitanPlayerLocationService.GetPlayerLocation()` + `get_HasValidLocation` (return type `TitanS2LatLng`, fields `latitude`/`longitude`/`latRadians`); its holo-game wrapper `GameExtendedTitanPlayerLocationService`; and `Niantic.Titan.GeoClientCoreUnity` `get_CurrentAvatarLocation`.

LatLng shape: the existing readers already handle both variants — `WildMapPokemon.get_Location()` exposes named `Latitude`/`Longitude` fields read via `read_runtime_field` (`runtime_observation.inc:141-142`), while `MapPokestop.get_Location()` returns a boxed `{double latitude; double longitude}` struct (`runtime_map_forts.inc:34-35`). Confirm which shape `ILocationProvider.get_CurrentLocation` returns at runtime and reuse the matching reader.

Missing mechanism 2 — **range thresholds.** Do not hardcode. GameMaster carries them (`STATIC_NAME`):

- `GameMasterDataExt_MapObjectsInteractionRangeSettings`, `RemoteInteractionRangeMeters` → PokéStop/Gym spin range (historically ~80 m).
- `EncounterRangeM` / `BaseEncounterRangeM` (on the map/encounter Pokémon side) → tap range to open an encounter.
- `pokestopCoolDownPeriodMs` → per-Fort local re-spin cooldown (historically ~5 min).

The two available ways to know "in range":

- **Compute in Kotlin (preferred, matches the architecture rule):** haversine(player, target) ≤ range. Distance/scoring/selection is a Kotlin concern; native only supplies the raw coordinates + cooldown it already has.
- **Read the game's own booleans (execute-time guard only):** `IMapPokestopInteractive` exposes `get_CanSpin`, `get_IsPlayerInRange`, `get_Distance`, `get_CooldownTimer`, `get_Interactable`. These are version-drift-proof but the interactive object typically only exists while the Fort is in view, and it adds a main-thread managed call. Use it as a final chokepoint right before `PoiItemSpinner.Spin`, not as the discovery mechanism.

Eligibility predicates (Kotlin `AutomationPolicy`):

```text
spinnable(fort)  = fort.spinAvailable            // = !IsCoolingDown, already published
                   && haversine(player, fort) <= spinRangeM
                   && lifecycle == OVERWORLD
                   && !onLocalCooldown(fort.id)   // pokestopCoolDownPeriodMs
catchable(spawn) = haversine(player, spawn) <= encounterRangeM
                   && passesSpeciesFilter(spawn)
                   && !alreadyAttempted(spawn.id)
```

## Pokémon-specific questions — mostly resolved

- Concrete wild Pokémon type: `Niantic.Holoholo.Map.WildMapPokemon` (`SOURCE_CORRELATED`).
- Spawn ID / species / location are read directly from that object today (no secondary lookup needed for discovery) — `runtime_map_reader.inc`.
- Event-driven add/update/remove candidates (holo-game layer, `STATIC_NAME`): `SpawnOrUpdateMapPokemon`, `AddMapPokemon`, `RemoveMapPokemon`. Prefer these over the low-level `GameMapObject` `OnGameEntity*` callbacks because they operate on the concrete type the reader already parses.

Still to prove at runtime: whether every nearby add/remove passes through those manager methods, whether they run on the Unity main thread, their call frequency under a batch RPC, and object lifetime after the call returns.

## Fort / PokéStop questions — mostly resolved

- Forts do **not** ride the wild-Pokémon path; they come through a separate service: `Niantic.Holoholo.Map.MapPlaceDirectoryService` → pokestops dictionary → `Niantic.Holoholo.Pokestop.MapPokestop` (`SOURCE_CORRELATED`).
- Spin availability/cooldown is read from `MapPokestop.get_IsCoolingDown` today.
- Event-driven candidates (holo-game layer, `STATIC_NAME`): `AddPokestop` / `RemovePokestop` on `MapPlaceDirectoryService`.

Still to prove: whether cooldown/spin-availability changes arrive as an update on the existing object or require a re-read, and whether Gyms flow through the same directory (`get_ActiveGyms` / `get_ActivePokestops` are distinct accessors).

## Auto catch-spin decision logic (Kotlin)

### Intended control flow

```text
native: on any new Pokémon or Fort (only while OVERWORLD / idle)
    -> publish the whole batch to Kotlin
Kotlin (AutomationCoordinator.plan):
    -> select a target (Pokémon or Fort) from the batch
    -> trigger the matching action back to native (Catch / OpenEncounter / Spin)
```

This is the existing two-path shape: observation goes up into `AutomationSnapshot`; `AutomationCoordinator.plan(snapshot, policy)` returns `List<AutomationAction>` that flow down to `spin.inc` / `catch.inc` / `open_encounter.inc`. The "idle" precondition is already the `lifecycleState == OVERWORLD` branch in `AutomationCoordinator.kt`.

### Ball-availability gate before catch — IMPLEMENTED (Kotlin, distance-agnostic)

Rule: catching consumes balls, so before catching, verify balls remain; if none, force a spin (to farm balls) instead of attempting a doomed catch.

Item-ID knowledge (from the discard UI, `AutomationCategoryFragment.kt:122-140`): `1 = Poké Ball`, `2 = Great Ball`, `3 = Ultra Ball`, `12 = Master Ball`.

Implemented decisions:

- **Only Poké Ball (id 1) is counted.** The auto-catch path is DIRECT_MAP, and native `MapPokemon.TryCapture` always throws `DirectMapPokeballThrow.ball_type = 1` (Poké Ball, hardcoded in `runtime_direct_map_actions.inc:1-8`). Counting Great/Ultra would let the gate believe a catch is possible when the flow can only spend Poké Balls. Master Ball is excluded for the same reason (never thrown by this flow).
  ```kotlin
  const val CATCH_BALL_ITEM_ID: Int = 1 // Poké Ball — the only ball DIRECT_MAP throws
  fun InventorySnapshot.catchBallCount(): Int =
      items.filter { it.itemId == CATCH_BALL_ITEM_ID }.sumOf { it.count }
  ```
  Revisit this if native TryCapture learns to pick the best available ball (option B): then the gate should count `{1,2,3}` again.
- **Threshold:** `AutomationPolicy.minBallsToCatch: Int = 1` (validated `>= 0`). Catch is suppressed when `catchBallCount() < minBallsToCatch`.
- **Unknown inventory is not "out of balls":** if `snapshot.inventory == null`, the gate is inactive and catching proceeds as before (avoids a regression when inventory is not wired into the snapshot).
- **No spinnable Fort while out of balls:** emit no action — the coordinator has no "move" capability today.

Wired into the OVERWORLD branch of `AutomationCoordinator.plan()`:

```text
outOfBalls = inventory != null && catchBallCount() < minBallsToCatch
catchIntended = a catch-all target OR an open-encounter target was selected

if catchIntended && !outOfBalls  -> emit Catch (DIRECT_MAP) / OpenEncounter
if autoSpin || (outOfBalls && catchIntended) -> emit Spin for every spinnable Fort
```

`planEncounter` (ENCOUNTER lifecycle) also honours the gate: when `outOfBalls`, it takes any configured snapshot but does not throw (a spin cannot run inside an active encounter).

Still distance-agnostic: the forced spin currently targets **all** `spinAvailable` forts, because player position / in-range is not wired yet. Once `ILocationProvider` lands, replace "every spinnable Fort" with "nearest in-range spinnable Fort" using the eligibility predicates above.

Not yet done: `CatchPlanner.decide()` still ignores inventory (the gate lives in the coordinator, not the planner); native `throw_runtime_ball_on_main_thread` still has no count precheck (Kotlin gate makes a doomed native throw unreachable in the auto flow).

## Encounter observation

Encounter lifecycle should stay event-driven where a stable runtime boundary exists. It does not need to share the exact same world-cache mechanism as nearby/Forts.

Desired architecture:

```text
Nearby candidate selected by Kotlin
    -> OpenEncounter command
    -> native executes guarded open
    -> encounter lifecycle/data callback or verified state transition
    -> EncounterObservation
    -> Kotlin performs final richer policy check
    -> Catch command or skip
```

Nearby/map state is candidate-discovery data; encounter-derived state is the stronger source for a final catch decision when richer information becomes available.

## Recommended runtime diagnostic experiment

Do not enable auto-catch while identifying the observer boundary. Use a read-only diagnostic probe.

Start with a very small candidate set:

```text
OnGameEntityVisible
OnGameEntityUpdate
OnGameEntityDelete
OnGameEntityHidden
OnMapQueryResponseReceived
ProcessCellsFromResponse
MergeCellUpdate
```

For each candidate hook, record only bounded diagnostic data:

```text
timestamp / elapsed ns
thread id
candidate method id/name
this pointer
entity/object pointer when present
stable entity id/type only when it can be read safely
```

Implementation requirements:

1. Hook must not change arguments, return values, or game state.
2. Hook must enqueue a fixed-size diagnostic record and return quickly.
3. Use a bounded/ring buffer; dropped diagnostic records are preferable to blocking the game thread.
4. Formatting/logging/bridge writes happen on a worker, never inside the hook.
5. Take occasional reconciliation snapshots as ground truth during the experiment.
6. Correlate callback timestamps with `+spawn`, `-spawn`, Fort changes, map movement, and encounter transitions.
7. Record call frequency; reject candidates that effectively fire every frame unless they can be reduced to a cheap dirty flag.

Example evidence pattern wanted:

```text
10:01:04.212  OnGameEntityVisible  entity=0x...
10:01:04.301  reconcile            +spawn ABC
```

Repeated correlation across many additions/removals can promote a candidate from `STATIC_*` to `RUNTIME_VERIFIED`.

## Decision after runtime experiment

Classify the result into one of these modes:

```text
A. Reliable entity add/update/remove callbacks
   -> hook-driven incremental NativeWorldCache

B. Reliable map/cell-change callback but incomplete entity lifecycle callbacks
   -> hook sets dirty; perform one narrow reconciliation read

C. No stable callback
   -> adaptive polling/reconciliation fallback
```

Do not select mode A merely because method names look correct in DummyDll.

## Next reverse targets

Inspect these assemblies/files as needed, driven by unresolved runtime questions rather than scanning every DLL blindly:

```text
holo-game.dll
holo-game-interfaces.dll
holo-game-shared-types.dll
Niantic.Platform.GameStates.dll
Niantic.Titan.GeoClient.dll
Niantic.Titan.GeoClientCoreUnity.dll
Niantic.Lightship.Maps.Core.dll
Niantic.Lightship.Maps.Reactive.dll
holo-protos.dll
```

Priority order (updated — discovery of the concrete types is done; the work is now the low-lag event boundary and the near-player gate):

1. **Wire player position.** Resolve `Niantic.Holoholo.ILocationProvider` (already a dispatcher candidate at `runtime_probe_discovery.inc:265`) via `get_CurrentLocation` + `add_OnLocationChanged`, and populate the two currently-absent player-coordinate fields. Fallbacks: `ITitanPlayerLocationService.GetPlayerLocation()` (Titan.Core) or `get_CurrentAvatarLocation` (GeoClientCoreUnity). Highest value; unblocks all distance logic.
2. **Pull range/cooldown from GameMaster** (`MapObjectsInteractionRangeSettings` / `RemoteInteractionRangeMeters`, `EncounterRangeM`, `pokestopCoolDownPeriodMs`) instead of hardcoding, or capture them as version-scoped constants.
3. **Find the coarse batch trigger** for mode B: confirm `OnMapQueryResponseReceived` / `ProcessCellsFromResponse` fires once per server map update and can drive a dirty flag (measure its frequency vs. per-entity `SpawnOrUpdateMapPokemon`).
4. Verify thread + frequency + object lifetime for the holo-game lifecycle candidates (`SpawnOrUpdateMapPokemon`/`RemoveMapPokemon`, `AddPokestop`/`RemovePokestop`) before promoting any to a permanent hook.
5. Look for a per-cell version/dirty marker (`PruneRecentUpdatedCells`, `MergeCellUpdate`, `S2CellCache`) to enable incremental re-reads.
6. Identify a stable encounter opened/closed/data-ready boundary.
7. Only then implement permanent hooks; read-only diagnostic hooks come first, and delete the transitional mode-C snapshot path once mode B/A is runtime-verified.

## Future-session checklist

Before repeating reverse work, read this file and answer:

- Which candidates have advanced to `RUNTIME_VERIFIED`?
- Which candidates were rejected due to call frequency, missing coverage, or unsafe lifetime?
- What Pokémon/Fort concrete entity types were identified?
- What native RVAs/addresses were confirmed for the exact 0.427.0 build?
- Did the observer settle on mode A, B, or C?
- Is the current game build still 0.427.0? If not, treat all addresses as version-scoped and re-verify them.
