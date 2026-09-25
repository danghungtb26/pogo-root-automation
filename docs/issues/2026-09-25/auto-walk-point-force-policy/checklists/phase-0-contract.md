# Phase 0 — Contract kiến trúc và discovery

Plan: [Tổng quan](00-overview.md)

Điều kiện bắt đầu: brainstorm và `AGENTS.md` đã được đọc; phạm vi generic candidate và việc hoãn fort-walk đã được người dùng xác nhận.
Điều kiện hoàn tất: contract candidate/active-route đủ rõ cho core, bridge và Kotlin tasks; source/test entrypoints được xác nhận từ checkout/CI.

Sau mỗi task, kiểm tra diff và kết quả Verify, cập nhật ngay khối details dưới task trước khi chuyển tiếp. Chỉ tick `[x]` khi Verify đạt. Không tạo hoặc sửa test code theo `AGENTS.md`.

- [x] **T-001** **[docs]** *(coder)* — Tạo `docs/issues/2026-09-25/auto-walk-point-force-policy/candidate-contract.md` làm contract kiến trúc: producer/native, bridge ingress, Kotlin admission policy, active route và location executor; chốt `candidateId`, `force`, capability gate, owner, generation/session, idle/active/terminal states và các `CandidateDecision`. Freshness chỉ kiểm lúc admission; route accepted được giữ trong RAM và tick tới terminal/local arrival, không có lease/renew. Ghi Q-01 với default `force` không hủy route USER; ghi Q-02 rằng không thêm native ack cho tới khi có consumer cần quyết định. UI nhập tọa độ trong tương lai dùng cùng USER route/executor sau validation, không cần native protocol. **AC:** AC-01…AC-06, AC-09. **Phụ thuộc:** Không. **Verify:** đối chiếu toàn bộ state transitions với brainstorm; mỗi transition có input, owner, route lifetime/result và fail-closed outcome; `git diff --check` sạch.

  <details>
  <summary>Đã hoàn thành — T-001</summary>

  - Tạo `candidate-contract.md`, định nghĩa producer/ingress/admission/executor/gameplay ownership; candidate ID, force, session/sequence, generation, owner và active-route lifecycle trong RAM.
  - Decision outcomes gồm `ACCEPTED`, `IGNORED_BUSY`, `REPLACED`, `UNCHANGED`, reject owner/stale/invalid, stale terminal và terminal route outcomes. B với `force=false` không thay A; A tiếp tục tick tới terminal/local arrival. Force chỉ thay native owner. Contract phân biệt native terminal wire (session + unique candidate ID) với generation Kotlin nội bộ cho tick/provider callback.
  - Sau làm rõ của người dùng, cập nhật state matrix: freshness chỉ tại ingress/admission; bỏ lease/renew/expiry; route kết thúc khi local arrival, native STOP/ARRIVED, force replacement, USER preemption, provider failure hoặc session reset. Q-01 giữ USER priority; Q-02 không thêm native ack.
  - Ghi thêm khả năng mở rộng UI: người dùng nhập/validate tọa độ rồi reuse USER route API/executor hiện có; không thêm native protocol, persistence mặc định hoặc loop movement thứ hai. Verify cuối: `git diff --check` sạch.
  - Verify: đối chiếu brainstorm AC/state transitions và rà contract đầy đủ; `git diff --check` exit 0; `rg -n '[[:blank:]]+$' .../candidate-contract.md` không tìm thấy trailing whitespace (exit 1 do không có match). Contract dài 88 dòng.

  </details>

- [x] **T-002** **[docs]** *(reviewer)* — Xác nhận implementation map cho contract bằng source hiện hành: Kotlin caller/service/overlay, native observation forwarding, protocol type/version, và test/build entrypoints trong `.github/workflows/ci.yml` cùng `zygisk/tests/`. Ghi path thực tế, dependency/include order và mọi khác biệt với `AGENTS.md` vào `candidate-contract.md`; không suy ra game method hay tạo test file. **AC:** AC-07. **Phụ thuộc:** T-001. **Verify:** mỗi target file trong phase sau tồn tại hoặc được đánh dấu `[new]`; lệnh test C++ chỉ được ghi nếu file đó tồn tại trong checkout/CI; diff không sửa source/test.

  <details>
  <summary>Đã hoàn thành — T-002</summary>

  - Bổ sung vào `candidate-contract.md` implementation map cho core/protocol/Kotlin ingress/location/native broker/fort feature; đánh dấu mọi file mới bằng `[new]` và ghi vị trí `main.cpp` include dependency, nested `coordinator.inc` → `navigation.inc`, single-translation-unit constraint.
  - Ghi caller path thực tế: `HeadlessAutomationService` → `RuntimeUiAutomationFacade` → `RuntimeUiEventRouter` → `NativeNavigationReceiver`; location path `AutoFortNavigationBus` → `JoystickOverlayService` → `JoystickLocationController`; capability publishing trong `runtime_ui_status_bridge.inc`/`host/runtime_capabilities.inc`.
  - Đối chiếu `.github/workflows/ci.yml` và `zygisk/tests/`: actual native test entrypoints được ghi; `runtime_command_protocol_test.cpp` và `runtime_observation_protocol_test.cpp` trong `AGENTS.md` không tồn tại trong checkout nên không được kê lệnh. Không có candidate-specific native host test; không tạo test code.
  - Verify: các current target paths đã được đọc/liệt kê từ checkout; files mới được đánh dấu `[new]`; command C++ ghi trong tài liệu chỉ trỏ test thấy trong workflow. `git diff --check` exit 0, `rg -n '[[:blank:]]+$' .../candidate-contract.md` không có match. Source/test không đổi; chỉ sửa tài liệu feature.

  </details>
