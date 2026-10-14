# Pokémon GO 0.433.0 binding evidence ledger

Ngày chốt baseline: 2026-10-13

Ledger này chỉ coi artifact và REA Evidence là authority cho binding. Các ghi chú
trong `reverse/reports/` là historical reference; token metadata không phải RVA
hay địa chỉ runtime. Quan sát owner/lifetime/postcondition cần process thật được
giữ ở trạng thái `UNVERIFIED_DEVICE`.

## Artifact identity

| Artifact/claim | SHA-256 / value | REA Evidence | Authority | Coverage và limitation |
|---|---|---|---|---|
| APKM `reverse/artifacts/pogo-0.433.0/com.nianticlabs.pokemongo_arm64-v8a_0.433.0.apkm` | `1b120bb485310777f196fdb8cb827b63887e91f83fe81dc0cdb7e67a2d518278` | `ev_1f307dab...`, `ev_bf551a9b...`, `ev_022695...` | shipped artifact, observed | Complete central-directory inventory and extraction; nested APK semantics are not decoded by these records. |
| Base APK `reverse/artifacts/pogo-0.433.0/base.apk` | `1c8f198cd212e7303e5a5e38e34857f4ced42eb432ac62b07807bfd286503690` | `ev_4d913f...`, `ev_42ee22...`, `ev_d5331c...`, `ev_59ea3043...` | shipped artifact, observed | Complete central-directory inventory/extraction plus decoded manifest/package identity from JADX 0.7.1. The DEX provider does not decode IL2CPP metadata members. |
| arm64 split `reverse/artifacts/pogo-0.433.0/split_config.arm64_v8a.apk` | `8f4a472ca8fd59cc794f4f0b32fff3920c55211bbab955ec8b79f7aa4fe5f0a0` | `ev_6ce607...`, `ev_011ef4...`, `ev_9c80a3...` | shipped artifact, observed | Complete central-directory inventory and extraction only. |
| `native/libil2cpp.so` | `6b1e11619362ddd53f9dbcd7be1d4296bdec5dda27b9f742c16b8403359c25a0` | Extraction `ev_9c80a3...`; search `ev_88f0e196...`; dossiers `ev_51dac350...`, `ev_7f6095cf...`, `ev_d7680400...` | shipped artifact + Ghidra 12.1.3, observed | SHA-256 verified by manifest; GNU build-id is `da59ce3024c4ccfe16fa6616281a4d111de67aa2`. REA opened the analyzed Program through a copy-on-write project snapshot with `-process -noanalysis -readOnly`. Three mapped RVAs land in analyzed code, but coarse containing `FUN_*` boundaries do not prove exact managed boundaries, ABI or behavior. See the [existing-project record](ghidra-existing-project-2026-10-14.md). |
| `native/global-metadata.dat` | `f5961a8548256da287b4f79466a169c302b90ec59be7f6461e8e58b482c0fb0d` | Child of extraction chain `ev_d5331c...`; derived mapping cross-checked by the three native dossiers | shipped artifact + deterministic static derivation | Metadata v39 was paired only with the pinned ELF. The focused map covers 219 images/modules, 46,636 types and 294,327 method definitions; it does not reconstruct native ABI or runtime behavior. |
| `rea/pogo-0.433.0-il2cpp-mapping.json` | `3323117e8a2902996a6e0e60fc9d652df36f746f0e6e7cbc77dd9eff1f0bb59c` | Inputs linked to `ev_d5331c...`/`ev_9c80a3...`; mapped addresses checked by `ev_51dac350...`, `ev_987116...`, `ev_7f6095...`, `ev_2dfe76...`, `ev_d76804...` | deterministic derived artifact | Schema v2: 672 focused method rows plus 6 type contracts, with full managed parameter/return types and field offsets for the selected contracts. Static identity/RVA aid only; exact native entry, runtime owner/lifetime and outcomes are outside coverage. |
| `build/rea/pogo-0.433.0-ghidra-analysis.snapshot.json` | `81c4c7a1fea5bf846c7a58ff0a8911168e7e7c18a56b9fac3f8671b56bf4eaab` | Snapshot of the read-only existing-project run | deterministic analysis snapshot | Records exact project/target/profile state; it is not a callable binding. |
| `build/rea/pogo-0.433.0-ghidra-focused-evidence.json` | `b2201daf2535c8c222d9274248a08b22059df16fa328f8d7b8baaf04a361390e` | 11 focused records, 7 new after canonical merge | deterministic REA export | Covers public IL2CPP API dossiers and the five focused method addresses. Four records duplicate canonical observations. |
| `rea/rea-evidence-2026-10-13.json` | `5eb3c187417c0b05956a46f1fe4819b2bb8ed89247f7838de8eb8cef944aecc0` | Canonical deterministic bundle | REA retained Evidence | 33 records, 2 residual-unknown identities and 4 immutable unknown revisions after importing the focused bundle. |
| Package | `com.nianticlabs.pokemongo` | `ev_59ea3043...` | shipped artifact, observed | Decoded from `AndroidManifest.xml` of the pinned base APK by JADX 0.7.1. |
| Version | `0.433.0` / versionCode `2026100702` | `ev_59ea3043...` | shipped artifact, observed | Decoded from the pinned base APK. Runtime now independently observes versionCode and installed metadata SHA-256, but the device observation itself remains `UNVERIFIED_DEVICE`. |

