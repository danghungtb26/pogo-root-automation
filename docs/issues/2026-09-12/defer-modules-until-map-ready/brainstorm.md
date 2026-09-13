# Brainstorm: Chờ map instance sẵn sàng rồi mới start module

**Type:** architecture  
**Date:** 2026-09-12

---

## Analysis

### 1. Vấn đề cần giải quyết là gì?

Người dùng muốn khi vừa vào Pokémon GO, các runtime module (catch_spin, discard, transfer, encounter, …) **không chạy ngay**. Thay vào đó, hệ thống phải **lắng nghe / phát hiện khi map instance của game khởi tạo xong**, rồi mới enable các module.

Mục tiêu ngầm:
- Tránh probe/observe/mutation khi Unity scene chưa sẵn sàng → giảm crash, binding sai, hoặc hành vi lỗi sớm.
- Đồng bộ lifecycle automation với overworld thực sự đã load.

### 2. Ràng buộc (hard / soft)

**Hard constraints (theo AGENTS.md và kiến trúc repo):**
- Không gọi managed IL2CPP trong giai đoạn `il2cpp_init` / probe ELF-only.
- Binding phải version-scoped (`0.427.0`), fail-closed khi chưa verify.
- Không dùng screenshot, `input tap`, hay đoán tọa độ.
- Module game-specific nằm sau adapter/native binding boundary.
- File source ≤ 500 dòng; dùng script có sẵn cho device ops.

**Soft constraints:**
- Giữ tương thích với flow hiện tại: START → DIAGNOSTIC → ENABLE module từ Kotlin controller.
- Không phá module chỉ cần inventory (discard/transfer) nếu chúng không phụ thuộc map.

### 3. Quality attributes quan trọng nhất

Thứ tự ưu tiên cho case này:

1. **Simplicity / Maintainability** — tái sử dụng signal sẵn có thay vì hook lifecycle mới.
2. **Testability** — có thể kiểm tra qua capability `READ_NEARBY` và log diagnostic.
3. **Performance** — poll nhẹ trên main thread, không block cold start.
4. **Scalability** — không quan trọng ở đây.

Không tối đa hóa “start nhanh nhất có thể”; ưu tiên **start đúng lúc**.

### 4. Hiện trạng: module đã start khi nào?

Luồng thực tế hôm nay **không phải** “vào game là module chạy ngay”:

```
Zygisk attach
  → binding_probe_thread (ELF probe, register module registry)
  → modules registered nhưng DISABLED
  → Kotlin ensureRunning → bridge.startRuntime()
  → chờ AUTO_DIAGNOSTIC (T+3s, retry 5s)
  → syncModules → setModuleEnabled(true) per config
  → native set_module_enabled → available() gate → on_enable → observer
```

Điểm mấu chốt:
- **Register ≠ Start.** Static init chỉ đăng ký module vào registry.
- **Enable do controller quyết định**, sau managed DIAGNOSTIC thành công.
- Native `set_module_enabled` gọi `feature->available(binding)` trước khi bật.

Vậy yêu cầu “vừa vào game không start module” **đã gần đúng với thiết kế hiện tại**, trừ khi user config bật auto-enable ngay sau DIAGNOSTIC (T+3s) — lúc đó map có thể chưa live.

### 5. Map instance detection hiện có chưa?

**Có — nhưng là discovery/poll, không phải event hook.**

| Thành phần | File | Vai trò |
|---|---|---|
| Signal chính | `RuntimeBinding.map_entity_read_verified` | `bind_map_entity_service` verify `MapEntityService.get_Cells` + field cells |
| Late discovery | `discover_scene_runtime_owners` | Main thread: `FindObjectOfType(MapSceneViewService)` hoặc `MapScene → ehis → MapSceneViewService → ehkb → egft → MapEntityService` |
| Gọi khi nào | `run_managed_runtime_diagnostic`, `ensure_observer_for_enabled_modules`, catch_spin re-probe | Point-in-time, không phải listener liên tục |
| Capability | `READ_NEARBY` trong `runtime_capabilities.inc` | Chỉ advertise khi `map_entity_read_verified` |
| Map read | `read_runtime_map_snapshot_on_main_thread` | Validate `get_Cells` thực sự đọc được |

**Chưa có:**
- Hook vào `MapScene.Initialize`, `MapSceneContext.Start`, hay `RunInternal`.
- Background waiter chuyên dụng “block toàn bộ module cho đến khi map ready”.
- Re-DIAGNOSTIC định kỳ khi module UNAVAILABLE vì map chưa sẵn sàng.

