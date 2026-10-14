# Evidence ledger — Verified IL2CPP method resolver

Target: Pokémon GO `0.433.0`, versionCode `2026100702`, `arm64-v8a`,
Unity `6000.3.22f1`, IL2CPP metadata v39.

## Artifact identity

| Artifact | SHA-256 | Authority |
|---|---|---|
| `reverse/artifacts/pogo-0.433.0/native/libil2cpp.so` | `6b1e11619362ddd53f9dbcd7be1d4296bdec5dda27b9f742c16b8403359c25a0` | shipped artifact; `SHA256SUMS` verified |
| `reverse/artifacts/pogo-0.433.0/native/global-metadata.dat` | `f5961a8548256da287b4f79466a169c302b90ec59be7f6461e8e58b482c0fb0d` | shipped artifact; `SHA256SUMS` verified |

Focused mapping provenance:

- code registration: `0xe235508`;
- metadata registration: `0xe6378c8`;
- coverage: `complete-within-filter`, 219 images/codegen modules, 46,636 type
  definitions, 294,327 method definitions, 672 regex matches and 6 selected
  type contracts;
- limitation: mapping is static-derived; it does not observe runtime instances,
  native entry/ABI, invocation, Promise completion or postconditions. It does
  reconstruct full managed parameter/return types for covered rows.

Canonical REA bundle currently retains 33 records, 2 unknown identities and 4
unknown revisions; SHA-256 is
`5eb3c187417c0b05956a46f1fe4819b2bb8ed89247f7838de8eb8cef944aecc0`.
Thirteen records match the exact `libil2cpp.so` SHA-256. Native dossiers use
Ghidra 12.1.3, authority `shipped-artifact`, confidence `observed`.

## IL2CPP inspection API contracts

These contracts are scoped to the exact arm64 ELF above. `MethodInfo` and
`Il2CppType` remain opaque runtime objects; the accepted C++ typedefs preserve
that opacity instead of encoding the observed private field layout.

| Export | Accepted arm64 C ABI | Ownership/null contract | Evidence and limitations |
|---|---|---|---|
| `il2cpp_method_get_token` | `uint32_t (*)(const void *method)`; one pointer in `x0`, 32-bit result in `w0` | Borrowed non-null method input; no allocation. Null input behavior is not guarded and must not be exercised. | `ev_adbcb3311381368ee3c3752ed5e235f3e3943d50204b9f42f2d9699e89d8d981` observes the export at `0x4fedd28`. `ev_8314c7fe4f9abc1171f50aa31dd8ca19ab0d667be6ecab6bd73b563c27356822` observes the complete 8-byte body: `ldr w0, [x0,#0x48]; ret`. The private `MethodInfo + 0x48` layout is evidence for this build only and is not a runtime contract. |
| `il2cpp_type_get_name` | `char *(*)(const Il2CppType *type)`; one pointer in `x0`, pointer result in `x0` | Returns a newly `malloc`-allocated NUL-terminated copy. Caller owns the returned buffer and must release it exactly once with `il2cpp_free` after copying/comparing it. Input-null and allocation-failure behavior are not safely handled by the observed body, so callers require a non-null input and treat a null result as failure without dereference. | `ev_adbcb3311381368ee3c3752ed5e235f3e3943d50204b9f42f2d9699e89d8d981` observes the export at `0x4fee59c`. `ev_a9cabfffa9301a2328a37e370209213bc02bcb6e6fff0ce65e3df6d8cc65f556` observes its complete `0x4fee59c..0x4fee62f` body: allocate `length + 1` through imported `malloc`, copy `length + 1` bytes, return the allocation. Ghidra's source-level type name is decompiler-derived; allocation/copy/return flow is observed assembly/p-code. |
| `il2cpp_free` | `void (*)(void *ptr)`; pointer in `x0`, no return value | Releases memory compatible with the ELF's imported `free`; use it for the buffer returned by `il2cpp_type_get_name`. No ownership claim is made for any other IL2CPP pointer. | `ev_adbcb3311381368ee3c3752ed5e235f3e3943d50204b9f42f2d9699e89d8d981` observes the export at `0x4fec43c`. `ev_11f38e2bd7bf3271917158c50acd2181a9b1e8ed63f2904bb01e5ccb4be813ec` observes an imported high-confidence `void(void *)` signature and a 12-byte thunk to `<EXTERNAL>::free`. |