The canonical bundle `reverse/rea/rea-evidence-2026-10-13.json` now contains
33 retained records, 2 residual-unknown identities and 4 immutable unknown
revision records. The Android/IL2CPP unknown is `unk_1aaa6e...`; native unknown
`unk_27690a...` is revision 3/`investigating`, supported by the search and three
mapped-address dossiers. Provider availability and token-to-RVA mapping are no
longer blockers; exact boundary/ABI/value-flow/runtime semantics remain open.

## Map source inventory

JADX 0.7.1 verified the APK/manifest identity but cannot decode IL2CPP metadata
members. The paired metadata map now establishes that every candidate source
name below exists in 0.433.0 and supplies focused method RVAs. The original
empty REA name searches (`ev_88f0e196...`, `ev_4e0f9e23...`) only describe the
unlabeled Ghidra database and are not absence evidence. Runtime collection still
excludes a source until its owner, lifecycle, field/signature and snapshot
semantics receive qualifying REA plus device evidence.

| Candidate source type | Offline verdict | Evidence / limitation | Runtime owner/lifetime |
|---|---|---|---|
| `WildMapPokemon` | `PRESENT_METADATA`; runtime `UNRESOLVED` | Focused v39 mapping contains the exact type and method rows; fields/owner/event semantics not decoded | `UNVERIFIED_DEVICE` |
| `IncenseMapPokemon` | `PRESENT_METADATA`; runtime `UNRESOLVED` | Same mapping coverage | `UNVERIFIED_DEVICE` |
| `TroyDiskMapPlacePokemon` | `PRESENT_METADATA`; runtime `UNRESOLVED` | Same mapping coverage | `UNVERIFIED_DEVICE` |
| `DailyEncounterMapPokemon` | `PRESENT_METADATA`; runtime `UNRESOLVED` | Same mapping coverage | `UNVERIFIED_DEVICE` |
| `PhotobombingMapPokemon` | `PRESENT_METADATA`; runtime `UNRESOLVED` | Same mapping coverage | `UNVERIFIED_DEVICE` |
| `IncidentMapPokemon` | `PRESENT_METADATA`; runtime `UNRESOLVED` | Same mapping coverage | `UNVERIFIED_DEVICE` |
| `PokestopSpawnPokemon` | `PRESENT_METADATA`; runtime `UNRESOLVED` | Same mapping coverage | `UNVERIFIED_DEVICE` |
| `RouteMapPokemon` | `PRESENT_METADATA`; runtime `UNRESOLVED` | Same mapping coverage | `UNVERIFIED_DEVICE` |
| `RaidMapPokemon` | `PRESENT_METADATA`; runtime `UNRESOLVED` | Same mapping coverage | `UNVERIFIED_DEVICE` |
| `StationMapPokemon` | `PRESENT_METADATA`; runtime `UNRESOLVED` | Same mapping coverage | `UNVERIFIED_DEVICE` |
| `MapEntityCell` / cell collection | `PRESENT_METADATA`; runtime `UNRESOLVED` | `get_AllCellWildPokemon` RVA `0x9dd35e4`, `GetMapPokemon` RVA `0x9dd4b54`; old `eghw` layout still rejected | `UNVERIFIED_DEVICE` |
| Fort entity/update/cooldown | `UNRESOLVED` | no 0.433.0 class/method Evidence | `UNVERIFIED_DEVICE` |
| Player-location service | `UNRESOLVED` | no 0.433.0 class/method Evidence | `UNVERIFIED_DEVICE` |

