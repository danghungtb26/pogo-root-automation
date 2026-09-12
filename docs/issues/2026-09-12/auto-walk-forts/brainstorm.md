# Auto-walk between forts when no Pokémon is nearby

## Questions

1. **What is the user trying to accomplish?**

   When the current area has no Pokémon, automatically walk to another usable fort. If a Pokémon appears during the walk, stop movement, let the native catch flow finish, and resume walking after a terminal catch result.

2. **What would success look like?**

   The feature can be enabled or disabled from the automation settings UI. With it enabled, movement starts only from a complete nearby-map snapshot with a valid player position and an available PokéStop. A found Pokémon pauses movement; movement resumes only after caught/fled is observed and a fresh empty scan is received.

3. **What are the inputs and outputs?**

   Inputs are the persisted toggle, nearby Pokémon, player position, fort list and fort availability, plus native found/caught/fled events. Outputs are start/stop-walking commands consumed by the joystick location service and diagnostic logs.

4. **Who or what is affected?**

   The native catch/spin runtime remains authoritative for map observations and catch results. The Android structured controller coordinates navigation, while the existing mock-location controller performs the actual walk. The settings screen and persisted automation config gain one opt-in switch.

5. **What constraints matter?**

   Movement must fail closed when map data or position is missing, must not compete with an active encounter/catch, and must preserve the existing map-tap walk path. Fort selection must use PokéStops that are currently spin-available and prefer a different/nearest destination.

6. **What edge cases should we handle?**

   Empty or incomplete scans, no valid forts, one-fort maps, stale target forts, arriving at a destination, repeated snapshots, feature disablement, service restart, and a Pokémon event arriving while movement is active.

7. **What are we explicitly not doing?**

   No screenshot coordinate inference, raw input fallback, new native movement binding, or movement based on an incomplete map observation. Gym selection is excluded because the current spin automation targets PokéStops.

8. **What existing components can we reuse?**

   `AutomationConfig` persists the toggle, `AutomationCategoryFragment` renders settings, `StructuredAutomationController` already receives snapshots/events, and `JoystickLocationController.walkTo` / `stopWalking` already provide cancellable mock-location movement.

9. **What needs to be verified?**

   Focused Kotlin tests for selection and pause/resume state transitions, existing core/bridge tests, a full Gradle build, native protocol/build checks, and manual logs showing navigation start, Pokémon pause, catch terminal result and resume.

10. **What remains uncertain?**

   The current runtime snapshot may omit player position in some lifecycle states, and native event delivery may vary by encounter result. The implementation therefore requires both native terminal events and a fresh empty scan before resuming, and logs the reason when it cannot navigate.

## Acceptance Criteria

No formal acceptance specification was provided; the following criteria are inferred from the request and should be confirmed by the BA/product owner:

- **AC-1**: A persisted UI toggle controls the feature and defaults to off for existing users.
- **AC-2**: With the toggle on and a complete empty nearby scan, the app selects a spin-available PokéStop other than the current target when possible and starts walking to it.
- **AC-3**: A Pokémon found while walking immediately stops walking and prevents a new fort command until the catch/encounter reaches a terminal caught or fled result.
- **AC-4**: After the terminal result, navigation resumes only after a fresh complete scan confirms no Pokémon is nearby.
- **AC-5**: Missing map data, invalid position, no usable fort, or feature disablement stops/does not start automatic movement.
- **AC-6**: Existing map-tap walking and native catch/spin behavior remain functional.

## Synthesis

The safest boundary is app-side orchestration around existing native observations and app-side mock-location movement. A small navigation state machine will consume structured snapshots and native automation events, publish idempotent walk/stop commands through a process-local bridge, and be consumed by `JoystickOverlayService`. The state machine will keep a target while walking, pause on any found Pokémon or active encounter, clear the pause only on a terminal result, and require a new empty scan before selecting/resuming a fort.

## Recommended approach

1. Add `autoWalkToFort` to the persisted config and automation settings UI.
2. Add an app-side `AutoFortNavigationCoordinator` for fort selection and pause/resume state.
3. Add an `AutoFortNavigationBus` so the headless automation service can control the existing joystick location service without moving movement logic into Kotlin’s catch module or into a new native binding.
4. Add focused unit tests for selection, fail-closed behavior, pause/resume and destination switching.
5. Build and run the existing verification suite; manual device validation should inspect navigation and catch event logs.

## Risks / open questions

- If the game does not provide player position in a complete nearby snapshot, navigation will remain stopped until a valid position is available; a fallback to the mock-location state could be added after confirming lifecycle behavior.
- If a terminal native result is not emitted for a particular failure path, the navigator will intentionally remain paused rather than issue a competing catch command; that path should be confirmed from device logs.
- The process-local command bridge assumes both Android services share the app process, which is the current manifest/service design.

