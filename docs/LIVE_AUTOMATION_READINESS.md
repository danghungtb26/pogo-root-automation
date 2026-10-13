# Live automation readiness

Updated: 2026-10-13

The canonical target is Pokémon GO `0.433.0` (`2026100702`), package
`com.nianticlabs.pokemongo`, arm64 `libil2cpp.so` build ID
`da59ce3024c4ccfe16fa6616281a4d111de67aa2`, metadata SHA-256
`f5961a8548256da287b4f79466a169c302b90ec59be7f6461e8e58b482c0fb0d`.

## Current status

Code/build status is complete for the bundled module installer and
session-scoped manual arm. Live status is **UNVERIFIED_DEVICE**. The available
REA bundle proves artifact identity/provenance but its current environment has
no JADX or native deep-analysis provider. Map-source, direct catch, Virtual GO
Plus, transfer and discard method contracts therefore remain `UNRESOLVED`.

The native profile checks the package and 0.433.0 GNU build ID. It deliberately
keeps mutation closed because runtime versionCode and metadata hash are not yet
independently verified. Policy switches cannot override that decision. The
runtime must report an unavailable/unsupported reason rather than invoking an
older 0.427.0 binding.

## Controller lifecycle

- The APK embeds one hash-verified Magisk ZIP and its manifest.
- First foreground open may request root once and install the module.
- Installed, reboot-required and active-runtime are distinct states.
- Reboot requires a user confirmation; no background component may prompt for
  root or reboot the target.
- Auto Catch is OFF on process start and every new runtime session. Only an
  explicit UI/overlay action can arm the exact current session.
- Disconnect, PID/session change, runtime error, STOP or service shutdown
  disarms and prevents replay on reconnect.

## Manual BlueStacks handoff

The user installs the APK through the **BlueStacks Air 1 UI**, completes the
first-open root/install flow, confirms reboot, and observes the runtime status.
No agent build gate launches or controls the emulator. If a manual run fails,
collect evidence with the retained target-guarded commands:

```bash
./scripts/collect-binding-diagnostics.sh
./scripts/logcat-full.sh
```

Do not infer capability readiness from a socket connection, START/CONFIG_SET
acknowledgement, installed ZIP, or a historical method/token. Promote a
capability only after exact REA binding evidence and live owner/lifetime/result
postconditions exist for 0.433.0.
