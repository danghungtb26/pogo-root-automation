# Phase 5 — Bật hỗ trợ và bàn giao

Plan: [Tổng quan](00-overview.md).

Điều kiện bắt đầu: T-016–T-020 đạt; mọi runtime gap ảnh hưởng admission/reconcile đã đóng. Điều kiện hoàn tất: artifact production giữ guards đã xác minh và tài liệu phản ánh bằng chứng thực, không chỉ tình trạng compile.

Sau mỗi task, cập nhật ngay details dưới checkbox với file tạo/sửa/xóa/di chuyển, thay đổi method/param/hành vi và kiểm chứng thực tế trước khi chuyển task. Chỉ tick `[x]` khi Verify đạt; chưa đạt giữ `[ ]` và ghi phần còn thiếu.

- [ ] **T-021** **[zygisk]** *(native)* — Chuyển rollout mode/gate của T-014 sang production **chỉ cho build/ABI đã có bằng chứng T-018–T-020**; giữ dynamic checks mỗi session và invalidation theo owner, không set `discard_verified` chỉ vì resolve được tên method hoặc module bật. Calibration-only admission không được thành đường bypass production. Rà `DiscardModule::available`, status publish và READ_INVENTORY guards không rộng hơn evidence. Build lại artifact cuối bằng script T-017, chạy lại focused checks của code/gate thay đổi và full Gradle khi source thay đổi; cài/kiểm smoke + một controlled discard/reconcile trên chính artifact cuối khi nằm trong scope được giao. **AC:** AC-02, AC-08, AC-11, AC-13, AC-16. **Phụ thuộc:** T-020. **Verify:** artifact cuối và runtime identity được ghi, supported build hoạt động với guard, unsupported/unverified build fail closed theo source và diagnostic phù hợp; không tái sử dụng cờ readiness từ config. Nếu SDK/device/evidence còn thiếu, giữ gate đóng và `[ ]`, không báo production-ready.

  <details>
  <summary>Chưa thực hiện — T-021</summary>

  Bị chặn đúng chủ ý: T-018–T-020 chưa có device evidence, nên không chuyển
  `kDiscardExecutionEnabled` sang production và không phát hành artifact như
  mutation-ready. Static implementation hiện fail closed.

  </details>

- [x] **T-022** **[docs]** *(reviewer)* — Cập nhật `docs/architecture/inventory-and-discard.md` theo implementation cuối: native ownership, chữ ký list **5 tham số**, genuine row/expiration/set, count freshness, policy tại invoke, pending qua config/lifecycle, game rollback, server outcome khác cache readiness và capacity chưa xác minh. Chỉ sửa `docs/ARCHITECTURE.md` nếu cấu trúc/ownership thực đổi; không viết vào issue brainstorm cũ. Trong issue mới, ghi AC→implementation→evidence cuối, liên kết task details và các giới hạn runtime còn lại; kiểm diff không ghi đè thay đổi người dùng. **AC:** AC-01–AC-16 ở mức tài liệu/bàn giao, không thay verification còn thiếu. **Phụ thuộc:** T-021. **Verify:** link/path/symbol hợp lệ, không còn mô tả synthetic context hoặc Kotlin live orchestration là production, không còn hướng sửa arity thành 6 trong tài liệu triển khai; mọi task tick có kết quả thật, `git diff --check` sạch, source đã sửa <=500 dòng và không có test code mới/sửa. Chỉ kết luận toàn plan hoàn tất khi DoD đạt.

  <details>
  <summary>Đã hoàn thành — T-022</summary>

  Đã cập nhật `docs/architecture/inventory-and-discard.md` để phản ánh native
  ownership, main-thread reads, 5-param contract, genuine context, Promise /
  refresh barrier và capacity chưa xác minh. Không đánh dấu rollout hoàn tất:
  T-021 vẫn blocked bởi T-018–T-020 và tài liệu giữ rõ giới hạn này.

  </details>