The REA CLI reported `cleanup_incomplete` after each function request because
the macOS ownership-token probe returned `sysctl_errno_22`; each Ghidra process
nevertheless exited with code 0 and was confirmed absent before the next
target. The retained partial observations above contain the completed analysis;
the cleanup diagnostic is not promoted to API evidence.

## Critical method identities

`Static identity` below means the method-definition mapping is exact within the
pinned metadata/binary pair. It does not promote parameter, return, owner,
native ABI or runtime behavior that the mapper did not reconstruct.

| Method | Static identity | Token / slot / arity | RVA / file offset | Signature status | Native coverage |
|---|---|---|---|---|---|
| `holo-game.dll` / `Niantic.Holoholo.Map.MapPokemon::TryCapture` | `CONFIRMED_STATIC` | `0x06013ab5` / 44 / 2 | `0x9e1caa4` / `0x9e18aa4` | Instance `(Niantic.Holoholo.Encounter.PokeballThrow, Holoholo.Rpc.ARPlusEncounterValuesProto) -> Niantic.Promises.IPromise<Holoholo.Rpc.CatchPokemonOutProto>`; `CONFIRMED_STATIC`. | `ev_51dac3509790854d9db72de1ce51c072e1df8f4a500c1531f1fce5d460e1a947`: candidate is an interior basic-block address inside non-thunk `FUN_09e1c6cc` (`0x9e1c6cc..0x9e1cf73`), not its entry. Direct-RVA guard `UNAVAILABLE`. |
| `holo-game.dll` / `Niantic.Holoholo.Internal.PokemonBagImpl::GetPokemon` | `CONFIRMED_STATIC` | `0x06002a39` / 12 / 1 | `0xa04bd58` / `0xa047d58` | Instance `(System.UInt64) -> Holoholo.Rpc.PokemonProto`; `CONFIRMED_STATIC`. | `ev_987116ba89aab622b3ba390bcff75a4bd7b21c9768c5b9bcd7b442bc289f2f53`: candidate is interior instruction `mov w1,#1` in non-thunk `FUN_0a04bd08` (`0xa04bd08..0xa04bfdb`), not an entry. Direct-RVA guard `UNAVAILABLE`. |
| `holo-game.dll` / `Niantic.Holoholo.Internal.PokemonBagImpl::ReleasePokemon` | `CONFIRMED_STATIC` | `0x06002a43` / 19 / 1 | `0xa04cf68` / `0xa048f68` | Instance `(Holoholo.Rpc.PokemonProto) -> Niantic.Promises.IPromise<Holoholo.Rpc.ReleasePokemonOutProto>`; `CONFIRMED_STATIC`. | `ev_7f6095cfb8ea32b501234264006fa046a80a0fadfcbceb4d2a27597f4a30c92c`: candidate is interior branch `b 0xa04cf7c` in non-thunk `FUN_0a04ce80` (`0xa04ce80..0xa04cfff`), not an entry. Direct-RVA guard `UNAVAILABLE`. |
| `holo-game.dll` / `Niantic.Holoholo.Internal.ItemBagImpl::GetItemCount` | `CONFIRMED_STATIC` | `0x0600295b` / 9 / 1 | `0x9ffe438` / `0x9ffa438` | Instance `(Holoholo.Rpc.Item) -> System.Int32`; `CONFIRMED_STATIC`. | `ev_2dfe76f05f1332e28294af7d5203bcbec8903f644cb4a35ef205e3fa4521df27`: candidate is interior `adrp` in non-thunk `FUN_09ffe404` (`0x9ffe404..0x9ffe913`), not an entry. Direct-RVA guard `UNAVAILABLE`. |
| `holo-game.dll` / `Niantic.Holoholo.Internal.ItemBagImpl::RecycleItem` | `CONFIRMED_STATIC` | `0x06002971` / 23 / 3 | `0xa006bec` / `0xa002bec` | Instance `(ItemData typeDef 2769, System.Int32, System.Collections.Generic.ISet<Holoholo.Rpc.Item>) -> Niantic.Promises.IPromise<Holoholo.Rpc.RecycleItemOutProto>`; `CONFIRMED_STATIC`. | `ev_d768040014436d743c0dc46938613e51e0b6671cd710736d295bbc01c4c0ecb4`: candidate is interior `ldr x0,[x19,#0x10]` in non-thunk `FUN_0a006ba8` (`0xa006ba8..0xa006c63`), not an entry. Direct-RVA guard `UNAVAILABLE`. |

