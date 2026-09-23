# Implementation evidence — inventory/discard 0.427.0

This note records the contract used by the implementation. It does not claim
device calibration; the mutation gate remains closed until the BlueStacks Air 1
checks in T-018–T-020 are complete.

## Binding contract

The reverse output gives `ItemInventoryService.ListSortedPlayerInventory` five
managed parameters, in this order:

1. `Func<ValueTuple<Item, HoloItemType>, bool> usableFilter`
2. `bool cannotDeleteItems`
3. `Func<ValueTuple<Item, HoloItemType>, bool> showUnusableFilter`
4. `Func<Item, bool> showUnavailableFilter`
5. `Action<Item> itemExpiredCallback`

Evidence: `reverse/pogo-0.427.0/dump.cs.gz` content lines 486502–486503 and
`script.json.gz` content lines 417754–417757. The native ABI adds `__this` and
`MethodInfo*`; neither is part of the managed arity or `runtime_invoke`
argument array.

The resolver now requires an instance method, exactly five parameters, a
`List<ItemInventoryItemWidget.ItemData>` return type and the generic type
markers for all five parameters. A method-name match or a null delegate is not
enough to admit the call. The method is resolved at
`zygisk/jni/modules/discard/inventory_binding_contract.inc` and used by both
the discovery path and the main-thread fallback.

`RecycleItem` remains the public mutation entry point with
`(ItemData, int, ISet<Item>)`. Its return type must contain `IPromise` and
`RecycleItemOutProto`; the second parameter must be `System.Int32` and the
third must be `ISet<Holoholo.Rpc.Item>`. The item-data class is taken from the
first parameter type rather than guessed by namespace.

## Model/context rules

`ItemData.itemExpirationData` is a real field at offset `0x38` and the reverse
body reads it on the verified `RecycleItem` path. The native preparation path
therefore rejects a row without a managed expiration object on Android. It uses
the row returned by `ListSortedPlayerInventory`, checks exact class, item id,
count and `recyclable`, and obtains `get_ExpiringItemsCopy()` from the same
inventory service. The old unrelated supplier and empty `HashSet` fallbacks are
removed; matching generic shape is not proof of matching service context.

Evidence: `dump.cs.gz` content lines 105339–105381 and 68169–68170, plus
`ItemInventoryService.get_ExpiringItemsCopy` at the curated reverse/source
references recorded in `brainstorm.md`.

## Outcome and lifecycle rules

- The observer schedules inventory reads through
  `request_main_thread_inventory`; it does not read the live bag dictionary on
  the observer thread.
- Auto-discard recomputes the current count, current limit and excess on the
  main thread immediately before preparation. Manual commands keep their
  requested amount and still reject an amount larger than the current count.
- A reset/config revision does not free an active Promise or clear an unknown
  barrier. A transport/layout/timeout branch is indeterminate and blocks new
  mutation. Game-owned rollback is never called by the framework.
- `Result=1` is reported as a server success event, but it retains the native
  mutation barrier until an authoritative inventory reconcile exists. Results
  2 and 3 suppress only the affected item; they do not create a retry loop or
  block unrelated inventory items. `NewCount` is diagnostic only and is never
  written into the cache.
- `kDiscardExecutionEnabled` is deliberately `false` until runtime calibration;
  resolving static symbols cannot make the production mutation available.

The reverse outcome contract is now explicit:

| Signal | Meaning | Opens a new discard? |
|---|---|---|
| `RecycleItem` Promise `completeCalled=false` | still pending | No |
| Promise `errorCalled=true`, missing fields/target, timeout or unknown `Result` | transport/outcome unknown | No; keep the mutation barrier |
| `RecycleItemOutProto.Result=1` | server success for this action | No; start `InventoryCache.UpdateInventory()` |
| `Result=2` / `Result=3` | game-classified rejection | No for that item; suppress only with the observed count/owner evidence |
| `UpdateInventory` Promise completes with `GetHoloholoInventoryOutProto` and a valid copied inventory snapshot | authoritative refresh postcondition | Yes, for the current owner generation |

The binary trace supports this split: `RecycleItem` sends route 137 before its
final Promise chain; `wi.ytr` applies the success path and `wi.yts` rolls back
on Promise error. `NewCount`, prediction, telemetry and a local ACK are not
used as cache writes or server proof. A full refresh response is accepted even
when its timestamp does not change (the valid no-delta case); a refresh error,
missing layout, stale owner or invalid snapshot remains unknown.

## State machine and calibration contract

The native identity is `(action_id, owner_generation, config_revision, item_id)`;
the Promise and refresh GC handles belong to that identity. The transitions are:

```text
Idle -> Queued -> Invoked -> AwaitingReconcile -> Idle
                         \-> BlockedUnknown (error/timeout/layout/GC loss)
```

Manual intents preserve the requested amount. Automatic intents recalculate
`current_count - current_limit` on the main thread immediately before
preparation; `100 -> 70` with limit `50` therefore invokes `20`. A config
revision change, disabled module, STOP, observer restart or late callback does
not cancel a managed RPC or clear `BlockedUnknown`. The observer registry keeps
maintenance polling alive while a discard Promise/reconcile is owned, even when
the discard module itself is disabled. Pending pointers and readiness are
session-only and are never persisted in Kotlin settings.

Calibration is intentionally staged: static contract and build guard, runtime
preflight/baseline, one explicitly authorized manual action, same-session
reconcile, then optional auto verification. The static artifact has not passed
the device stages, so production mutation remains closed.

## Managed lifetime and freshness implementation

The list, row and expiration set are consumed synchronously on Unity's main
thread and are never retained across a tick. The only objects that cross a
managed call boundary are retained with GC handles: the discard Promise and
the reconcile `UpdateInventory` Promise are both created, checked, polled and
freed on every terminal path. The process-scoped filter delegate has its own
GC root because it is reused by later list calls. Owner generation invalidates
the readiness mirror and prevents a stale completion from clearing a new
session's barrier.

## Known limits

The current inventory payload still carries the curated count as both
`used_slots` and `capacity` to satisfy the existing wire invariant. That pair
is not a verified bag-capacity reading and is not used for discard admission.
The cache baseline/reconcile implementation is present, but its concrete owner,
Promise layout, no-delta behavior and cold-launch lifetime still require device
evidence. Therefore T-018–T-021 remain open and the result is not
production-ready.
