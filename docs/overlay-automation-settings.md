# Overlay automation settings

The built-in joystick overlay now includes an `Automation ⚙` button. It opens a focusable overlay dialog on top of Pokémon GO, so settings can be changed without returning to the controller activity.

## Settings

### General

- Automation master ON/OFF
- Auto catch ON/OFF
- Auto spin PokéStop ON/OFF
- Auto berry on encounter ON/OFF
- Action/result toast ON/OFF

Auto berry is gated by the encounter screen detector. The current screen executor opens the berry tray, selects the first visible berry slot, feeds it to the Pokémon, and marks the current encounter so a berry is not repeatedly applied before every throw.

### Auto discard

- Auto discard ON/OFF
- Per-item maximum counts for common balls, healing items and berries
- Blank limit means the item is unmanaged
- A configured maximum means `InventoryPlanner` discards only the amount above the maximum

The local API also accepts arbitrary item ids:

```text
POST /v1/config?autoDiscard=true&discardLimits=1:100,2:100,3:200
```

### Auto transfer

- Auto transfer ON/OFF
- Transfer Pokémon below an IV percentage (default: 100%)
- Default exclusions:
  - 100 IV
  - Shiny
  - special background (BG)
- Favorite is also protected by default as a safety guard
- Legendary and Mythical remain protected internally
- Pokémon with unknown IV remain protected

`StoredPokemon.hasSpecialBackground` and `TransferPolicy.keepSpecialBackground` carry BG state through the stable automation domain.

## Toasts

The notifier supports:

- Berry used
- Catch attempt
- Pokémon broke free / retrying
- Caught
- Run away
- PokéStop spun
- Discarded
- Transferred

For the current screen executor, caught/run-away classification is heuristic: an encounter that remains open is treated as broke-free, a return to overworld is treated as run-away, and an unknown/result screen after the throw is treated as caught. A future injected runtime executor should replace these heuristics with exact catch-result codes while retaining the same notifier API.

Discard and transfer toasts are emitted only after `GameAdapter.execute(...)` confirms success.

## Runtime boundary

Auto berry, catch and spin can execute now through root screen capture + normalized Android input.

Auto discard and auto transfer require exact inventory/Pokémon metadata. The settings, policies and `MaintenanceAutomationRunner` are implemented, but the current controller does not attach a live inventory/storage `PogoRuntimeSource` yet. The runner will execute discard/transfer as soon as that adapter is connected; until then it intentionally does not guess item counts, IV, shiny or special-background state from screen pixels.

This is deliberate to avoid destructive false-positive transfers/discards.
