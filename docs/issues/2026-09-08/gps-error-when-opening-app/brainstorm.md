# Brainstorm: Pokémon GO báo lỗi GPS khi mở PoGo Root Automation

**Type:** bug
**Date:** 2026-09-08

---

## Analysis

### 1. Hành vi hiện tại

Khi PoGo Root Automation đang chạy, Pokémon GO báo lỗi GPS/không nhận được vị trí. Kiểm tra trực tiếp trên `emulator-5564` cho thấy:

- `JoystickOverlayService` đang là foreground service.
- Android đang ghi nhận `gps provider [mock]` và `network provider [mock]`, với identity `10088/dev.pogoroot.automation`.
- Pokémon GO (`com.nianticlabs.pokemongo`) đã đăng ký nhận GPS/fused location và các provider mock đã deliver location tới Pokémon GO.

Đây là bằng chứng hệ thống đang nhận location giả từ app, không phải chỉ là lỗi hiển thị overlay.

### 2. Hành vi kỳ vọng

Chỉ mở màn hình controller hoặc chạy headless bridge không được tạo mock GPS. Mock location chỉ nên hoạt động sau khi người dùng chủ động bấm `Start built-in joystick`; khi joystick dừng, test providers phải được gỡ khỏi Android.

### 3. Điều kiện và phạm vi xảy ra

Lỗi xảy ra trên BlueStacks `emulator-5564`, nơi có quyền root và module Zygisk. Code hiện tại dùng Android test providers nên ảnh hưởng toàn hệ thống thiết bị, không chỉ process của controller.

Có một điểm cần phân biệt: `MainActivity.onCreate()` chỉ gọi `HeadlessAutomationService.start()`; nó không gọi `JoystickOverlayService`. Vì vậy, nếu user chỉ mở app mà không bấm start joystick, thời điểm mở app có thể chỉ trùng với một `JoystickOverlayService` đã được start từ trước hoặc được Android giữ/restart lại.

### 4. Có tái hiện được không?

Có thể tái hiện theo flow:

1. Start built-in joystick một lần.
2. Đóng/đổi khỏi controller hoặc mở Pokémon GO.
3. Kiểm tra `dumpsys location`: GPS và network xuất hiện với trạng thái `[mock]`.
4. Pokémon GO đăng ký GPS/fused updates và có thể báo lỗi GPS do nhận mock provider/location.

Live diagnostics hiện tại cho thấy cả `JoystickOverlayService` và `HeadlessAutomationService` đang chạy. `dumpsys location` cũng có các dòng `gps provider [mock]`/`network provider [mock]` và location delivery tới Pokémon GO.

### 5. Nguyên nhân gốc

`RootMockLocationProvider.start()`:

- cấp `android:mock_location allow` cho package bằng root;
- tạo hoặc tái sử dụng test provider tên `gps`;
- tạo hoặc tái sử dụng test provider tên `network`;
- bật cả hai provider.

`JoystickOverlayService.onStartCommand()` luôn gọi `controller.start(...)`, sau đó controller gọi `sink.start()`. Service trả về `START_STICKY`, nên nếu service đã từng được start, Android có thể giữ hoặc khởi tạo lại service sau khi process bị reclaim. Khi service bị hủy đúng lifecycle, `onDestroy()` mới gọi `controller.stop()` để remove hai provider.

Ngoài ra, `applyGameForeground()` chỉ ẩn/hiện overlay; khi Pokémon GO không ở foreground, nó không stop location controller. Vì vậy UI có thể biến mất nhưng mock GPS vẫn tiếp tục tồn tại.

### 6. Dependency map

**Upstream:**

- `MainActivity` khởi động `HeadlessAutomationService`.
- Nút `Start built-in joystick` gửi `ACTION_START` tới `JoystickOverlayService`.
- Android service lifecycle có thể gửi lại intent/null intent cho service sticky.

**Core path:**

