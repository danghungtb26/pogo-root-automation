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

## Observe: main-thread reads, bounded polling, and native ownership

Inventory observation is owned by the native discard module:
`DiscardModule::observe(ObserverTickContext&)` requests a bounded
`request_main_thread_inventory()` read at its own low cadence (~30 s). The
observer thread is only a scheduler; it never reads the live bag dictionary,
list or cache directly. Recycle Promise and refresh maintenance is dispatched
through the shared observer even while the discard module is disabled, so a
configuration toggle cannot abandon an in-flight game operation.

`DiscardModule::available()` is gated on the exact build, inventory read,
verified cache owner/timestamp/refresh methods, genuine inventory service
context and `RecycleItem` contract. Static resolution alone does not enable
mutation; `kDiscardExecutionEnabled` remains closed until device calibration.

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

The observer thread schedules inventory every `kInventoryTicks` (~30 s), while
the actual `GetItemCount` calls and all mutation/Promise/cache operations run
on Unity's main thread. A valid snapshot also requires the verified
`InventoryCache.GetLatestTimestamp()` baseline; a failed read is unknown and
does not publish an empty bag.

## Data flow

```text
Pokémon GO runtime
    |  IItemBag(ItemBagImpl).GetItemCount(Item) per curated item id
    v
observer scheduler (~30s cadence)
    -> Unity main-thread bridge
    -> read_runtime_inventory + InventoryCache timestamp baseline
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
    -> main-thread InventoryCache.UpdateInventory + full-response reconcile
```

The catch-spin coordinator consumes its own native snapshot directly; the
snapshot telemetry sent to Kotlin is not a catch/spin or discard decision input.
Kotlin persists the discard settings and mirrors them into the native module
through `DISCARD_CONFIG_SET`; it no longer plans automatic discard actions.

## Native reader details

- Binding: `IItemBag` is resolved as a Zenject service; `ItemBagImpl` yields
  `item_bag` and `item_bag_get_item_count` (`GetItemCount(Holoholo.Rpc.Item)`),
  setting `inventory_read_verified` (`runtime_probe_service_owners.inc`). Its
  `cwch` cache field is separately checked for zero-argument
  `GetLatestTimestamp(): System.Int64` and
  `UpdateInventory(): IPromise<GetHoloholoInventoryOutProto>`.
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
- `recycle_runtime_item_on_main_thread` reads the live stack count via
  `GetItemCount`, populates the verified `ItemData` layout (`item`, `type`,
  `category`, stack `count`, flags, optional `MedicineData` for potions/revives)
  and calls `RecycleItem(itemData, numToRecycle, expiringItemsCopy)` using
  `IItemInventoryService.get_ExpiringItemsCopy()` when available.
- `kDiscardExecutionEnabled` is a compile-time safety gate; the runtime binding
  diagnostic must still verify `RecycleItem` and its `ItemData` parameter class.
- Automatic amount is recomputed from the current count and current configured
  limit on the main-thread callback. Manual amount is preserved and must fit
  the current stack. The intent carries config revision, action id and owner
  generation; a stale callback is rejected before `RecycleItem`.
- `Result=1` is server success for the mutation only. Native then retains the
  barrier until `UpdateInventory()` completes with an exact
  `GetHoloholoInventoryOutProto` and a valid copied inventory snapshot.
  Response `NewCount`, prediction and telemetry are diagnostic only; native
  does not write them into the cache or call game rollback methods.

The **only** recycle entry point is `IItemBag.RecycleItem` — there is no
lower-level `(itemId, count)` overload (the `RecycleInventoryItem` RPC, Method
137, is only sent internally by `RecycleItem`). It is UI-coupled: `ItemData` is a
nested `ItemInventoryItemWidget.ItemData` and the third argument is an
`ISet<Item>`.

To finish calibration (on device):

1. Confirm the `RecycleItem` first parameter resolves to the nested `ItemData`
   class; the diagnostic log line reports `discard binding verified`.
2. Verify the `ItemData` field offsets and the genuine expiration/set context
   while the bag UI is closed; missing or duplicate rows fail closed.
3. Confirm the concrete recycle Promise result and the subsequent full-refresh
   Promise/cache postcondition on device. A no-delta full response is valid only
   when the response type and copied snapshot are both verified.

Inventory read and the ball gate remain independent; native discard stays
fail-closed whenever its mutation binding is not verified.

## Verification status

- Kotlin (bridge/adapter/core) compiles.
- Native multi-ABI build/package passes, but this is not runtime proof. The
  value-type `GetItemCount` invoke, cache owner, curated id coverage, Promise
  layout and cold-launch lifetime still need BlueStacks Air 1 evidence.
- Production mutation is intentionally unavailable until the runtime/device
  checklist closes; Kotlin remains the owner of persisted policy/UI only.
