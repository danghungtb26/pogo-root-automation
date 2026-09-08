# Direct Pokémon GO map target binding

The controller now has a versioned `ObservationType.MAP_TARGET` contract. The
native bridge also has a target-process envelope and companion forwarder for
that event, but the current build still has no Pokémon GO-specific tap/camera
binding. A real adapter must be verified for one exact Pokémon GO
package/version/ABI/build fingerprint before it can advertise `READ_MAP_TARGET`.

## Runtime contract

The runtime binding observes a real touch in the Pokémon GO process and emits a
structured observation only after it has established that the touch hit the
overworld map rather than a dialog, encounter, gym, or other UI layer. The
payload is encoded by `MapTargetPayloadCodec`:

```text
int32  payload_version = 1
string tap_id
double target_latitude
double target_longitude
float  screen_x
float  screen_y
int32  viewport_width
int32  viewport_height
bool   has_camera_snapshot_id
string camera_snapshot_id   // present when the bool is true
```

All integers are big-endian, strings are UTF-8 with a 32-bit byte length, and
the payload is carried inside the normal `ObservationEvent` frame with
`observationType = MAP_TARGET`. `playerLatitude`/`playerLongitude` and the
observation timestamps remain on the enclosing bridge event.

The runtime should resolve screen coordinates to `GeoPoint` inside its
version-specific Unity/IL2CPP binding whenever possible. The controller does
not infer latitude/longitude from screen pixels or from a stale camera matrix.
If the map state or projection is unavailable, the runtime must emit no target
and keep the capability fail-closed.

The native seam is `send_runtime_map_target_observation(...)` in
`zygisk/jni/main.cpp`; it uses `runtime_observation_protocol.h` to encode the
payload. The companion validates that envelope, adds the runtime session and
bridge sequence, then forwards a normal observation frame. This seam is
intentionally separate from the game binding: it does not guess Unity classes,
method addresses, camera state, or projection math.

## Capability and walk handoff

The runtime must include `READ_MAP_TARGET` in `RuntimeReady.capabilities` only
after tap capture, map hit filtering, projection and calibration have been
verified for the exact build. The headless controller ignores `MAP_TARGET`
events when that capability is absent. When the feature setting
`mapTapWalk=true` is enabled, a valid target is handed to the app-local
`MapTargetRepository`; the built-in location overlay consumes it once and
starts target-following on the same writer as joystick and teleport.

The first implementation uses `RootMockLocationProvider` for the walk. It does
not submit `AutomationAction.MoveTo` through the POGO runtime, because the
current probe announces no mutation capability and rejects client-owned
commands. A verified client-side `MOVE` binding can be added later as a
separate backend.
