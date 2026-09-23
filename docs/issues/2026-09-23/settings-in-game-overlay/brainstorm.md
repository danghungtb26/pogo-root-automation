# Brainstorm: Mở Settings trong khi vẫn ở Pokémon GO

**Loại:** architecture / ux
**Ngày phân tích đầu tiên:** 2026-09-23
**Phạm vi game/build/ABI:** Pokémon GO `0.427.0`; reverse notebook xác nhận version game, package/version code/ABI cho UI chưa cần xác minh.

## Bước 1 — Yêu cầu và phạm vi

### Vấn đề

Shortcut `Settings` trong floating overlay hiện mở `AutomationSettingsActivity`.
Vì đây là Android `Activity` của app controller, Android đưa task controller lên
foreground và Pokémon GO không còn là ngữ cảnh hiển thị chính.

### Hành vi mong muốn

1. User đang ở Pokémon GO, mở float icon rồi chọn `Settings`.
2. Settings hiển thị bằng overlay của controller ở trên Pokémon GO, không gọi
   `AutomationSettingsActivity`.
3. Có list category và submenu chi tiết trong cùng flow overlay.
4. Back/dismiss quay về đúng cấp UI mà không rời Pokémon GO.
5. Các field numeric vẫn mở được IME và lưu đúng config hiện tại.
6. Khi game background hoặc service dừng, Settings overlay phải được đóng sạch.

### Phạm vi

- Android overlay, `JoystickOverlayService`, settings navigation và IME.
- Không thay đổi native gameplay, bridge protocol, game binding hoặc location
  controller.
- `AutomationConfigRepository` và `OverlayPositionStore` tiếp tục là owner
  persistence hiện tại.

### Điều kiện hoàn tất ban đầu

- Shortcut Settings không còn start Activity trong game flow.
- Settings vẫn render đủ category/editor hiện có.
- Config sau Save vẫn dùng cùng repository và tăng revision.
- Overlay không để lại window/dialog stale khi rời game hoặc destroy service.

### Quyết định đã chốt

- Khi rời Pokémon GO giữa lúc đang sửa dở, draft bị hủy; không giữ draft trong
  RAM cho lần quay lại.
- Settings dùng panel gần full-screen có margin, không neo cạnh float icon. Mục
  tiêu là đủ diện tích cho list, ScrollView và IME trong khi vẫn giữ overlay ở
  trên Pokémon GO.
- Save ghi config xong phải gọi runtime sync ngay; không chờ facade nhận revision
  ở poll kế tiếp.

## Bước 2 — Bằng chứng từ reverse

### Phạm vi reverse

- Nguồn: `reverse/pogo-0.427.0/`.
- Version game: `0.427.0`, theo `reverse/pogo-0.427.0/OBSERVATION_RESEARCH.md:15-24`.
- Package, version code và ABI: **Chưa xác minh từ reverse output**.
- Các RVA trong dump chỉ áp dụng cho đúng build đã tạo dump, theo
  `reverse/pogo-0.427.0/OBSERVATION_RESEARCH.md:26-40`.

### Symbol liên quan

| Symbol | Nguồn | Kết quả |
|---|---|---|
| `TapGesture` | `reverse/pogo-0.427.0/classes/TapGesture.cs:1-79` | Có gesture Unity với tap/movement threshold, nhưng không phải Android Settings hoặc overlay. |

Không tìm thấy `Settings`, `AutomationSettingsActivity`, `WindowManager`,
`Activity`, `Dialog`, `Canvas` hoặc `Overlay` trong curated reverse classes.

**Kết luận:** đây là vấn đề Android window ownership. Không cần thêm binding,
hook Unity/IL2CPP hoặc render Settings bên trong game process.

## Bước 3 — Function/method, param và hành vi

### `JoystickOverlayService` — Đã xác nhận

- Nguồn: `app/src/main/java/dev/pogoroot/automation/overlay/JoystickOverlayService.kt:36-499`.
- `ensureOverlay()` tạo `MainOverlayView` và truyền callback `onSettings`.
- Callback hiện tại tại `:234-240`:

```kotlin
startActivity(
    Intent(this@JoystickOverlayService, AutomationSettingsActivity::class.java)
        .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK),
)
```

- Đây là nguyên nhân trực tiếp khiến controller Activity lên foreground.
- `applyGameForeground(state)` tại `:180-209` ẩn overlay và dismiss dialog khi
  state không phải `FOREGROUND`.
- `onDestroy()` tại `:160-177` dispose các overlay hiện có; Settings mới phải
  được thêm vào cùng lifecycle.

### `AutomationSettingsActivity` — Đã xác nhận

- Nguồn: `app/src/main/java/dev/pogoroot/automation/overlay/AutomationSettingsActivity.kt:13-132`.
- Các method chính:

```kotlin
override fun onCreate(savedInstanceState: Bundle?)
override fun onBackPressed()
fun openCategory(categoryId: String)
fun currentCooldownMode(): TeleportCooldownMode
fun setCooldownMode(mode: TeleportCooldownMode)
```

- Activity tạo repository/store, gắn `AutomationSettingsListFragment`, điều phối
  Fragment back stack và toolbar Save/Back.
- Activity được khai báo trong manifest tại
  `app/src/main/AndroidManifest.xml:29-31`.

### `AutomationSettingsListFragment` — Đã xác nhận

- Nguồn: `app/src/main/java/dev/pogoroot/automation/overlay/AutomationSettingsListFragment.kt:14-92`.
- `onCreateView(...)` cast `activity` thành `AutomationSettingsActivity`, tạo
  `ListView` và click row gọi `host.openCategory(category.id)`.
- `onResume()` đọc lại category summary từ repository.
- Coupling với Activity khiến Fragment không thể dùng trực tiếp trong Service
  overlay.

### `AutomationCategoryFragment` — Đã xác nhận

- Nguồn: `app/src/main/java/dev/pogoroot/automation/overlay/AutomationCategoryFragment.kt:22-312`.
- `newInstance(categoryId: String)` lưu category id vào `Bundle`.
- `onCreateView(...)` cast host thành `AutomationSettingsActivity`, dựng editor
  theo category rồi bọc trong `ScrollView`.
- `save()` gọi closure đã tạo lúc render.
- Các editor đọc config bằng `host.repository.read()` và ghi bằng
  `host.repository.update { ... }`.
- `numberInput(...)` dùng `EditText` với `TYPE_CLASS_NUMBER`; `intValue(...)`
  validate số và fallback default.

### `AutomationConfigRepository` — Đã xác nhận

- Nguồn: `app/src/main/java/dev/pogoroot/automation/config/AutomationConfig.kt:79-176`.
- Signature:

```kotlin
fun read(): HeadlessAutomationConfig
@Synchronized
fun update(transform: (HeadlessAutomationConfig) -> HeadlessAutomationConfig): HeadlessAutomationConfig
```

- `update` tăng `configRevision`, ghi SharedPreferences namespace
  `headless_automation` và trả lại config đã đọc.
- Activity, overlay và HTTP API phải dùng cùng repository; không tạo config state
  bền vững thứ hai.

### `OverlayUi` và input — Đã xác nhận / chưa xác minh runtime

- `newOverlayParams(width, height)` tại
  `app/src/main/java/dev/pogoroot/automation/overlay/OverlayUi.kt:24-34` dùng
  `TYPE_APPLICATION_OVERLAY` nhưng có `FLAG_NOT_FOCUSABLE`.
- Root này phù hợp shortcut/tap/drag, nhưng không đủ để host nhiều `EditText`
  cần IME.
- `TeleportLocationDialog` đã tạo `EditText` và đặt
  `dialog.window?.setType(overlayWindowType())` tại
  `app/src/main/java/dev/pogoroot/automation/overlay/TeleportLocationDialog.kt:10-47`.
- **Chưa xác minh:** `ScrollView` nhiều field trong focusable overlay có resize,
  IME và rotation ổn định trên BlueStacks Air 1.

## Bước 4 — Luồng hành vi và điểm tích hợp

### Luồng hiện tại

