# Kế hoạch: Point-walk candidate và force policy

**Ngày:** 2026-09-25
**Nguồn:** [Brainstorm](../brainstorm.md)
**Phạm vi:** Thiết kế/triển khai contract tổng quát cho native point candidate → Kotlin fake-location; tích hợp và sửa flow fort-walk ở phase riêng sau khi contract generic ổn định.

**Trạng thái thực thi:** T-001…T-009 hoàn tất. T-010 đã qua full Gradle, native host checks và multi-ABI build; runtime matrix giữ mở vì script xác nhận BlueStacks Air 1 (`127.0.0.1:5565`) chưa kết nối.

## Mục tiêu

Kotlin nhận candidate tọa độ từ native, lưu route đã được nhận trong RAM, rồi để location controller tick theo thời gian và dịch chuyển fake location từng bước tới target. Khi B có `force=false` trong lúc A còn active, Kotlin bỏ qua B và tiếp tục A tới arrival/terminal; candidate đơn lẻ không có lease và không cần native renew. Native vẫn chọn candidate theo game state, còn Kotlin sở hữu policy thực thi fake-location và location input arbitration.

Flow `RuntimeAutoFortNavigation` ban đầu là baseline để tìm coupling. T-008/T-009 đã chuyển producer/consumer fort-walk sang candidate contract mà không chuyển fort target selection khỏi native.

## Phạm vi

- Định nghĩa candidate/active-route lifecycle, `force`, owner, freshness lúc admission, terminal correlation và kết quả admission.
- Thêm model policy thuần trong `core/`, payload có version trong `bridge/protocol/` và C++ wire/forwarding tương ứng.
- Thêm Kotlin admission/owner handling trên đường native candidate tới mock-location controller, đồng thời giữ user location input arbitration.
- Sau khi các phần nền tảng đạt Verify, T-008/T-009 sửa native auto-fort producer và Kotlin adapter để dùng contract mới.
- Không thêm hoặc sửa test code. `AGENTS.md` cấm tạo test files/test cases hoặc sửa test; chạy test hiện có vẫn được phép và theo yêu cầu khi bàn giao code.

## Ngoài phạm vi

- Không thêm PoGo/Unity binding hoặc đọc raw game state từ Kotlin.
- Không đổi cách native chọn fort/candidate trong phase contract, bridge và Kotlin policy.
- Không thêm transport thứ hai, file queue hay persistence cho active route.
- Không triển khai ngay khi chỉ có plan; mỗi task vẫn cần được giao/thực hiện riêng.

## Giả định và quyết định cần giữ

- Mặc định an toàn: `force` chỉ thay active route native; không chiếm joystick, favorite, map target do user chọn. Nếu muốn force vượt owner USER, cần requirement rõ và cập nhật AC-04 trước khi code phụ thuộc vào đó.
- Candidate hợp lệ được nhận khi không có native route active, kể cả `force=false`.
- Active A tiếp tục tới local arrival, native STOP/ARRIVED, force replacement, thao tác USER, provider failure hoặc session reset.
- Candidate/terminal mang identity hoặc generation và session để lệnh cũ không tác động lên route mới.
- Kết quả admission tối thiểu phân biệt accepted, ignored-busy, replaced và rejected stale/invalid/owner-conflict. T-001 chốt nơi hiển thị/ghi các kết quả này; chưa mặc định thêm native acknowledgement.
- Không cần binding PoGo để triển khai contract cho tọa độ đã có. Khi sửa fort-walk, chỉ tái dùng binding hiện được guard và rà lại source/reverse nếu cần đổi game-side semantics.

## Nguồn và baseline

- `docs/ARCHITECTURE.md:7-12`: native giữ gameplay/runtime decision; Kotlin được phép xử lý walk-to-location và fake location.
- `docs/issues/2026-09-25/auto-walk-point-force-policy/brainstorm.md`: AC-01…AC-09, API `NativeWalkCandidate` và state transitions.
- Baseline Kotlin: `NativeNavigationReceiver.kt`, `AutoFortNavigationBus.kt`, `JoystickLocationController.kt`, `RuntimeUiEventRouter.kt`, `RuntimeUiStateStore.kt`, `JoystickOverlayService.kt`.
- Baseline native: `zygisk/jni/shared/bridge_kotlin/runtime_navigation_protocol.h`, `runtime_navigation_bridge.inc`, `runtime_bridge_broker.inc`, `zygisk/jni/modules/catch_spin/auto_fort_navigation.h`, `navigation.inc`.
- Baseline protocol: `bridge/protocol/.../BridgeProtocol.kt` (`NAVIGATION(11)`) và `RuntimeNavigationPayloadCodec.kt` version 1.
- Native test path trong AGENTS chưa khớp checkout: AGENTS nêu `zygisk/jni/runtime_command_protocol_test.cpp` và `runtime_observation_protocol_test.cpp`; CI hiện gọi `zygisk/tests/runtime_auto_fort_navigation_test.cpp` và `runtime_automation_event_protocol_test.cpp`. T-002 ghi nhận test entrypoint thực tế; không tạo test mới để lấp phần thiếu.

## AC → task