`JoystickOverlayService` → `JoystickLocationController` → `RootMockLocationProvider` → `LocationManager` test providers → Android GPS/fused pipeline.

**Downstream:**

- Google Play Services fused/network location.
- Pokémon GO GPS/fused requests.
- Pokémon GO mock-location/GPS validation và map position.

**Shared state:**

`RootMockLocationProvider` dùng provider names toàn hệ thống (`gps`, `network`), nên mọi app yêu cầu location trên emulator đều có thể bị ảnh hưởng.

### 7. Phạm vi ảnh hưởng

Ảnh hưởng toàn bộ flow GPS trên BlueStacks khi joystick service còn sống: Pokémon GO, Google Play Services và các app khác dùng `LocationManager`. Headless bridge đơn thuần không phải nguồn trực tiếp của mock location theo code hiện tại.

### 8. Vì sao bug tồn tại?

- Location provider được start ở service lifecycle, không gắn chặt với một explicit `ACTION_START` hợp lệ.
- `START_STICKY` phù hợp với service muốn sống lâu nhưng nguy hiểm cho một mock provider toàn hệ thống nếu không có state opt-in rõ ràng.
- Ẩn overlay bị tách khỏi việc stop location, tạo cảm giác app đã tắt nhưng provider vẫn hoạt động.
- Chưa có device regression test xác nhận `dumpsys location` không còn `[mock]` sau khi controller UI đóng/dừng.

### 9. Rủi ro khi sửa

- Stop provider khi game chuyển background có thể làm mất walk/teleport đang chủ động chạy nếu đó không phải policy mong muốn.
- Chỉ đổi `START_STICKY` thành `START_NOT_STICKY` không dọn provider đã được tạo trong một process đang sống.
- Chỉ revoke app-op nhưng không remove test providers có thể vẫn để lại trạng thái mock trong location service.
- Gỡ mock provider trong khi Pokémon GO đang request location có thể cần game retry/reload map; đây là hành vi mong muốn khi joystick đã stop nhưng cần kiểm thử.

### 10. Minimal correct fix

Hướng tối thiểu an toàn:

1. Chỉ khởi động `JoystickLocationController` khi nhận đúng `ACTION_START`; không tự start khi intent null hoặc action khác.
2. Đổi joystick service thành `START_NOT_STICKY` để Android không tự resurrect một mock provider sau khi process bị kill.
3. Giữ `onDestroy()` cleanup provider, đồng thời thêm một đường stop idempotent có thể gọi rõ ràng từ UI/ADB.
4. Quyết định policy riêng: chuyển app/game ra background chỉ ẩn overlay hay cũng dừng mock location. Không nên âm thầm đổi policy này nếu user vẫn muốn walk tiếp tục.

Không nên sửa bằng screenshot/input tap hoặc anti-detection; lỗi hiện tại là lifecycle của mock provider và có thể xử lý ở app location boundary.

### 11. Cách xác minh

- Mở MainActivity khi joystick chưa từng start → `dumpsys location` không có `gps/network provider [mock]` của package.
- Start joystick → providers mock xuất hiện và location state báo provider ready.
- Stop joystick → service biến mất, hai test providers bị remove, Pokémon GO có thể request GPS thật sau khi reload.
- Kill/restart app process → joystick không tự quay lại nếu user chưa start lại.
- Chạy unit tests cho service/controller lifecycle và device smoke check trên BlueStacks.

---

## Acceptance Criteria (from spec)

> Source: no dedicated GPS bug spec found → inferred — needs BA/user confirmation.

