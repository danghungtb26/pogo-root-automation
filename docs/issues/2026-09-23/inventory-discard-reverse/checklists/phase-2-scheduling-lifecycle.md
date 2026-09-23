# Phase 2 — Scheduling và lifecycle

Plan: [Tổng quan](00-overview.md).

Điều kiện bắt đầu: T-003 và các binding/context task liên quan đạt. Điều kiện hoàn tất: native có intent mới tại invoke, barrier đúng action và maintenance tiếp tục an toàn khi cấu hình/lifecycle thay đổi.

Sau mỗi task, cập nhật ngay details dưới checkbox với file tạo/sửa/xóa/di chuyển, thay đổi method/param/hành vi và kiểm chứng thực tế trước khi chuyển task. Chỉ tick `[x]` khi Verify đạt; chưa đạt giữ `[ ]` và ghi phần còn thiếu.

- [x] **T-008** **[zygisk]** *(native)* — Đổi `DiscardModule::observe()` trong `DISCARD/module.inc` sang `request_main_thread_inventory(ProbeContext&, RuntimeInventoryObservation*)` ở `RT/mainthread/runtime_inventory_request.inc`; không trực tiếp đọc bag/list/cache trên observer thread chỉ vì thread đã attach IL2CPP. `DISCARD/inventory_reader.inc` kiểm baseline/owner generation theo T-007, đọc/copy count thuần trên main thread, trả unknown rõ khi fail và chỉ publish snapshot hợp lệ. Không giữ `get_AllItems()` Values collection qua tick. Giữ cadence bounded và tránh main-thread queue chồng lấn. **AC:** AC-04, AC-15. **Phụ thuộc:** T-003, T-007. **Verify:** mọi game read của đường discard đi qua main-thread bridge; zero chỉ xuất hiện sau valid baseline, read fail không publish bag rỗng; audit không dùng `used_slots/capacity` đang là curated sum để quyết định discard hoặc bag-full; payload capacity được xử lý tại T-015, không đổi nghĩa ngầm ở đây.

  <details>
  <summary>Đã hoàn thành — T-008</summary>

  `DiscardModule::observe()` chỉ gọi `request_main_thread_inventory()`; mọi
  `GetItemCount`, cache timestamp và list access chạy trên Unity main thread.
  `inventory_reader.inc` không publish snapshot khi read/baseline fail, và
  curated `used_slots/capacity` không tham gia admission. Bridge token tránh
  task chồng lấn. Runtime owner/no-delta evidence còn được ghi tại T-018/T-020.

  </details>

- [x] **T-009** **[zygisk]** *(native)* — Thay đường queue amount cũ bằng native intent ở `DISCARD/module.inc`, `execute.inc`, `recycle_item_action.inc`, `RT/mainthread/runtime_main_thread_actions.inc`, handler kStartDiscard và `shared/core/runtime_native_prelude.inc`. **Đề xuất — chưa tồn tại:** `RuntimeDiscardIntent` mang source auto/manual, item_id, desired revision, session/owner generation, action token; auto mang limit dự kiến, manual mang requested amount/command_id. Trong callback main thread, revalidate revision/toggle/expiry/identity rồi reread count có baseline và limit hiện hành trước preparation/invoke; auto tính excess bằng phép toán kiểm miền số, không replay amount đã queue. Manual không tự chuyển thành auto và không đổi lượng đã yêu cầu; lượng vượt count bị reject. Queue timeout giữ token ở trạng thái chưa rõ cho tới khi callback được xác nhận chưa invoke hoặc đã hoàn tất, không xóa token để dispatch lại. **AC:** AC-05, AC-06, AC-08, AC-13. **Phụ thuộc:** T-003, T-004, T-006, T-008. **Verify:** walkthrough count100→70/limit50 cho amount20; count<=limit không gọi game; revision/toggle thay đổi trước callback hủy candidate chưa invoke; amount<=0/>count, overflow, owner stale bị chặn; callback trễ sau timeout không tạo action thứ hai. Ghi trace đường chạy, device evidence ở T-019/T-020.

  <details>
  <summary>Đã hoàn thành — T-009</summary>

  - **Sửa:** `PendingRuntimeDiscard` mang source auto/manual,
    `config_revision`, `owner_generation`, `action_id`, inventory generation và
    Promise ownership; main-thread start revalidates intent before invoke.
  - **Policy:** auto tính excess hiện tại với overflow guard; manual giữ amount
    yêu cầu; amount vượt count/zero/owner stale/baseline thiếu bị reject.
  - **Verify:** codewalk 100→70/limit50 ⇒ 20, focused preparation/readiness
    tests PASS, native build PASS. Queue wait/Promise timeout chuyển unknown và
    không tạo action thứ hai; runtime late callback thuộc T-020.

  </details>