No candidate has a proven collection owner, create/update/remove signal, stable
identity, expiry accessor, scene generation or complete-snapshot semantics.
Consequently the only honest scan verdict is `UNRESOLVED`; the runtime must not
advertise `READY_COMPLETE`, `READY_PARTIAL` or live map capabilities from this
ledger.

## Catch and Virtual GO Plus contracts

| Contract | Verdict | Missing evidence |
|---|---|---|
| Direct catch owner/method/request/result | `STRONG_STATIC_IDENTITY`; runtime incomplete | `MapPokemon.TryCapture(PokeballThrow, ARPlusEncounterValuesProto) -> IPromise<CatchPokemonOutProto>`, token `0x06013ab5`, slot 44, RVA `0x9e1caa4`; schema-v2 type contracts include the `PokeballThrow` value layout and `CatchPokemonOutProto.Status`/`CapturedPokemonId`. Dossier `ev_51dac350...` proves only coarse containment. Live owner/lifetime, concrete Promise behavior, authoritative result correlation and postcondition remain `UNVERIFIED_DEVICE`. |
| Source-to-live-`MapPokemon` eligibility | `UNRESOLVED` | Every source row above lacks class/reference Evidence |
| Virtual GO Plus session/connectivity | `UNRESOLVED` | Owner and connect/disconnect state |
| Virtual GO Plus nearby/select/catch | `UNRESOLVED` | Method tuple, Poké Ball item-1 rule, result/captured ID and cooldown semantics |

`DIRECT_CATCH` and the fallback capability remain `UNAVAILABLE`. The retained
REA queries `ev_88f0e196...`/`ev_4e0f9e23...` found no matching analyzed
procedure/defined string, but their coverage cannot resolve a managed tuple or
prove absence. No historical `TryCapture` name or 0.427.0 layout is promoted to
0.433.0 authority.

## Storage method identity table

The exact metadata map now distinguishes abstract interface declarations from
implementation rows and provides full managed signatures plus implementation
RVAs. These are strong static identities, but not live-ready contracts: native
entry boundaries, owner lifetime, concrete Promise behavior and authoritative
storage/inventory postconditions remain unresolved.

| Class | Method | Slot | Token | REA Evidence / status | Limitation |
|---|---|---:|---:|---|---|
| `Niantic.Holoholo.IPokemonBag` | `GetPokemon(UInt64) -> PokemonProto` | 8 | `0x06001058` | abstract/interface pointer `0x0` — `STRONG_STATIC_IDENTITY` | Interface identity only; live implementation owner remains unknown. |
| `Niantic.Holoholo.Internal.PokemonBagImpl` | `GetPokemon(UInt64) -> PokemonProto` | 12 | `0x06002a39` | RVA `0xa04bd58`, dossier `ev_987116...` — `STRONG_STATIC_IDENTITY` | Coarse containing function; live owner/lifetime is `UNVERIFIED_DEVICE`. |
| `Niantic.Holoholo.IPokemonBag` | `ReleasePokemon(PokemonProto) -> IPromise<ReleasePokemonOutProto>` | 15 | `0x0600105f` | abstract/interface pointer `0x0` — `STRONG_STATIC_IDENTITY` | Concrete Promise behavior and storage postcondition remain unknown. |
| `Niantic.Holoholo.Internal.PokemonBagImpl` | `ReleasePokemon(PokemonProto) -> IPromise<ReleasePokemonOutProto>` | 19 | `0x06002a43` | RVA `0xa04cf68`, dossier `ev_7f6095cf...` — `STRONG_STATIC_IDENTITY` | Result fields are mapped; containing function is coarse and owner/postcondition are `UNVERIFIED_DEVICE`. |
| `Niantic.Holoholo.IItemBag` | `GetItemCount(Item) -> Int32` | 5 | `0x06000f85` | abstract/interface pointer `0x0` — `STRONG_STATIC_IDENTITY` | Interface identity only; live implementation owner remains unknown. |
| `Niantic.Holoholo.Internal.ItemBagImpl` | `GetItemCount(Item) -> Int32` | 9 | `0x0600295b` | RVA `0x9ffe438`, dossier `ev_2dfe76...` — `STRONG_STATIC_IDENTITY` | Coarse containing function; live owner/lifetime is `UNVERIFIED_DEVICE`. |
| `Niantic.Holoholo.IItemBag` | `RecycleItem(ItemData, Int32, ISet<Item>) -> IPromise<RecycleItemOutProto>` | 19 | `0x06000f93` | abstract/interface pointer `0x0` — `STRONG_STATIC_IDENTITY` | `ItemData` and result are mapped; runtime owner/refresh remain unknown. |
| `Niantic.Holoholo.Internal.ItemBagImpl` | `RecycleItem(ItemData, Int32, ISet<Item>) -> IPromise<RecycleItemOutProto>` | 23 | `0x06002971` | RVA `0xa006bec`, dossier `ev_d7680400...` — `STRONG_STATIC_IDENTITY` | Exact managed signature only; native entry, owner and postcondition are `UNVERIFIED_DEVICE`. |

