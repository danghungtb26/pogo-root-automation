# Phase 4 — Verification và tài liệu

Plan: [Tổng quan](00-overview.md)

Điều kiện bắt đầu: T-008 native producer và T-009 Kotlin consumer đạt focused Verify.
Điều kiện hoàn tất: full Kotlin/Android checks, native build, runtime smoke evidence phù hợp và tài liệu active-route ownership/lifecycle cập nhật; không còn Verify chưa đạt được báo hoàn tất.

Sau mỗi task, kiểm tra diff và kết quả Verify, cập nhật ngay khối details dưới task trước khi chuyển tiếp. Chỉ tick `[x]` khi Verify đạt. Không tạo hoặc sửa test code theo `AGENTS.md`.

- [ ] **T-010** **[reviewer]** *(reviewer)* — Chạy kiểm tra tổng thể và rà tài liệu sau tích hợp: `./gradlew test assembleDebug --rerun-tasks`; C++ host checks theo đúng path CI đã xác nhận ở T-002; `ANDROID_NDK="$HOME/Library/Android/sdk/ndk/28.2.13676358" ./scripts/build-magisk.sh`; cập nhật `docs/ARCHITECTURE.md` và brainstorm/contract nếu boundary hoặc decision cuối khác baseline. Dùng script thiết bị hiện có như `./scripts/bluestacks-smoke-test.sh`/`./scripts/device-smoke-test.sh` và `./scripts/install-magisk-module.sh` theo options hiện hành nếu runtime verification được yêu cầu/cho phép; target là BlueStacks Air 1. Không dùng ad hoc ADB, không đọc log khi script thành công. **AC:** AC-01…AC-08. **Phụ thuộc:** T-009. **Verify:** Gradle full check và native build đạt; device scenario nhận A, bỏ B không-force, thay bằng C force, stale terminal A, user override và session reset có postcondition quan sát được; `git diff --check` sạch; source <500 dòng.

  <details>
  <summary>Chưa hoàn tất — T-010</summary>

  - Đã cập nhật `docs/ARCHITECTURE.md`, `candidate-contract.md`, overview và brainstorm: Kotlin admission/coordinator giữ active candidate trong RAM; native vẫn chọn fort và phát terminal theo candidate ID; type 11/lease bus không còn live location path; typed coordinate UI sau này reuse `userWalkTo(target)`.
  - Verify host-side: `./gradlew test assembleDebug --rerun-tasks` thành công (51 tasks); sáu C++ host checks đúng CI đều compile và chạy exit 0; `ANDROID_NDK=/Users/admin/Library/Android/sdk/ndk/28.2.13676358 ./scripts/build-magisk.sh` thành công, multi-ABI ZIP được tạo. NDK 27.1 theo hướng dẫn chung không có tại path trên máy này; NDK 28.2 trùng bản pin trong CI và có sẵn.
  - Device smoke: `ANDROID_SERIAL=127.0.0.1:5565 ./scripts/bluestacks-smoke-test.sh` trả `FAIL: no adb device connected` trước mọi thao tác game/cài module. Không chạy `device-smoke-test.sh`/installer khi target offline. Vì vậy chưa có postcondition runtime cho candidate A, B no-force, C force, late STOP(A), USER override hoặc session reset; không suy pass từ compile/build. Giữ task chưa tick tới khi BlueStacks Air 1 online và các scenario chạy được qua script/UI flow.
  - `NativeNavigationReceiverTest` focused regression đã pass ở T-009. `git diff --check HEAD` sạch; tracked Kotlin/Java/C++/shell/XML sources không có file nào vượt 500 dòng sau khi loại file bus đã xóa khỏi worktree (`JoystickOverlayService.kt` 464, `coordinator.inc` 474). Không tạo/sửa test code.

  </details>
