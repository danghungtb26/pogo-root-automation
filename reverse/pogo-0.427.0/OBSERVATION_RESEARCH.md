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
- Primary assembly inspected so far: `Niantic.Platform.GameMapObject.dll`
- Blob inspected for that assembly: `87d620036c690ae4f529f772ee672f87bbb5a010`

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

Nothing in this first pass is `RUNTIME_VERIFIED` yet.

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

## Pokémon-specific question still unresolved

The static pass proves that a generic GameEntity/cell update pipeline exists. It does **not** yet prove:

- which concrete GameEntity subtype represents a wild map Pokémon at these callbacks;
- whether every nearby Pokémon addition/removal passes through `OnGameEntityVisible/Delete`;
- whether the callbacks expose spawn ID/species/location directly or require a secondary lookup;
- whether callbacks are invoked on the Unity main thread;
- whether a callback can run at high/per-frame frequency;
- whether an entity object remains valid after the callback returns.

The existing runtime code already has map-entity / wild-map-Pokémon bindings. A future pass should correlate those known objects with these GameMapObject callbacks instead of inventing a new object model.

## Fort / PokéStop question still unresolved

The same generic GameEntity pipeline may also carry Fort/PokéStop state, but this has not been proven.

Runtime validation should compare callback object/type identity against the existing map place directory / active PokéStop bindings and record whether spin availability/cooldown changes arrive as entity updates or through a separate service.

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

Priority order:

1. Resolve signatures/declaring types and native addresses for the GameMapObject candidates above.
2. Identify the concrete wild-Pokémon GameEntity type and its stable ID/species/location accessors.
3. Identify whether Fort/PokéStop updates use the same entity callbacks or a different service.
4. Identify a stable encounter opened/closed/data-ready boundary.
5. Only then implement permanent hooks; diagnostic hooks come first.

## Future-session checklist

Before repeating reverse work, read this file and answer:

- Which candidates have advanced to `RUNTIME_VERIFIED`?
- Which candidates were rejected due to call frequency, missing coverage, or unsafe lifetime?
- What Pokémon/Fort concrete entity types were identified?
- What native RVAs/addresses were confirmed for the exact 0.427.0 build?
- Did the observer settle on mode A, B, or C?
- Is the current game build still 0.427.0? If not, treat all addresses as version-scoped and re-verify them.