| ID | Rule / Requirement | Formula / Expected | Acceptance note |
|----|--------------------|--------------------|-----------------|
| GPS-AC-1 | Mở controller không tự mock GPS | MainActivity launch alone không tạo test provider | Gắn với triệu chứng “mở app là lỗi GPS” |
| GPS-AC-2 | Explicit start mới bật mock | Chỉ `ACTION_START` hợp lệ mới gọi `RootMockLocationProvider.start()` | Không tự start theo null/unknown service intent |
| GPS-AC-3 | Stop phải cleanup | Sau stop, `gps` và `network` không còn `[mock]` bởi `dev.pogoroot.automation` | Có thể xác minh bằng `dumpsys location` |
| GPS-AC-4 | Không tự resurrect provider | Service restart/process reclaim khi chưa có opt-in không tạo lại mock provider | Kiểm tra sau force-stop/restart/reboot |
| GPS-AC-5 | Headless không ảnh hưởng GPS | Headless bridge chạy độc lập nhưng không gọi `LocationManager` mock path | Giữ structured bridge read-only/fail-closed |
| GPS-AC-6 | Không regression location có chủ đích | Khi user start joystick, publish/teleport/walk vẫn hoạt động và provider ready | Chỉ cleanup khi stop hoặc policy đã xác nhận |

- Bug được xem là hết khi `GPS-AC-1` và `GPS-AC-3` pass trên `emulator-5564`.
- Không được gọi là fix hoàn chỉnh nếu chỉ overlay biến mất nhưng `dumpsys location` vẫn báo provider mock.

---

## Synthesis

### Key Insight

GPS lỗi vì app đang chạy `JoystickOverlayService`, không phải vì APK mở màn hình Activity đơn thuần. Service cấp mock-location app-op, bật đồng thời `gps` và `network` test providers, rồi Android chuyển các update đó cho Pokémon GO; live `dumpsys location` đã xác nhận chính xác điều này.

### Recommended Approach

Tạm thời bấm `Stop joystick` trước khi mở/chơi Pokémon GO; nếu service bị kẹt, force-stop riêng package controller để `onDestroy()` cleanup provider. Khi sửa code, gate controller theo `ACTION_START`, dùng `START_NOT_STICKY`, và bổ sung device regression check cho trạng thái `[mock]`.

### Risks to Watch

- Dừng provider giữa walk/teleport sẽ làm game mất vị trí tạm thời.
- Android/BlueStacks có thể giữ app-op mock sau khi provider bị remove; cần kiểm tra cả provider và app-op.
- Pokémon GO có thể cần restart/reload map sau khi chuyển từ mock sang GPS thật.

### Open Questions

- Khi user chuyển Pokémon GO ra background, có muốn tiếp tục mock/walk hay phải stop location ngay?
- “GPS error” cụ thể là `GPS signal not found`, `Error 12` hay thông báo khác?
- Có cần app tự revoke `android:mock_location` khi stop joystick, hay giữ quyền để lần start tiếp theo nhanh hơn?

## Section 12 — Runtime bridge `Permission denied`

### Evidence

- Package controller có UID `10088`; logcat ghi đúng toast `runtime bridge: Permission denied`.
- `runtime-status.sh` trên thiết bị trả `runtime_state=not_seen` và `probe_state=not_running`, tức native Zygisk runtime chưa từng tạo bridge socket.
- Magisk trên emulator là `v27.2-kitsune-4`; module cũ có file `zygisk/unloaded`.
- Module cũ được build với header Zygisk API 5, trong khi Magisk v27.x dùng API 4. Vì native library không được load, thư mục bridge vẫn ở mode `0700`; app không thể traverse tới `runtime.sock`, nên lỗi bề mặt là `Permission denied`.

### Root cause

Đây là lỗi packaging/runtime ABI trước khi là lỗi quyền của app: native companion không được Zygisk load, rồi client app cố kết nối vào một socket chưa tồn tại trong thư mục không cho app traverse. Có hai lớp cần sửa: pin header API tương thích với Magisk v27.x và đặt thư mục bridge ở `0711`; quyền kết nối thực tế vẫn được native companion kiểm tra bằng `SO_PEERCRED` và allowlist UID.

### Fix applied

1. Pin workflow và tài liệu vào header Zygisk API 4 chính thức.
2. Đổi `post-fs-data.sh` để thư mục bridge là `0711`.
3. Sửa `JoystickOverlayService` chỉ khởi động khi có `ACTION_START`, dùng `START_NOT_STICKY`.
4. Khi stop, remove test providers và revoke `android:mock_location` app-op để không giữ GPS giả toàn hệ thống.

