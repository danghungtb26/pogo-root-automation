# Brainstorm: Hoàn thiện auto-catch qua `TryCapture` (route duy nhất)

**Loại:** feature / refactor — điều tra & brainstorm, chưa triển khai.
**Ngày:** 2026-09-11.
**Phạm vi lượt này:** đọc source repo + reverse dump + **disassemble `libil2cpp.so`** để làm rõ
`MapPokemon.TryCapture` và dữ liệu `PokeballThrow` cần gì. Không gọi mutation trên thiết bị,
không viết UT.
**Quyết định phạm vi:** **KHÔNG dùng `SoftSfidaCaptureRpc`** (route bắt ngầm của Niantic) —
bị loại theo yêu cầu. `TryCapture` là route bắt duy nhất được xét ở đây.
**Tham chiếu:** [../../2026-09-09/background-auto-catch/brainstorm.md](../../2026-09-09/background-auto-catch/brainstorm.md).

---

## 0. TL;DR

- `MapPokemon.TryCapture(PokeballThrow, ARPlusEncounterValuesProto)` (RVA `0x7F91ECC`) **bên trong
  gửi RPC `CatchPokemon` (method 103)** với timeout 5s. Đã đọc mã máy xác nhận.
- **Throw data giờ ép Excellent + curve + bullseye** (đã áp dụng): `ball=1, reticle=0.05,
  hit=true, spin=true, missed=false`. Công thức server `serverReticle = 2.0 − ReticleSize` →
  1.95 = chắc chắn Excellent; `HitPokemon = !Missed = true`. Marshalling native đã đúng, không
  phải nguyên nhân lỗi. Xem §3.
- Việc còn thiếu để route chạy end-to-end (theo thứ tự ưu tiên):
  1. **G5** — nearby owner null → không có target (chặn tất cả).
  2. **G1** — không quan sát được kết quả `IPromise<CatchPokemonOutProto>` → luôn `INDETERMINATE`.
  3. **G3** — server có chấp nhận `CatchPokemon(103)` khi wild **chưa** `Encounter(102)` không.
  4. **G2** — guard thiếu dependency (`xpAwardService`@0xE0, `codeGateService`@0xC8) → nguy cơ crash.
- **Verdict:** `TryCapture` **sửa được và là con đường đúng**. Trọng tâm là G1 (observer) + G3 (encounter
  handshake) + G5 (nearby), không phải sửa `PokeballThrow`.

---

## 1. Code auto-catch hiện tại làm gì

Route **DIRECT_MAP** (`autoEncounter=false, autoCatch=true, catchAll=true`):

```
AutomationCoordinator → CatchMode.DIRECT_MAP → AutomationRunner → bridge command
  → native request_main_thread_direct_catch(encounter_id)
  → direct_catch_runtime_on_main_thread()  (catch_spin/direct_map_actions.inc)
       find_active_map_pokemon_by_id() → exact-class WildMapPokemon
       đọc dep @0x38/0x40/0x68/0xE8 (sanity)  ; get_SpawnPointId ; gate đếm Poké Ball
       dựng DirectMapPokeballThrow{ball=1, reticle=0, hit=0, spin=0, missed=0}
       runtime_invoke(TryCapture, map_pokemon, {&throw_data, nullptr})
       → chỉ check promise null/non-null → kInvoked | kIndeterminate
```

## 2. `.so` nói gì về `TryCapture` (RVA `0x7F91ECC`)

Disassemble bằng `llvm-objdump` (NDK 21.4), section `il2cpp`, SHA khớp reverse.

**Prologue — cách nhận `PokeballThrow`:**
```
sub sp,#96 ; mov x19,x0(this) ; mov x25,x1 ; mov x24,x2 ; mov x23,x3
str x1,[sp] ; str w24,[sp,#8]        ← dựng lại struct 12 byte trên stack
```
→ `PokeballThrow` (value-type 12 byte) truyền qua **x1 = byte 0–7** (`BallType`+`ReticleSize`),
**w2 = byte 8–11** (`HitBullseye`,`Spinning`,`Missed`). `x3` = `ARPlusEncounterValuesProto` (ref, null-able).

**Dựng `CatchPokemonProto` từ throw + this:**
| Địa chỉ | Thao tác | Ý nghĩa |
|---|---|---|
| `0x7F92078` | vcall `[vtable+776]` → lưu `[proto+16]` | `EncounterId` ← `this.EncounterId` |
| `0x7F92098` | vcall `[vtable+792]` → lưu `[proto+40]` | `SpawnPointGuid` ← `this.SpawnPointId` |
| `0x7F920CC` | `mov x0,sp; bl 0x…` | `get_ServerReticleSize(PokeballThrow*)` |
| `0x7F920E0` | `fcvt d0,s0` → `stur d0,[..,#-40]` | `NormalizedReticleSize` (double) |
| `0x7F920F0` | `bic w8, #1, (Missed) → sturb [..,#-24]` | **`HitPokemon = !Missed`** |
| `0x7F92118` | `stur q1,[..,#-16]` (16B = 2 double) | `SpinModifier` + `NormalizedHitPosition` |
| `0x7F92230`, `0x7F922B4` | `mov w1,#103` | RPC id 103 = `Method.CatchPokemon` (2 nhánh) |
| `0x7F922F8` | `fmov s0,#5.0` | timeout 5 giây |
| `0x7F9221C` | `ldr x0,[x19,#56]` | đọc `rpcHandler`@0x38 để `Send` |

