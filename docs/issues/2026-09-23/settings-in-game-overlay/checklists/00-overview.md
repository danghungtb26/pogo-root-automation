# Plan: Settings overlay trong Pokémon GO

**Issue:** [Brainstorm](../brainstorm.md)
**Ngày lập plan:** 2026-09-23
**Scope:** `app` Android controller, overlay UI và settings refactor
**Không đổi:** `core`, `bridge/protocol`, `game-adapter`, `zygisk`, gameplay/native binding

## Bối cảnh và quyết định đã chốt

`JoystickOverlayService` hiện gọi `startActivity(AutomationSettingsActivity)`
từ callback Settings. Điều này đưa task controller lên foreground và làm
Pokémon GO rời khỏi ngữ cảnh hiển thị. `AutomationSettingsActivity` vẫn được
giữ làm fallback khi user mở controller trực tiếp, nhưng shortcut trong game sẽ
dùng một Settings overlay thuộc `JoystickOverlayService`.

Quyết định UX/lifecycle mới nhất:

- Settings là panel gần full-screen có margin, không neo cạnh float icon.
- Draft chưa Save bị hủy khi background, `UNKNOWN`, Cancel, rotation làm đóng
  view hoặc service destroy; không persist draft vào RAM/SharedPreferences.
- Save ghi `AutomationConfigRepository` rồi gọi
  `HeadlessAutomationService.requestRuntimeConfigSync(context)` ngay.
- Overlay Settings dùng `TYPE_APPLICATION_OVERLAY` focusable riêng; shortcut
  root hiện tại tiếp tục non-focusable.
- Không thêm PoGo/Unity/IL2CPP binding và không chuyển gameplay logic sang Kotlin.

## Nguồn và bằng chứng

- [Brainstorm Settings overlay](../brainstorm.md)
- [`docs/ARCHITECTURE.md`](../../../../ARCHITECTURE.md)
- [`JoystickOverlayService.kt`](../../../../../app/src/main/java/dev/pogoroot/automation/overlay/JoystickOverlayService.kt)
- [`AutomationSettingsActivity.kt`](../../../../../app/src/main/java/dev/pogoroot/automation/overlay/AutomationSettingsActivity.kt)
- [`AutomationCategoryFragment.kt`](../../../../../app/src/main/java/dev/pogoroot/automation/overlay/AutomationCategoryFragment.kt)
- [`AutomationConfig.kt`](../../../../../app/src/main/java/dev/pogoroot/automation/config/AutomationConfig.kt)
- Reverse UI search đã kết luận không có game symbol cần binding; không có task
  `native`/`reverse` trong plan này.

## Kết quả mong muốn

```text
Pokémon GO foreground
  -> float icon -> shortcut menu -> Settings
  -> focusable full-screen overlay có margin
  -> LIST -> CATEGORY(categoryId)
  -> chỉnh draft -> Save
       -> repository.update
       -> requestRuntimeConfigSync ngay
       -> đọc lại config và render summary
  -> Back/dismiss -> cấp UI trước
  -> game background/service destroy -> dismiss + hủy draft
```

## Phạm vi và dependency

- `AutomationConfigRepository` tiếp tục là durable config owner trong
  `headless_automation`.
- `OverlayPositionStore` tiếp tục lưu UI/cooldown state; không thêm draft key.
- `AutomationSettingsCatalog` tiếp tục là nguồn category title/summary.
- `HeadlessAutomationService.requestRuntimeConfigSync(context)` là API hiện có;
  không tạo transport mới.
- IME/focus/resize của overlay nhiều field là rủi ro device-specific; phải được
  kiểm chứng trên BlueStacks Air 1 trước khi hoàn tất.
- Repository rule cấm thêm hoặc sửa test source trong feature implementation;
  plan chỉ chạy test hiện có.

## Mapping acceptance criteria → task