## Verification record

- `shasum -a 256 -c reverse/artifacts/SHA256SUMS`: all 10 entries passed.
- REA deterministic merge/export after the focused API/address dossiers
  produced 33 records, 2 unknown identities and 4 immutable revision records;
  canonical bundle SHA-256 is `5eb3c187...`.
- `inspect_android_package(base.apk)`: JADX 0.7.1 returned package
  `com.nianticlabs.pokemongo`, version `0.433.0`, versionCode `2026100702` in
  Evidence `ev_59ea3043...`; it did not decode IL2CPP metadata members.
- Historical `open_binary(libil2cpp.so)` ephemeral-import attempts timed out at
  300 and 1,800 seconds; see the [timeout note](ghidra-timeout-2026-10-13.md).
- The 2026-10-14 REA overlay opened a copy-on-write snapshot of the existing
  Ghidra project with `-process -noanalysis -readOnly`, verified the same target
  SHA-256 and returned retained Evidence `ev_88f0e196...` and
  `ev_4e0f9e23...`. Both result sets are empty. The bridge and provider reject
  annotation/naming; the original locked project was never opened or changed.
  See the [existing-project record](ghidra-existing-project-2026-10-14.md).
- `scripts/il2cpp-metadata-map.mjs` parsed metadata v39 and the ELF relocation/
  codegen-module tables using explicit registration addresses. Schema v2
  regeneration is byte-identical to the committed focused map; its SHA-256 is
  `3323117e...`, with 672 methods and 6 type contracts.
- REA `inspect_managed_artifact` Evidence `ev_66a9d9de...` records the generated
  metadata-only `holo-game.dll` as an auxiliary derivation. It is not shipped
  target evidence and is not used to authorize a binding or capability.
- REA `analyze_function` checked all five focused mapped RVAs in the existing
  Program. All land in analyzed code; merged Ghidra boundaries keep exact
  native entry/thunk, direct-call ABI and call-path work open.
- REA dossiers `ev_8314c7fe...`, `ev_a9cabfff...` and `ev_11f38e2b...`
  corroborate `il2cpp_method_get_token`, allocated `il2cpp_type_get_name` and
  `il2cpp_free`; implementation uses public APIs plus RAII ownership.
- NDK `llvm-readelf -n`: build ID matched
  `da59ce3024c4ccfe16fa6616281a4d111de67aa2`.
- Host implementation verification: six existing native CI tests pass;
  arm64-v8a/x86_64 Magisk build with installed NDK 28.2 passes; Gradle
  `test assembleDebug --rerun-tasks` passes. Two protocol test paths named by
  the current repository instructions do not exist, so the aggregate build
  gate remains incomplete rather than being replaced with new tests.

## Runtime implementation readiness

- **Static identity:** implemented. A shared descriptor resolver requires exact
  image/class/name, static/instance bit, full parameters/return type, unique
  match and token for all five mutation methods.
- **RVA:** evidence-only. Every profile entry uses `kUnavailable`; no
  `base + RVA` native call or internal `MethodInfo` layout is used because the
  current Ghidra dossiers expose only coarse containment.
- **Build identity:** code-complete. Runtime independently reads package,
  versionCode, loaded ELF build-id and installed metadata SHA-256. Device
  observation remains `UNVERIFIED_DEVICE`.
- **Mutation readiness:** not live-ready. Method identity, owner session,
  invocation and outcome gates are separate; catch/transfer/discard execution
  and outcome gates remain false. A Promise success without authoritative
  map/storage/inventory postcondition is reported indeterminate, not completed.