## 3. `PokeballThrow` — cần cụ thể là gì

Struct (dump.cs:305529) và ánh xạ sang proto gửi lên server:

Struct `PokeballThrow` (dump.cs:305529). Server đọc grade theo công thức đã disassemble:
`serverReticle = 2.0 − ReticleSize` (`get_ServerReticleSize` @`0x89F42D4`);
`GetGrade` (@`0x89F0DA0`): `serverReticle ≥ ExcellentThrowThreshold (~1.7)` → Excellent,
`≥ GreatThrowThreshold` → Great, `≥ NiceThrowThreshold` → Nice, `Missed` → Miss.
**Cao hơn = tốt hơn → `ReticleSize` càng NHỎ càng tốt** (0 → serverReticle 2.0 = kịch trần).

| Field | Type | Offset | Vào `CatchPokemonProto` | Ý nghĩa | Excellent throw |
|---|---|---|---|---|---|
| `BallType` | `Item` (enum int32) | 0x0 | `Pokeball` @0x18 | loại bóng tiêu thụ. `Item.PokeBall = 1` | `1` |
| `ReticleSize` | `float` | 0x4 | `NormalizedReticleSize` @0x20 = `2.0 − ReticleSize` | reticle chuẩn hoá → grade | `0.05` (→ 1.95, chắc chắn Excellent) |
| `HitBullseye` | `bool` | 0x8 | `NormalizedHitPosition` @0x40 = `HitBullseye?1:0` | trúng tâm vòng | `true` (→ 1.0) |
| `Spinning` | `bool` | 0x9 | `SpinModifier` @0x38 = `Spinning?1:0` | curveball | `true` (→ 1.0) |
| `Missed` | `bool` | 0xA | `HitPokemon` @0x30 = `!Missed` | ném trượt hẳn (phí bóng) | `false` (→ HitPokemon=true) |

**Kết luận về throw data:**
- Bắt được ở tỉ lệ cơ bản chỉ cần `BallType` hợp lệ + `Missed=false` (→ `HitPokemon=true`).
- Để **luôn Excellent + curve + bullseye** (yêu cầu hiện tại), set:
  `ReticleSize=0.05` (serverReticle 1.95, không dùng 2.0 kịch trần cho tự nhiên),
  `HitBullseye=true`, `Spinning=true`, `Missed=false`, `BallType=1`.
  **Đã áp dụng** trong `catch_spin/direct_map_actions.inc` (khối `DirectMapPokeballThrow throw_data`).
- **Đính chính:** `ReticleSize=0` KHÔNG phải "không bonus" — nó cho serverReticle=2.0 = Excellent
  tuyệt đối. (Ghi chú cũ ở đây sai; công thức `2.0 − ReticleSize` mới là đúng.)
- **Không cần** tự dựng `CatchPokemonProto`, không cần `ARPlusEncounterValuesProto` (null OK):
  `TryCapture` tự build proto từ `PokeballThrow` + `this`. Marshalling native đang đúng (§5).

## 4. Việc thật sự cần làm (theo thứ tự)

### G5 — Nearby owner null (blocker số 1)
`INearbyPokemonService`/`IMapSceneViewService` resolve null → `map_read=0` → 0 spawn tới Kotlin →
không command nào được gửi → G1/G3 chưa từng chạy tới. **Phải sửa/re-probe discovery trước.**
Chi tiết & tiêu chí ở brainstorm 09-09 §12 (BG-11..BG-13). Không ép Kotlin tạo target giả.

### G3 — Encounter handshake nền (ĐÃ IMPLEMENT native)
`TryCapture` chỉ gửi 103; **không** gọi `Encounter(102)`. Với wild chưa tap, server có thể từ chối 103.
Đã thêm bước gọi `SendEncounterRequest` (102) **nền** trước khi throw, không mở UI:

- **Cơ chế (tránh block main thread):** `WildMapPokemon` cache `EncounterOutProto` ở field
  `egws`@`0x258` sau khi 102 resolve. Native đọc `egws`:
  - `egws == null` → gọi `SendEncounterRequest` (102) rồi trả outcome mới **`kEncounterRequested`**
    (không ném bóng). Không chờ đồng bộ — continuation của game tự fill `egws`.
  - `egws != null` → encounter đã mở → throw 103 như cũ.
- **Vòng lặp:** `kEncounterRequested` → `catch.inc` trả `error_code="encounter_requested"` (phase
  REJECTED, không mutation) → runner re-plan cùng target cycle sau → lúc đó `egws` đã có → throw.
  Không block, không suspend, không ném bóng thừa.
- **Guard:** chỉ gọi 102 khi `background_encounter_verified` (binding resolve) + `egwm`@0x228
  (ILocationProvider) + `egwn`@0x230 (IRpcHandler) là managed pointer hợp lệ. Cờ compile
  `kBackgroundEncounterEnabled` để tắt (A/B test 103-đơn) khi cần.
- **Files:** `direct_map_bindings.inc` (resolve + verified), `direct_map_actions.inc` (egws-gate +
  invoke), `catch.inc` (map outcome), `runtime_native_prelude.inc` (enum + fields),
  `StructuredAutomationController.kt` (log non-fatal).
