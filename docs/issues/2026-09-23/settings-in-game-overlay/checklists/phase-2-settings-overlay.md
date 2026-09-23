# Phase 2 — Dựng Settings overlay full-screen

Plan: [Tổng quan](00-overview.md)

Điều kiện bắt đầu: T-001 đã tách editor factory và giữ được Activity fallback.

Điều kiện hoàn tất: Có `AutomationSettingsOverlay` mới với state `LIST` /
`CATEGORY`, window `TYPE_APPLICATION_OVERLAY` focusable, panel gần full-screen
có margin, Back/dismiss và draft cancellation rõ ràng.

Sau mỗi task, cập nhật ngay khối `<details>` dưới checkbox với file và Verify
thực tế. Chỉ tick `[x]` khi Verify đạt; nếu chưa đạt giữ `[ ]` và ghi nguyên nhân.

- [x] **T-002** **[app]** *(android)* — Tạo `AutomationSettingsOverlay` trong `app/src/main/java/dev/pogoroot/automation/overlay/` và thêm `newFocusableOverlayParams(width: Int, height: Int): WindowManager.LayoutParams` vào `OverlayUi.kt` (**Đề xuất — chưa tồn tại**); render panel gần full-screen có margin với `LIST`/`CATEGORY(categoryId)`, toolbar Back/Save/Close, `ListView` category và `ScrollView` editor. Khi mở editor giữ draft chỉ trong view/session; khi `dismiss()`/Cancel/background/rotation làm đóng thì clear editor, focus và IME, không ghi draft. **AC:** AC-01, AC-02, AC-04, AC-05, AC-SET-09, AC-SET-11. **Phụ thuộc:** T-001. **Verify:** review `WindowManager.addView/removeView/updateViewLayout` đối xứng; xác nhận settings params không có `FLAG_NOT_FOCUSABLE`, có `SOFT_INPUT_ADJUST_RESIZE`; compile app pass và source file ≤500 dòng.

  <details>
  <summary>Đã hoàn thành — T-002</summary>

  Đã tạo `AutomationSettingsOverlay.kt` (321 dòng) và thêm
  `newFocusableOverlayParams` vào `OverlayUi.kt`. Overlay có hai state LIST và
  CATEGORY, panel full-screen có margin 16dp, toolbar Back/Save/Close, danh sách
  category và editor `ScrollView`. Window params không có
  `FLAG_NOT_FOCUSABLE`, có `SOFT_INPUT_ADJUST_RESIZE`; `dismiss()` và
  `onConfigurationChanged()` clear editor, focus và IME nên draft chưa Save bị
  hủy đúng yêu cầu.

  Verify thực tế: `./gradlew :app:testDebugUnitTest --rerun-tasks` pass,
  `:app:compileDebugKotlin` và `:app:testDebugUnitTest` hoàn tất; source file
  mới dưới 500 dòng. Chỉ còn các warning deprecated Android đã tồn tại trong
  project.

  Không có deviation.

  </details>
