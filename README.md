# pogo-root-automation

Rooted Android automation framework for experimenting with structured game-state observation and deterministic automation around the official Pokémon GO client.

## Scope

- GPS/location spoofing is **out of scope**. Use a separate location provider such as GPS Joystick.
- The project is split into a stable automation core and a version-specific game adapter.
- Initial milestone is read-only: process lifecycle + nearby snapshot + despawn countdown.
- Mutation features (catch, spin, discard, transfer) are added only after the read-only adapter is reliable.
- No anti-cheat bypass, integrity bypass, root hiding, or account-safety evasion is implemented here.

## Target architecture

```text
GPS Joystick -> Android location -> Official Pokémon GO
                                      ^
                                      | instrumentation bridge
Controller APK <-> Bridge <-> GameAdapter <-> Root/Zygisk runtime
      |
      +-> Automation Core
          +-> Nearby / countdown
          +-> Catch scheduler
          +-> Spin scheduler
          +-> Inventory rules
          +-> Transfer rules
```

## Milestones

1. **M0 Foundation** — Android project, domain contracts, tests.
2. **M1 Root runtime** — Zygisk lifecycle targeting and controller bridge boundary.
3. **M2 Nearby read-only** — structured nearby snapshot and despawn countdown.
4. **M3 Catch** — manual action, then deterministic auto-catch scheduler.
5. **M4 Spin** — fort observation and auto-spin.
6. **M5 Inventory** — inventory reader and discard planner/executor.
7. **M6 Transfer** — storage reader, dry-run transfer rules, then executor.
8. **M7 Hardening** — version adapters, recovery, diagnostics, long-run tests.

> This project can violate Pokémon GO's terms of service when used to automate gameplay. Use test accounts and devices you control.