- **Còn cần verify trên Air 1 (G5 mở đường trước):** thứ tự 102→103 có được server chấp nhận không,
  và số cycle chờ `egws` hợp lý. `encounter_id`+`spawn_point_id` lấy sẵn từ map scan (106).

### G1 — Result observer (mô hình PULL / SCAN_MAP, không hook)

**Bối cảnh mới:** nearby đã chuyển sang **pull chủ động** — Kotlin gửi `SCAN_MAP(cycle)` →
native đọc map+nearby+forts+inventory+player trên main thread (`runtime_scan_map_control.inc`,
có `discover_scene_runtime_owners` re-probe → đỡ G5) → trả `REQUEST_CATCH_SPIN`. Ở chế độ
`g_runtime_explicit_snapshot_mode`, nhánh observation-thread nearby + **map-query hook
(`map_hooks.inc`, `module.inc on_enable`) bị bypass → là code chết**, nên gỡ.

**Kotlin pipeline đã sẵn sàng:** `AutomationCommandResult.catchOutcome` + `capturedPokemonId` đã được
`StructuredAutomationWildState.registerIfCaught`, `publishCatchOutcome`, và
`AutomationRunner.hasAuthoritativeCatchOutcome` (direct-map cần `CAUGHT`/`FLED`) tiêu thụ. **Không cần
thêm plumbing Kotlin — chỉ thiếu nguồn outcome từ native.**

**Ràng buộc kiến trúc:** headless bỏ UI → không có `EncounterInteractionState` →
`CaptureCompleted`/`PokemonCaptured`/`HandlePokemonFled` không được gọi. `TryCapture` trả
`IPromise<CatchPokemonOutProto>` resolve **bất đồng bộ** (round-trip server), nên **không** có mặt trong
result đồng bộ của lệnh catch. `MapContentHandler.RegisterPokemonCaughtOrFled(ulong)` chỉ báo
"encounter kết thúc", KHÔNG phân biệt caught/fled.

Map enum `CatchPokemonOutProto.Status` (dump 625351–625365): `CATCH_SUCCESS=1→CAUGHT`,
`CATCH_ESCAPE=2→BREAKOUT`, `CATCH_FLEE=3→FLED`, `CATCH_MISSED=4→MISSED`, `CATCH_ERROR=0`,
`CATCH_ITEM_REPLACEMENT=5`.

**Hai hướng lấy outcome (fork thiết kế — chờ chốt):**

| | A. Promise continuation (native) | B. SCAN_MAP delta inference (Kotlin) |
|---|---|---|
| Cách làm | Đăng ký continuation lên `IPromise<CatchPokemonOutProto>`; khi resolve, đọc `Status`+`CapturedPokemonId`, gửi late result theo command_id | Sau catch, cycle SCAN_MAP kế đọc nearby + pokemon-storage; target biến mất + storage +1 → CAUGHT; biến mất + storage giữ → FLED; còn đó → đang thử/BREAKOUT |
| Authoritative | ✅ Status thật của server | ⚠️ suy luận (brainstorm cảnh báo "biến mất ≠ caught"); có storage delta thì đỡ hơn |
| `capturedPokemonId` | ✅ có (để transfer/keep) | ❌ khó lấy đúng |
| Hợp mô hình pull/no-hook | Trung tính (interop phức tạp, không phải inline-hook) | ✅ rất hợp; native chỉ cần thêm pokemon-storage count vào SCAN_MAP |
| Rủi ro | Layout `Promise<T>` từ dump không tin cậy; GC/thread/lifetime; khó nhất | Correlation cycle, breakout nhiều lần, filter IV/shiny thiếu metadata |

- Trước khi có outcome: chỉ báo "đã gửi, đang chờ", **không** toast caught/fled; dedupe theo
  session+command+target.
- Cần verify trên Air 1 dù chọn hướng nào.

#### G1 — Đã chốt hướng A (promise continuation). Staging + phát hiện

**Chốt (2026-09-11):** dùng **A** (authoritative, có `capturedPokemonId`). Cũng đã **gỡ map-query hook**
chết (`map_hooks.inc` + include + `on_enable` + field `map_query_*`), rebuild OK — nearby thuần pull.

**Offset đã verify (dump, tin cậy vì proto không generic):**
`CatchPokemonOutProto.Status`@`0x10` (enum int32), `CapturedPokemonId`@`0x20` (ulong).
Map: `1 CatchSuccess→CAUGHT`, `2 CatchEscape→BREAKOUT`, `3 CatchFlee→FLED`, `4 CatchMissed→MISSED`,
`0 CatchError`, `5 CatchItemReplacement`.

**BLOCKER kỹ thuật cho A:** `TryCapture` trả `RpcPromise<CatchPokemonOutProto> : Promise<CatchPokemonOutProto>`.
Trong dump, **mọi field của `Promise<T>`/`RpcPromise<T>` đều = 0x0** (generic — dump không có offset thật
của bản instantiate). ⇒ **không hardcode offset promise được**; phải resolve field lúc runtime bằng
`class_get_field_from_name` trên instantiated class, và chỉ **xác định/verify được trên thiết bị**.