```text
Pokémon GO foreground
  -> float icon
  -> shortcut menu
  -> Settings
  -> JoystickOverlayService.onSettings
  -> startActivity(AutomationSettingsActivity, NEW_TASK)
  -> controller Activity lên foreground
  -> foreground detector không còn thấy Pokémon GO foreground
  -> overlay bị ẩn
```

Các cạnh gọi được xác nhận từ `MainOverlayView.kt:89-104`,
`JoystickOverlayService.kt:211-256` và `GameForegroundDetector.kt:31-88`.

### Luồng config hiện tại

```text
Settings / quick toggle / HTTP API
  -> AutomationConfigRepository.update(...)
  -> SharedPreferences + configRevision
  -> RuntimeUiAutomationFacade.pollRuntime()
  -> RuntimeDesiredStateMapper
  -> RuntimeUiClient / native desired state
```

`RuntimeUiAutomationFacade` theo dõi revision tại
`app/src/main/java/dev/pogoroot/automation/service/RuntimeUiAutomationFacade.kt:121-145`.
Settings UI không được tự quyết định readiness hoặc action game.

### Luồng đích

```text
Pokémon GO foreground
  -> float icon -> shortcut menu -> Settings
  -> Settings overlay do JoystickOverlayService sở hữu
  -> LIST hoặc CATEGORY(categoryId)
  -> Save -> repository.update(...) -> render lại summary
  -> Back/dismiss -> cấp UI trước
  -> background/destroy -> dismiss + clear focus/IME
```

### Rủi ro cần xử lý

- Overlay permission thiếu: giữ hướng dẫn trong `MainActivity`, không mở UI giả.
- State `BACKGROUND`/`UNKNOWN`: fail closed và đóng Settings.
- Service destroy: remove window/dialog đối xứng với `addView`.
- Rotation/display change: remeasure; draft chưa lưu bị hủy nếu lifecycle làm
  Settings đóng.
- API/quick toggle ghi đồng thời: đọc lại repository sau update.
- Back phải đóng IME trước, sau đó lùi category/list, không dừng service.

## Bước 5 — Hướng xử lý

### Phương án A — Focusable Settings overlay — Khuyến nghị

Tạo component mới do `JoystickOverlayService` sở hữu, dùng
`TYPE_APPLICATION_OVERLAY` và state `LIST/CATEGORY`.

API đề xuất, **chưa tồn tại**:

```kotlin
internal class AutomationSettingsOverlay(
    context: Context,
    windowManager: WindowManager,
    repository: AutomationConfigRepository,
    positionStore: OverlayPositionStore,
    onSaved: () -> Unit,
    onClosed: () -> Unit,
) {
    fun ensure()
    fun open()
    fun openCategory(categoryId: String)
    fun handleBack(): Boolean
    fun dismiss()
    fun setVisible(visible: Boolean)
    fun dispose()
}
```

Thêm params riêng, **đề xuất — chưa tồn tại**:

```kotlin
internal fun newFocusableOverlayParams(width: Int, height: Int): WindowManager.LayoutParams
```

Params này dùng overlay type hiện có, bỏ `FLAG_NOT_FOCUSABLE` khi Settings mở
và đặt `SOFT_INPUT_ADJUST_RESIZE`. Shortcut root vẫn giữ non-focusable.

Tách phần dựng form khỏi Fragment thành factory nhận `Context`, repository và
store. Activity cũ có thể dùng lại factory trong giai đoạn migration; overlay
không phụ thuộc `FragmentManager` hoặc `AutomationSettingsActivity`.

Guards:

- Chỉ mở khi overlay permission có, service còn sống và game đang `FOREGROUND`.
- Background/unknown/destroy phải dismiss, clear focus và đóng IME.
- Save chỉ ghi config/UI preference; không suy ra action game hoàn tất.
- Sau Save đọc lại config, render summary và gọi runtime sync ngay.

### Phương án B — Một `AlertDialog` overlay

Tái sử dụng pattern `TeleportLocationDialog`, render list/category trong một
dialog focusable và thay content khi navigate.

- Ưu điểm: ít thay đổi WindowManager, đã có mẫu input overlay.
- Nhược điểm: khó kiểm soát dialog sizing, Back, rotation và form dài.
- Vẫn phải tách editor khỏi Activity/Fragment.

