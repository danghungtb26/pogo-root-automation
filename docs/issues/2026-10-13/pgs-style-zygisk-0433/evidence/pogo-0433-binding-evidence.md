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
| Base APK `reverse/artifacts/pogo-0.433.0/base.apk` | `1c8f198cd212e7303e5a5e38e34857f4ced42eb432ac62b07807bfd286503690` | `ev_4d913f...`, `ev_42ee22...`, `ev_d5331c...` | shipped artifact, observed | Complete central-directory inventory and extraction; current REA session has no configured JADX provider. |
| arm64 split `reverse/artifacts/pogo-0.433.0/split_config.arm64_v8a.apk` | `8f4a472ca8fd59cc794f4f0b32fff3920c55211bbab955ec8b79f7aa4fe5f0a0` | `ev_6ce607...`, `ev_011ef4...`, `ev_9c80a3...` | shipped artifact, observed | Complete central-directory inventory and extraction only. |
| `native/libil2cpp.so` | `6b1e11619362ddd53f9dbcd7be1d4296bdec5dda27b9f742c16b8403359c25a0` | Child of extraction chain `ev_9c80a3...`; no deep native dossier | shipped artifact + local ELF note | SHA-256 verified by manifest. GNU build-id note is `da59ce3024c4ccfe16fa6616281a4d111de67aa2`. Ghidra/Hopper/IDA is unavailable in the current REA server. |
| `native/global-metadata.dat` | `f5961a8548256da287b4f79466a169c302b90ec59be7f6461e8e58b482c0fb0d` | Child of extraction chain `ev_d5331c...`; no metadata-member Evidence | shipped artifact, observed identity | Exact bytes are pinned; IL2CPP metadata members are not decoded by retained REA Evidence. |
| Package | `com.nianticlabs.pokemongo` | none for decoded manifest | plan input, `UNRESOLVED` in REA | `inspect_android_package` is unavailable because `REA_JADX_MCP_JAR` 0.7.1 is not configured. |
| Version | `0.433.0` / versionCode `2026100702` | none for decoded manifest | plan input, `UNRESOLVED` in REA | Must be rechecked with the exact base APK after the JADX provider is available. Runtime mutation remains fingerprint-gated. |

The canonical bundle `reverse/rea/rea-evidence-2026-10-13.json` imported with
17 retained records and 2 residual unknowns. The Android unknown is
`unk_1aaa6e...`; the native deep-analysis unknown is `unk_27690a...`.

## Map source inventory

JADX/native deep-analysis providers were unavailable in this run. The table is
therefore a complete discovery queue, not a declaration that a type exists.
Every unresolved source is excluded from runtime collection and mutation.

| Candidate source type | Offline verdict | Evidence / limitation | Runtime owner/lifetime |
|---|---|---|---|
| `WildMapPokemon` | `UNRESOLVED` | `unk_1aaa6e...`; no decoded 0.433.0 class record | `UNVERIFIED_DEVICE` |
| `IncenseMapPokemon` | `UNRESOLVED` | `unk_1aaa6e...`; no decoded 0.433.0 class record | `UNVERIFIED_DEVICE` |
| `TroyDiskMapPlacePokemon` | `UNRESOLVED` | `unk_1aaa6e...`; no decoded 0.433.0 class record | `UNVERIFIED_DEVICE` |
| `DailyEncounterMapPokemon` | `UNRESOLVED` | `unk_1aaa6e...`; no decoded 0.433.0 class record | `UNVERIFIED_DEVICE` |
| `PhotobombingMapPokemon` | `UNRESOLVED` | `unk_1aaa6e...`; no decoded 0.433.0 class record | `UNVERIFIED_DEVICE` |
| `IncidentMapPokemon` | `UNRESOLVED` | `unk_1aaa6e...`; no decoded 0.433.0 class record | `UNVERIFIED_DEVICE` |
| `PokestopSpawnPokemon` | `UNRESOLVED` | `unk_1aaa6e...`; no decoded 0.433.0 class record | `UNVERIFIED_DEVICE` |
| `RouteMapPokemon` | `UNRESOLVED` | `unk_1aaa6e...`; no decoded 0.433.0 class record | `UNVERIFIED_DEVICE` |
| `RaidMapPokemon` | `UNRESOLVED` | `unk_1aaa6e...`; no decoded 0.433.0 class record | `UNVERIFIED_DEVICE` |
| `StationMapPokemon` | `UNRESOLVED` | `unk_1aaa6e...`; no decoded 0.433.0 class record | `UNVERIFIED_DEVICE` |
| `MapEntityCell` / cell collection | `UNRESOLVED` | `unk_1aaa6e...`, `unk_27690a...`; old `eghw` layout rejected | `UNVERIFIED_DEVICE` |
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
| Direct catch owner/method/request/result | `UNRESOLVED` | Exact tuple, parameter types, Promise/result, captured storage ID, server status and postcondition |
| Source-to-live-`MapPokemon` eligibility | `UNRESOLVED` | Every source row above lacks class/reference Evidence |
| Virtual GO Plus session/connectivity | `UNRESOLVED` | Owner and connect/disconnect state |
| Virtual GO Plus nearby/select/catch | `UNRESOLVED` | Method tuple, Poké Ball item-1 rule, result/captured ID and cooldown semantics |