**Staging (fail-closed, từng bước verify được):**
1. **✅ ĐÃ LÀM — Device diagnostic (read-only):** sau khi `TryCapture` trả promise,
   `log_direct_catch_promise_layout` log runtime class + walk parent chain, in field name/offset/type.
   Đã bind `il2cpp_class_get_parent` (typedef + Il2CppApi + RESOLVE). Không GC-root, không đổi hành vi
   (đọc ngay trong tick main-thread khi object còn sống). → một lượt chạy direct-catch trên Air 1 sẽ in
   layout thật của `RpcPromise<CatchPokemonOutProto>` (offset `_hasCompleted`/value).
2. **⏭️ Poll + deliver (chờ offset từ log Step 1):** GC-root promise (`il2cpp_gchandle_new` — chưa bind),
   pending-slot (handle + command_id + encounter_id); mỗi cycle SCAN_MAP poll: khi completed đọc
   `CatchPokemonOutProto.Status`+`CapturedPokemonId`, free handle, gửi **late command result** mang
   `catchOutcome`+`capturedPokemonId`.
3. **Kotlin:** pipeline đã sẵn (`AutomationCommandResult.catchOutcome/capturedPokemonId` →
   `wildState`/`publishCatchOutcome`/runner). Chỉ cần late-result đến đúng command_id.

**Blocker cho Step 2:** offset completion/value của promise — lấy từ log Step 1 trên Air 1. Không ship
code poll offset mù.

### G2 — Guard đủ dependency (chống crash)
`TryCapture` deref thêm `xpAwardService`@0xE0, `codeGateService`@0xC8 (và scheduler@0x50 ở nhánh
timeout). Guard native hiện chỉ kiểm rpc@0x38/dir@0x40/telemetry@0x68/bag@0xE8. Thêm 0xE0 + 0xC8 vào
`is_probable_managed_pointer` trước invoke.

## 5. `PokeballThrow` là struct → phần marshalling ĐANG ĐÚNG

`runtime_invoke(method, obj, args, &exc)` với value-type arg nhận **con trỏ tới dữ liệu unboxed**.
Native truyền `args[0] = &throw_data` (struct 12 byte, `static_assert` khớp offset) và
`args[1] = nullptr` (ARPlus null). Prologue `.so` cho thấy runtime tái tạo đúng struct từ x1:w2.
→ **Không cần sửa cách gọi hàm.** Đây là điểm từng bị nghi ngờ nhưng đã loại trừ.

## 6. Địa chỉ / binding cho triển khai (build 0.427.0, arm64-v8a)

| Symbol | RVA / id | Vai trò |
|---|---|---|
| `MapPokemon.TryCapture(PokeballThrow, ARPlusEncounterValuesProto)` | `0x7F91ECC` (RPC 103) | ném + gửi catch |
| `PokeballThrow.get_ServerReticleSize()` | `0x89F42D4` | chuẩn hoá reticle |
| `WildMapPokemon.SendEncounterRequest()` | `0x7F98564` (RPC 102) | mở encounter nếu G3 cần |
| `IEncounterState.CaptureCompleted(CatchPokemonOutProto)` | dump.cs:307249 | observer kết quả |
| `MapContentHandler.RegisterPokemonCaughtOrFled(ulong)` | `0x7F54EBC` | observer + map sync |
| `MapEntityService.RemoveWildPokemon(ulong)` | `0x7F636B4` | map sync sau kết quả |
| `MapContentHandler.ForceRefreshVisibleCells()` | `0x7F53FAC` | refresh map |

`CatchPokemonProto` layout (đích của throw): `EncounterId`@0x10, `Pokeball`@0x18,
`NormalizedReticleSize`@0x20, `SpawnPointGuid`@0x28, `HitPokemon`@0x30, `SpinModifier`@0x38,
`NormalizedHitPosition`@0x40, `ArPlusValues`@0x48, `ShouldFlee`@0x50.

> Enum/RVA/offset chỉ đúng cho `0.427.0` (versionCode `2026082702`, arm64-v8a). Không suy rộng.

## 7. Khuyến nghị

1. **Đóng G5 trước** (nearby discovery) — không có bước này thì không có gì để bắt.
2. Thêm **G1 observer** qua hook `CaptureCompleted`/`RegisterPokemonCaughtOrFled`; chỉ toast sau
   authoritative outcome, dedupe theo session+command+target.
3. **Verify G3 trên Air 1**: thử 103 đơn; nếu server từ chối thì chèn 102 (`SendEncounterRequest`)
   trước — vẫn headless, không UI.
4. Thêm **G2 guard** cho 0xE0/0xC8.
5. **Không** đụng `PokeballThrow`/cách marshalling — đã xác nhận đúng. Bonus throw (Excellent/Curve)
   để sau, là intent tuỳ chọn.

## 8. Câu hỏi mở (cần thiết bị)

1. Server có nhận `CatchPokemon(103)` khi wild chưa `Encounter(102)` không? (G3)
2. Nếu cần 102 trước: gọi `SendEncounterRequest()` lẻ có tự set context server đủ để 103 chạy không?
3. "bg" có gồm tắt màn hình / app xuống nền, hay chỉ ngầm khi vẫn ở map?

## 9. Cập nhật triển khai — đọc Promise của `SendEncounterRequest`

Yêu cầu follow-up là không coi `kEncounterRequested` như một kết quả thành công.
Đây chỉ là outcome nội bộ báo rằng `WildMapPokemon.SendEncounterRequest()` đã
được gọi và cần chờ `IPromise<PokemonEncounterResponse>` hoàn tất trước khi chạy
`TryCapture`.

