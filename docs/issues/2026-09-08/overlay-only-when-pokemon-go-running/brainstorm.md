# Brainstorm: Chỉ hiển thị overlay khi Pokémon GO đang chạy

**Type:** ux
**Date:** 2026-09-08

---

## Analysis

### 1. Người dùng là ai?

Người dùng vận hành PoGo Root Automation trên thiết bị Android hoặc emulator đã root, thường mở Pokémon GO cùng với built-in joystick, shortcut menu, cooldown badge và widget kết quả scan. Họ đã quen với overlay nổi, nhưng không muốn các nút của tool che màn hình Home, app khác hoặc màn hình cài đặt.

Ngữ cảnh sử dụng thường là chuyển nhanh giữa Pokémon GO và màn hình điều khiển. Vì vậy trạng thái hiển thị phải tự động, không bắt user phải mở lại `MainActivity` mỗi lần quay về game.

### 2. Mục tiêu của người dùng là gì?

“Khi đang ở Pokémon GO thì thấy và dùng được overlay; khi rời Pokémon GO thì overlay biến mất hoàn toàn khỏi màn hình. Khi quay lại Pokémon GO, overlay xuất hiện lại mà không cần start service lần nữa.”

Trong phân tích này, “Pokémon GO đang chạy” được hiểu là **Pokémon GO đang là app foreground/đang được user nhìn thấy**, không chỉ là process còn tồn tại trong memory. Nếu chỉ cần process alive mà không cần foreground, đó là một scope khác và cần chốt lại trước khi implement.

### 3. Pain point hiện tại là gì?

`MainActivity` gọi `startForegroundService()` để khởi động `JoystickOverlayService` (`app/src/main/java/dev/pogoroot/automation/MainActivity.kt:165-170`). Sau đó `JoystickOverlayService.onStartCommand()` luôn gọi `ensureOverlay()` (`app/src/main/java/dev/pogoroot/automation/overlay/JoystickOverlayService.kt:88-115`), tạo và add các cửa sổ `WindowManager`:

- `MainOverlayView` cho float icon/menu/joystick;
- `CooldownOverlayView`;
- `ScanResultOverlays` gồm hundo và shiny widget.

Service trả về `START_STICKY`, nên service và các window có thể tiếp tục sống khi user bấm Home hoặc chuyển sang app khác. `runtime-status.sh` chỉ kiểm tra PID/process và báo `connected`/`disconnected`; nó không cho biết Pokémon GO có đang là foreground app hay không (`zygisk/module/bin/runtime-status.sh:59-65`). Vì vậy dùng runtime status một mình sẽ không giải quyết đúng pain point UX.

Ngoài các view chính, `FavoriteLocationsDialog`, `TeleportLocationDialog` và confirm dialog cũng được đặt thành `TYPE_APPLICATION_OVERLAY`. Nếu chỉ ẩn ba view chính mà quên các dialog, yêu cầu “overlay không hiện ngoài Pokémon GO” vẫn bị vi phạm.

### 4. Những user flow nào bị ảnh hưởng?

#### Flow A — Start service khi đang ở `MainActivity`

1. User cấp quyền draw over other apps nếu cần.
2. User bấm `Start built-in joystick`.
3. Vì `MainActivity` đang foreground chứ chưa phải Pokémon GO, overlay giữ ở trạng thái hidden.
4. UI/notification cần nói rõ overlay sẽ xuất hiện khi mở Pokémon GO, tránh hiểu nhầm service đã lỗi.

#### Flow B — Mở Pokémon GO

1. User mở `com.nianticlabs.pokemongo` hoặc `com.nianticlabs.pokemongo.ares`.
2. Detector nhận được state foreground.
3. Service gọi một coordinator duy nhất để show main overlay, cooldown và scan widgets.
4. Main overlay nên trở lại trạng thái `COLLAPSED` để không tự bật một menu/joystick panel bất ngờ; vị trí đã lưu vẫn được giữ.

#### Flow C — Rời Pokémon GO

