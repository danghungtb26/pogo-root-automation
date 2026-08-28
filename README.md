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

## Runtime targets

### Physical Android

Primary production/debug target: rooted ARM64 Android with Magisk/Zygisk.

### BlueStacks 5

BlueStacks is supported as an emulator runtime target for lifecycle/bridge testing when the instance has working `su` plus a Zygisk-compatible injector. The Magisk module is packaged with both:

- `zygisk/arm64-v8a.so`
- `zygisk/x86_64.so`

BlueStacks may expose x86_64 as its Android/Zygote ABI while translating ARM application libraries. For that reason, game bindings must use collected runtime diagnostics and must never assume that a binding validated on ARM64 hardware is valid on BlueStacks.

After installing the controller APK and the multi-ABI Magisk ZIP and rebooting the instance, run:

```bash
./scripts/bluestacks-smoke-test.sh
./scripts/collect-binding-diagnostics.sh
```

The BlueStacks smoke test checks emulator identity, root access, the ABI-specific Zygisk library, Pokémon GO package presence and the generic attach → stop → reattach lifecycle. It does not configure root, Play Integrity, hiding or anti-detection.

## Milestones

1. **M0 Foundation** — Android project, domain contracts, tests.
2. **M1 Root runtime** — Zygisk lifecycle targeting and controller bridge boundary.
3. **M1.1 BlueStacks runtime** — x86_64/multi-ABI packaging and emulator diagnostics.
4. **M2 Nearby read-only** — structured nearby snapshot and despawn countdown.
5. **M3 Catch** — manual action, then deterministic auto-catch scheduler.
6. **M4 Spin** — fort observation and auto-spin.
7. **M5 Inventory** — inventory reader and discard planner/executor.
8. **M6 Transfer** — storage reader, dry-run transfer rules, then executor.
9. **M7 Hardening** — version adapters, recovery, diagnostics, long-run tests.

> This project can violate Pokémon GO's terms of service when used to automate gameplay. Use test accounts and devices you control.