Reverse `0.427.0` có các class liên quan:
- `Niantic.Holoholo.Map.MapEntityService` — `get_Cells`
- `Niantic.Holoholo.Map.MapScene` — `Initialize`, `Start`
- `Niantic.Holoholo.MapSceneContext` — `get_MapEntityService`, `Initialize`, `Start`

Native đã bind field chain obfuscated (`ehis`, `ehkb`, `egft`, `egix`).

### 6. Module nào thực sự cần map?

| Module | Cần map? | Gate `available()` hiện tại |
|---|---|---|
| catch_spin (nearby/auto-catch) | **Có** cho nearby | `encounter_read_verified \|\| forts_read_verified` — **không** yêu cầu map |
| catch_spin (spin-only) | Không | forts đủ |
| discard | Không (inventory) | `inventory_read_verified && discard_verified` |
| transfer | Không | `transfer_verified` |
| encounter/throw | Không trực tiếp map | encounter/throw bindings |

→ Gate “chờ map cho **mọi** module” sẽ **chặn nhầm** discard/transfer nếu áp dụng blanket.

### 7. Các phương án thiết kế

#### Option A — Per-module `available()` (tối thiểu)

Thêm `map_entity_read_verified` vào `CatchSpinModule::available()` khi config cần nearby/direct catch.

- **Ưu:** Khớp pattern sẵn có; Kotlin `syncModules` tự UNAVAILABLE.
- **Nhược:** Chỉ chặn enable lúc T+3s; nếu map xuất hiện sau đó cần retry enable (catch_spin đã có re-probe trong scan loop, nhưng module vẫn UNAVAILABLE cho đến khi sync lại).

#### Option B — Shared map readiness gate + periodic re-probe (khuyến nghị)

1. Định nghĩa `map_runtime_ready(binding)` = `map_entity_read_verified && map_entity_service != nullptr && get_Cells bound`.
2. Trong `ensure_observer_for_enabled_modules` hoặc observer tick: nếu module map-dependent đang ENABLING/desired nhưng chưa ready → gọi `discover_scene_runtime_owners` (đã có một lần; mở rộng thành retry có backoff).
3. Kotlin: khi capabilities cập nhật thêm `READ_NEARBY`, gọi lại `syncModules(forceUnavailableRetry=true)`.
4. Module không phụ thuộc map: giữ enable độc lập.

- **Ưu:** Robust với map load trễ; không cần hook mới.
- **Nhược:** Poll-based; timing phụ thuộc observer interval.

#### Option C — Hook lifecycle Initialize/Start (effort cao)

Hook `MapSceneContext.RunInternal` hoặc `MapScene.Initialize` trên main thread, set atomic `map_scene_initialized`.

- **Ưu:** Event-driven, timing chính xác hơn poll.
- **Nhược:** Hook fragility, cần verify trên BlueStacks Air 1; vi phạm tinh thần “read-only until verified binding”; effort và risk cao hơn Option B.

#### Option D — Kotlin-only delay (không khuyến nghị)

Tăng `AUTO_DIAGNOSTIC_INITIAL_DELAY_MS` hoặc hardcode “đợi 30s rồi enable”.

- **Ưu:** Dễ code.
- **Nhược:** Không biết map thực sự ready; máy chậm/nhanh đều fail.

### 8. Integration points

```
Pokémon GO Unity main thread
  MapScene / MapSceneViewService / MapEntityService
       ↓ discover_scene_runtime_owners (main thread bridge)
Native RuntimeBinding.map_entity_read_verified
       ↓ capabilities (READ_NEARBY)
Bridge RuntimeReady event
       ↓ RuntimeLifecycleCoordinator.syncModules
set_module_enabled → available() → on_enable → observer
```

Boundary owner:
- **Native:** phát hiện map ready, advertise capability.
- **Kotlin:** quyết định *khi nào* enable module theo config + capability.
- **Per-module `available()`:** module tự khai báo dependency.

### 9. Data flow

1. Game tạo map scene objects (async sau login/load).
2. DIAGNOSTIC hoặc observer tick gọi `discover_scene_runtime_owners`.
3. `bind_map_entity_service` set `map_entity_read_verified=true`.
4. Capabilities refresh → `READ_NEARBY` xuất hiện.
5. Coordinator retry enable module map-dependent.
6. `ensure_observer_for_enabled_modules` start observation loop.

