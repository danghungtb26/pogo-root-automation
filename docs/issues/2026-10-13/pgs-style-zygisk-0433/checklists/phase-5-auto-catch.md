# Phase 5 — Auto Catch: TryCapture primary, Virtual GO Plus fallback

Plan: [Tổng quan](00-overview.md)

## Điều kiện bắt đầu

- `T-408` đạt ở mức world-state implementation/build và live status còn
  `UNVERIFIED_DEVICE`.
- Catch contract `T-303` có owner/method/request/Promise/result/postcondition cụ thể.
- Scan contract `T-306` đã triển khai; Virtual GO Plus contract `T-307` có thể
  `SUPPORTED` hoặc giữ fallback capability `UNAVAILABLE` với blocker rõ.
- Build fingerprint 0.433.0 và action arbiter đang fail closed.

> Cập nhật reverse 2026-10-14: metadata map đã resolve
> `MapPokemon.TryCapture` token `0x06013ab5`, slot 44, arity 2, RVA
> `0x9e1caa4`; REA dossier `ev_51dac350...` xác nhận RVA nằm trong analyzed
> code. Exact function boundary/signature/ABI, Promise/result, captured ID,
> map postcondition và source eligibility vẫn thiếu; `VirtualGoPlus` chưa có
> qualifying contract. T-303/T-307 và T-501–T-506 do đó vẫn fail-closed.

## Task

- [ ] **T-501** **[zygisk]** *(native)* — Port direct binding/target resolver 0.433.0 trong `direct_map_bindings.inc`, `direct_map_actions.inc`, `map_actions.inc` và `runtime_catch_range.inc`: consume normalized targets từ mọi source `SUPPORTED_OFFLINE` của `T-403`, resolve lại live `MapPokemon`/subclass theo source + stable id trên main thread, validate encounter/species/coordinate/generation/expiry; source không hỗ trợ direct `TryCapture` phải có eligibility reason riêng, không ép cast. **AC:** `AC-13`, `AC-14`; candidate code chỉ chấp nhận type/session/freshness hợp lệ và live status giữ `UNVERIFIED_DEVICE` khi chưa có runtime proof. **Phụ thuộc:** `T-303`, `T-306`, `T-403`, `T-408`. **Verify:** Native build + existing host checks; audit source→resolver table và invalid/unknown/expired rejection paths. Live target/object match nằm trong handoff user.

  <details>
  <summary>Bị chặn — T-501</summary>

  PARTIAL/BLOCKED: exact managed `TryCapture` descriptor, token, request value
  layout và result fields đã được chứng minh statically và implement bằng
  hybrid resolver. T-303 vẫn thiếu live owner/source eligibility, concrete
  Promise lifecycle và authoritative postcondition; T-307 cùng typed
  world-state T-403/T-406 vẫn chưa có. `DIRECT_CATCH`/fallback giữ unavailable;
  không tạo success từ dispatch hoặc Promise-only result.

  </details>

- [ ] **T-502** **[zygisk]** *(native)* — Hoàn thiện policy trong `catch.inc`, `catch_proximity.inc`, `coordinator.inc`: chỉ dùng Poké Ball item id `1`, yêu cầu inventory/map/location fresh, range hợp lệ, chọn gần nhất rồi tie-break bằng stable target id, giữ một pending catch; hết ball trả `NO_BALL` và nhường spin, không retry loop. **AC:** Target và ball selection deterministic, fail closed khi state stale/unknown và tuân action arbiter. **Phụ thuộc:** `T-405`, `T-406`, `T-501`. **Verify:** Existing native checks; trace trên snapshot có nhiều target xác nhận ordering/range/tie-break; zero-ball/stale-state không dispatch mutation.

  <details>
  <summary>Bị chặn — T-502</summary>

  BLOCKED: exact direct-catch và Virtual GO Plus contracts T-303/T-307 chưa được chứng minh, đồng thời typed world-state T-403/T-406 chưa tồn tại. `DIRECT_CATCH`/fallback vẫn unavailable dưới profile fail-closed; không port method/layout cũ hoặc tạo success giả.

  </details>

- [ ] **T-503** **[zygisk]** *(native)* — Nối dispatch và terminal result qua `main_thread_direct_actions.inc`, `direct_catch_promise_observer.inc`, `direct_catch_map_sync.inc` và `requested_catch_ids.inc`: giữ Promise bằng GC handle đúng lifetime, parse authoritative server result/captured storage ID, chờ map postcondition, mark inventory/storage dirty; timeout là `INDETERMINATE` và suppress blind retry. Sửa `runtime_capabilities.inc` và `runtime_ui_status_bridge.inc` để `DIRECT_CATCH` bắt buộc binding/outcome/map-sync contract và hiển thị `UNVERIFIED_DEVICE` cho tới khi user xác minh, không như gate hiện tại chỉ dựa binding. **AC:** Code không báo success khi mới dispatch; failure/escape/no-ball/timeout là trạng thái riêng; captured storage ID được provenance-tag và capability không publish `LIVE_READY` sớm. **Phụ thuộc:** `T-303`, `T-502`. **Verify:** Native/host checks cho pending/terminal/timeout/dedupe paths, build/package và static gate review; success/failure server thật chuyển sang handoff user.

  <details>
  <summary>Bị chặn — T-503</summary>

  BLOCKED: exact direct-catch và Virtual GO Plus contracts T-303/T-307 chưa được chứng minh, đồng thời typed world-state T-403/T-406 chưa tồn tại. `DIRECT_CATCH`/fallback vẫn unavailable dưới profile fail-closed; không port method/layout cũ hoặc tạo success giả.

  </details>