- [x] **T-010** **[zygisk]** *(native)* — Tách reset policy/candidate khỏi pending mutation trong `DISCARD/execute.inc`, `config.inc`, `module.inc` và `RT/control/runtime_desired_state_reconcile.inc`. `on_enable/on_disable`, CONFIG_SET và revision unrelated không được free Promise/clear blocked như đã hủy RPC. `finish_runtime_discard` và mọi poll nhận/kiểm action identity; old completion không thay state mới. Bổ sung maintenance/drain hook vào registry/observer và STOP/disconnect paths (`RT/module/runtime_feature_modules.inc`, `RT/observation/runtime_observation.inc`, `RT/control/runtime_control.inc`) theo T-003: `observe_enabled()` hiện chỉ chạy module enabled; các nhánh disable-all/stop-observer/socket failure đều phải được xét. Khi không thể tiếp tục poll an toàn, giữ uncertainty và yêu cầu rebootstrap/baseline hợp lệ khi nối lại, không đọc pointer stale. **Đề xuất — chưa tồn tại:** tách `DISCARD/pending_state.inc` nếu cần chứa identity, handle ownership và transition. **AC:** AC-12, AC-13. **Phụ thuộc:** T-003, T-009. **Verify:** bảng lifecycle có owner handle/token rõ cho queued, invoked, awaiting-reconcile, blocked; disable không phát action mới nhưng không làm mất request đang chạy; mất IPC không phát completion giả; owner/session mới không dùng pointer cũ; cold start không restore pending từ disk. Existing readiness checks chạy ở T-016; source đã chạm <=500 dòng.

  <details>
  <summary>Đã hoàn thành — T-010</summary>

  `reset_runtime_discard_state()` giữ active/blocked state và Promise handle.
  `runtime_feature_modules.inc` duy trì poll qua registry khi module disabled;
  `ensure_observer_for_enabled_modules()` vẫn giữ observer cho discard
  maintenance. `reconcile.inc` kiểm action/handle/owner trước khi clear barrier;
  config/STOP không restore pending từ disk. Unknown/owner stale không replay
  tự động. Verify bằng codewalk và native build; reconnect/cold relaunch live
  matrix vẫn là T-020.

  </details>

- [x] **T-011** **[zygisk]** *(native)* — Thống nhất admission barrier cho auto/manual discard và các mutation đang phối hợp. Sửa `DISCARD/execute.inc`, `module.inc`, `runtime_discard_blocks_catch_spin()` cùng caller native catch/spin/transfer hiện có: kiểm pending/unknown/reconcile trước queue và kiểm lại trong main-thread callback ngay trước invoke để tránh hai candidate cùng vượt check ở observer. Giữ policy ở module; shared runtime chỉ điều phối token/maintenance, host không quyết định item. Một item bị suppression không khóa toàn gameplay nếu mutation đã xác định kết quả và cache đã reconcile; request unknown vẫn giữ barrier theo contract. **AC:** AC-08, AC-10, AC-12, AC-13. **Phụ thuộc:** T-009, T-010. **Verify:** review interleaving manual+auto, discard+catch/spin/transfer, duplicate command và late queue callback: mỗi trường hợp chỉ một mutation được admit; giữ command id/expiry/sequence checks; không tạo lock-order inversion hay giữ binding mutex khi chờ main thread. Chạy existing navigation/readiness tests liên quan ở T-016.

  <details>
  <summary>Đã hoàn thành — T-011</summary>

  `runtime_discard_blocks_catch_spin()` now covers active and blocked/reconcile
  states; auto admission checks the same barrier, and manual start rechecks the
  pending identity immediately before `RecycleItem`. Existing catch/spin
  navigation/coordinator callers use the barrier; transfer keeps its own
  mutation barrier. No binding mutex is held while waiting for the main-thread
  bridge. Duplicate command, expiry and owner/config checks remain intact.

  </details>
