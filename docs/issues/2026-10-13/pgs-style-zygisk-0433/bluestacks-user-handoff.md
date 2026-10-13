# BlueStacks Air 1 user handoff

Build date: 2026-10-13

## Artifacts

| Artifact | Size | SHA-256 |
|---|---:|---|
| `app/build/outputs/apk/debug/app-debug.apk` | 6,931,934 bytes | `eb4bc5926df1ad67ce861644added742833ee074f92919ab0bc1b91651f7d42f` |
| `build/pogo-root-automation-magisk-multiabi.zip` | 3,207,708 bytes | `aa7787a1abbf927671730218f739ddde4c72a749f8960638da5de7e201c71790` |
| `build/pogo-root-automation-magisk-multiabi.manifest.json` | 787 bytes | `6beb1ccc768e28a13fd877e6d14b13e31aa16cf37f57277982f458c273eb5ce6` |

The APK contains exactly the ZIP and manifest above. The bundled ZIP hash was
checked against the manifest and the standalone artifact.

## Important limitation

This candidate is safe for installer/lifecycle validation, but gameplay
mutation is deliberately fail-closed. REA could verify the 0.433.0 artifact
chain and GNU build ID, but the current environment had no JADX/native deep
provider. Runtime verification of versionCode and metadata hash is not yet
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