Đã triển khai native observer theo mô hình pull trên main thread:

- Lưu return Promise bằng `il2cpp_gchandle_new`, tránh giữ raw pointer không được
  GC bảo vệ.
- Poll các field runtime của Promise bằng metadata (`completeCalled`,
  `errorCalled`, `completedValue`, `errorValue`); không hardcode offset của generic
  `Promise<T>`.
- Chỉ khi Promise complete thành công và `WildMapPokemon.egws` đã có
  `EncounterOutProto` thì mới giải phóng handle và invoke `TryCapture`.
- Promise lỗi, mất GC target, thiếu field layout hoặc timeout 15 giây đều fail
  closed; không ném bóng trong các trường hợp này.

Acceptance criteria bổ sung (inferred — needs device confirmation):

| ID | Rule / Requirement | Formula / Expected | Acceptance note |
|----|--------------------|-------------------|-----------------|
| AC-PROMISE-1 | Giữ return của Encounter | Log `background encounter returned ... promise != null` tiếp theo `promise retained` | Không discard return của `SendEncounterRequest`. |
| AC-PROMISE-2 | Đọc trạng thái async | Log `promise state ... complete=0/1 error=0/1` trên main thread | Không đọc managed Promise từ observer thread. |
| AC-PROMISE-3 | Gate bước catch | Chỉ có `DIRECT_MAP_CATCH_DIRECT` sau `promise completed` và `egws` non-null | Promise pending/failed/timeout không gọi `TryCapture`. |

## 10. Chẩn đoán log map luôn rỗng — 20:40–20:41

### Kết luận

Trong log `build/logs/logcat-full-20260911-204111-62485.txt`, native **không hề đi tới bước
đọc cells**. Vì vậy chưa thể kết luận `get_Cells()` trả về dictionary rỗng; nguyên nhân hiện tại
là binding `MapEntityService` chưa được resolve.

### Bằng chứng từ log

| Log | Ý nghĩa |
|---|---|
| `runtime bootstrap ... encounter_read=1 map_read=0` | Encounter đã bind, map chưa bind ngay từ bootstrap. |
| `scene map service unavailable` | `FindObjectOfType(MapSceneViewService)` và fallback `MapScene` đều không lấy được object. |
| `runtime map scan begin ... map_verified=0 map_service=0x0 get_cells=0x0` | Scan bắt đầu với toàn bộ map binding cần thiết bằng null. |
| `runtime map scan rejected reason=map_and_fort_bindings_unavailable` | Reader return trước khi gọi `MapEntityService.get_Cells()`. |
| `native catch_spin scan cycle=1..20 ... nearby_success=0 nearby=0` | Tất cả 20 chu kỳ đều không có nearby/map data. |

Không có dòng `runtime map scan complete success=1`, `background encounter returned`,
`promise retained` hoặc `DIRECT_MAP_CATCH_DIRECT` trong file log này.

### Root cause trong code

`runtime_catch_spin_read_snapshot()` chỉ gọi lại `discover_scene_runtime_owners()` khi observer
chưa chạy (`coordinator.inc:69`). Nhưng các chu kỳ `SCAN_MAP` đang chạy từ observer/pull loop,
nên sau lần discovery sớm bị fail, map owner không bao giờ được thử lại:

```cpp
if (!before.map_entity_read_verified && !context.observation_running.load()) {
    discover_scene_runtime_owners(context);
}
```

Đây là lỗi retry/lifecycle, không phải bằng chứng map thật sự không có Pokémon. Secondary risk là
discovery hiện chỉ dùng `FindObjectOfType(Type)`; nếu map service không phải active direct object,
cần thêm fallback `FindObjectsOfType`/include-inactive hoặc lấy owner từ object chain đã verify.

### Acceptance criteria cho bản sửa

| ID | Rule / Requirement | Expected |
|---|---|---|
| AC-MAP-1 | Retry discovery sau khi scene/map đã ready | Có log discovery sau bootstrap và `map_verified=1`. |
| AC-MAP-2 | Phân biệt binding fail và cells rỗng | Khi binding đủ nhưng cells rỗng, log phải ghi `get_Cells` đã được gọi và số cell đọc được. |
| AC-MAP-3 | Không spam main thread | Retry có throttle/backoff; không gọi `FindObjectOfType` mỗi tick. |
| AC-MAP-4 | Fail closed | Khi retry vẫn fail, không gửi encounter/catch và phải giữ `nearby_success=0`. |

### Hướng xử lý khuyến nghị

1. **Đã sửa:** bỏ điều kiện phụ thuộc `context.observation_running` ở
   `runtime_catch_spin_read_snapshot()`. Khi `map_entity_read_verified=false`, map scan có thể
   gọi lại `discover_scene_runtime_owners()` ngay cả khi observer đã chạy.
2. Nếu retry sau khi scene ready vẫn không lấy được object, bổ sung discovery bằng
   `FindObjectsOfType` hoặc owner chain `MapScene → ehis → ehkb → egft`.
3. Bổ sung log ở ngay trước/sau `get_Cells()` để xác nhận riêng trường hợp binding đã đủ nhưng
   dictionary không có cell.