1. User bấm Home, mở app khác, mở Settings hoặc khóa màn hình.
2. Detector chuyển sang `BACKGROUND`/`UNKNOWN`.
3. Tất cả floating windows chuyển sang hidden; mọi dialog overlay đang mở được dismiss/cancel.
4. Service không tự stop và không reset location state chỉ vì UI bị ẩn. Việc pause walk/automation là một policy khác, chưa nằm trong yêu cầu hiện tại.

#### Flow D — Quay lại Pokémon GO

1. User chuyển về Pokémon GO.
2. Detector xác nhận foreground sau debounce ngắn.
3. Overlay hiện lại, menu ở trạng thái thu gọn, cooldown/scan widgets được render theo state hiện tại.
4. User tiếp tục dùng joystick/shortcut như trước.

### 5. Empty state và error state là gì?

- **Chưa cấp Usage Access:** không được đoán foreground bằng dữ liệu cũ; overlay phải hidden và `MainActivity` cần chỉ dẫn mở `Settings.ACTION_USAGE_ACCESS_SETTINGS`. Nên hiển thị trạng thái kiểu `Usage access required` thay vì im lặng.
- **Detector trả về `UNKNOWN`:** fail closed — ẩn overlay cho tới khi có bằng chứng Pokémon GO đang foreground. Không giữ overlay visible vô thời hạn vì lần poll trước nói game đang foreground.
- **Không có Pokémon GO được cài hoặc package không nằm trong allowlist:** overlay hidden; có thể báo package unsupported trong màn hình điều khiển.
- **Usage event bị trễ hoặc bị thiếu:** dùng polling + debounce, không nhấp nháy view theo từng event đơn lẻ. Nếu trạng thái không chắc chắn sau timeout, ưu tiên hidden.
- **Window đã bị remove hoặc `WindowManager` ném exception:** coordinator phải coi view là không visible, ghi log và không làm service crash. Không gọi `ensure()` giả định rằng `lateinit` view vẫn còn attached nếu đã `dispose()`.
- **Dialog đang nhập dữ liệu khi user rời game:** dismiss dialog; không tự submit dữ liệu dang dở và không tự mở lại khi quay về.

### 6. Edge case cho người dùng

- **Hai package Pokémon GO:** hỗ trợ đúng hai package đã có trong adapter/native allowlist: `com.nianticlabs.pokemongo` và `com.nianticlabs.pokemongo.ares` (`game-adapter/pogo/.../PogoGameAdapterFactory.kt:37-41`, `zygisk/jni/main.cpp:26-29`). Không match theo chuỗi gần giống hoặc package clone.
- **Game process còn sống nhưng app đã background:** phải hidden; process liveness không đồng nghĩa foreground.
- **Home/app khác/Settings của app controller:** phải hidden. `AutomationSettingsActivity` là activity thông thường, không phải floating window; nó không cần bị đóng chỉ vì detector hide overlay.
- **Màn hình khóa hoặc display không interactive:** nên hidden để không có cửa sổ overlay hoạt động trên lock screen.
- **Chuyển app nhanh:** transition foreground/background có thể tạo event liên tiếp; cần debounce/hysteresis để tránh flash overlay. Mục tiêu UX là hội tụ trong khoảng một polling interval, không phải phản hồi tức thời ở từng lifecycle event.
- **Split-screen hoặc multi-window:** một app có thể visible đồng thời với app khác. Cần chốt v1 là chỉ show khi Pokémon GO là top/resumed app, hay show khi Pokémon GO còn visible trong split-screen. Khuyến nghị v1 chọn top/resumed để hành vi dễ giải thích.
- **Pokémon GO đang loading:** activity đã foreground nhưng game chưa sẵn sàng runtime. Overlay có thể hiện vì đây là điều kiện UI; capability/runtime readiness vẫn phải do headless/bridge kiểm soát riêng, không dùng overlay visibility để suy diễn binding đã sẵn sàng.
- **Restart service:** nếu service đang active thì detector bắt đầu hidden cho tới khi có bằng chứng foreground; không hiển thị overlay stale khi app controller vừa khởi động.
- **Location walk đang chạy khi user rời game:** theo scope hiện tại walk vẫn tiếp tục vì đây là state của location controller, không phải visibility của UI. Nếu muốn tự động dừng walk khi game background, cần acceptance criterion và policy riêng.