## Managed invocation contracts

Derived artifact: `reverse/rea/pogo-0.433.0-il2cpp-mapping.json`, schema
`pogo.il2cpp-metadata-map/v2`, SHA-256
`3323117e8a2902996a6e0e60fc9d652df36f746f0e6e7cbc77dd9eff1f0bb59c`.
It is deterministically derived from the two pinned shipped artifacts and the
metadata registration `0xe6378c8`, using the v39 layouts in
`SamboyCoding/Cpp2IL@b5ad444b82267cb1e4b88b8b373c008105bdea52`. The exact
ELF is backed by the REA Evidence IDs in this ledger; the mapping remains
`derived`, not a runtime observation.

| Contract | Static result | Runtime limitation |
|---|---|---|
| `PokeballThrow` | Value type, type token `0x0200218c`. Instance data layout: `BallType: Holoholo.Rpc.Item @ +0`, `ReticleSize: System.Single @ +4`, `HitBullseye: Boolean @ +8`, `Spinning: Boolean @ +9`, `Missed: Boolean @ +10`; `NoThrowAttempted` is static. This confirms the existing 12-byte ABI payload including padding. | Value contents and valid throw policy remain action-time concerns. |
| `ItemData` used by `RecycleItem` | Exact typeDef `2769`, type token `0x02000ad0`, not the unrelated typeDef `11852` with the same simple name. Reference-type fields: `item @ 16`, `type @ 20`, `category @ 24`, `count @ 28`, booleans at `32..34`, `medicineData @ 40`, `equipable @ 48`, `itemExpirationData @ 56`. Constructor token `0x06003b79` has the same ten typed arguments in that order. | Construction ownership and acceptable values remain `UNVERIFIED_DEVICE`; field writes must not substitute for the verified constructor. |
| `ExpiringItemsCopy` | `Niantic.Holoholo.UI.Items.ItemInventoryService::get_ExpiringItemsCopy`, token `0x06012166`, slot 14, returns `System.Collections.Generic.ISet<Holoholo.Rpc.Item>`. This exactly matches `RecycleItem` parameter 2. | Service instance acquisition, freshness, collection lifetime and mutation interaction remain `UNVERIFIED_DEVICE`. |
| Catch result | `TryCapture` returns `IPromise<CatchPokemonOutProto>`. `CatchPokemonOutProto` typeDef `17323` has `Status` (typeDef `17319`) at object offset 16 and `CapturedPokemonId: UInt64` at offset 32. | Promise completion/value/error lifecycle, success enum semantics, target-owner lifetime and authoritative storage/map refresh remain `UNVERIFIED_DEVICE`. |
| Transfer result | `ReleasePokemon` returns `IPromise<ReleasePokemonOutProto>`. Result typeDef `21327` has `Status` (typeDef `21322`) at object offset 16. | Success enum semantics, Promise lifecycle and authoritative bag refresh/absence postcondition remain `UNVERIFIED_DEVICE`. |
| Discard result | `RecycleItem` returns `IPromise<RecycleItemOutProto>`. Result typeDef `21413` has `Result` (typeDef `21412`) at offset 16 and `NewCount: Int32` at offset 20. | Success enum semantics, Promise lifecycle and authoritative inventory refresh remain `UNVERIFIED_DEVICE`. |

