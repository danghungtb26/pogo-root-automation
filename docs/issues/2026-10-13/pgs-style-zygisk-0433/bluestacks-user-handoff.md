# BlueStacks Air 1 user handoff

Build date: 2026-10-13

## Artifacts

| Artifact | Size | SHA-256 |
|---|---:|---|
| `app/build/outputs/apk/debug/app-debug.apk` | 6,963,598 bytes | `1923001ae1e1c2890ae77c30abcacef3c84e195e1cea36997416aec3c559ee0e` |
| `build/pogo-root-automation-magisk-multiabi.zip` | 3,241,145 bytes | `c8fac6605c86effdbc2ca8009a6815775b085a313acd70a2c82eb26722c55061` |
| `build/pogo-root-automation-magisk-multiabi.manifest.json` | 787 bytes | `3184ac904e1d1afe60ea061417b9b11382a13490733f08e1335821bbc473cfa6` |

The APK contains exactly the ZIP and manifest above. The bundled ZIP hash was
checked against the manifest and the standalone artifact.

These build outputs are ignored and are not carried by Git. On another machine,
rebuild them with the repository commands in `AGENTS.md`. For the reverse inputs,
download and verify the files listed in
[`reverse/artifacts/README.md`](../../../../reverse/artifacts/README.md). The
failed 30-minute Ghidra probe and its non-persistent cleanup behavior are recorded
in [`evidence/ghidra-timeout-2026-10-13.md`](evidence/ghidra-timeout-2026-10-13.md).

## Important limitation

This candidate is safe for installer/lifecycle validation, but gameplay
mutation is deliberately fail-closed. REA could verify the 0.433.0 artifact
chain and GNU build ID; JADX 0.7.1 now also verifies package/version/versionCode
and Ghidra 12.1.3 accepts the arm64 ELF. No qualifying IL2CPP metadata member or
native dossier has yet resolved the gameplay contracts. Runtime verification of
versionCode and metadata hash is not yet
implemented, so `exact_build_verified` remains false. Direct catch, Virtual GO
Plus, full-source map scan, transfer and discard are expected to report
unavailable/unsupported—not execute.

## Prerequisites

- Use only the instance named **BlueStacks Air 1**.
- Magisk and Zygisk must already be available; use a test account/device you
  control.
- Do not use the other BlueStacks instance or an Android Studio AVD.
- Stop immediately on a crash, wrong package/version, repeated root prompt, or
  any unexpected gameplay mutation.

## Manual sequence

1. Install `app-debug.apk` through the BlueStacks UI.
2. Open the controller app in the foreground. Expect at most one `su` prompt.
3. Expect the UI to show module id/version/hash and progress through
   `NOT_CHECKED` → `INSTALLING` → `REBOOT_REQUIRED`. Root denial must show
   `ROOT_REQUIRED`/error with a user Retry button and no prompt loop.
4. Confirm the in-app reboot dialog only when ready. No reboot should occur
   without that confirmation.
5. After reboot, open the controller and Pokémon GO 0.433.0. Expect installed
   module/runtime session state to become `ACTIVE`; Auto Catch must remain OFF.
6. Use the UI/overlay once to request arm for the current session. With the
   current profile, expect an unsupported/unverified capability reason and no
   catch/spin/transfer/discard mutation.
7. Close/kill Pokémon GO or otherwise end the runtime session, then relaunch.
   Expect a new session id and Auto Catch OFF without deleting saved policy.

Do not continue to full-source scan, direct/fallback catch, transfer or discard
until the blocked Phase 3 REA contracts are supplied and a new artifact is
built. The current artifact is not `LIVE_READY` for those features.

## Failure evidence

Only after the manual flow fails, use the retained repository entry points:

```bash
./scripts/collect-binding-diagnostics.sh
./scripts/logcat-full.sh
```

Send back the installer state/error code, displayed module hash, runtime session
id/capability reason, the diagnostic output, and the log path. Do not use raw
ADB commands or recreate deleted deploy/E2E/headless scripts.
