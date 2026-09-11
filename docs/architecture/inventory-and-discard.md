# Inventory observation and auto-discard

## Summary

Auto-discard is split into the same two directions as the rest of the runtime:
native observes inventory and publishes it; the Kotlin service decides what to
discard; native executes the discard.

As of this change the **observation half is implemented** (inventory is read and
published, `READ_INVENTORY` is advertised, and `AutomationSnapshot.inventory` is
populated). The **execution half is scaffolded but disabled** — the executor,
parser, binding, and dispatch exist, but `kDiscardExecutionEnabled = false` keeps
`DiscardModule` unavailable and `DiscardItem` actions rejected until the
`RecycleItem`/`ItemData` construction is verified on device. Wiring the read path
also activates the out-of-balls → spin gate, which depends on
`snapshot.inventory`.

## Ball check is on-demand, not observed

The catch-vs-spin ball decision does **not** use the inventory observation. The
DIRECT_MAP catch always throws a Poké Ball, so the native catch executor reads
`IItemBag.GetItemCount(Poké Ball)` **right before the throw**
(`direct_catch_runtime_on_main_thread`); zero → it skips the throw and returns
`MainThreadActionOutcome::kOutOfBalls` → result `out_of_balls`. The controller
sets `AutomationSnapshot.outOfBalls`, and the coordinator then skips catching and
forces a spin to farm balls (cleared once a spin completes). This keeps inventory
observation out of the ball path entirely.

## Observe: poll, not event-driven, and owned by the discard module

Inventory observation now has a single consumer — discard planning — so it is a
per-module observer: `DiscardModule::observe(ObserverTickContext&)` reads
inventory at its own low cadence (~30 s) on the shared observer's attached
thread. The root observer thread only dispatches ticks to enabled modules'
`observe()` hooks; it does not read inventory itself. `DiscardModule::available()`
is gated on `inventory_read_verified` so enabling it turns inventory observation
on (discard execution stays gated separately by `kDiscardExecutionEnabled`).

Inventory is **polled at a low cadence**, not driven by an item-added event.

Rationale:

- Discard is maintenance, not time-critical. Reacting to a new item instantly
  has no benefit; discarding a few seconds later is fine.
- Items arrive in bursts (a single spin yields several), so per-item event hooks
  would fire repeatedly for no gain — the same burst problem that makes raw
  per-entity map hooks (mode A) a poor fit.
- The game does expose an event path
  (`NianticInventoryCache.InventoryUpdateEventArgs` /
  `FullInventoryUpdateEventArgs`) if true event-driven inventory is ever wanted,
  but subscribing to a C# event from native is more complex than a poll and is
  not worth it for discard.

The observer thread reads inventory every `kInventoryTicks` (~30 s) from an
attached il2cpp thread (`ItemBag` is a data service, not a Unity
`MonoBehaviour`, so it does not need the Unity main-thread bridge that the map
cell walk uses).

## Data flow

```text
Pokémon GO runtime
    |  IItemBag(ItemBagImpl).GetItemCount(Item) per curated item id
    v
observer thread (runtime_observation.inc, ~30s cadence)
    -> read_runtime_inventory (runtime_map_forts.inc)
    -> send_runtime_inventory_payload  (INVENTORY observation, payload v1)
    v
runtime bridge
    v
BridgePogoRuntimeSource
    -> RuntimeInventoryPayloadCodec.decode -> RawInventoryObservation (cached)
    -> readInventory()
    v
PogoGameAdapter.readInventory -> PogoInventoryMapper -> InventorySnapshot
    v
AutomationCoordinator.plan
    -> InventoryPlanner (discard by InventoryPolicy.maxCountByItemId)
    -> out-of-balls gate (catchBallCount, Poké Ball only)
    v
AutomationAction.DiscardItem   (execution still blocked, see below)
```

## Native reader details

- Binding: `IItemBag` is resolved as a Zenject service; `ItemBagImpl` yields
  `item_bag` and `item_bag_get_item_count` (`GetItemCount(Holoholo.Rpc.Item)`),
  setting `inventory_read_verified` (`runtime_probe_discovery.inc`).
- `read_runtime_inventory` calls `GetItemCount` for a **curated set** of common
  item ids (balls, potions, revives, berries) and emits `{itemId, count}` for
  the non-zero ones. `used_slots`/`capacity` are reported as the item sum (the
  real bag capacity is not read yet; discard uses per-item limits, not
  capacity). A complete enumeration would use
  `NianticInventoryCache.GetCurrentItems` instead — extend the curated list or
  switch to enumeration as needed.

## Wire format (INVENTORY payload v1, magic `POGV`)

```text
u32 magic (0x504F4756)
u32 usedSlots
u32 capacity
u32 itemCount
itemCount x { u32 itemId, u32 count }
```

Item names are not sent; the Kotlin mapper fills `#<id>` when blank.

## Execute: scaffolded but DISABLED (device verification required)

The executor is written and wired but gated off, mirroring the encounter catch
path (`kCatchExecutionEnabled = false`):

- `modules/discard/parse.inc` — `parse_runtime_discard_command` (action tag 5,
  reads `itemId`/`amount` from the frame `BridgeActionCodec` already produces).
- `modules/discard/execute.inc` — `execute_runtime_discard` + `recycle_runtime_item`
  and the `kDiscardExecutionEnabled` gate; dispatched in `runtime_control.inc`.
- Binding: `IItemBag.RecycleItem(ItemData, int, ISet<Item>)` + the `ItemData`
  class are resolved in `runtime_probe_discovery.inc`, setting `discard_verified`.
- `recycle_runtime_item` constructs `ItemData` via `object_new` + field writes
  (`item`@0x10, `count`@0x1C, `recyclable`@0x21 from the 0.427.0 dump) and calls
  `RecycleItem(itemData, amount, null)`.
- `kDiscardExecutionEnabled = false` — the executor returns `binding_unavailable`,
  and `DiscardModule::available()` gates on this flag, so the module stays
  unavailable and `DiscardItem` actions are cleanly rejected. No behavior change
  until the flag is flipped.

The **only** recycle entry point is `IItemBag.RecycleItem` — there is no
lower-level `(itemId, count)` overload (the `RecycleInventoryItem` RPC, Method
137, is only sent internally by `RecycleItem`). It is UI-coupled: `ItemData` is a
nested `ItemInventoryItemWidget.ItemData` and the third argument is an
`ISet<Item>`.

To finish (on device):

1. Confirm `find_runtime_class("Niantic.Holoholo.Inventory", "ItemData")` resolves
   the nested type (nested-class lookup may need the enclosing type); the
   diagnostic log line reports `discard binding verified`.
2. Verify the `ItemData` field offsets and whether `RecycleItem` reads more than
   `item`/`count`/`recyclable` (e.g. `type`), and whether the `ISet<Item>` may be
   null (construct an empty `HashSet<Item>` if not).
3. Confirm the recycle post-condition, then flip `kDiscardExecutionEnabled` to
   `true`.

Until then, inventory read, discard planning, and the ball gate all work; only
the actual recycle is withheld.

## Verification status

- Kotlin (bridge/adapter/core) compiles.
- Native is edits only, not cross-compiled here (needs the Zygisk API header).
  The value-type `GetItemCount` invoke and the curated id coverage need on-device
  verification.