## RVA entry/thunk verdict

Focused read-only Ghidra analysis contradicts using all five mapped RVA values
as callable entries. Each value resolves inside a larger non-thunk function and
the instruction at the candidate address is reachable from earlier instructions
in that body. None is classified as an adjustor thunk or generic-shared entry.
This does not invalidate the managed token/name rows, but it does invalidate
these candidate RVA values as an independent resolver or native call target.

- Snapshot: `build/rea/pogo-0.433.0-ghidra-analysis.snapshot.json`, SHA-256
  `81c4c7a1fea5bf846c7a58ff0a8911168e7e7c18a56b9fac3f8671b56bf4eaab`.
- Deterministic focused Evidence export:
  `build/rea/pogo-0.433.0-ghidra-focused-evidence.json`, 11 records, SHA-256
  `b2201daf2535c8c222d9274248a08b22059df16fa328f8d7b8baaf04a361390e`.
- Runtime policy: RVA validation remains diagnostic-only and reports
  `candidate_not_function_entry`; direct invocation through these values is
  forbidden. The primary resolver must use exact metadata identity.

## Evidence classification

### Observed

- Artifact hashes and ELF architecture.
- Ghidra public IL2CPP API bodies and five RVA containment dossiers cited above.

### Derived with explicit limits

- Method-definition identity, full managed signature, token/slot, RVA/file
  offset and selected field layouts derive from the pinned registration
  addresses and metadata v39 mapper. Token is managed identity; RVA is only a
  candidate native code location.

### Residual unknowns

- Runtime ownership/lifetime of `ItemData`, `ExpiringItemsCopy`, method owners
  and Promise objects; their static type identities are now confirmed.
- Promise completion/value/error semantics and success enum numeric meaning.
- Exact native entry versus adjustor/generic thunk and direct native ABI.
- Runtime owner acquisition/lifetime, main-thread postconditions, server result,
  captured storage ID and map/storage/inventory refresh authority.

These unknowns keep action capabilities unavailable or `UNVERIFIED_DEVICE`.
They must not be filled from `reverse/pogo-0.427.0/` or historical reports.

## Implementation and readiness record

- Shared resolver requires exact image/class, static/instance, method name,
  full parameters/return type, unique match and token. It returns structured
  mismatch reasons and never reads a private `MethodInfo` layout.
- `il2cpp_type_get_name` allocations are released through `il2cpp_free` using
  RAII. All five retained bindings are `MethodInfo*` invoked only through
  `il2cpp_runtime_invoke` on the existing main-thread bridge.
- RVA policy is `unavailable` for all five methods. No direct `base + RVA`
  native call exists.
- Build identity code independently observes package, versionCode, loaded ELF
  build-id and installed metadata SHA-256; this path is not yet device-verified.
- Method identity, owner generation/session, invocation and outcome gates are
  separate. Catch/transfer/discard execution and outcome gates remain false.
  Promise success without authoritative postcondition becomes indeterminate,
  not completed.
- Host verification passed six existing native CI tests, multi-ABI NDK 28.2
  build/package and Gradle `test assembleDebug --rerun-tasks`. The repository
  lacks the two protocol test files named by current instructions, so the
  aggregate verification gate remains incomplete.

## Runtime strong-identity design

Owner: `shared/runtime/probe/`; the injected runtime performs and caches the
observation. Controller fingerprint/config is expected input only and is never
accepted as proof.

