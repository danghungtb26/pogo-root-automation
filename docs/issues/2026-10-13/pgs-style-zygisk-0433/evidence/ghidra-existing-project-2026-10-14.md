# REA existing-project search — 2026-10-14

## Target identity

- Ghidra project:
  `reverse/artifacts/pogo-0.433.0/pogo-0.433.0.gpr` + `.rep`
- Project Program: `/libil2cpp.so`
- Admitted target:
  `reverse/artifacts/pogo-0.433.0/native/libil2cpp.so`
- SHA-256:
  `6b1e11619362ddd53f9dbcd7be1d4296bdec5dda27b9f742c16b8403359c25a0`
- Provider: Ghidra `12.1.3`, REA `6.4.0`, arm64 ELF

The Ghidra GUI held the caller-owned project lock. The wrapper did not stop the
GUI, remove its lock or write the project. It created an APFS copy-on-write
snapshot of the `.gpr/.rep` pair and opened only that snapshot with
`-process libil2cpp.so -noanalysis -readOnly`.

## Retained Evidence

| Evidence | Operation | Query | Result | Coverage limitation |
|---|---|---|---|---|
| `ev_88f0e196cfe698c1e70b7317cc7e1545c656a1d6dcc8a5881c36779a426a3e7a` | `search_procedures` | Combined regex for all Phase 3 map sources, `TryCapture`, storage bag methods and Virtual GO Plus | Empty | Complete only for procedure names already defined in this Program. It does not decode `global-metadata.dat` or prove managed types absent. |
| `ev_4e0f9e23b90f3d8b84bea2279c898de469ec00349e810ad6dbb04efa9121e498` | `search_strings` | Same combined regex | Empty | Complete only for Ghidra-defined string Data. Undefined/packed bytes and the separate metadata artifact are outside coverage. |
| `ev_51dac3509790854d9db72de1ce51c072e1df8f4a500c1531f1fce5d460e1a947` | `analyze_function` | mapped `MapPokemon.TryCapture` RVA `0x9e1caa4` | Contained by `FUN_09e1c6cc`, `0x9e1c6cc..0x9e1cf73` | Confirms the mapped RVA is analyzed executable code; the coarse containing function is not proof of the exact managed boundary or ABI. |
| `ev_7f6095cfb8ea32b501234264006fa046a80a0fadfcbceb4d2a27597f4a30c92c` | `analyze_function` | mapped `PokemonBagImpl.ReleasePokemon` RVA `0xa04cf68` | Contained by `FUN_0a04ce80`, `0xa04ce80..0xa04cfff` | Same boundary/ABI/value-flow limitation. |
| `ev_d768040014436d743c0dc46938613e51e0b6671cd710736d295bbc01c4c0ecb4` | `analyze_function` | mapped `ItemBagImpl.RecycleItem` RVA `0xa006bec` | Contained by `FUN_0a006ba8`, `0xa006ba8..0xa006c63` | Same boundary/ABI/value-flow limitation. |
| `ev_3e02a75f7b51b944c17e70376fdad40b8126ddae52673f4a186980ed72623018` | `update_unknown` | Native deep-analysis residual unknown | Revision 3, `investigating` | Metadata-to-RVA mapping now exists; exact boundaries, ABI, value flow, owners and postconditions remain open. |

The canonical bundle export contains 26 Evidence records, 2 unknown identities
and 4 immutable unknown revisions. The repeatable search snapshot is
`build/rea/pogo-0.433.0-ghidra-analysis.snapshot.json` (4 records after the
three mapped-address dossiers), SHA-256
`89e175bd7b8ae9d8907ddeb02d01466db3d328888973fce6f31182c1b816e732`.

## Metadata mapping

`scripts/il2cpp-metadata-map.mjs` consumed the pinned ELF and
`native/global-metadata.dat` (`f5961a...`) as one identity pair. Metadata v39,
Unity `6000.3.22f1`, CodeRegistration `0xe235508` and MetadataRegistration
`0xe6378c8` yielded 219 codegen modules, 46,636 type definitions and 294,327
method definitions. The focused Phase 3–5 regex produced 583 method rows in
`reverse/rea/pogo-0.433.0-il2cpp-mapping.json`.

The mapping is deterministic static-derived evidence. It records the mapping
rule and Cpp2IL layout reference, but deliberately does not infer parameter
types, native ABI, generic instantiations, runtime owners or action outcomes.

## Safety and verification

- Positive smoke: focused `search_procedures(TryCapture)` completed through
  REA/Ghidra without importing or re-analyzing the 243 MB ELF.
- Negative smoke: `annotate_native_function` returned
  `capability_unavailable` with reason `Existing Ghidra project mode is
  read-only.`
- The session descriptor records `read_only=true` and
  `project_ephemeral=false`; Java bridge mutation cases independently reject
  the same operations.
- Ghidra still validates `currentProgram.getExecutableSHA256()` against the
  target snapshot admitted by REA.
- On some closes, REA reported `cleanup_incomplete` with
  `sysctl_errno_22` from its macOS ownership helper even though the reported
  JVM PID had already exited. This is retained as a tooling cleanup limitation;
  it did not touch the original project and did not change the Evidence result.

## Residual blocker for Phase 3–5

The prior metadata-to-RVA gap is resolved for the focused mapping. T-303,
T-304 and T-307 still do not meet their acceptance criteria because the
existing Program merges some mapped RVAs into larger `FUN_*` bodies and no
Evidence yet establishes parameter/return types, native ABI, call paths,
owner lifetime, Promise/result or server/map/storage postconditions. Phase 4
typed collectors and Phase 5 mutation paths therefore remain fail-closed. The
next qualifying probes are focused clone analysis at the mapped RVAs followed
by read-only runtime owner/lifetime and value-flow observation.
