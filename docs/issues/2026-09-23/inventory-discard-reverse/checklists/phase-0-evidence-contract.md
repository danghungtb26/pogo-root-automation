# Phase 0 — Bằng chứng và contract

Plan: [Tổng quan](00-overview.md). Dùng alias đường dẫn và đính chính AC-03 tại overview.

Điều kiện bắt đầu: đọc brainstorm, AGENTS.md, kiến trúc và source hiện hành; ghi nhận diff có sẵn. Điều kiện hoàn tất: có contract tĩnh đủ triển khai guards; điều kiện chỉ runtime mới chứng minh được chuyển thành gate T-018–T-020, không coi đã xác minh.

Sau mỗi task, cập nhật ngay details dưới checkbox với file tạo/sửa/xóa/di chuyển, thay đổi method/param/hành vi và kiểm chứng thực tế trước khi chuyển task. Chỉ tick `[x]` khi Verify đạt; chưa đạt giữ `[ ]` và ghi phần còn thiếu.

- [x] **T-001** **[reverse]** *(native)* — Chốt binding/model contract của `ItemInventoryService`. Tái dùng D:486503 và S:417755–417756: **5** param theo overview; đọc body wrapper `0x7E7F4A8` → `chwm` `0x7E7E590`, callers và filter `chvt` để xác định null contract của `showUnusableFilter`, `showUnavailableFilter`, `itemExpiredCallback`. Ghi bảng từng delegate: nguồn instance, signature, khi nào null hợp lệ và lifetime; xác minh service owner, nguồn row, duplicate expiring row và quan hệ `get_ExpiringItemsCopy()` với field `eece` được thay khi tạo model. Đối chiếu `CanItemBeRecycled(ItemSettingsProto)` D:486527/RVA `0x7E7F918`, expiration D:105339–105381. Ghi kết quả trong details hoặc evidence của issue mới. **AC:** AC-02, AC-03, AC-07, AC-16. **Phụ thuộc:** Không. **Verify:** dẫn chứng metadata + binary cho mỗi contract được chốt; tách rõ runtime hypotheses; không còn hướng sửa arity thành 6; không dùng null/empty context chưa chứng minh làm default triển khai.

  <details>
  <summary>Đã hoàn thành — T-001</summary>

  - **Tạo:** `docs/issues/2026-09-23/inventory-discard-reverse/implementation-evidence.md` ghi contract 5 tham số, owner/model context và giới hạn runtime.
  - **Sửa:** `zygisk/jni/modules/discard/inventory_binding_contract.inc` thêm guard return/owner/arity/type cho `ListSortedPlayerInventory` và `RecycleItem`; discovery và fallback cùng dùng contract.
  - **Sửa:** `item_data_factory.inc` bỏ supplier/empty-set fallback; `recycle_item_action.inc` yêu cầu `itemExpirationData` trên Android trước mutation.
  - **Kiểm chứng:** đối chiếu reverse `dump.cs.gz`/`script.json.gz` ở D:486502–486503, D:68169–68170, D:105339–105381 và S:417754–417757; ba focused host tests discard/readiness pass; `git diff --check` pass. Nullability/lifetime của delegate và service vẫn là runtime gate, không được ghi là đã xác minh.
  - **Sai khác:** không đổi arity thành 6; chưa mở production mutation.

  </details>