- [ ] **T-504** **[bridge]** *(native/android)* — Mở rộng `runtime_automation_event_protocol.h`, `runtime_automation_event_bridge.inc`, `RuntimeAutomationEventMapper.kt`, `AutomationEvents.kt` và UI status để phát `CATCH_DISPATCHED`, terminal outcome, target id/species, captured storage ID chỉ ở boundary nội bộ, error reason và postcondition state. Notification/UI không dùng “caught” trước terminal success. **AC:** Event model và UI mapper chỉ cho Auto Transfer nhận success có storage ID hợp lệ. **Phụ thuộc:** `T-407`, `T-503`. **Verify:** Native/Gradle existing checks; protocol version/codec/mapper match và static event correlation review. UI/event live output chuyển sang handoff user.

  <details>
  <summary>Bị chặn — T-504</summary>

  BLOCKED: exact direct-catch và Virtual GO Plus contracts T-303/T-307 chưa được chứng minh, đồng thời typed world-state T-403/T-406 chưa tồn tại. `DIRECT_CATCH`/fallback vẫn unavailable dưới profile fail-closed; không port method/layout cũ hoặc tạo success giả.

  </details>

### T-505 — Chuyển khỏi phạm vi agent

Theo yêu cầu người dùng ngày 2026-10-13, E2E direct Auto Catch trên BlueStacks
không còn là task agent và không chặn code-complete. Trạng thái trước khi đổi:
chưa thực hiện. Nội dung session OFF → user arm → catch outcome/map sync → kill
→ relaunch OFF được chuyển nguyên vẹn sang checklist handoff do `T-804` tạo.

- [ ] **T-506** **[zygisk]** *(native)* — Thêm strategy/fallback tách file (đề xuất — chưa tồn tại: `catch_strategy.inc`, `virtual_go_plus_binding.inc`, `virtual_go_plus_action.inc`) theo contract `T-307`. Strategy `AUTO` ưu tiên direct `TryCapture`; chỉ chọn `VIRTUAL_GO_PLUS` trước dispatch nếu direct unavailable và fallback contract có đủ offline evidence. Cả hai dùng cùng manual arm, target snapshot, Poké Ball item 1 và action arbiter; cấm fallback nếu direct đã dispatch/pending/indeterminate, đồng thời dedupe stable target id xuyên strategy. Live status của cả hai giữ `UNVERIFIED_DEVICE`. **AC:** `AC-13`, `AC-15`; direct là primary, fallback không tạo duplicate catch và thiếu fallback evidence không làm hỏng direct. **Phụ thuộc:** `T-307`, `T-503`, `T-504`. **Verify:** Native checks/build; host/static strategy matrix direct/fallback/neither/pending/indeterminate cho đúng selection/rejection; không chạy game/device.

  <details>
  <summary>Bị chặn — T-506</summary>

  BLOCKED: exact direct-catch và Virtual GO Plus contracts T-303/T-307 chưa được chứng minh, đồng thời typed world-state T-403/T-406 chưa tồn tại. `DIRECT_CATCH`/fallback vẫn unavailable dưới profile fail-closed; không port method/layout cũ hoặc tạo success giả.

  </details>

### T-507 — Chuyển khỏi phạm vi agent

Theo yêu cầu người dùng ngày 2026-10-13, E2E Virtual GO Plus fallback trên
BlueStacks không còn là task agent. Trạng thái trước khi đổi: chưa thực hiện.
Các case direct ưu tiên, fallback khi direct unavailable, không fallback sau
pending/indeterminate, Poké Ball 1 và kill/relaunch OFF được chuyển sang handoff
do `T-804` tạo. Fallback giữ `UNVERIFIED_DEVICE` cho tới khi user trả kết quả.

## Điều kiện hoàn tất

- Catch chỉ dùng validated full-source world state và manual arm; direct
  `TryCapture` là primary, Virtual GO Plus là fallback capability-gated.
- Promise outcome + map postcondition quyết định terminal state.
- Captured storage ID có provenance và sẵn sàng cho transfer, không lộ như success giả.
- Kill/disconnect/session mới làm Auto Catch OFF và không có failover sau dispatch.
- Native/Android code, host checks và artifacts hoàn tất; E2E direct/fallback là
  phần user tự chạy sau handoff, không phải điều kiện tick task agent.

Sau mỗi task, agent phải cập nhật checkbox và khối `<details>` của chính task đó
trước khi chuyển sang task kế tiếp.