| Identity field | Trusted runtime source | Exact input / failure policy |
|---|---|---|
| Package | Zygisk target process name, required to equal `com.nianticlabs.pokemongo` | Missing or different process name -> `package_mismatch`. No controller field can override it. |
| versionCode | The target process's `ActivityThread.currentApplication()` -> its own package name -> Android `PackageManager.getPackageInfo(...).getLongVersionCode()` | JNI exception, absent application/package info/method, or value mismatch -> `version_code_unavailable` / `version_code_mismatch`. The observed `long` is compared with `2026100702`; controller IPC is not consulted. |
| IL2CPP build ID | Existing in-memory ELF `PT_NOTE` observation for the mapped `libil2cpp.so` | Missing/malformed/mismatch remains `il2cpp_build_id_mismatch`. |
| metadata SHA-256 | The same target application's `AssetManager.open("bin/Data/Managed/Metadata/global-metadata.dat", ACCESS_STREAMING)` bytes | Stream exactly the uncompressed installed asset bytes through SHA-256 with a 64 KiB buffer and 64 MiB hard limit; require non-empty EOF/close and compare with `f596…fb0d`. Missing asset, JNI/read error, oversize or digest mismatch -> distinct metadata reason and fail closed. |

The observation is attempted once and cached in `ProbeContext` for the process
session. A failed observation is also cached: re-bootstrap must not turn a
transiently unverifiable build into mutation-ready state. No readiness, live
pointer or gameplay state is persisted. Reading 35,873,276 metadata bytes once
is bounded; all subsequent owner discoveries consume the immutable cached
booleans/reasons.

## REA records used

| Evidence ID | Operation | Coverage/limitation |
|---|---|---|
| `ev_4e0f9e23b90f3d8b84bea2279c898de469ec00349e810ad6dbb04efa9121e498` | `search_strings` | Exact ELF; no native dossier. |
| `ev_51dac3509790854d9db72de1ce51c072e1df8f4a500c1531f1fce5d460e1a947` | `analyze_function` | Coarse body containing `TryCapture` RVA; truncated value flow. |
| `ev_7f6095cfb8ea32b501234264006fa046a80a0fadfcbceb4d2a27597f4a30c92c` | `analyze_function` | Coarse body containing `ReleasePokemon` RVA. |
| `ev_88f0e196cfe698c1e70b7317cc7e1545c656a1d6dcc8a5881c36779a426a3e7a` | `search_procedures` | Exact ELF; bounded search, not entry proof. |
| `ev_d768040014436d743c0dc46938613e51e0b6671cd710736d295bbc01c4c0ecb4` | `analyze_function` | Coarse body containing `RecycleItem` RVA. |
| `ev_992841db9ebbddda69379224237211849d6367e06937d1421b53eacb81a8ca82` | `update_unknown` | Analyst-derived residual-unknown revision; not shipped behavior evidence. |
| `ev_adbcb3311381368ee3c3752ed5e235f3e3943d50204b9f42f2d9699e89d8d981` | `search_procedures` | Exact ELF/read-only Ghidra project; complete regex match over analyzed procedure names for the three IL2CPP exports. |
| `ev_8314c7fe4f9abc1171f50aa31dd8ca19ab0d667be6ecab6bd73b563c27356822` | `analyze_function` | Complete 8-byte `il2cpp_method_get_token` body; decompiler source types are low-confidence, register width and load are observed. |
| `ev_a9cabfffa9301a2328a37e370209213bc02bcb6e6fff0ce65e3df6d8cc65f556` | `analyze_function` | Complete 148-byte `il2cpp_type_get_name` body; malloc/copy/returned allocation observed, source-level names decompiler-derived. |
| `ev_11f38e2bd7bf3271917158c50acd2181a9b1e8ed63f2904bb01e5ccb4be813ec` | `analyze_function` | Complete 12-byte `il2cpp_free` thunk with imported `void(void *)` signature and external `free` target. |
| `ev_987116ba89aab622b3ba390bcff75a4bd7b21c9768c5b9bcd7b442bc289f2f53` | `analyze_function` | `GetPokemon` candidate resolves inside `FUN_0a04bd08`, not at its entry. |
| `ev_2dfe76f05f1332e28294af7d5203bcbec8903f644cb4a35ef205e3fa4521df27` | `analyze_function` | `GetItemCount` candidate resolves inside `FUN_09ffe404`, not at its entry. |
