# REA overlay for an existing Ghidra project

This directory contains a version-scoped overlay for `rea-agents@6.4.0`. It
adds a read-only project selector while preserving REA target admission,
provider identity, Evidence generation and deterministic bundle export.

The wrapper is [`scripts/rea-ghidra-project.sh`](../../../scripts/rea-ghidra-project.sh).
It never patches the installed npm package or opens the caller-owned project
directly. Instead it creates copy-on-write snapshots of the REA package and the
`.gpr`/`.rep` pair, applies `rea-agents-6.4.0.patch`, and launches Ghidra with
`-process`, `-noanalysis` and `-readOnly`. This also avoids an exclusive lock
held by an open Ghidra GUI session.

The analysis profile records the canonical caller-owned project directory as
identity, while the launcher receives the disposable snapshot directory through
a separate environment field. Repeated runs therefore keep one stable profile
digest instead of baking the random temporary directory into Evidence IDs.

The Java bridge and Node client both advertise and enforce read-only mode.
Analysis seeds, analysis extensions, naming and annotation operations are
rejected. The bridge still verifies that the active Program executable SHA-256
matches the artifact admitted by REA.

## Pokémon GO 0.433.0

```bash
export REA_AGENTS_ROOT=/path/to/node_modules/rea-agents
export GHIDRA_INSTALL_DIR=/usr/local/opt/ghidra/libexec
export JAVA_HOME=/usr/local/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home

scripts/rea-ghidra-project.sh \
  --project reverse/artifacts/pogo-0.433.0/pogo-0.433.0.gpr \
  --program libil2cpp.so -- \
  search reverse/artifacts/pogo-0.433.0/native/libil2cpp.so TryCapture \
    --kind procedures --provider ghidra \
    --snapshot build/rea/pogo-0.433.0-ghidra-analysis.snapshot.json \
    --format json --full-output
```

Reusing the same `--snapshot` path loads retained records first and writes the
updated snapshot atomically after the query. The current snapshot contains the
repeatable combined search plus three mapped-address function dossiers; its
SHA-256 is `89e175bd7b8ae9d8907ddeb02d01466db3d328888973fce6f31182c1b816e732`.

For a long-lived Evidence session, replace the command after `--` with
`--mcp`, import the canonical bundle first, perform focused queries, and call
`export_evidence_bundle` before closing the target.

On the current macOS/Xcode combination, REA's process-ownership helper can
occasionally report `cleanup_incomplete` / `sysctl_errno_22` after Ghidra has
already exited. Treat that as a cleanup diagnostic, verify the reported PID is
gone, and do not infer that the query failed; the wrapper's project is a
disposable snapshot and the caller-owned `.gpr/.rep` remains untouched.

The overlay itself does not add IL2CPP metadata-to-native symbol recovery. The
companion `scripts/il2cpp-metadata-map.mjs` now derives a focused mapping from
the exact 0.433.0 ELF/metadata pair, and REA can inspect the mapped addresses in
the existing analyzed Program. The current Ghidra database has some coarse,
merged function boundaries, so a mapped RVA landing inside a containing
`FUN_*` dossier validates analyzed-code containment but not the exact method
boundary, ABI, value flow or runtime postcondition. Those claims remain
fail-closed until focused clone analysis and runtime verification complete.
