# Phase 3 — Tích hợp service và sync runtime

Plan: [Tổng quan](00-overview.md)

Điều kiện bắt đầu: T-002 đã có overlay component có lifecycle API và editor
factory; mọi draft semantics nằm trong overlay owner.

Điều kiện hoàn tất: Shortcut Settings không gọi Activity; Service sở hữu overlay
và đóng đúng theo foreground/destroy/configuration; Save ghi config, gọi sync
ngay và render lại từ config đọc lại.

Sau mỗi task, cập nhật ngay khối `<details>` dưới checkbox với file và Verify
thực tế. Chỉ tick `[x]` khi Verify đạt; nếu chưa đạt giữ `[ ]` và ghi nguyên nhân.

- [x] **T-003** **[app]** *(android)* — Tích hợp `AutomationSettingsOverlay` vào `JoystickOverlayService`: khởi tạo trong `ensureOverlay()`, đổi callback `onSettings` của `MainOverlayView` để mở overlay thay vì `startActivity(AutomationSettingsActivity)`, ẩn/collapse main overlay khi Settings mở, dismiss Settings khi `applyGameForeground` nhận `BACKGROUND`/`UNKNOWN`, xử lý `onConfigurationChanged()` theo chính sách hủy draft và dispose trong `onDestroy()`. **AC:** AC-01, AC-02, AC-05, AC-07, AC-SET-09, AC-SET-11. **Phụ thuộc:** T-002. **Verify:** `rg` không còn `startActivity(AutomationSettingsActivity` trong shortcut path; static review xác nhận foreground gate, clear focus/IME và add/remove đối xứng; app compile pass.

  <details>
  <summary>Đã hoàn thành — T-003</summary>

  Đã tích hợp `AutomationSettingsOverlay` qua
  `SettingsOverlayController.kt` vào `JoystickOverlayService.kt`.
  Callback Settings hiện mở panel trong cùng cửa sổ overlay, ẩn/collapse float
  menu, và chỉ cho mở khi service đang ở foreground của Pokémon GO. Khi game
  chuyển background/unknown, rotation hoặc service destroy, overlay được
  dismiss/dispose; callback đóng chỉ khôi phục shortcut khi service vẫn đang ở
  foreground.

  Verify thực tế: `rg` không còn đường gọi
  `startActivity(AutomationSettingsActivity` trong shortcut path; các window
  được tạo trong `ensure()` và gỡ trong `dispose()`. Lệnh
  `./gradlew :app:testDebugUnitTest --rerun-tasks` pass với compile app và unit
  test. Không có deviation.

  </details>

- [x] **T-004** **[app]** *(android)* — Chuẩn hóa Save callback cho overlay và Activity fallback: sau `AutomationConfigRepository.update { ... }` gọi `HeadlessAutomationService.requestRuntimeConfigSync(context)` ngay, đọc lại config để render summary/shortcut; giữ `AutomationSettingsActivity` chỉ là đường mở trực tiếp và không tạo repository/config state thứ hai. **AC:** AC-03, AC-06, AC-07, AC-SET-10. **Phụ thuộc:** T-001, T-003. **Verify:** review call order `update -> requestRuntimeConfigSync -> read/render`; kiểm tra API hiện có tại `HeadlessAutomationService.kt:208`; chạy focused app test hiện có và kiểm chứng không đổi wire/gameplay path.

  <details>
  <summary>Đã hoàn thành — T-004</summary>

  Save của overlay gọi editor trước, sau đó `onSettingsSaved()` gọi ngay
  `HeadlessAutomationService.requestRuntimeConfigSync(this)` và đọc lại config
  qua `renderShortcutStates()`/`renderCooldown()`. Activity fallback cũng gọi
  cùng API ngay sau `fragment.save()` và trước khi pop màn hình. Cả hai đường
  dùng repository hiện có, không tạo state/config store thứ hai.

  Verify thực tế: API sync tại `HeadlessAutomationService.kt:208` được tái sử
  dụng; `./gradlew :app:testDebugUnitTest --rerun-tasks` pass. Không đổi
  protocol, wire hoặc gameplay path; không có deviation.

  </details>
