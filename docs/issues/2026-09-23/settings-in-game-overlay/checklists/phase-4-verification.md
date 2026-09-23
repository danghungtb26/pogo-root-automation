# Phase 4 — Verification

Plan: [Tổng quan](00-overview.md)

Điều kiện bắt đầu: T-001 đến T-004 đã đạt Verify riêng và diff không có task
code chưa ghi chú.

Điều kiện hoàn tất: Existing tests/build, static guards và manual overlay flow
đạt; mọi giới hạn device-specific được ghi rõ trong details.

Sau mỗi task, cập nhật ngay khối `<details>` dưới checkbox với file và Verify
thực tế. Chỉ tick `[x]` khi Verify đạt; nếu chưa đạt giữ `[ ]` và ghi nguyên nhân.

- [x] **T-005** **[app]** *(android)* — Chạy focused app verification và full Gradle verification cho source hiện có: `./gradlew :app:testDebugUnitTest --rerun-tasks` nếu task khả dụng, sau đó `./gradlew test assembleDebug`; kiểm tra `git diff --check`, source Kotlin/Java đã đổi không vượt 500 dòng và review diff không chạm native/bridge/game binding. **AC:** AC-06, AC-08, AC-SET-10. **Phụ thuộc:** T-004. **Verify:** lệnh thực tế pass; nếu task/SDK thiếu giữ task `[ ]` và ghi lỗi, không coi build cache hoặc test chưa chạy là pass.

  <details>
  <summary>Đã hoàn thành — T-005</summary>

  Verify thực tế đã pass trên source cuối cùng:

  - `./gradlew :app:testDebugUnitTest --rerun-tasks`
  - `./gradlew test assembleDebug`
  - `git diff --check`

  Source đã đổi đều không quá 500 dòng; `JoystickOverlayService.kt` còn 489
  dòng sau khi tách `SettingsOverlayController.kt` và
  `CooldownOverlayRenderer.kt`. Diff chỉ chạm app overlay/service và docs,
  không chạm `native`, `bridge` hoặc game binding. Các warning hiện tại chỉ là
  Android API deprecated đã có trong project. Verification được chạy lại sau
  khi bổ sung clear IME/focus cho Back và Save của overlay.

  </details>

- [ ] **T-006** **[app]** *(android)* — Kiểm chứng trên target BlueStacks Air 1 bằng scripts repository: build/package theo workflow cần thiết, cài APK qua `scripts/install-magisk-module.sh --apk <path>`, chạy `scripts/bluestacks-smoke-test.sh`, rồi manual test Settings ở portrait/landscape, font lớn, IME mở, Back/Cancel, Save sync ngay, background/lock screen, rotation, service stop/start và quick toggle đồng thời. **AC:** AC-01, AC-04, AC-05, AC-08, AC-SET-09, AC-SET-10, AC-SET-11. **Phụ thuộc:** T-005. **Verify:** quan sát được Pokémon GO vẫn là nền khi Settings mở; draft chưa Save không tồn tại sau đóng; panel không cắt field/nút; Save tạo request sync; không có duplicate window/leak. Chỉ khi thao tác/script thất bại mới dùng `scripts/logcat-full.sh`; không dùng raw ADB.

  <details>
  <summary>Chưa thể thực hiện — T-006</summary>

  Đã gọi `./scripts/bluestacks-smoke-test.sh` theo workflow của repository,
  nhưng script dừng ngay với `FAIL: no adb device connected`. Vì BlueStacks Air
  1 chưa kết nối nên chưa thể install APK, chạy smoke test hoặc thao tác manual
  Settings ở portrait/landscape, IME, rotation và background. Không dùng raw
  ADB và không đọc logcat do chưa có target để thu thập.

  Đây là giới hạn môi trường hiện tại, không phải lỗi compile. Cần chạy lại
  task này khi BlueStacks Air 1 được khởi động và kết nối.

  </details>