### 7. Có nhất quán với phần còn lại của app không?

Có, nếu visibility được đặt ở `JoystickOverlayService` — nơi hiện đang sở hữu toàn bộ overlay và vòng refresh `cooldownTick` — thay vì để từng view tự dò package. Một `GameForegroundCoordinator` hoặc `OverlayVisibilityCoordinator` sẽ làm nguồn state duy nhất cho:

```text
Foreground detector
        ↓
JoystickOverlayService
        ├── MainOverlayView
        ├── CooldownOverlayView
        ├── ScanResultOverlays
        └── overlay dialogs
```

Các component chỉ cần API `setVisible(Boolean)`/`hide()` rõ ràng. Không nên tạo một cờ visibility riêng ở mỗi view hoặc để `ScanResultOverlayView.render()` tự làm visible mà không biết game state.

Về source of truth, `AutomationConfigRepository`, `RuntimeStatusRepository` và structured bridge vẫn giữ vai trò hiện tại. Foreground visibility là một UI/lifecycle signal mới; không nên sửa `RuntimeSessionManager` hoặc native binding chỉ để phục vụ việc ẩn overlay.

### 8. Accessibility cần lưu ý gì?

- Khi overlay hidden, nó không được giữ focus/accessibility node có thể nhận tương tác ngoài Pokémon GO.
- Khi show lại, focus không nên tự nhảy vào menu; main overlay trở về collapsed và user chủ động mở shortcut.
- Nếu thiếu Usage Access, trạng thái lỗi phải có text rõ ràng trong `MainActivity`, không chỉ dùng màu hoặc Toast thoáng qua.
- Không thay đổi touch target/content description hiện có của float icon, cooldown và scan widgets trong scope này.
- Dialog bị dismiss khi rời game cần tránh mất dữ liệu im lặng; với form teleport, dữ liệu chưa submit có thể bỏ nhưng cần nhất quán và không thực hiện action ngầm.

### 9. Thiết kế lý tưởng là gì?

Tách hai lifecycle:

1. **Service lifecycle:** service được user start/stop và giữ `START_STICKY` như hiện tại để controller/API không bị phụ thuộc vào việc user đang nhìn game.
2. **Overlay visibility lifecycle:** chỉ được `VISIBLE` khi detector xác nhận Pokémon GO foreground. Mọi state khác (`BACKGROUND`, `UNKNOWN`, thiếu quyền) đều hidden.

Hướng triển khai được khuyến nghị:

- Tạo abstraction nhỏ, ví dụ `GameForegroundDetector`, trả về `FOREGROUND`, `BACKGROUND`, hoặc `UNKNOWN` và nhận allowlist package từ một cấu hình cố định của PoGo adapter.
- Dùng `UsageStatsManager.queryEvents()` với `PACKAGE_USAGE_STATS`; trên API 29+ đọc `ACTIVITY_RESUMED`/`ACTIVITY_PAUSED`, còn API 28 dùng `MOVE_TO_FOREGROUND`/`MOVE_TO_BACKGROUND`. Android ghi nhận `ACTIVITY_RESUMED` là activity chuyển foreground; tài liệu cũng xác nhận các API query usage của package khác cần user cấp Usage Access.
- Poll ở worker/background thread với chu kỳ khoảng 500–1000 ms; chỉ post transition về main thread. Không chạy root shell/dumpsys trên mỗi tick UI.
- Thêm một `applyOverlayVisibility(isGameForeground)` trong `JoystickOverlayService`. Hàm này điều phối tất cả window và dismiss dialog khi chuyển hidden.
- Ưu tiên giữ view attached và đổi visibility thay vì remove rồi add lại mỗi lần đổi app. Cách này giữ vị trí, giảm lỗi lifecycle và tránh trạng thái `lateinit` đã khởi tạo nhưng window đã bị remove.
- Khi hidden, collapse main overlay và ngừng render/resize scan widget ra màn hình; repository/state vẫn có thể được cập nhật. Khi show lại, render snapshot mới nhất.
- Bổ sung luồng hướng dẫn Usage Access trong `MainActivity`; nếu quyền chưa có thì không start vào trạng thái “visible giả”.