Không có shared “map ready event” riêng trên bridge hôm nay — có thể suy ra từ capability hoặc thêm phase mới nếu cần rõ ràng hơn.

### 10. Cách test

| Level | Cách verify |
|---|---|
| Unit / host | Compile native protocol tests (không cover map) |
| Integration | Logcat: `scene map service unavailable` → sau load overworld → `map_entity_read_verified=1`, `READ_NEARBY` |
| Device (BlueStacks Air 1) | Vào game, **chưa** vào map → module map-dependent UNAVAILABLE; sau overworld → auto enable hoặc retry sync |
| Regression | discard/transfer vẫn enable khi inventory ready dù map chưa có |

Script: `./scripts/logcat-full.sh` khi test fail.

### 11. Rủi ro

| Rủi ro | Mức | Mitigation |
|---|---|---|
| DIAGNOSTIC T+3s chạy trước map → `map_read=0` vĩnh viễn nếu không retry | Cao | Periodic re-probe + capability refresh + syncModules retry |
| Blanket gate chặn discard/transfer | Trung bình | Gate theo module, không global |
| `FindObjectOfType` miss trong scene transition | Trung bình | Re-probe trên observer tick; catch_spin đã làm tương tự |
| Hook Initialize (Option C) crash trên build mới | Cao | Tránh trừ khi poll không đủ |
| User ở màn hình không phải overworld (login, shop) | Trung bình | Map không bao giờ ready → module map-dependent giữ UNAVAILABLE (đúng hành vi) |

### 12. Migration path

**Incremental — không cần big-bang:**

1. **Phase 1:** Thêm `map_entity_read_verified` vào `available()` của module cần map (catch_spin khi auto-catch/nearby bật).
2. **Phase 2:** Mở rộng `ensure_observer_for_enabled_modules` hoặc observer pre-tick: retry `discover_scene_runtime_owners` khi desired module chờ map.
3. **Phase 3:** Kotlin listen capability change → `syncModules(forceUnavailableRetry=true)`.
4. **Optional Phase 4:** Capability `MAP_READY` riêng hoặc bridge event nếu UI cần hiển thị trạng thái.

Rollback: revert gate trong `available()` — module quay behavior cũ.

---

## Acceptance Criteria (from spec)

> Source: không có spec formal — inferred từ yêu cầu user và AGENTS.md fail-closed policy.

| ID | Rule / Requirement | Expected | Acceptance note |
|---|---|---|---|
| AC-1 | Vào game, module automation **không** observe/mutate trước khi map ready (với module phụ thuộc map) | Module map-dependent ở UNAVAILABLE hoặc disabled cho đến khi `map_entity_read_verified` | Done khi log không có map read/observe trước overworld load |
| AC-2 | Phát hiện map instance ready qua binding verified | `map_entity_read_verified=true` và/hoặc capability `READ_NEARBY` | Done khi diagnostic/scene discovery bind được `MapEntityService.get_Cells` |
| AC-3 | Sau map ready, module được enable theo config | `syncModules` / `set_module_enabled` success | Done khi catch_spin (nếu bật) chuyển ENABLED sau overworld |
| AC-4 | Module không phụ thuộc map vẫn hoạt động | discard/transfer enable khi inventory/transfer binding ready | Không block blanket |
| AC-5 | Fail-closed khi map không bao giờ xuất hiện | Không crash, không guess binding | UNAVAILABLE + log rõ, không fallback input/screenshot |
| AC-6 | Retry khi map load trễ sau DIAGNOSTIC | Re-probe + re-enable trong vòng reasonable (observer tick / diagnostic retry) | Done khi test cold start → login chậm vẫn enable được |

---

## Synthesis

### Key Insight

**Có thể implement**, và repo **đã có ~70% hạ tầng cần thiết**: module không auto-run khi attach process; map readiness được biểu diễn bằng `map_entity_read_verified` + `discover_scene_runtime_owners`. Thiếu phần **chờ có ý thức (wait/retry)** và **gate rõ ràng theo từng module** — hiện DIAGNOSTIC chạy sớm (T+3s) và catch_spin có thể enable trước khi map live vì `available()` chưa yêu cầu map.

“Lắng nghe map instance khởi tạo xong” trong kiến trúc này nên hiểu là **poll + verify binding trên main thread**, không phải Unity event listener thuần — trừ khi đầu tư hook (Option C).

