# Phase 3 — Tích hợp lại fort-walk

Plan: [Tổng quan](00-overview.md)

Điều kiện bắt đầu: T-003…T-007 đạt Verify và generic candidate path đã sẵn sàng. Đây là phase sau theo yêu cầu của người dùng; không kéo fort target-selection vào các phase nền tảng.
Điều kiện hoàn tất: native fort-walk giữ target selection/game eligibility và phát lifecycle đúng contract; Kotlin chuyển legacy fort route sang admission coordinator.

Sau mỗi task, kiểm tra diff và kết quả Verify, cập nhật ngay khối details dưới task trước khi chuyển tiếp. Chỉ tick `[x]` khi Verify đạt. Không tạo hoặc sửa test code theo `AGENTS.md`.

- [x] **T-008** **[zygisk]** *(native)* — Sửa producer fort-walk tại `zygisk/jni/modules/catch_spin/auto_fort_navigation.h`, `navigation.inc` và điểm gọi trong `coordinator.inc` để diễn đạt target hiện tại, candidate thay thế và terminal lifecycle theo T-001. Giữ fort selection, busy/pause và game-state eligibility ở native; khi A arrived/invalid/pause, phát transition target-correlated rõ trước/sau candidate B; không chuyển selector sang Kotlin. **AC:** AC-01…AC-06. **Phụ thuộc:** T-003…T-007. **Verify:** `c++ -std=c++17 -Wall -Wextra -Werror -Izygisk/jni zygisk/tests/runtime_auto_fort_navigation_test.cpp -o /tmp/runtime_auto_fort_navigation_test` rồi `/tmp/runtime_auto_fort_navigation_test`; `ANDROID_NDK="$HOME/Library/Android/sdk/ndk/27.1.12297006" ./scripts/build-magisk.sh`; không sửa test code.

  <details>
  <summary>Đã hoàn thành — T-008</summary>

  - Production `navigation.inc` giờ dùng `RuntimeAutoFortWalkCandidateProducer` và wire observation type 12. WALK chỉ phát một lần cho target/ID đang active; cùng target không phát lặp. Arrival phát ARRIVED(A) trước WALK(B); target mất/đổi tọa độ, map invalid, nearby spawn, busy action hoặc disable phát STOP(A) đúng ID trước khi có candidate mới. ID tăng đơn điệu trong process session, force=false; selector/eligibility vẫn dùng native snapshot và policy cũ. Disable reset selector và kết thúc candidate; pause giữ target để tiếp tục bằng ID mới khi có snapshot hợp lệ.
  - Live references tới `publish_runtime_navigation`/`NAVIGATION(11)` đã được gỡ khỏi `catch_spin`; class `RuntimeAutoFortNavigation` còn lại chỉ để giữ API mà host regression hiện có đang tiêu thụ, không còn instance trong production.
  - Verify: focused host regression `runtime_auto_fort_navigation_test.cpp` compile với `-Wall -Wextra -Werror` và chạy exit 0. `ANDROID_NDK=/Users/admin/Library/Android/sdk/ndk/28.2.13676358 ./scripts/build-magisk.sh` thành công, tạo ZIP multi-ABI; NDK 27.1 được ghi trong checklist nhưng không có tại đường dẫn đó trên máy này. Source walkthrough kiểm tra sequence: arrival/invalid/pause A terminal trước candidate B; busy lặp không phát STOP trùng khi không còn ID; target ổn định không phát WALK lặp. Không sửa test code.

  </details>

- [x] **T-009** **[app]** *(android)* — Chuyển `HeadlessAutomationService`/`RuntimeUiAutomationFacade` và location consumer khỏi legacy `NAVIGATION(11)` cho fort-walk sang candidate receiver/coordinator đã thêm; xóa/disable đường dispatch kép để mỗi native route chỉ có một active owner/lifecycle. Giữ `NativeNavigationReceiver` hoặc compatibility path chỉ khi consumer khác còn dùng; ghi quyết định theo source. **AC:** AC-01…AC-08. **Phụ thuộc:** T-008, T-006, T-007. **Verify:** `./gradlew :app:testDebugUnitTest --tests dev.pogoroot.automation.runtime.NativeNavigationReceiverTest --rerun-tasks`; walkthrough native A→candidate B false/true, ARRIVED(A)→B, stale STOP(A), session reset, user route và no double dispatch.

  <details>
  <summary>Đã hoàn thành — T-009</summary>

  - `HeadlessAutomationService` và `RuntimeUiAutomationFacade` chỉ tạo/nối `NativeWalkCandidateReceiver`; `RuntimeUiEventRouter` chỉ dispatch type 12 qua candidate receiver. Legacy type 11 vẫn tiêu thụ observation sequence nhưng payload không parse và không thể điều khiển location. Xóa `RuntimeUiEvent.Navigation` cùng lease state đã không còn consumer.
  - Gỡ `NativeNavigationReceiver` khỏi live service, xóa `AutoFortNavigationBus` lease/replay path, và bỏ listener/apply handler cũ khỏi `JoystickOverlayService`. Native route giờ chỉ có `WalkCandidateCoordinator` làm owner; coordinator gọi location executor, `JoystickLocationController` vẫn là writer duy nhất. `NativeNavigationReceiver`/navigation codec được giữ vì `NativeNavigationReceiverTest` vẫn là regression consumer hiện có; chúng không được production service tham chiếu.
  - Source walkthrough: A active rồi B `force=false` -> policy giữ A và A tick tiếp; B `force=true` -> atomically replace A theo generation; ARRIVED(A) trước WALK(B) kết thúc A rồi nhận B; stale STOP(A) không match B ID/generation; session reset gọi receiver/coordinator reset; USER walk/joystick/favorite/map preempt native và `force` không chiếm USER; type 11 không dispatch; chỉ `WalkCandidateLocationActions` gọi `controller.walkTo`, không còn dispatch kép. API `userWalkTo(target)` sẵn để UI tọa độ thủ công reuse, chưa thêm màn hình nhập.
  - Verify: focused `./gradlew :app:testDebugUnitTest --tests dev.pogoroot.automation.runtime.NativeNavigationReceiverTest --rerun-tasks` thành công (29 tasks, 0 test failures); `git diff --check` sạch. `JoystickOverlayService.kt` là 464 dòng, mọi file Kotlin đã sửa vẫn dưới 500. Không tạo/sửa test code.

  </details>