Android chính thức cảnh báo `getRunningTasks()` đã bị giới hạn/deprecated và không nên dùng cho core logic điều khiển hành vi; `getRunningAppProcesses()` chỉ mô tả process importance, không phải một nguồn xác nhận top app ổn định. Vì vậy hai cách này không nên là implementation chính. Root `dumpsys activity` có thể là fallback cho device nội bộ, nhưng parsing output theo ROM/Android version sẽ brittle và việc gọi root liên tục tốn tài nguyên; chỉ nên cân nhắc nếu thiết bị test không thể cấp Usage Access.

### 10. Cách validate cải thiện UX

#### Unit/instrumentation

- Test reducer/coordinator với chuỗi `UNKNOWN → FOREGROUND → BACKGROUND → FOREGROUND` và đảm bảo tất cả component nhận cùng transition.
- Test cả hai package được allowlist và package lạ bị reject.
- Test `UNKNOWN`/thiếu permission luôn hidden.
- Test `hide()` nhiều lần và `show()` nhiều lần không add trùng view, không remove một view đã bị remove, không làm service crash.
- Test dialog đang mở được dismiss khi chuyển background.
- Test overlay visibility không thay đổi `JoystickLocationState`, `WalkStatus`, current point hoặc config automation.

#### Device/emulator manual matrix

- Start service từ `MainActivity`, xác nhận chưa thấy overlay; mở Pokémon GO và xác nhận overlay xuất hiện.
- Trong Pokémon GO, mở/collapse menu, bật cooldown/scan result, sau đó bấm Home và mở một app khác; xác nhận không còn bất kỳ floating window/dialog nào.
- Quay lại Pokémon GO; xác nhận overlay xuất hiện lại ở đúng vị trí, main menu collapsed, state config/cooldown không bị reset.
- Force-stop Pokémon GO rồi mở lại; kiểm tra transition không để overlay stale.
- Khóa/mở màn hình, xoay màn hình và thử split-screen nếu thiết bị hỗ trợ.
- Thử cả Google Play package và `com.nianticlabs.pokemongo.ares`.
- Thu hồi Usage Access; xác nhận overlay fail closed và có hướng dẫn sửa quyền.

### 11. Các phương án và trade-off

| Phương án | Ưu điểm | Nhược điểm / quyết định |
|---|---|---|
| `UsageStatsManager` + usage access | API Android chuẩn, phù hợp để suy ra activity foreground, không phụ thuộc root/ROM | Cần permission đặc biệt do user cấp; polling/event có thể trễ | **Khuyến nghị cho v1** |
| Root `dumpsys activity` | Có thể dùng trên thiết bị rooted, không cần thêm màn hình Usage Access | Parse output phụ thuộc Android/ROM, gọi `su` tốn chi phí, dễ hỏng sau update | Chỉ fallback có feature flag/device-specific |
| `ActivityManager.getRunningTasks()` | Code ngắn, thường có vẻ trả top task trên emulator cũ | Deprecated/giới hạn cho app bên thứ ba và tài liệu khuyên không dùng cho core logic | Loại |
| `getRunningAppProcesses()` | Có thể quan sát process importance | Không chứng minh Pokémon GO là app top; process có thể foreground service/background | Chỉ dùng tham khảo/debug |
| Chỉ đọc `RuntimeStatusRepository` | Tận dụng bridge/root status hiện có, biết process còn sống | Không biết process có đang hiển thị cho user | Không đủ cho yêu cầu |
| Hook Unity/native để biết focus | Có thể có tín hiệu sát với game | Version-specific, mở rộng binding boundary không cần thiết cho UX | Không chọn |

### 12. Dependency map và phạm vi thay đổi dự kiến

```text
MainActivity
  ├── request Usage Access / start overlay service
  └── show permission/status guidance

JoystickOverlayService
  ├── GameForegroundDetector
  ├── OverlayVisibilityCoordinator
  ├── MainOverlayView
  ├── CooldownOverlayView
  ├── ScanResultOverlays
  └── Favorite/Teleport/confirm dialogs
```