### Recommended Approach

**Option B + tinh chỉnh Option A:**

1. Giữ nguyên “không enable module cho đến khi controller gọi” — đã đúng.
2. Với module cần map: thêm `map_entity_read_verified` (hoặc predicate `map_runtime_ready`) vào `available()`.
3. Thêm retry `discover_scene_runtime_owners` trong observer/control loop khi module desired nhưng map chưa verified.
4. Kotlin retry `syncModules` khi capabilities có `READ_NEARBY` mới.
5. **Không** gate global cho discard/transfer.

Không cần hook `MapScene.Initialize` ở bước đầu.

### Risks to Watch

- Map load trễ hơn 3s nhưng không có retry → module kẹt UNAVAILABLE mãi.
- Áp dụng gate map cho tất cả module → discard/transfer bị chặn oan.
- Gọi managed quá sớm trên main thread trước khi bridge ready → giữ fail-closed như hiện tại.

### Open Questions

- ~~User muốn **tất cả** module chờ map, hay chỉ catch_spin / map-mutation modules?~~ → **Resolved in Section 13:** tất cả feature module, không ngoại lệ discard/transfer.
- Có cần UI hiển thị “Đang chờ map…” trên overlay không?
- Auto-enable sau map ready có cần delay thêm (settle frame) sau `get_Cells` success lần đầu?
- Có scenario nào user vào game nhưng ở màn hình không có map lâu dài (cần timeout policy)?

---

## Section 13 — Yêu cầu mới: tất cả module luôn chờ map

**Input user:** “tất cả các module luon” — mọi automation module phải chờ map instance ready, không chỉ catch_spin.

### Phạm vi “tất cả module”

| Module | Gate map? | Ghi chú |
|---|---|---|
| catch_spin | **Có** | Trước đây có thể enable sớm (spin-only path) |
| discard | **Có** | Trước đây chỉ cần inventory; user chấp nhận delay đến overworld |
| transfer | **Có** | Tương tự discard |
| encounter | **Có** | Throw assist cũng chờ map |
| **core** (START/STOP/DIAGNOSTIC) | **Không** | Control plane — phải chạy trước map để probe và `discover_scene_runtime_owners` |

→ “Tất cả module” = **tất cả feature module** trong registry, **trừ** runtime core control.

### Thiết kế khuyến nghị (global gate)

**Một điểm gate duy nhất** thay vì sửa từng `available()` riêng lẻ — dễ audit và không sót module mới.

#### Native (ưu tiên)

Trong `set_module_enabled` (`runtime_feature_modules.inc`), trước `feature->available()`:

```cpp
if (module != Module::CORE && !binding.map_entity_read_verified) {
    // error: runtime_map_not_ready
    return false;
}
```

Hoặc helper `map_runtime_ready(binding)` dùng chung.

**Lợi ích:** Mọi module mới tự động bị gate; Kotlin nhận `runtime_module_unavailable` thống nhất.

#### Kotlin (bổ sung)

Trong `syncModules`, trước `setModuleEnabled`:

```kotlin
if (module != RuntimeFeatureModule.CORE && !ready.capabilities.contains("READ_NEARBY")) {
    // UNAVAILABLE, lastError = "map instance not ready"
    return@forEach
}
```

Double gate: Kotlin fail-fast theo capability đã advertise; native fail-closed nếu bridge stale.

### Map discovery khi chưa có module nào enabled

**Vấn đề gà-trứng:** module không enable → observer không chạy → re-probe trong observer không có.

**Giải pháp:** Map wait **độc lập** với module enable:

1. **Đã có:** `run_managed_runtime_diagnostic` gọi `discover_scene_runtime_owners` cuối DIAGNOSTIC.
2. **Cần thêm:** Retry map discovery trong vòng **diagnostic retry** (Kotlin `AUTO_DIAGNOSTIC_RETRY_DELAY_MS = 5s`) hoặc lightweight **map-wait tick** trên control thread native:
   - Chỉ chạy khi `g_runtime_active && !map_entity_read_verified && !any_enabled()`
   - Gọi `discover_scene_runtime_owners` trên main thread
   - Refresh capabilities → bridge push `RuntimeReady` mới
3. Kotlin `runAutomaticDiagnosticIfDue` / capability update → `syncModules(forceUnavailableRetry=true)`