### Verification plan

- Build lại hai ABI, APK và Magisk ZIP.
- Cài ZIP, reboot emulator, xác nhận `zygisk/unloaded` biến mất và native runtime chuyển khỏi `not_seen`.
- Cài APK, khởi động headless bridge, kiểm tra logcat/runtime status không còn `Permission denied`.
- Force-stop controller và kiểm tra `dumpsys location` không còn `gps`/`network` provider mock của package.

## Section 13 — Filesystem parent boundary và abstract bridge socket

### New evidence

- Sau khi module API 4 load thành công, `/data/adb/pogo_root_automation` đã là `0711` và socket filesystem là `0666`, nhưng parent `/data/adb` vẫn là `0700`.
- `runtime-status.sh` báo `runtime_state=connected`; native socket tồn tại, nhưng app vẫn nhận `runtime bridge: Permission denied`.
- Khi force-stop app, `dumpsys location` vẫn giữ test providers cũ; điều này xác nhận cần cleanup ở lần khởi tạo process mới, không chỉ dựa vào `Service.onDestroy()`.

### Refined root cause

`0711` ở thư mục con không thể vượt qua parent `/data/adb` mode `0700`. Vì vậy filesystem Unix socket luôn có thể trả `EACCES` cho Android app dù socket đã chmod `0666`; SELinux không phải yếu tố chính trên emulator này vì `getenforce` là `Disabled`.

### Additional fix

1. Đổi transport bridge sang abstract Unix socket `pogo_root_automation_runtime`; không phụ thuộc quyền traverse filesystem.
2. Giữ `SO_PEERCRED` và allowlist UID root-owned làm authorization boundary.
3. Dọn test providers stale ở `AutomationApplication.onCreate()` trước khi khởi tạo service mới.
4. Tiếp tục xóa filesystem socket cũ trong `post-fs-data.sh` để migration không để lại artifact gây nhầm lẫn.

### Updated acceptance check

- Native runtime: `protocol=4`, `runtime_state=connected`, `probe_state=ready`.
- Controller: kết nối tới abstract socket không còn `EACCES`.
- App launch không có joystick service; `dumpsys location` không có `gps/network provider [mock]` của UID `10088`.

## Section 14 — Tự bật joystick khi Pokémon GO được mở

### Bối cảnh và quan sát hiện trạng

User xác nhận hành vi mong muốn là khi Pokémon GO được mở lên thì joystick tự bật, không cần switch sang controller app để bấm nút. Kiểm tra code cho thấy `MainActivity.startBuiltInJoystick()` là nơi duy nhất gọi `JoystickOverlayService` với `ACTION_START`; `HeadlessAutomationService` và `AutomationBootReceiver` hiện chỉ khởi động automation server, chưa điều phối joystick theo app foreground.

Kiểm tra device `emulator-5564` cũng khớp với nguyên nhân này: khi joystick đã được gọi, dumpsys hiển thị `JoystickOverlayService` với action `dev.pogoroot.automation.action.START_JOYSTICK`; chưa có trigger foreground-to-service tương ứng. Việc phải switch app vì vậy là do thiếu coordinator ở service nền, không phải do Pokémon GO tự bật/tắt overlay.

### Phạm vi cần đạt