Phạm vi app có khả năng chạm tới `MainActivity`, `AndroidManifest.xml`, `JoystickOverlayService`, các overlay view và test mới cho detector/coordinator. Không cần chạm vào core movement, `RuntimeStatusRepository`, Zygisk protocol hoặc game adapter. Không thêm screenshot, OCR, `input tap`/`input swipe`, hoặc screen automation fallback.

### 13. Rủi ro implementation

- Usage Access chưa được user cấp làm overlay “không hiện”, dễ bị hiểu nhầm là service hỏng nếu thiếu guidance rõ ràng.
- Ẩn một số view nhưng bỏ sót dialog hoặc widget động sẽ tạo hành vi không nhất quán.
- Remove/re-add window trong mỗi transition có thể gây `BadTokenException`, view leak hoặc mất vị trí; giữ window attached và quản lý visibility an toàn hơn.
- Poll quá dày hoặc chạy `su` trên main thread có thể làm giật overlay, block service và tốn pin.
- Nếu pause location/automation ngoài ý muốn khi hide UI, behavior sẽ vượt scope và có thể làm user mất session walk.

---

## Acceptance Criteria (from spec)

> Source: không tìm thấy spec UI tương ứng trong `docs/newspec/**` hoặc `docs/specs/**`; các tiêu chí dưới đây là **inferred — needs BA confirm**, dựa trên ticket/user requirement “overlay chỉ hiện khi Pokémon GO đang chạy” và implementation hiện tại.

| ID | Rule / Requirement | Formula / Expected | Acceptance note |
|----|--------------------|--------------------|-----------------|
| AC-1 | Chỉ hiện khi Pokémon GO foreground | `foregroundPackage ∈ {com.nianticlabs.pokemongo, com.nianticlabs.pokemongo.ares}` → main/cooldown/scan overlay visible | Đáp ứng trực tiếp yêu cầu; package lạ không được match |
| AC-2 | Ẩn khi rời game | `foregroundPackage ∉ allowlist` hoặc screen non-interactive → mọi floating window và overlay dialog hidden/dismissed | Bao phủ Home, app khác, Settings và lock screen |
| AC-3 | Fail closed khi không biết trạng thái | detector state `UNKNOWN` hoặc thiếu Usage Access → overlay hidden | Không dùng state foreground cũ để giữ view trên app khác |
| AC-4 | Transition tự động | Start service ở `MainActivity` → hidden; mở Pokémon GO → show; rời rồi quay lại → hide/show lại không cần restart service | Có debounce để tránh flash trong lúc chuyển app |
| AC-5 | Hai package chính thức | Cả `com.nianticlabs.pokemongo` và `com.nianticlabs.pokemongo.ares` đều pass; package clone/không rõ bị reject | Đồng nhất với adapter/native allowlist hiện có |
| AC-6 | Không sót child UI | Khi hidden, `FavoriteLocationsDialog`, teleport dialog và confirm dialog không còn hiển thị trên app khác | Dismiss dữ liệu dang dở, không submit ngầm, không tự mở lại |
| AC-7 | Không phá state service | Visibility transition không stop controller, không reset current point/config/cooldown/walk state | Pause location/automation là scope riêng, cần BA confirm nếu muốn |
| AC-8 | Không add/remove trùng | Show/hide lặp lại không tạo duplicate `WindowManager` view, leak hoặc crash; vị trí overlay được giữ | Ưu tiên attached view + `setVisibility`/coordinator |
| AC-9 | Permission guidance | Khi chưa có Usage Access, `MainActivity` hiển thị trạng thái và mở đúng `Settings.ACTION_USAGE_ACCESS_SETTINGS` | User biết vì sao overlay đang hidden và cách sửa |
| AC-10 | Performance/lifecycle | Poll không block main UI; sau service destroy không còn callback detector/tick hoặc window attached | Cần kiểm tra rotate, force-stop và restart service |
| AC-11 | Test matrix | Unit test detector/reducer + device test Home/app khác/game relaunch/lock/rotate/hai package | Tiêu chí hoàn thành do hiện chưa có UI automation framework |

