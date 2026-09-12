# Inventory observation and auto-discard

## Summary

Auto-discard is fully native after Kotlin sends the current policy snapshot:
native observes inventory, applies the limits, invokes the recycle request and
polls its Promise. Kotlin persists the settings and mirrors them to native.

The native module reads and publishes inventory for diagnostics, then uses the
revisioned `RuntimeDiscardConfig` to select one over-limit stack at a time.
`RecycleItem` is invoked on Unity's main thread and its Promise is retained and
polled before another discard is queued. The binding guard remains fail-closed.

## Ball check is on-demand, not observed

The native catch-spin coordinator reads the inventory together with each map
snapshot. The DIRECT_MAP catch always throws a Poké Ball, so an unavailable
inventory source is treated as unknown and native does not catch. The current
reader omits zero-count stacks, so a missing Poké Ball stack is interpreted as
zero and switches the native decision to spinning to farm balls. The main-thread
direct-catch path also retains its last-moment `kOutOfBalls` guard. Kotlin's
`AutomationSnapshot.outOfBalls` remains only for the encounter throw flow.

## Observe: poll, not event-driven, and owned by the discard module

Inventory observation is owned by the native discard module:
`DiscardModule::observe(ObserverTickContext&)` reads
inventory at its own low cadence (~30 s) on the shared observer's attached
thread. The root observer thread only dispatches ticks to enabled modules'
`observe()` hooks; it does not read inventory itself. `DiscardModule::available()`
is gated on both inventory-read and `RecycleItem` bindings so the auto path
cannot activate partially.

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
native discard module
    -> compare against RuntimeDiscardConfig.maxCountByItemId
    -> main-thread IItemBag.RecycleItem + Promise polling
```

The catch-spin coordinator consumes its own native snapshot directly; the
snapshot telemetry sent to Kotlin is not a catch/spin or discard decision input.
Kotlin persists the discard settings and mirrors them into the native module
through `DISCARD_CONFIG_SET`; it no longer plans automatic discard actions.

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

## Execute: native main-thread Promise state machine

The executor is wired like native transfer:

- `modules/discard/parse.inc` — `parse_runtime_discard_command` (action tag 5,
  reads `itemId`/`amount` from the frame `BridgeActionCodec` already produces).
- `modules/discard/execute.inc` — auto/manual queueing, `RecycleItem` invocation,
  GC-handle retention and Promise polling; dispatched in `runtime_control.inc`.
- Binding: `IItemBag.RecycleItem(ItemData, int, ISet<Item>)` + the `ItemData`
  class are resolved in `runtime_probe_discovery.inc`, setting `discard_verified`.
- `recycle_runtime_item_on_main_thread` constructs `ItemData` via `object_new` + field writes
  (`item`@0x10, `count`@0x1C, `recyclable`@0x21 from the 0.427.0 dump) and calls
  `RecycleItem(itemData, amount, null)`.
- `kDiscardExecutionEnabled` is a compile-time safety gate; the runtime binding
  diagnostic must still verify `RecycleItem` and its `ItemData` parameter class.

The **only** recycle entry point is `IItemBag.RecycleItem` — there is no
lower-level `(itemId, count)` overload (the `RecycleInventoryItem` RPC, Method
137, is only sent internally by `RecycleItem`). It is UI-coupled: `ItemData` is a
nested `ItemInventoryItemWidget.ItemData` and the third argument is an
`ISet<Item>`.

To finish (on device):

1. Confirm the `RecycleItem` first parameter resolves to the nested `ItemData`
   class; the diagnostic log line reports `discard binding verified`.
2. Verify the `ItemData` field offsets and whether `RecycleItem` reads more than
   `item`/`count`/`recyclable` (e.g. `type`), and whether the `ISet<Item>` may be
   null (construct an empty `HashSet<Item>` if not).
3. Confirm the recycle post-condition and Promise result on device.

Inventory read and the ball gate remain independent; native discard stays
fail-closed whenever its mutation binding is not verified.

## Verification status

- Kotlin (bridge/adapter/core) compiles.
- Native is edits only, not cross-compiled here (needs the Zygisk API header).
  The value-type `GetItemCount` invoke and the curated id coverage need on-device
  verification.
