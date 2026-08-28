# Architecture

## Boundaries

```text
Controller APK
    |
    v
Bridge protocol
    |
    v
GameAdapter API <--- Fake adapter (development/tests)
    |
    v
Version-specific adapter
    |
    v
Zygisk/runtime instrumentation
    |
    v
Official Pokémon GO process
```

GPS Joystick is deliberately outside this project. Pokémon GO receives the location through Android; the automation framework only observes the resulting game state.

## Design rules

1. **Core does not know about hooks.** No offsets, JNI handles, Zygisk types, or game classes may leak into `core`.
2. **Adapters declare capabilities.** Unsupported operations must be explicit rather than guessed.
3. **Unknown expiry stays unknown.** Countdown code never fabricates an exact despawn time.
4. **Read before write.** Nearby observation must be reliable before catch/spin actions are implemented.
5. **Single writer.** When mutation work begins, all game-changing actions will pass through one serialized `ActionExecutor`.
6. **Fail closed on game updates.** An unsupported Pokémon GO build disables mutations instead of trying stale bindings.

## Modules

### `core`
Stable domain models and deterministic logic. This should have the highest test coverage.

### `game-adapter:api`
Port used by the application/automation layer. Version-specific runtime code implements this contract.

### `game-adapter:fake`
Deterministic development adapter used to build UI, countdown logic, and tests before game instrumentation exists.

### `bridge:protocol`
Messages crossing the controller/runtime boundary. Serialization transport is intentionally deferred until the native runtime lifecycle is proven.

### `app`
Controller APK. The first screen intentionally uses the fake adapter and demonstrates live countdown behavior.

### `zygisk`
Native/root boundary. The first stub only targets Pokémon GO process lifecycle and leaves all game internals untouched.
