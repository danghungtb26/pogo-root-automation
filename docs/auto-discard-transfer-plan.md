# Auto discard / auto transfer

Destructive maintenance runs only from live runtime state. The screen driver is never allowed to mass-delete items or Pokémon.

## Auto discard

Reference behavior from PogoEnhancer:

1. read an item count from the in-process ItemBag
2. compare it with the configured keep limit
3. calculate `discard = current - keep`
4. invoke the game's recycle-item method with `(itemId, discard)`

This project keeps the same separation:

- overlay/API owns settings
- runtime bridge owns live inventory data
- core `InventoryPlanner` decides what to discard
- maintenance executor performs only the planned deltas

## Auto transfer

The runtime supplies complete Pokémon metadata before a transfer is eligible. Core transfer rules protect by default:

- 100 IV
- shiny
- special background
- favorite
- legendary
- mythical
- unknown/incomplete metadata

The runtime executor receives a Pokémon id only after `TransferPlanner` approves it.

## Runtime gate

Maintenance commands are disabled unless the runtime snapshot reports all of:

- target process connected
- exact game build known
- inventory bindings ready for discard
- Pokémon storage bindings ready for transfer
- snapshot fresh enough for the current automation pass

This prevents a stale offset or ambiguous screen state from deleting data.