## 11. Log sau khi bỏ guard discovery — map đã chạy, Promise poll làm crash

Log `build/logs/logcat-full-20260912-093024-80606.txt` xác nhận việc bỏ guard đã mở đúng đường:

- `map_verified=1`, `map_service` và `get_cells` đều khác null.
- `cells=26`, `wild=5`, `nearby_success=1` ở cycle 1 và cycle 2.
- Native match được target `id=4674225908875914523`.
- Cycle 1 gọi `DIRECT_MAP_SEND_ENCOUNTER`, nhận Promise và retain thành công.
- Cycle 2 match lại target rồi process crash trước log `promise state`, trước
  `DIRECT_MAP_CATCH_DIRECT`, nên `TryCapture` vẫn chưa được gọi.

Crash là `SIGSEGV` trong `libil2cpp.so`, fault address `0xbef64020`. Register `x0` tại crash là
`0x00000000bef64a98`, trùng với giá trị `gc_handle=3203811992` đã log ở cycle 1 sau khi bị ép
về `uint32_t`. Disassembly của `il2cpp_gchandle_new` trên build này cho thấy nó trả về handle
dạng pointer/tagged pointer qua thanh ghi 64-bit; `il2cpp_gchandle_get_target` cũng xử lý handle
64-bit. Native typedef hiện tại dùng `uint32_t`, làm mất nửa cao của handle. Khi poll gọi
`gchandle_get_target`, IL2CPP dereference handle bị cắt và crash.

### Required fix trước khi test tiếp

**Đã sửa:** đổi kiểu lưu/truyền GC handle sang `uintptr_t` (`Il2CppGcHandle`) đúng ABI của build
này trong toàn bộ `Il2CppGcHandleNew`, `Il2CppGcHandleGetTarget`, `Il2CppGcHandleFree`,
`PendingEncounterRequest` và log format tương ứng. Native build đã pass; cần push/install rồi
đọc log mới để xác nhận Promise poll không còn crash.

## 12. Kết quả log sau khi sửa GC handle

Log `build/logs/logcat-full-20260912-094235-81929.txt` xác nhận Promise flow đã chạy end-to-end
đến bước gọi catch:

- Map: `map_verified=1`, `cells=26`, `wild=6`, `nearby_success=1`.
- Encounter: Promise được retain bằng handle 64-bit `0x765efa35f0`.
- Poll: `complete=1`, `error=0`, có `PokemonEncounterResponse`; không còn crash.
- Catch: `DIRECT_MAP_CATCH_DIRECT` được invoke với `ball=1`, `reticle=0.050`, `hit=1`,
  `spinning=1`, `missed=0`; runtime trả Promise không exception.

Tuy nhiên `direct map catch outcome observer verified=0`, nên log này chỉ chứng minh request
catch đã được gửi/Promise đã được tạo; chưa chứng minh server trả `CATCH_SUCCESS` và chưa có
`capturedPokemonId`. Cần hoàn thiện observer kết quả catch trước khi báo caught.

## 13. Poll Promise của `TryCapture` và đồng bộ lại map — 09:55

### Kết luận kỹ thuật

Có thể đọc kết quả `TryCapture` bằng đúng mô hình đã dùng cho
`SendEncounterRequest`: giữ Promise bằng `il2cpp_gchandle_new`, poll trên Unity main
thread, đọc `completeCalled`, `errorCalled`, `completedValue`, sau đó đọc
`CatchPokemonOutProto.Status` và `CapturedPokemonId` từ object kết quả.

Reverse dump của build `0.427.0` xác nhận:

- `Status` là enum tại offset `0x10`: `CATCH_SUCCESS=1`, `CATCH_ESCAPE=2`,
  `CATCH_FLEE=3`, `CATCH_MISSED=4`.
- `CapturedPokemonId` là `ulong` tại offset `0x20`.
- `MapContentHandler.RegisterPokemonCaughtOrFled(ulong)` là postcondition để loại
  entity đã caught/fled khỏi map.
- `MapContentHandler.ForceRefreshVisibleCells()` là bước yêu cầu map fetch lại
  visible cells.

### Đã triển khai

- Tạo `direct_catch_promise_observer.inc`: lưu GC handle 64-bit, poll Promise,
  decode status/id, log state/result, gửi late result nếu direct catch có
  `commandId`.
- Tạo `direct_catch_map_sync.inc`: retry resolve `MapContentHandler` lúc Promise
  terminal, gọi `RegisterPokemonCaughtOrFled`, fallback `RemoveWildPokemon`, rồi
  gọi `ForceRefreshVisibleCells`.
- Thêm task main-thread riêng `kPollDirectCatch`; coordinator vẫn poll dù đang
  suspended sau catch và chỉ resume sau khi Promise terminal.
- Native auto catch không coi Promise non-null là caught nữa; chỉ status server
  `CATCH_SUCCESS` mới được map vào `CAUGHT`.

### Verification

- `build-magisk.sh` pass arm64-v8a và x86_64.
- `./gradlew test assembleDebug` pass; các module hiện không có test case nên
  Gradle báo `NO-SOURCE` cho test tasks.
- Push/install/reboot đã pass qua script repository. Snapshot log mới chưa lấy
  được vì BlueStacks Air 1 chưa reconnect lại `127.0.0.1:5565` sau reboot
  (`Connection refused`); cần đọc log sau khi emulator lên lại để xác nhận các