| Acceptance | Task |
|---|---|
| AC-01: Settings không gọi Activity, game vẫn nhìn thấy | T-002, T-003 |
| AC-02: list/category/Back đúng cấp | T-001, T-002 |
| AC-03: Save dùng repository và sync ngay | T-001, T-004 |
| AC-04: numeric input/IME | T-001, T-002, T-006 |
| AC-05: background/destroy hủy overlay và draft | T-002, T-003, T-006 |
| AC-06: config revision/concurrent update | T-001, T-004, T-005 |
| AC-07: Activity fallback explicit | T-001, T-004 |
| AC-08: không PoGo hook, build/test pass | T-005, T-006 |
| AC-SET-09: draft chưa Save bị hủy | T-001, T-002, T-003 |
| AC-SET-10: sync ngay sau Save | T-004, T-005 |
| AC-SET-11: full-screen margin/landscape | T-002, T-006 |

## Các phase

1. [Phase 1 — Tách editor và giữ Activity fallback](phase-1-settings-editor.md)
2. [Phase 2 — Dựng Settings overlay full-screen](phase-2-settings-overlay.md)
3. [Phase 3 — Tích hợp service và sync runtime](phase-3-service-integration.md)
4. [Phase 4 — Verification](phase-4-verification.md)

## Rủi ro và cách xử lý

| Rủi ro | Cách xử lý | Task |
|---|---|---|
| `AutomationCategoryFragment` cast cứng Activity | Tách `AutomationSettingsEditorFactory` nhận `Context`/repository/cooldown callbacks; Activity và overlay cùng dùng | T-001 |
| Root overlay có `FLAG_NOT_FOCUSABLE` | Thêm params focusable riêng cho Settings, giữ root shortcut non-focusable | T-002 |
| Draft bị ghi dở hoặc tồn tại sau background | Chỉ gọi `save` khi Save; `dismiss` clear editor/draft và IME | T-001, T-003 |
| Settings che game hoặc window stale | Full-screen có margin, gate foreground, dismiss đối xứng với `addView` | T-002, T-003, T-006 |
| Save UI thành công nhưng native chưa nhận config | Gọi `requestRuntimeConfigSync` ngay sau `repository.update`; không coi receipt là gameplay completion | T-004 |
| File Kotlin vượt 500 dòng | Tách factory/shared builders nếu cần; kiểm tra line count sau refactor | T-001, T-005 |

## Quy tắc thực thi và ghi chú bắt buộc

Sau mỗi task, kiểm tra diff và kết quả Verify, cập nhật ngay checkbox cùng khối
`<details>` nằm dưới nó, rồi mới chuyển task tiếp theo. Chỉ đánh dấu `[x]` khi
đạt Verify. Trong `<details><summary>Đã hoàn thành — T-ID</summary>`, ghi file
đã tạo/sửa/xóa/di chuyển, function/method/param hoặc hành vi đã thêm/đổi/bỏ,
và lệnh kiểm chứng cùng kết quả thực tế. Mục không phát sinh ghi `Không`. Nếu
còn lỗi, chưa kiểm chứng hoặc bị chặn, giữ `[ ]`, ghi trạng thái và phần còn
thiếu. Không dồn ghi chú tới cuối phase và không đánh dấu hoàn tất dựa trên dự
kiến.

Mỗi file phase lặp lại quy tắc này dưới đây. Không chạy raw ADB; thao tác device
phải dùng script repository và target là BlueStacks Air 1.

## Definition of Done

- Tất cả task trong phạm vi đạt Verify và đã ghi details thực tế ngay dưới task.
- Shortcut Settings không còn start `AutomationSettingsActivity`.
- Editor dùng chung giữa overlay và Activity fallback; Save semantics nhất quán.
- Draft bị hủy đúng lifecycle; full-screen margin/IME đã kiểm chứng trên target.
- Save gọi runtime sync ngay; không đổi bridge/gameplay ownership.
- `./gradlew test assembleDebug` và focused app check pass khi môi trường cho phép.
- Device smoke/manual flow dùng scripts repository đạt; failure nếu có được ghi
  rõ, không báo plan hoàn tất khi verification bị chặn.
- `git diff --check` sạch và source không phải Markdown đã đổi không vượt 500 dòng.
