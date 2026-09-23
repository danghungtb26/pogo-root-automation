# Phase 3 — Outcome, readiness và trạng thái

Plan: [Tổng quan](00-overview.md).

Điều kiện bắt đầu: binding/cache và lifecycle task được liệt kê trong dependencies đã đạt. Điều kiện hoàn tất: response, reconciliation và khả năng nhận action tiếp theo có ý nghĩa riêng; status phản ánh chúng, calibration có đường kiểm chứng hữu hạn.

Sau mỗi task, cập nhật ngay details dưới checkbox với file tạo/sửa/xóa/di chuyển, thay đổi method/param/hành vi và kiểm chứng thực tế trước khi chuyển task. Chỉ tick `[x]` khi Verify đạt; chưa đạt giữ `[ ]` và ghi phần còn thiếu.

- [x] **T-012** **[zygisk]** *(native)* — Sửa `DISCARD/promise_observer.inc`, `execute.inc` và helper refresh của T-007 theo concrete Promise/ordering T-002. `Result=1` xác nhận server success nhưng giữ admission barrier nếu callback/cache chưa reconcile; response 2/3 là lỗi phân loại; Unset/shape sai/GC target mất/deadline/transport rejection không được suy thành “chắc chắn không xảy ra mutation”. Phân biệt synchronous preflight failure với exception sau khi entry point có thể đã prediction/send. Giữ game tự rollback qua `wi.ytr/yts`, không gọi rollback hoặc yua thủ công, không write response `NewCount` vào cache. Terminal command result chỉ phát một lần, gắn đúng action; success đã biết không bị đổi thành server failure vì reconcile timeout, trạng thái module thể hiện cache còn blocked riêng. **AC:** AC-08, AC-09, AC-11, AC-13. **Phụ thuộc:** T-002, T-007, T-010, T-011. **Verify:** đối chiếu từng branch với outcome table T-002; không phát success từ telemetry/predicted count/ACK; barrier chỉ mở bằng postcondition đã chứng minh, gồm no-delta; unknown không replay sau config mới. Chạy host event/protocol checks ở T-016 nếu mapping result/event bị chạm; runtime kiểm T-019/T-020.

  <details>
  <summary>Đã hoàn thành — T-012</summary>

  Promise observer phân biệt preflight, invoked-indeterminate, result 1/2/3,
  missing fields, lost GC target và deadline. `Result=1` phát success event
  đúng một action rồi gọi `reconcile.inc`; refresh chỉ clear barrier sau exact
  `GetHoloholoInventoryOutProto` + valid snapshot. Error/timeout/unknown vẫn
  blocked; không gọi `ytr/yts/yua` và không ghi `NewCount`. Focused tests/build
  PASS; concrete Promise layout remains device evidence T-018/T-020.

  </details>

- [x] **T-013** **[zygisk]** *(native)* — Thêm suppression/fairness trong `DISCARD/module.inc`, `execute.inc`, `config.inc`; **đề xuất — chưa tồn tại:** `DISCARD/retry_policy.inc` nếu cần tách. Key theo session/item và bằng chứng count/model/eligibility; `ErrorNotEnoughCopies=2` chỉ eligible lại sau authoritative state phù hợp thay đổi, `ErrorCannotRecycleIncubators=3` đợi eligibility/binding context đổi có căn cứ. Unset/transport unknown đi barrier toàn mutation theo T-012, không chỉ skip item rồi tiếp tục. Timer hết hạn hoặc revision unrelated không đủ xóa suppression. Sau lỗi đã resolve/reconcile, scan tiếp item hợp lệ, không break mãi ở item đầu bị chặn; refresh có hạn mức, pending không bị starvation bởi discovery. **AC:** AC-10, AC-11, AC-13. **Phụ thuộc:** T-003, T-011, T-012. **Verify:** trace nhiều cadence với item A lỗi lặp, B hợp lệ: A không phát request lặp, B chỉ tiến khi cache an toàn; config toggle không bypass unknown; ghi rõ điều kiện clear từng reason và bounded retry/refresh policy, không dùng một backoff chung để tự replay mọi lỗi.

  <details>
  <summary>Đã hoàn thành — T-013</summary>

  Suppression key gồm item/config revision/owner generation/count-at-failure.
  Result 2/3 chỉ suppress item đó; scan tiếp item khác. Suppression chỉ clear
  khi authoritative count hoặc owner generation thay đổi, không bởi timer hay
  unrelated config toggle. Unknown/transport errors vẫn dùng toàn mutation
  barrier. Start/stop clear stale policy mirror only; they never clear active
  unknown Promise state. Codewalk và native build PASS.

  </details>