- [x] **T-002** **[reverse]** *(native)* — Chốt dấu hiệu completion và cache readiness. Từ `RecycleItem(ItemData,int,ISet<Item>)` D:68170/RVA `0x81689A8`, lần theo concrete return sau `Then/Catch`, layout/MethodInfo và thứ tự game callbacks so với fields mà `DISCARD/promise_observer.inc` đang đọc. Đối chiếu rollback `wi.ytr`/`wi.yts` và `yua`; đọc phần cần thiết của `InventoryCache.UpdateInventory()` D:270918/RVA `0x88DD138`, `drz.bqcb` D:270891, `HandleInventoryDelta` D:20259, `ItemBagImpl.yud/yuj/yuk`. Chốt tín hiệu baseline, delta generation và prediction đã reconcile, bao gồm refresh no-delta. Nếu dùng generic cache read, xác định specialization/rgctx đúng; không gọi generic RVA với null MethodInfo. **AC:** AC-04, AC-09, AC-11, AC-13. **Phụ thuộc:** Không. **Verify:** có bảng outcome → bằng chứng server → postcondition cache → có được mở action mới không; có nhánh transport unknown không được mở chỉ vì local rollback hoặc một refresh thành công; từng observation đề xuất có nguồn và giới hạn; thiếu bằng chứng thiết kế thì task liên quan giữ blocked.

  <details>
  <summary>Đã hoàn thành — T-002</summary>

  - **Reverse:** đối chiếu `RecycleItem` route 137, `wi.ytr` success, `wi.yts`
    rollback và `InventoryCache.UpdateInventory` route 4/`bqcb` full-response
    handler trong `implementation-evidence.md`.
  - **Implementation:** Promise `Result` chỉ phân loại action; baseline dùng
    `GetLatestTimestamp`; reconcile yêu cầu exact
    `GetHoloholoInventoryOutProto` và valid copied snapshot. Full response
    không đổi timestamp vẫn được chấp nhận là no-delta; error/timeout/layout/
    stale owner giữ barrier.
  - **Verify:** codewalk, native multi-ABI build và focused tests pass.
    Concrete Promise layout/no-delta trên thiết bị vẫn thuộc T-018/T-020 và
    không bị nâng thành device PASS.

  </details>

- [x] **T-003** **[docs]** *(native)* — Ghi contract state machine và calibration dựa trên `DISCARD/execute.inc`, `config.inc`, `module.inc`, `RT/control/runtime_desired_state_reconcile.inc`, `RT/module/runtime_feature_modules.inc` và `RT/observation/runtime_observation.inc`. **Đề xuất — chưa tồn tại:** state `Idle/Queued/Invoked/AwaitingReconcile/BlockedUnknown`, identity `(session, owner_generation, action_id, config_revision)`, admission phân biệt auto/manual, suppression từng item, maintenance khi disabled. Auto intent mang item/limit/revision; manual giữ amount người gọi và không tự tăng amount. Chốt bảng chuyển trạng thái cho config mới, disable, timeout 5 giây của queue, deadline Promise hiện 15 giây, disconnect, STOP, owner đổi và process chết. Thiết kế calibration tách static binding → runtime preflight → một manual action có guard → auto trong session đã chứng minh → production; không để yêu cầu “đã action-verified” chặn chính action calibration đầu tiên. **AC:** AC-05, AC-08, AC-10, AC-12, AC-13, AC-14. **Phụ thuộc:** T-001, T-002. **Verify:** walkthrough 100→70/limit50 cho amount20, queue cũ bị từ chối trước invoke, disable không coi RPC là hủy, callback cũ không giải phóng action mới; ghi rõ đường maintenance không phụ thuộc `observe_enabled()` và reconnect cần baseline mới. Không có pointer/pending/readiness persisted vào config.

  <details>
  <summary>Đã hoàn thành — T-003</summary>

  - **Tạo/cập nhật:** `implementation-evidence.md` ghi state
    `Idle/Queued/Invoked/AwaitingReconcile/BlockedUnknown`, identity gồm
    `action_id`, `owner_generation`, `config_revision`, cùng calibration stages.
  - **Sửa:** `execute.inc`, `reconcile.inc`, `promise_observer.inc` giữ Promise
    handles/barrier qua config và disable; registry observer gọi maintenance
    discard ngay cả khi module disabled; `reset_runtime_inventory_readiness`
    invalidates owner/session readiness.
  - **Verify:** walkthrough 100→70/limit50, queue identity và no-persistence
    đã review; runtime device transitions vẫn được ghi riêng tại T-020.

  </details>
