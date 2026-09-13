# Discard preparation and named action feedback

## Evidence and fix

The saved `logcat-full-20260912-194324-54906.txt` capture reports repeated
`native discard inventory list unavailable item_id=2 filter=0x0`, followed by
`ItemData unavailable` and `discard_invoke_indeterminate`. These attempts failed
before the RecycleItem invocation. Older captures also contain exceptions from
the former synthetic ItemData path; that path remains removed.

In the local 0.427.0 reverse dump, ItemInventoryGuiService.<>c (TypeDefIndex
13740) declares the singleton `<>9`, lazy delegate cache `<>9__14_1`, and instance
method `chvt(ValueTuple<Item,HoloItemType>)`. Static class initialization creates
the singleton, but does not guarantee creation of the lazy UI delegate.

The native binding now reuses an existing delegate or constructs its own using
the exact `System.Delegate.CreateDelegate(Type, Object, String, Boolean, Boolean)`
overload and the reverse-derived `chvt` method. Its process-scoped GC handle
keeps it alive across host STOP/START. It does not open the inventory UI, modify
the game's static delegate cache, or re-run the singleton constructor. If binding
fails, inventory lookup stops; retrying ListSortedPlayerInventory with a null
filter is no longer used as a fallback.

Discard validates the actual ItemData class, matching item ID, fresh count, and
recyclable flag before invoking RecycleItem. Scene registry enumeration now
unboxes MoveNext's Boolean result instead of reading the boxed object's header.
Preparation failures have a separate rejection code and descriptive toast.
An invocation with an unknown result, lost Promise target, or timeout keeps the
native mutation gate blocked, preventing blind retries. The executor and Promise
reader are separate include files under the source line limit.

## Toast contract

Automation observation type 10 now uses payload version 2: the existing POGE
marker/type/uint64 IDs, then two UTF-8 strings with big-endian uint32 byte lengths:
`subjectName` (maximum 256 bytes) and `detail` (maximum 512 bytes). Kotlin accepts
versions 1 and 2, rejects malformed text, and uses generic labels for v1 without
showing storage IDs. Deploy the controller APK and native module together.

Native owns the name lookup and a bounded cache associating encounter IDs with
species names, then captured storage IDs with those same names. Transfer also
supplies its resolved species ID, so its terminal toast remains named after the
Pokémon is removed from the bag. The tables derive from the pinned APK's
HoloPokemonId and Item enums with display spelling overrides for common items.
Names are English species/item labels; they do not depend on the game's selected
language and do not affect gameplay decisions. Unknown entries use Pokémon/Item.

Examples: `Catch success: Pikachu`, `Transfer success: Pikachu`,
`Discard success: Great Ball ×9`. Discard failures include their preparation or
Promise error description after the item name and amount.

## Verification boundary

Host tests exercise delegate construction with an empty UI cache, exact overload
selection, bind/retention failure, and reuse; discard preflight rejects missing,
stale, mismatched or protected rows without invoking the mutation. Name tests
cover catch-to-transfer correlation, all 1,025 species entries, common items,
unknown IDs and bounded retention. C++/Kotlin codec tests cover v2 names and
failure detail plus legacy frames and malformed lengths. CI runs these tests.

`./gradlew test assembleDebug` and the native multi-ABI build verify compilation
and host behavior. They do not verify IL2CPP reflection or server behavior in the
game. The new log collection attempt on 2026-09-13 could not connect to BlueStacks
Air 1 at 127.0.0.1:5565. Live validation remains required: launch with saved discard
config without opening the item bag, verify the count decreases to its configured
limit and start/success toasts use names, then repeat after game relaunch.
