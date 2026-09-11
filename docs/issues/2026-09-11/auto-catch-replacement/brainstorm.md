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
