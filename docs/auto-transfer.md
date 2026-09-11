# Auto Transfer — thiết kế & provenance

Tài liệu mô tả feature **auto transfer** (tự động release/"transfer" Pokémon) sau đợt
implement trên branch `refactor/independent-runtime-control`: luồng dữ liệu, các file/
hàm đã dùng, và **nguồn gốc** của từng il2cpp method (lấy từ dump thật hay từ repo, cái
nào từng đoán và đã sửa).

## 1. Mô hình: một feature, nhiều nguồn feed

Chỉ có **một** loại transfer: `policy.autoTransfer` + `TransferPolicy` +
`TransferPlanner`. "Quét box" và "event sau catch" không phải hai feature — chúng là hai
**nguồn** cùng đổ dữ liệu vào `AutomationSnapshot.storage`, rồi cùng đi qua một quyết
định policy và một đường dispatch.

- **Nguồn event-sau-catch (wild)** — đã implement. Con wild vừa bắt được đưa vào
  `snapshot.storage` để planner cân nhắc release.
- **Nguồn quét-box** — tương lai (Phase 3). Chỉ cần một reader storage khác đổ vào cùng
  `snapshot.storage`; planner đã sẵn sàng.

Ranh giới an toàn: luồng direct-map trycatch (đang bật) **không có IV/shiny** → template
`iv=null` → với `TransferPolicy.keepUnknownIv=true` (mặc định) sẽ **được giữ lại**. Chỉ
encounter-catch (có IV) mới bị release theo ngưỡng IV.

## 2. Luồng dữ liệu end-to-end

```
[NATIVE target proc]                         [companion appproc]          [Kotlin app]
catch (TryCapture/Throw)
  └─ execute_runtime_catch ─ RuntimeActionResult{catch_outcome_wire, captured_pokemon_id}
        └─ send_runtime_result ───► parse_runtime_result ──► send_command_result ──► BridgePayloadDecoder
                                                                                       │ AutomationCommandResult
                                                                                       ▼
                                     StructuredAutomationController.consumeResult
                                       CAUGHT + capturedPokemonId
                                         └─ pendingWildTransfers[id] = StoredPokemon(from encounter)
                                     readSnapshot(): mergePendingWildTransfers → snapshot.storage
                                         └─ AutomationCoordinator.plan(): if autoTransfer →
                                              TransferPlanner.plan(storage, policy) → TransferPokemon(id)
                                         └─ AutomationRunner dispatch (1 mutation/lần, cap TRANSFER_POKEMON, OVERWORLD)
        ◄── BridgeActionCodec tag 6 (pokemonId) ──────────────────────────────────────┘
  parse_runtime_transfer_command → execute_runtime_transfer
    └─ GetPokemon(id) → ReleasePokemon(proto)
        └─ result COMPLETED ──► ... ──► consumeResult:
                                          resolveWildTransferResult → publish TRANSFERRED (+Toast)
                                          drop khỏi pendingWildTransfers
```

## 3. Phase 2 — quyết định wild-only (Kotlin, đã test-compile)

Không thêm policy mới; tái dùng `TransferPolicy`/`TransferPlanner.shouldTransfer` sẵn có.

| File | Thay đổi |
|------|----------|
| `app/.../headless/StructuredAutomationController.kt` | `WildCatchTemplate` + `caughtWildTemplateByCommand` + `pendingWildTransfers`; `rememberWildCatchTemplate()` (stash lúc dispatch từ `snapshot.encounter` hoặc `snapshot.nearby`); `registerWildTransferIfCaught()` (CAUGHT + `capturedPokemonId` → `StoredPokemon`); `mergePendingWildTransfers()` (đổ vào `snapshot.storage`, dedup theo id); `resolveWildTransferResult()` (drop + publish `TRANSFERRED`); nới lọc log `missing capability`. Cap 64, clear khi reconnect. |
| `bridge/protocol/.../BridgeProtocol.kt` | `AutomationCommandResult.capturedPokemonId: String?` |
| `bridge/protocol/.../BridgePayloadEncoder.kt` / `BridgePayloadDecoder.kt` | wire optional cho `capturedPokemonId` (scheme `marker=hasThrowOrSnapshot` + trailing optional, backward-compatible) |