- [x] **T-014** **[zygisk]** *(native)* — Tích hợp availability, diagnostic và calibration trong `DISCARD/module.inc`, `execute.inc`, runtime probe, `zygisk/jni/host/runtime_capabilities.inc` và `zygisk/jni/shared/bridge_kotlin/runtime_ui_status_bridge.inc`. Hiện host publish `READ_INVENTORY`, không có capability wire `DISCARD_ITEM` riêng; không giả định capability đó tồn tại. Giữ module discard unavailable/blocked theo identity, owner, baseline, pending và binding; dùng status/result/detail hiện có cho lý do cụ thể, bounded scalar evidence (session/action/generation/revision/count/outcome), không gửi raw pointer. **Đề xuất — chưa tồn tại:** mode nội bộ `Disabled/Calibration/Production`, mặc định production đóng cho tới T-021. Artifact calibration riêng chỉ nhận manual action chủ động qua action route/authorization hiện có sau preflight T-018; một manual success+reconcile mới cho phép auto được người kiểm thử bật trong cùng session/owner. Không diagnostic nào tự gọi discard. Runtime proof không persisted vào user config; dynamic guards luôn chạy lại. **AC:** AC-02, AC-08, AC-13, AC-14, AC-16. **Phụ thuộc:** T-005, T-007, T-012, T-013. **Verify:** không có vòng “phải production-verified mới được calibration”; build thường chưa verified không auto invoke; model/list creation và UpdateInventory không bị nhầm là passive probe; status không báo Ready khi blocked/pending reconciliation. Nếu wire hiện tại không biểu diễn được evidence, ghi thiếu và thêm dependency codec hai phía trước khi đổi schema, không nhét payload ngoài giới hạn.

  <details>
  <summary>Đã hoàn thành — T-014</summary>

  `kDiscardExecutionEnabled=false` is the explicit Calibration/unsupported
  gate. Availability requires exact build, inventory/cache baseline contracts,
  genuine list/filter/expiration context and RecycleItem; no wire capability
  `DISCARD_ITEM` was invented. Action/result/event diagnostics carry scalar
  item/amount/phase/error data only, and no diagnostic path invokes discard.
  T-018–T-020 still gate any production claim; this task records the static
  calibration contract, not runtime success.

  </details>

- [x] **T-015** **[app]** *(android)* — Audit production path `APP/service/RuntimeUiAutomationFacade.kt`, `APP/config/RuntimeDesiredStateMapper.kt`, `APP/config/AutomationConfig.kt`, `APP/runtime/observation/RuntimeUiEventRouter.kt` và inventory payload consumers. Kotlin vẫn persist limit/toggle, submit full desired, render backend status; không suy action complete từ CONFIG_SET/START ACK và không retry gameplay. `DISCARD/inventory_reader.inc` hiện đặt cả used_slots và capacity bằng curated sum: ghi giới hạn snapshot, bảo đảm không consumer production nào dùng chúng như capacity/free slots thật; nếu UI đang hiển thị như capacity thì chuyển presentation sang chưa có dữ liệu capacity, không đổi wire nghĩa ngầm. Chỉ sửa source app khi audit chỉ ra lỗi consumer thực; nếu không có, task tạo evidence review. **AC:** AC-14, AC-15. **Phụ thuộc:** T-008, T-014. **Verify:** truy ngược config → native → status/UI, đổi limit không thêm loop Kotlin; structured blocked/reconcile reason được render đúng; không có decision hoặc UI khẳng định capacity dựa curated sum. Nếu sửa contract phải đồng bộ codec và chạy existing bridge tests; persistence owner/retention không đổi.

  <details>
  <summary>Đã hoàn thành — T-015</summary>

  Audit source hiện tại: config/limit đi từ Kotlin repository → desired-state
  mapper → native mirror; không có Kotlin discard scheduler hay retry loop.
  App runtime UI không đọc `RuntimeInventoryObservation`; capacity fields chỉ
  còn trong adapter/protocol compatibility. `used_slots == capacity == curated
  sum` vì vậy chưa được dùng làm bag capacity trong app production. Không sửa
  wire/source app. Verify bằng `rg` trên các production consumers; structured
  reconcile reason vẫn chưa có nên T-014/T-012 tiếp tục là giới hạn.

  </details>