- **AUTO-JOY-1 — Detect foreground:** dùng `GameForegroundDetector` và Usage Access hiện có để nhận diện cả `com.nianticlabs.pokemongo` và `com.nianticlabs.pokemongo.ares`.
- **AUTO-JOY-2 — Auto-start:** khi game chuyển sang foreground, `HeadlessAutomationService` tự gọi `JoystickOverlayService.ACTION_START`.
- **AUTO-JOY-3 — Cleanup:** khi game rời foreground, hoặc state là `UNKNOWN`/thiếu quyền, dừng joystick để dọn mock provider và tránh lỗi GPS signal rỗng ngoài game.
- **AUTO-JOY-4 — Transition-safe:** chỉ start/stop khi eligibility đổi trạng thái; nút Stop thủ công không bị monitor gọi start lại liên tục khi game vẫn đang foreground.
- **AUTO-JOY-5 — Permission-safe:** chỉ auto-start khi có Usage Access và overlay permission; thiếu một trong hai thì fail closed.
- **AUTO-JOY-6 — Lifecycle:** monitor chạy từ boot/headless service, được cancel trong `onDestroy`, không tạo thêm native hook hay screenshot/input fallback.

### Các lựa chọn

1. **Theo dõi trong `HeadlessAutomationService` (chọn):** tái sử dụng service nền đã chạy từ boot, `GameForegroundDetector` và lifecycle hiện có; thay đổi nhỏ, đúng boundary Android controller.
2. **Theo dõi bằng Zygisk/native khi process game attach:** phản hồi sớm hơn nhưng mở rộng native responsibility, tăng rủi ro runtime bridge và không cần thiết cho việc biết package foreground.
3. **Theo dõi bằng Activity/Accessibility riêng:** thêm permission và thành phần sống riêng, phức tạp hơn Usage Access hiện có.

### Thiết kế chọn

Thêm một coordinator polling nhẹ trong `HeadlessAutomationService` khoảng 750 ms. Mỗi lần poll, eligibility chỉ đúng khi detector trả `FOREGROUND` và overlay permission còn đủ. Khi `false -> true`, gửi `ACTION_START`; khi `true -> false`, stop service. Lần sync đầu tiên cũng stop nếu không eligible để dọn service/mock provider cũ. Nếu lệnh start bị Android từ chối tạm thời, không ghi nhận transition thành công để lần poll sau retry. Khi controller bắt đầu, seed location theo thứ tự điểm đã lưu rồi tọa độ mặc định hợp lệ `21.027764,105.834160`, tránh bật mock provider nhưng chưa publish location.

### Acceptance Criteria

- [ ] AC-AUTO-1: Sau khi headless service đã chạy và đủ Usage Access + overlay permission, mở Pokémon GO sẽ tự tạo `JoystickOverlayService`; không cần mở controller app.
- [ ] AC-AUTO-2: Khi switch khỏi Pokémon GO, joystick service dừng và mock GPS được cleanup.
- [ ] AC-AUTO-3: Thiếu Usage Access hoặc overlay permission không auto-start, không để mock provider rỗng tồn tại.
- [ ] AC-AUTO-4: Stop thủ công không bị restart liên tục trong cùng một phiên foreground; vào lại game mới cho phép auto-start lại.
- [ ] AC-AUTO-5: Nếu chưa có điểm lưu, auto-start publish tọa độ seed hợp lệ thay vì để `last mock location=null`.
- [ ] AC-AUTO-6: Cả hai package Pokémon GO được xử lý; `git diff --check`, test Gradle và smoke check device đều pass.

### Open Questions update

- Câu hỏi trước đây “service start lúc Pogo chưa mở có nên auto-launch không?” đã được chốt theo yêu cầu mới: có, nhưng chỉ khi game đạt foreground và quyền cần thiết; ngoài game phải stop để bảo đảm an toàn GPS.
- Việc controller/headless service đã chạy hay chưa vẫn là điều kiện nền tảng sau cài mới. Boot receiver đảm nhiệm sau reboot; lần đầu sau cài đặt cần mở controller một lần để cấp Usage Access/overlay và khởi động service nền.

### Synthesis

Giải pháp phù hợp là bổ sung foreground-to-joystick coordinator trong `HeadlessAutomationService`, giữ nguyên bridge/native boundary. Cách này giải quyết trực tiếp việc phải switch app, đồng thời ngăn lỗi GPS do joystick/mock provider bị giữ khi Pokémon GO không còn foreground.