Không cần sửa config: `AutomationConfig.autoTransfer` + `AutomationPolicyBridge` đã map sẵn
sang `AutomationPolicy.autoTransfer`/`TransferPolicy`.

**Thông báo sau transfer:** `AutomationEventType.TRANSFERRED` (trước đây khai báo nhưng
chưa ai publish) giờ được bắn khi transfer `COMPLETED`; `ToastAutomationEventSink` hiện
Toast nếu `showActionToasts` bật.

## 4. Phase 0 — native transfer executor (scaffold, gate `false`)

Theo đúng pattern discard/catch (disabled-until-verified-on-device).

| File | Thay đổi |
|------|----------|
| `zygisk/jni/shared/core/runtime_native_prelude.inc` | `RuntimeBinding`: `pokemon_inventory`, `pokemon_inventory_get_pokemon`, `pokemon_inventory_release`, `transfer_verified` |
| `zygisk/jni/shared/runtime/probe/runtime_probe_discovery.inc` | Block resolve `IPokemonBag` → `GetPokemon(1)` + `ReleasePokemon(1)` trên `PokemonBagImpl`, set `transfer_verified`, log diagnostic |
| `zygisk/jni/modules/transfer/parse.inc` (mới) | `parse_runtime_transfer_command` (tag 6 + pokemonId) |
| `zygisk/jni/modules/transfer/execute.inc` (mới) | `kTransferExecutionEnabled=false`; `release_runtime_pokemon` (`id→GetPokemon→ReleasePokemon`); `execute_runtime_transfer` (guards + phase) |
| `zygisk/jni/modules/transfer/module.inc` | `available()` gate trên `exact_build_verified && kTransferExecutionEnabled && transfer_verified` |
| `zygisk/jni/shared/runtime/control/runtime_control.inc` | dispatch case transfer + forward `catch_outcome_wire`/`captured_pokemon_id` cho catch |
| `zygisk/jni/host/runtime_capabilities.inc` | advertise `TRANSFER_POKEMON` |
| `zygisk/jni/main.cpp` | include `transfer/parse.inc`, `transfer/execute.inc` |

## 5. Phase 1 — outcome + captured id (plumbing đầy đủ, production gated)

`captured_pokemon_id` được thread qua toàn bộ chuỗi result, khớp wire hai đầu:

`RuntimeActionResult.captured_pokemon_id` → `send_runtime_result` (target→companion) →
`parse_runtime_result` → `runtime_bridge_broker.inc` → `send_command_result`
(companion→Kotlin) → `BridgePayloadDecoder`.

`catch_outcome_wire` vốn đã plumbed sẵn toàn chain; dispatch giờ mới forward nó. Hôm nay
`execute_runtime_catch` vẫn trả rỗng nên hành vi **không đổi** cho tới khi device work điền.

## 6. Provenance — il2cpp names lấy ở đâu

Dump tham chiếu: `reverse/pogo-0.427.0/dump.cs.gz` (build 0.427.0, giải nén `gunzip -c`).

| Symbol | Nguồn | Vị trí |
|--------|-------|--------|
| `IPokemonBag` / `PokemonBagImpl` | **Repo** (bảng `services[]`) + `docs/LIVE_AUTOMATION_READINESS.md:61` | `runtime_probe_discovery.inc:264` |
| Namespace `Niantic.Holoholo.Internal` (của `PokemonBagImpl`) | **Dump** | `dump.cs` class `PokemonBagImpl` (`// Namespace: Niantic.Holoholo.Internal`, ~dòng 69138) |
| `PokemonProto GetPokemon(ulong id)` | **Dump** | interface `dump.cs:27957`, impl `:69228` |
| `IPromise<ReleasePokemonOutProto> ReleasePokemon(PokemonProto pokemon)` | **Dump** | interface `dump.cs:27978`, impl `:69260` |
| `PokemonProto.Id` (ulong) = storage id | **Dump** | `dump.cs:598078` (`public ulong Id`), field `id_` `:597867` |
| `IItemBag.RecycleItem` / `ItemData` (tham chiếu discard) | Repo (có sẵn) | `runtime_probe_discovery.inc:350-354` |
| `SoftSfidaCaptureOutProto`, `SoftSfidaCaptureRpc` (catch outcome — chưa dùng) | Repo (có sẵn, scaffold dormant) | `modules/catch_spin/direct_map_bindings.inc` |

