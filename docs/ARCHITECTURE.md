# Architecture

## Boundaries

```text
Controller APK
    |
    v
    Persistent bridge protocol
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
Versioned binary-framed messages crossing the controller/runtime boundary.
The runtime session, observation sequence, command correlation, result phases,
and size limits are transport contracts; game payload decoding remains in the
POGO adapter.

### `app`
Controller APK. The headless service runs the structured runtime controller as
its only automation path. Until runtime identity, observations, and action
capabilities are verified, it remains read-only and fail-closed.

### `zygisk`
Native/root boundary. It owns process/binding discovery and the companion
broker. The current build exposes only a read-only runtime-ready contract;
version-specific game invocation remains disabled until a verified binding is
available.
