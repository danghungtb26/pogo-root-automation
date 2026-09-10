# Inventory observation and auto-discard

## Summary

Auto-discard is split into the same two directions as the rest of the runtime:
native observes inventory and publishes it; the Kotlin service decides what to
discard; native executes the discard.

As of this change the **observation half is implemented** (inventory is read and
published, `READ_INVENTORY` is advertised, and `AutomationSnapshot.inventory` is
populated). The **execution half is not** — there is no verified client-owned
discard executor yet, so `DiscardModule` stays unavailable and `DiscardItem`
actions are rejected. Wiring the read path also activates the out-of-balls →
spin gate, which depends on `snapshot.inventory`.

## Observe: poll, not event-driven

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

## Execute: not implemented (remaining work)

`DiscardModule::available()` returns false and there is no discard executor.
To complete it:

- The clean path is a low-level `RecycleInventoryItem` RPC that takes
  `(itemId, count)`. `IItemBag.RecycleItem(ItemInventoryItemWidget.ItemData,
  int, ISet<Item>)` (RVA `0x81689A8`) also exists but is UI-coupled (needs a
  widget `ItemData` and an `ISet<Item>`), so it is awkward to call from native.
- Add the executor, gate `DiscardModule::available()` on a `discard_verified`
  flag, and verify the recycle result on device.

Until then, enabling `autoDiscard` produces `DiscardItem` plans that native
rejects (the discard feature module cannot enable). The inventory read, discard
planning, and the ball gate all work regardless.

## Verification status

- Kotlin (bridge/adapter/core) compiles.
- Native is edits only, not cross-compiled here (needs the Zygisk API header).
  The value-type `GetItemCount` invoke and the curated id coverage need on-device
  verification.