marker `runtime direct catch promise result`, `map sync method` và scan cycle
tiếp theo.

## 14. Audit đường reload map sau khi Promise hoàn tất — 09:58

### Phát hiện

`refresh_map_after_direct_catch()` được chạy bên trong callback đã được dispatch lên
Unity main thread. Việc gọi `discover_scene_runtime_owners()` tại vị trí này là sai
ngữ cảnh: hàm đó có thể gọi `request_main_thread_object()` rồi chờ một callback khác
trên chính main thread, tạo deadlock.

### Điều chỉnh

Đã bỏ lời gọi discovery có chờ callback. Resolver mới chỉ dùng các thao tác đồng bộ
ngay trên main thread:

- dùng `MapSceneViewService` hiện tại nếu binding còn hợp lệ;
- nếu không có, gọi trực tiếp `FindObjectOfType(MapSceneViewService)`;
- fallback qua `FindObjectOfType(MapScene)` rồi đọc owner chain `ehis → ehkb`;
- sau đó gọi `RegisterPokemonCaughtOrFled`, fallback `RemoveWildPokemon`, và
  `ForceRefreshVisibleCells`.

Native build sau điều chỉnh đã pass. Log runtime cần xác nhận không còn deadlock và có
đủ các marker `runtime direct catch promise result`, `map sync method=...` cùng cycle
scan tiếp theo.

Lần push lại bản đã sửa chưa thực hiện được vì BlueStacks Air 1 đang mất kết nối
(`127.0.0.1:5565 Connection refused`); không dùng ADB thủ công để bypass trạng thái này.

## 15. Audit việc không quét được forts — 10:21

### 1. Hiện trạng và kỳ vọng

Log mới nhất là `build/logs/logcat-full-20260912-102116-86002.txt`, process hiện tại là
PID `5890`. Map entity reader đã hoạt động: `map_verified=1`, `map_service` khác null,
`get_cells` khác null, và scan đọc được `cells=26`, `wild=4`. Tuy nhiên mọi cycle đều trả
`forts=0`.

Kỳ vọng là mỗi map tick phải đi qua directory pokestop, enumerate các entry, đọc
`MapPokestop.Id`, `Location`, `IsCoolingDown`, rồi trả danh sách forts cho filter/spin.

### 2. Bằng chứng tái hiện

Các marker quan trọng ở cycle hiện tại:

```text
10:21:00.661 managed diagnostic: owner label=MapSceneViewService.MapEntityService
                         object=0x787fd1f550 class=...MapEntityService exact=1
10:21:00.667 runtime map scan begin stage=5 map_verified=1
                         map_service=0x787fd1f550 get_cells=0x79a12a6850
                         forts_verified=0 fort_directory=0x0 pokestops=0x0
10:21:00.667 runtime map scan forts skipped reason=binding_unverified
10:21:00.667 runtime map scan complete success=1 cells=26 wild=4 forts=0 complete=1
```

Cùng chuỗi này lặp lại ở `10:21:02.691`, `10:21:06.728`, `10:21:10.772` và
`10:21:14.806`. Vì vậy đây không phải trường hợp dictionary được enumerate nhưng rỗng;
`read_runtime_map_forts()` bị chặn trước khi gọi `enumerate_map_dictionary()`.

### 3. Phạm vi ảnh hưởng

- Map và wild scan vẫn chạy.
- Fort reader bị disable bởi `forts_read_verified=0`.
- Direct spin cũng không thể verified vì `spin_verified` phụ thuộc vào fort reader.
- Chưa có bằng chứng nào cho thấy `MapPokestop.get_Id`, `get_Location` hoặc
  `get_IsCoolingDown` bị crash hay trả dữ liệu sai; các getter này chưa được gọi trong
  cycle lỗi.

### 4. Nguyên nhân gốc

Binding hiện tại trong `zygisk/jni/modules/catch_spin/direct_map_bindings.inc` chỉ tìm
directory theo owner chain:

```text
MapEntityService --egiq--> MapPlaceDirectoryService --ehft--> Dictionary<string, IMapPokestop>
```

Log xác nhận kết quả của chain này là `directory=0x0`, `pokestops=0x0`. Trong khi đó
reverse dump của đúng build `0.427.0` còn có hai owner chain đã biết:

```text
MapSceneViewService --ehjt--> IMapPlaceDirectoryService
MapContentHandler    --egga--> IMapPlaceDirectoryService
```

`MapSceneViewService` và `MapContentHandler` đã được resolve để lấy `MapEntityService`,
nhưng code discovery chưa đọc `ehjt` hoặc `egga`. Do đó directory tồn tại ở owner chain
khác nhưng binding không giữ được nó, làm capability bị fail-closed.

### 5. Dependency map

```text
MapSceneViewService
  ├─ ehkb → MapContentHandler
  │          ├─ egft → MapEntityService → egix → Cells       [đang chạy]
  │          └─ egga → IMapPlaceDirectoryService              [chưa bind]
  └─ ehjt → IMapPlaceDirectoryService                         [chưa bind]
             └─ ehft → Dictionary<string, IMapPokestop>
                        └─ MapPokestop getters → fort observation
```

### 6. Các hướng xử lý và lựa chọn

