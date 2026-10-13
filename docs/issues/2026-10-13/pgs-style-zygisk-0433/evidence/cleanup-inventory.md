# Cleanup inventory

Ngày audit: 2026-10-13. Historical issue documents are evidence only and are
not production consumers. Search scope covered current source, CI, Gradle,
README and maintained architecture/readiness documents.

| Candidate | Direct consumer / side effect | Replacement | Verdict |
|---|---|---|---|
| `scripts/build-magisk.sh` | CI and local native multi-ABI build/package | Canonical entry point | `KEEP` |
| `scripts/package-magisk.sh` | Called by `build-magisk.sh`; validates/package/hash manifest | None | `KEEP` |
| `scripts/vendor-pogoenhancer-libs.sh` | Reproduces pinned third-party jars/AARs; network download with blob checks | None | `KEEP` |
| `scripts/logcat-full.sh` | User-requested failure diagnostics only | None | `KEEP` |
| `scripts/collect-binding-diagnostics.sh` | User-requested failure evidence; selects target through `lib/emulator.sh` | None | `KEEP` |
| `scripts/lib/emulator.sh` | Shared exact-target guard for retained diagnostics | None | `KEEP` |
| `scripts/push-emulator.sh` | Uploads module and bypasses APK first-open installer | Bundled APK self-install | `DELETE` |
| `scripts/install-magisk-module.sh` | Host-side module/APK install and optional reboot | Bundled APK installer + confirmed in-app reboot | `DELETE` |
| `scripts/device-smoke-test.sh` | Device lifecycle mutation/force-stop | User manual handoff | `DELETE` |
| `scripts/bluestacks-smoke-test.sh` | Calls device smoke and mutates target lifecycle | User manual handoff | `DELETE` |
| `scripts/binding-probe-test.sh` | Launches/controls game and polls device | Retained diagnostics after manual failure | `DELETE` |
| `scripts/headless-control.sh` | Sole current consumer of loopback HTTP control API | App UI/overlay + runtime bridge | `DELETE` |
| `AutomationControlServer` | Opens loopback 8765 and accepts arm/config commands | App UI/overlay intents | `DELETE` |

No non-historical production caller requires a deleted entry point. CI still
listed the old scripts only for syntax checking and is updated with the same
change. Retained diagnostic scripts keep their exact target selection and are
not part of the build or E2E gate.