Phù hợp migration ngắn, nhưng không nên là kiến trúc chính nếu Settings có
nhiều category và numeric field.

### Phương án C — Giữ Activity hoặc inject UI vào Unity — Không khuyến nghị

Transparent Activity vẫn là controller Activity và vẫn có thể đổi foreground.
Inject UI vào Unity cần reverse UI/canvas/input, vượt phạm vi và không cần thiết
cho yêu cầu hiện tại.

### Quyết định

Chọn **Phương án A**. Settings overlay là panel gần full-screen có margin, dùng
focusable window riêng và hủy draft khi đóng/background/rotation. Sau mỗi Save,
UI ghi repository rồi gọi runtime sync ngay. Giữ `AutomationSettingsActivity`
cho đường mở trực tiếp từ controller trong giai đoạn chuyển tiếp, nhưng loại bỏ
nó khỏi shortcut path trong Pokémon GO.

## Bước 6 — Tiêu chí chấp nhận và xác minh

| ID | Điều kiện | Postcondition | Cách kiểm chứng |
|---|---|---|---|
| AC-01 | Pokémon GO foreground, overlay permission có | Tap Settings không gọi Activity; game vẫn nhìn thấy phía dưới | Manual trên BlueStacks Air 1; review callback không còn `startActivity`. |
| AC-02 | Settings list mở | Đủ category, tap mở đúng submenu, Back lùi đúng cấp | Đi qua toàn bộ category và Back/outside touch. |
| AC-03 | Editor có field | Save ghi cùng `AutomationConfigRepository`/`OverlayPositionStore` và gọi runtime sync ngay | Reopen UI, kiểm tra giá trị, summary và request sync sau Save. |
| AC-04 | Numeric editor mở | IME focus/resize đúng, invalid input không crash | Test portrait/landscape, font lớn và keyboard. |
| AC-05 | Game background/unknown hoặc service destroy | Settings window/dialog biến mất, draft chưa lưu bị hủy, không leak/stale input | Home/app khác/lock screen/stop service rồi quay lại game; xác nhận field chưa Save không được giữ. |
| AC-06 | API/quick toggle ghi đồng thời | UI đọc revision/config mới, không tạo config owner thứ hai | Toggle trong lúc Settings mở rồi reopen. |
| AC-07 | Controller mở trực tiếp | Activity fallback vẫn hoạt động nhưng không được gọi từ overlay | Mở controller từ launcher/MainActivity. |
| AC-08 | Implementation hoàn tất | Không thêm PoGo hook/gameplay logic; panel full-screen có margin; build/test hiện có pass | `./gradlew test assembleDebug`, `git diff --check`, sau đó dùng scripts device hiện có. |

### Kế hoạch xác minh

1. Review static: callback Settings, add/remove WindowManager, lifecycle và
   `FLAG_NOT_FOCUSABLE`.
2. Chạy `./gradlew test assembleDebug` sau khi triển khai; không thêm/sửa test
   source.
3. Dùng `scripts/build-magisk.sh`, `scripts/push-emulator.sh`,
   `scripts/install-magisk-module.sh --apk <path>` và
   `scripts/bluestacks-smoke-test.sh` theo flow repository.
4. Manual test Settings trên BlueStacks Air 1 với background, rotation, IME,
   Back, outside touch, service stop/start và đồng thời quick toggle.
5. Chỉ khi script/device operation thất bại mới thu log bằng
   `scripts/logcat-full.sh`.

### Kết luận và câu hỏi còn mở

Nguyên nhân là `startActivity(AutomationSettingsActivity)` trong
`JoystickOverlayService`, không phải game binding. Giải pháp đúng là Settings
overlay focusable thuộc Service, với editor được tách khỏi Activity host.

Điểm còn cần xác minh trên thiết bị là IME/resize của overlay nhiều field và
runtime sync ngay sau Save. Chính sách draft và layout full-screen đã được chốt;
draft phải hủy khi Settings đóng do background, service destroy hoặc rotation
theo lifecycle đã chọn. Không cần thêm reverse pass cho UI Pokémon GO.