| AC | Nguồn / hành vi | Task |
|---|---|---|
| AC-01 | Candidate hợp lệ được nhận khi native route idle | T-001, T-003, T-004, T-006, T-008, T-009 |
| AC-02 | `force=false` giữ A; B bị bỏ qua và A tiếp tục tới terminal/local arrival | T-001, T-003, T-006, T-007, T-008, T-009 |
| AC-03 | `force=true` thay active native route atomically | T-001, T-003, T-006, T-007, T-008, T-009 |
| AC-04 | Native candidate không cướp route USER theo mặc định | T-001, T-006, T-007, T-008, T-009 |
| AC-05 | Candidate trùng ID/target không restart active route | T-001, T-003, T-006, T-007, T-008, T-009 |
| AC-06 | Terminal cũ không dừng target mới | T-001, T-004, T-005, T-006, T-008, T-009 |
| AC-07 | Sai version/identity/sequence/freshness/coordinate fail-closed | T-002, T-004, T-005, T-006, T-009, T-010 |
| AC-08 | Kotlin arrival chỉ dừng fake movement, không dispatch gameplay | T-006, T-007, T-009, T-010 |
| AC-09 | API cho UI nhập tọa độ đi qua cùng `USER` route/coordinator/executor; không thêm writer | T-001, T-007 (UI entry screen là mở rộng tương lai) |

## Phase index

1. [Phase 0 — Contract kiến trúc và discovery](phase-0-contract.md): chốt lifecycle/priority và xác nhận source/test entrypoints.
2. [Phase 1 — Core model và bridge contract](phase-1-model-bridge.md): thêm model/policy thuần và candidate wire contract hai phía.
3. [Phase 2 — Kotlin admission và location ownership](phase-2-kotlin-location.md): route candidate, owner arbitration, persistent in-session execution.
4. [Phase 3 — Tích hợp lại fort-walk](phase-3-fort-walk.md): phase sau, phụ thuộc generic candidate path đạt Verify.
5. [Phase 4 — Verification và tài liệu](phase-4-verification.md): focused/full checks, native build và runtime verification bằng script sẵn có.

## Dependency và câu hỏi còn mở

- T-001 là prerequisite cho model và wire tasks vì cần chốt candidate identity, capability gate, active-route lifetime, terminal states và `CandidateDecision`.
- T-002 phải đối chiếu test commands với checkout/CI trước khi ghi lệnh C++ cuối cùng.
- T-003/T-004 có thể làm song song sau T-001; T-005 phụ thuộc T-004; T-006 phụ thuộc T-003/T-005; T-007 phụ thuộc T-006.
- T-008 chỉ bắt đầu sau khi T-003…T-007 đạt Verify. T-009 tích hợp Kotlin consumer sau native producer; T-010 kiểm chứng toàn tuyến.
- `JoystickOverlayService.kt` hiện 489 dòng và `catch_spin/coordinator.inc` 474 dòng. Khi sửa, giữ mọi source ≤500; tách collaborator `.kt`/`.inc` cohesive nếu wiring vượt ngưỡng và giữ single native translation unit/include order.
- Q-01 đã chốt: `force=true` không hủy location route do user chọn.
- Q-02: native có cần nhận ack cho candidate bị bỏ qua/thay thế không? T-001 định nghĩa kết quả nội bộ; chỉ thêm wire ack nếu producer cần phản ứng với kết quả.

## Execution notes bắt buộc

Sau mỗi task, kiểm tra diff và kết quả Verify, cập nhật ngay checkbox cùng khối `<details>` nằm dưới nó, rồi mới chuyển task. Chỉ đánh dấu `[x]` khi đạt Verify. Trong `<details><summary>Đã hoàn thành — T-ID</summary>...</details>`, ghi file đã tạo/sửa/xóa/di chuyển, function/method/param hoặc hành vi đã thêm/đổi/bỏ, và lệnh kiểm chứng cùng kết quả thực tế. Mục không phát sinh ghi “Không”. Nếu còn lỗi, chưa kiểm chứng hoặc bị chặn, giữ `[ ]`, ghi trạng thái và phần đã làm/điểm còn thiếu. Không dồn ghi chú tới cuối phase, không đánh dấu hoàn tất dựa trên dự kiến.

Mỗi file phase cũng nhắc lại quy tắc này; mọi task bên dưới có sẵn một `<details>` trạng thái chưa thực hiện. Nhãn owner là vai trò theo plan, không tự tạo agent song song.

## Definition of Done

- Contract generic ghi rõ candidate admission, `force`, owner, active-route lifetime, terminal correlation và decision outcomes.
- Kotlin/native wire thay đổi đồng bộ; runtime identity/session/sequence/freshness/size/coordinate guards còn fail-closed.
- Location input có một writer và user-originated route không bị native force ghi đè theo default policy.
- UI nhập tọa độ về sau gọi cùng USER route API sau khi validate input; không thêm native protocol hoặc movement loop.
- Phase fort-walk chỉ được tick sau khi native producer và Kotlin consumer dùng cùng semantics; native vẫn chọn candidate theo game state.
- Mọi task trong phạm vi đạt Verify và có actual details; focused existing checks, `./gradlew test assembleDebug`, native build và runtime checks theo phạm vi đạt.
- Không tạo/sửa test code; không source file nào vượt 500 dòng; `git diff --check` sạch.