1. **Khuyến nghị:** giữ nguyên fail-closed, bổ sung fallback owner chain theo thứ tự
   `MapSceneViewService.ehjt`, sau đó `MapContentHandler.egga`; từ directory lấy `ehft`,
   rồi mới set `forts_read_verified`.
2. Chỉ nới điều kiện `forts_read_verified` để quét trực tiếp từ cells: không đủ an toàn,
   vì cells hiện là `MapEntityCell` và không chứng minh được layout/danh sách pokestop.
3. Bỏ điều kiện binding và đọc địa chỉ đoán: không chấp nhận, vì có thể đọc nhầm object
   và làm crash target process.

### 7. Phạm vi sửa tối thiểu dự kiến

Chỉ cần mở rộng việc resolve directory trong direct map binding; không cần thay đổi filter,
tick counter, Promise catch hay logic enumerate. Nên log thêm `directory_source=ehjt|egga|egiq`
và địa chỉ `ehft` để phân biệt owner chain nào hoạt động trên thiết bị.

### 8. Rủi ro và cách phòng ngừa

- Chỉ chấp nhận object nếu `runtime_object_is_exact_class(..., MapPlaceDirectoryService)`
  thành công.
- Chỉ set capability verified khi directory, dictionary và cả ba getter pokestop đều tồn tại.
- Không gọi discovery chờ callback từ callback main thread; fallback phải dùng field đã đọc
  đồng bộ trong cùng main-thread task hoặc dùng owner đã cache.
- Nếu cả hai fallback đều null, tiếp tục trả `forts=0` với log `binding_unverified`, không
  đoán offset khác.

### 9. Câu hỏi bug template

- **Expected:** map scan đọc được các fort đang hiển thị và đưa fort đủ điều kiện vào filter.
- **Actual:** map scan chỉ đọc cells/wild; fort phase bị skip vì `binding_unverified`.
- **Reproduction:** chạy auto catch sau khi `MapSceneViewService` ready, xem các cycle có
  `map_verified=1` nhưng `forts_verified=0`, `directory=0x0`, `pokestops=0x0`.
- **Evidence:** PID `5890`, các dòng `22066`, `22075`, `22078`, `22081` trong log nêu trên.
- **Root cause:** chỉ resolve `MapEntityService.egiq`; bỏ sót `MapSceneViewService.ehjt` và
  `MapContentHandler.egga`.
- **Fix boundary:** bổ sung binding directory và log source; giữ nguyên fail-closed reader.
- **Validation:** capability phải thành `forts_verified=1`, log phải có số dictionary entries,
  số fort đọc được và không còn `forts skipped reason=binding_unverified`.

## Acceptance Criteria (from spec)

Không có spec riêng cho lỗi này; các tiêu chí dưới đây được suy ra từ behavior hiện tại và
reverse output của build `0.427.0`.

| ID | Rule / Requirement | Expected |
|---|---|---|
| AC-FORT-1 | Resolve directory từ owner chain đã verify | `ehjt`, `egga` hoặc `egiq` trả đúng `MapPlaceDirectoryService`. |
| AC-FORT-2 | Resolve dictionary và getter | `ehft`, `get_Id`, `get_Location`, `get_IsCoolingDown` đều khác null. |
| AC-FORT-3 | Cho phép fort scan | `forts_read_verified=1`; không log `binding_unverified`. |
| AC-FORT-4 | Quan sát được từng stage | Có log owner source, dictionary entry count, fort read count và skip reason nếu fail. |
| AC-FORT-5 | Fail closed | Nếu binding không exact/đủ, không enumerate địa chỉ đoán và không spin. |
| AC-FORT-6 | Không phá map/wild scan | `cells` và `wild` hiện có tiếp tục được đọc bình thường. |

## Synthesis

Kết luận: forts hiện không được quét vì binding bị disable trước bước đọc map, không phải
vì map không có fort hoặc vì filter loại hết fort. Fix đúng là bind `IMapPlaceDirectoryService`
từ `MapSceneViewService.ehjt` hoặc `MapContentHandler.egga`, sau đó đọc dictionary `ehft`;
không cần bỏ guard. Phần triển khai được ghi ở mục 16 bên dưới.

## 16. Triển khai fallback binding fort directory

Đã triển khai fix trong native:

- Tách `update_direct_map_fort_capabilities()` để tính lại `forts_read_verified` và
  `spin_verified` sau mỗi lần owner discovery.
- Tách `bind_direct_map_place_directory()` và thử lần lượt `MapEntityService.egiq`,
  `MapSceneViewService.ehjt`, `MapContentHandler.egga`.
- Gọi lại directory binding trong `discover_scene_runtime_owners()` sau khi scene owner
  late-bind thành công; đây là phần cần thiết vì initial discovery xảy ra trước khi map
  scene ready.
- Thêm log `map directory candidate` và `map directory bound source=...` để xác nhận
  owner chain thực tế trên thiết bị.

Guard fail-closed vẫn giữ nguyên: chỉ bind object exact class `MapPlaceDirectoryService`
và chỉ enable forts/spin khi dictionary cùng ba getter `MapPokestop` đã đủ. Native Magisk build
đã pass arm64-v8a/x86_64; Gradle `test assembleDebug` cũng pass. Chưa push/install trong lượt
này; cần test runtime để xác nhận `forts_verified=1`, dictionary entries và số fort đọc được.