## Tiếp nối 1 — Chốt UX và lifecycle Settings

**Ngày cập nhật:** 2026-09-23

### Bước 1 — Quyết định từ user

| Chủ đề | Quyết định |
|---|---|
| Draft khi rời game | Hủy draft; không giữ trong RAM. Chỉ dữ liệu đã Save mới tồn tại sau khi Settings đóng. |
| Layout | Panel gần full-screen có margin, đủ rộng cho `ListView`, `ScrollView` và IME; vẫn là `TYPE_APPLICATION_OVERLAY` nằm trên Pokémon GO. |
| Runtime sync | Sau `AutomationConfigRepository.update(...)`, gọi `HeadlessAutomationService.requestRuntimeConfigSync(...)` ngay. Không chờ poll kế tiếp. |

### Bước 2 — Tác động đến luồng và state

Luồng Settings được chốt:

```text
open Settings
  -> tạo session/list state mới
  -> mở category
  -> người dùng sửa field trong draft của category
  -> Save
       -> repository.update(...)
       -> đọc lại config
       -> render summary
       -> requestRuntimeConfigSync()
  -> Back/dismiss sau Save
  -> quay về list/shortcut
```

Các đường đóng chưa Save đều hủy draft:

```text
category đang sửa
  -> background / UNKNOWN / rotation làm đóng / service destroy / Cancel
  -> clear focus + đóng IME
  -> bỏ view và draft trong RAM
  -> lần mở lại đọc giá trị đã Save gần nhất từ repository
```

Không được gọi `repository.update` trong lúc user chỉ thay đổi control nếu
category đang theo semantics Save. Quick toggle ngoài shortcut vẫn có thể ghi
ngay vì đó là action riêng, không phải draft của Settings.

### Bước 3 — Cập nhật hướng triển khai và kiểm chứng

- `AutomationSettingsOverlay` cần có layout gần full-screen với margin ổn định,
  không phụ thuộc vị trí float icon. Float icon có thể vẫn còn phía dưới hoặc
  được disable touch trong lúc Settings mở.
- `dismiss()` phải hủy editor/draft, clear focus, đóng IME và remove/update
  window đối xứng; không serialize draft vào `SharedPreferences`.
- Save callback phải gọi theo thứ tự:

```kotlin
repository.update { ... }
HeadlessAutomationService.requestRuntimeConfigSync(context)
onSaved(readBackConfig())
```

  Đây là thứ tự đề xuất; `requestRuntimeConfigSync` là API hiện có, còn
  `onSaved(readBackConfig())` là hành vi host cần bảo đảm, không phải API mới đã
  có.
- Acceptance bổ sung:

| ID | Điều kiện | Postcondition | Cách kiểm chứng |
|---|---|---|---|
| AC-SET-09 | Category có thay đổi nhưng chưa Save | Background, Cancel, rotation hoặc destroy không làm thay đổi repository; mở lại thấy giá trị đã Save trước đó | Nhập giá trị mới, đóng theo từng nhánh, mở lại và đọc summary/editor. |
| AC-SET-10 | Save thành công | Repository đã ghi, runtime sync được request ngay, UI render config đọc lại | Theo dõi call path/log và mở lại Settings; không dùng receipt làm bằng chứng action game hoàn tất. |
| AC-SET-11 | Settings mở trên màn hình nhỏ hoặc landscape | Panel chiếm vùng gần full-screen có margin, không cắt title/field/nút Save, game vẫn là nền nhìn thấy được | BlueStacks Air 1: portrait/landscape, IME mở, font lớn. |

### Kết luận cập nhật

Ba câu hỏi UX/lifecycle đã được giải quyết. Phương án triển khai không còn cần
policy giữ draft trong RAM hoặc layout neo cạnh float icon. Việc còn lại trước
implementation là xác minh kích thước margin hợp lý trên target và kiểm tra
`requestRuntimeConfigSync` được gọi sau Save nhưng vẫn giữ nguyên ranh giới:
Kotlin gửi desired config, native quyết định readiness và gameplay.