Không cần enable bất kỳ feature module nào chỉ để chờ map.

### Hệ quả hành vi

| Trước | Sau (tất cả module) |
|---|---|
| discard/transfer có thể chạy ngay sau DIAGNOSTIC nếu inventory ready | Chờ overworld + `map_entity_read_verified` |
| catch_spin spin-only có thể chạy không cần map | Cũng chờ map |
| Module enable ~T+3s sau attach | Enable khi map live (có thể T+10s–30s tùy load) |
| User ở màn login/loading | Mọi feature module UNAVAILABLE — đúng ý user |

**Trade-off chấp nhận:** Auto-discard/transfer trễ hơn vài giây đến vài chục giây so với hiện tại. User đã chọn đổi latency lấy an toàn lifecycle.

### Acceptance Criteria cập nhật

| ID | Rule | Expected |
|---|---|---|
| AC-1 | **Mọi** feature module không observe/mutate trước map ready | catch_spin, discard, transfer, encounter đều UNAVAILABLE |
| AC-4 | ~~discard/transfer không bị block~~ | **Đổi:** discard/transfer **cũng** chờ map — done khi chỉ enable sau `READ_NEARBY` |
| AC-7 (mới) | Core control vẫn hoạt động trước map | START/DIAGNOSTIC chạy được; map discovery qua DIAGNOSTIC/retry |
| AC-8 (mới) | Map wait không phụ thuộc module enabled | `discover_scene_runtime_owners` chạy qua diagnostic/retry khi `!any_enabled()` |

### Implementation checklist (khi code)

1. `runtime_feature_modules.inc` — global map gate trong `set_module_enabled` (exclude CORE).
2. `runtime_control.inc` hoặc diagnostic loop — periodic map re-probe khi runtime active, map chưa verified, chưa có module enabled.
3. `RuntimeLifecycleCoordinator.kt` — gate `syncModules` on `READ_NEARBY` for non-CORE modules.
4. Optional: capability `MAP_READY` tách khỏi `READ_NEARBY` nếu sau này cần phân biệt “service bound” vs “cells non-empty”.
5. Log rõ: `runtime_map_not_ready` để debug UNAVAILABLE state trên overlay.

### Risks bổ sung (global gate)

- **Login screen lâu:** module không bao giờ start nếu user không vào overworld — hành vi mong muốn, cần message UI rõ.
- **Scene transition** (shop, raid lobby): map object có thể tạm unavailable — cần quyết định: disable module khi map mất, hay giữ enabled với binding cache? → Khuyến nghị phase 1: **chỉ gate lúc enable**, không auto-disable khi map tạm mất (tránh flapping).
- **Module CORE ngoại lệ** phải document rõ để không ai thêm gate nhầm lên control actions.

---

## Section 14 — Implemented (2026-09-12)

| File | Change |
|---|---|
| `zygisk/jni/shared/runtime/module/runtime_feature_modules.inc` | `map_runtime_ready()` + gate trong `set_module_enabled` (trừ `kRuntimeCore`); một lần `discover_scene_runtime_owners` trước khi reject `runtime_map_not_ready` |
| `app/.../RuntimeLifecycleCoordinator.kt` | Gate `syncModules` on `READ_NEARBY`; diagnostic retry khi managed ready nhưng map chưa ready; auto-retry UNAVAILABLE khi map vừa sẵn sàng |

Verified: `./gradlew test assembleDebug --rerun-tasks` passed.

---

## Section 15 — Reactive map-ready → immediate module start (2026-09-12)

**Gap:** Gate trước đó chỉ chạy khi engine loop gọi `ensureRunning` (~vài giây) hoặc DIAGNOSTIC retry 5s.

**Bổ sung:**

| Layer | Behavior |
|---|---|
| Native `runtime_control.inc` | Command loop dùng `poll(500ms)`; `maybe_probe_pending_map_readiness` gọi `discover_scene_runtime_owners` và push capabilities ngay khi map verified |
| `RuntimeBridgeClient` | `setRuntimeReadyListener` — fire khi nhận `RuntimeReady` mới từ broker |
| `RuntimeLifecycleCoordinator` | `onRuntimeCapabilitiesUpdated` — `syncModules` + config sync ngay khi `READ_NEARBY` xuất hiện |
| `HeadlessAutomationService` | Wire listener → coordinator |

Latency mục tiêu: **≤500ms** sau map instance live (poll interval native) + thời gian bridge round-trip.