### Đính chính (đã sửa)

Bản đầu t **đoán** binding release và **sai**, đã sửa theo dump:

| Chi tiết | Bản đoán (sai) | Dump (đúng) |
|----------|----------------|-------------|
| Param của `ReleasePokemon` | `ulong id` | `PokemonProto pokemon` (object) |
| Cách lấy target | truyền thẳng id | `GetPokemon(ulong id)` → `PokemonProto` rồi mới release |
| Namespace impl | `Niantic.Holoholo` | `Niantic.Holoholo.Internal` |
| Tên method / arity | `ReleasePokemon` / 1 | đúng (trùng dump) |

## 7. Il2CppApi runtime functions đã dùng (native)

Từ struct `Il2CppApi` có sẵn — không phải app method của game:
`class_get_method_from_name` (resolve `GetPokemon`/`ReleasePokemon`), `runtime_invoke`
(gọi 2 method trên), `object_get_class`, `is_probable_managed_pointer` (validate proto),
`runtime_binding_snapshot`, `parse_decimal_u64` (pokemonId string → `ulong`).

## 8. Còn lại — chỉ device RE làm được

1. **Flip `kTransferExecutionEnabled = true`** (`modules/transfer/execute.inc`) sau khi
   xác minh post-condition của `IPromise<ReleasePokemonOutProto>` (release chạy thật,
   storage refresh) trên device. Binding shape đã khớp dump 0.427.0.
2. **Điền `catch_outcome_wire` + `captured_pokemon_id`** trong luồng catch: đọc
   `SoftSfidaCaptureOutProto` (`soft_sfida_outcome_observer_verified=false`; struct
   `PendingDirectCatch` đã có sẵn `catch_outcome_wire`). Không có id này thì
   `registerWildTransferIfCaught` skip → chưa transfer gì.

Khi hai việc trên xong, toàn bộ pipeline (Phase 2 đã sẵn sàng) tự sáng lên.

### Late binding & re-probe an toàn

`*_verified` được discovery set **một lần** lúc START rồi đóng băng (re-discover bị từ
chối khi có module đang observe, vì observer cũng gọi il2cpp trên thread khác). Nếu một
service (vd `PokemonBagImpl`) chưa có trong Zenject container tại thời điểm START thì
`transfer_verified=false` và **không tự sửa** giữa session.

Cơ chế bù (chỉ **thủ công**, không auto — để START không tự chạy diagnostic gây pause):

- **Native** (`runtime_control.inc`, `run_runtime_control_diagnostic`): action `DIAGNOSTIC`
  cho **re-run discovery khi `!any_enabled()`** (observer đã dừng → không tranh chấp),
  rồi `send_runtime_capability_update` để advertise lại. Khi đang có module observe thì
  vẫn giữ binding đóng băng (trả "binding stays frozen"). Chỉ chạy khi có lệnh `DIAGNOSTIC`.
- **Client**: `DIAGNOSTIC` chỉ được kích qua API thủ công
  (`RuntimeLifecycleCoordinator.runDiagnostic` → `AutomationControlServer`). **Không** có
  auto re-probe trong `syncModules`/`ensureRunning`: module vẫn auto-attach bình thường,
  nhưng START/sync không tự bắn diagnostic.

Vì sao không auto: `run_managed_runtime_diagnostic` gọi nhiều managed reflection + Zenject
`Resolve` từ thread injected, contend GC/domain lock với main thread → game có thể khựng.
Nên diagnostic chỉ chạy khi người dùng chủ động gọi.

Giới hạn cố hữu: re-probe chỉ chạy khi **chưa có module nào observe**. Nếu binding cần
verify muộn *trong khi* các module khác đang chạy, phải **STOP → START** (đã hỗ trợ qua
`ensureIdle()`+`ensureRunning()`) để probe lại toàn bộ.

## 9. Trạng thái verify

- Native aarch64: `clang++ --target=aarch64-linux-android24 -std=c++17 -Wall -Wextra
  -Werror -fsyntax-only` → **exit 0**. (armv7 fail là pre-existing; module chỉ hỗ trợ
  arm64.)
- Kotlin: `:app:compileDebugKotlin`, `:core:compileKotlin`, `:bridge:protocol:compileKotlin`
  → **exit 0**. (Repo không có test sources.)