- Requirement “overlay chỉ hiện khi Pokémon GO đang chạy/đang dùng” → **AC-1, AC-2, AC-3, AC-4**.
- Requirement “không còn overlay ngoài game” → **AC-2, AC-6, AC-8**.
- Requirement “không làm hỏng joystick/automation hiện có” → **AC-7, AC-10, AC-11**.

---

## Synthesis

### Key Insight

Process Pokémon GO còn sống không đồng nghĩa Pokémon GO đang hiển thị cho user. `runtime-status.sh` hiện chỉ cung cấp liveness, nên không thể dùng một mình để quyết định visibility. Muốn đáp ứng đúng UX, cần một foreground detector riêng và một coordinator tại `JoystickOverlayService` để điều khiển đồng bộ tất cả window/dialog.

### Recommended Approach

Chốt v1 theo nghĩa “Pokémon GO đang foreground”, dùng `UsageStatsManager`/usage events với Usage Access và fail closed khi permission hoặc state không chắc chắn. Giữ service/controller sống độc lập, chỉ ẩn/hiện các overlay view; khi rời game thì collapse main overlay và dismiss dialog, khi quay lại thì show lại ở trạng thái an toàn. Tạo test cho state transitions, hai package Pokémon GO, permission thiếu và lifecycle show/hide trước khi implement UI code.

### Risks to Watch

- User chưa cấp Usage Access sẽ thấy overlay không hiện nếu không có hướng dẫn rõ.
- Dùng API top-task/process hoặc root `dumpsys` như nguồn chính có thể sai trên Android/ROM mới.
- Sót dialog hoặc dùng remove/re-add window không an toàn có thể làm overlay vẫn nổi hoặc service crash.

### Open Questions

- ~~“Đang chạy” có chắc nghĩa là **foreground/top app**, hay chỉ cần process Pokémon GO còn alive?~~ → Đã chốt trong Section 15: Pokémon GO phải đang nằm trên màn hình/foreground.
- Có cần show overlay khi Pokémon GO visible trong split-screen/multi-window không, hay chỉ khi là top/resumed app?
- Khi Pokémon GO background, `walkTo`/joystick automation có tiếp tục chạy hay phải pause/stop? Khuyến nghị giữ nguyên behavior hiện tại cho scope visibility.
- Có chấp nhận yêu cầu user cấp Usage Access không? Nếu không, có cho phép fallback root `dumpsys activity` riêng cho emulator/device đã biết không?
- Khi service được start lúc Pokémon GO chưa mở, có cần auto-launch game hay chỉ giữ overlay hidden chờ user mở game?

## Section 14 — Đánh giá tính khả thi

Có thể triển khai trong phạm vi app hiện tại, không cần thay đổi Zygisk, game binding hoặc core movement. Phần chính là thêm detector foreground, xin Usage Access, rồi để `JoystickOverlayService` ẩn/hiện đồng bộ `MainOverlayView`, `CooldownOverlayView`, `ScanResultOverlays` và các dialog overlay.

Mức độ rủi ro thấp đến vừa: Android cung cấp đủ tín hiệu để nhận biết app foreground, nhưng cần xử lý permission, polling/debounce và lifecycle của `WindowManager`. Nếu chấp nhận Usage Access, đây là hướng nên làm cho v1; nếu không, phải chọn fallback root `dumpsys` với rủi ro phụ thuộc ROM/Android version.

Kết luận: **làm được**, có thể bắt đầu implement sau khi chốt một điểm UX là “đang chạy” nghĩa là foreground/top app. Theo đề xuất hiện tại, service vẫn chạy nền và chỉ ẩn UI; không tự dừng `walkTo` hoặc automation khi user rời Pokémon GO.

## Section 15 — Chốt định nghĩa “đang chạy”

Người dùng xác nhận “đang chạy” nghĩa là Pokémon GO đang nằm trên màn hình và là app foreground mà user đang sử dụng. Process Pokémon GO còn sống nhưng đã bị đưa xuống nền không đủ điều kiện để hiển thị overlay.

Vì vậy, acceptance chính là: mở Pokémon GO trên màn hình thì overlay hiện; chuyển sang Home/app khác hoặc khóa màn hình thì overlay ẩn. Phạm vi này không yêu cầu tự dừng service, location controller hay headless automation.
