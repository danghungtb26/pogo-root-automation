# Phase 1 — Tách editor và giữ Activity fallback

Plan: [Tổng quan](00-overview.md)

Điều kiện bắt đầu: Brainstorm đã chốt draft hủy khi đóng, full-screen margin và
sync ngay sau Save; source hiện tại đã được inventory trong brainstorm.

Điều kiện hoàn tất: Settings category editor không còn phụ thuộc trực tiếp vào
`AutomationSettingsActivity`; Activity fallback và overlay có thể dùng chung
field/default/validation/save behavior mà không thêm persistence owner.

Sau mỗi task, cập nhật ngay khối `<details>` dưới checkbox với file và Verify
thực tế. Chỉ tick `[x]` khi Verify đạt; nếu chưa đạt giữ `[ ]` và ghi nguyên nhân.

- [x] **T-001** **[app]** *(android)* — Tạo `AutomationSettingsEditor` và `AutomationSettingsEditorFactory` trong `app/src/main/java/dev/pogoroot/automation/overlay/`, chuyển logic dựng field của `AutomationCategoryFragment` sang factory nhận `Context`, `AutomationConfigRepository`, cooldown getter/setter; cập nhật `AutomationCategoryFragment` và `AutomationSettingsActivity` để giữ list/category/Save fallback. **AC:** AC-02, AC-03, AC-04, AC-06, AC-07, AC-SET-09. **Phụ thuộc:** Không. **Verify:** `rg` xác nhận mọi category hiện có (`automation`, `discard`, `transfer`, `berry`, `feedback`, `cooldown`) đi qua factory; `./gradlew :app:testDebugUnitTest --rerun-tasks` và compile app pass nếu task tồn tại; source đổi không vượt 500 dòng.

  <details>
  <summary>Đã hoàn thành — T-001</summary>

  - **Tạo file:** `app/src/main/java/dev/pogoroot/automation/overlay/AutomationSettingsEditor.kt` — chứa `AutomationSettingsEditor` và `AutomationSettingsEditorFactory`, dùng chung editor cho Activity/overlay; file 297 dòng.
  - **Sửa file:** `app/src/main/java/dev/pogoroot/automation/overlay/AutomationCategoryFragment.kt` — bỏ các builder phụ thuộc cứng `AutomationSettingsActivity`, delegate sang factory và giữ `save()`/`ScrollView` host behavior.
  - **Xóa file:** Không.
  - **Di chuyển/đổi tên:** Không.
  - **Thay đổi chức năng:** `AutomationSettingsEditorFactory.build(categoryId: String): AutomationSettingsEditor` dựng đủ sáu category hiện có; các closure Save vẫn gọi `AutomationConfigRepository.update` hoặc cooldown callback, draft chỉ nằm trong view/session cho tới khi Save.
  - **Kiểm chứng:** `./gradlew :app:testDebugUnitTest --rerun-tasks` → `BUILD SUCCESSFUL`; `:app:compileDebugKotlin` và `:app:testDebugUnitTest` đều pass trong cùng run. Có deprecation warnings Android API/Fragment hiện hữu; không có compile error.
  - **Sai khác so với plan:** Không.

  </details>