`DIRECT_CATCH` and the fallback capability remain `UNAVAILABLE`; no historical
`TryCapture` name or 0.427.0 layout is promoted to 0.433.0 authority.

## Storage method identity table

The rows below preserve the requested identities, slots and tokens as search
keys. They are **not** implementation addresses. Because the imported REA
bundle has no IL2CPP metadata-member record, every tuple remains `UNRESOLVED`
for binding authority even when a historical report contains the same text.

| Class | Method | Slot | Token | REA Evidence / status | Limitation |
|---|---|---:|---:|---|---|
| `Niantic.Holoholo.IPokemonBag` | `GetPokemon(id)` | 8 | `0x06001058` | none — `UNRESOLVED` | Need exact metadata signature, parameter and return type from 0.433.0. |
| `Niantic.Holoholo.Internal.PokemonBagImpl` | `GetPokemon(id)` | 12 | `0x06002a39` | none — `UNRESOLVED` | Need implementation owner and exact overload. |
| `Niantic.Holoholo.IPokemonBag` | `ReleasePokemon(pokemon)` | 15 | `0x0600105f` | none — `UNRESOLVED` | Promise/result and storage postcondition are unknown. |
| `Niantic.Holoholo.Internal.PokemonBagImpl` | `ReleasePokemon(pokemon)` | 19 | `0x06002a43` | none — `UNRESOLVED` | Owner lifetime is `UNVERIFIED_DEVICE`. |
| `Niantic.Holoholo.IItemBag` | `GetItemCount(itemType)` | 5 | `0x06000f85` | none — `UNRESOLVED` | Need exact enum parameter and return type. |
| `Niantic.Holoholo.Internal.ItemBagImpl` | `GetItemCount(itemType)` | 9 | `0x0600295b` | none — `UNRESOLVED` | Need implementation owner and exact overload. |
| `Niantic.Holoholo.IItemBag` | `RecycleItem(itemProto, numToRecycle, expiringItemsCopy)` | 19 | `0x06000f93` | none — `UNRESOLVED` | `ItemData`, `ExpiringItemsCopy`, Promise/result and refresh signal are unknown. |
| `Niantic.Holoholo.Internal.ItemBagImpl` | `RecycleItem(itemData, numToRecycle, expiringItemsCopy)` | 23 | `0x06002971` | none — `UNRESOLVED` | Owner/lifetime and runtime postcondition are `UNVERIFIED_DEVICE`. |

## Verification record

- `shasum -a 256 -c reverse/artifacts/SHA256SUMS`: all 10 entries passed.
- REA import: 17 records, 2 unknowns; summary query returned the same counts.
- `inspect_android_package(base.apk)`: capability unavailable; missing caller-supplied
  JADX MCP 0.7.1 JAR, so no Android class/method claim was promoted.
- `open_binary(libil2cpp.so)`: artifact identity/format/architecture matched
  SHA-256 and arm64. Deep provider availability check reported Ghidra not
  configured and no selected native provider, so no function/RVA claim was made.
- NDK `llvm-readelf -n`: build ID matched
  `da59ce3024c4ccfe16fa6616281a4d111de67aa2`.

