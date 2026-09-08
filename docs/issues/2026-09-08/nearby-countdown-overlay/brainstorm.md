# Brainstorm: Hiển thị cooldown teleport trên overlay

**Type:** feature
**Date:** 2026-09-08

---

> Cập nhật phạm vi: Section 11 là yêu cầu đã được người dùng xác nhận và supersede nhánh phân tích ban đầu về countdown despawn nearby.

## Analysis

### 1. Tính năng này giải quyết vấn đề gì?

Repo đã có `CountdownService` để tính `remainingMillis`, trạng thái hết hạn và độ tin cậy của expiry, nhưng kết quả mới chỉ nằm trong `core` và chưa được đưa lên UI. Người dùng cần nhìn nhanh thời gian còn lại của Pokémon nearby ngay trên floating overlay thay vì phải xem qua log/API hoặc tự suy ra từ snapshot.

Phạm vi hợp lý của yêu cầu là countdown theo từng nearby spawn. Đây không phải countdown cho animation/action catch, vì các delay cũ đã bị loại khỏi `AutomationConfig` và architecture hiện tại không còn coi chúng là nguồn trạng thái UI.

### 2. Ai được hưởng lợi?

Người vận hành app trên thiết bị/emulator root, khi overlay joystick đang mở cùng Pokémon GO. Tính năng áp dụng cho mọi user có runtime bridge phát được nearby snapshot; không phụ thuộc việc automation mutation đã được allowlist hay chưa.

Hiện tại nếu không có overlay, user vẫn có thể dùng automation như cũ. Nếu runtime chưa phát nearby hoặc expiry chưa biết, UI phải nói rõ trạng thái đó thay vì hiển thị một số giả.

### 3. Các use case cốt lõi

Must-have:

- Khi nearby snapshot có spawn với `expiresAtEpochMs`, overlay hiển thị tên Pokémon và countdown dạng `MM:SS`.
- Countdown tự giảm theo thời gian dù không có observation mới mỗi giây; khi chạm hết hạn, item không còn được xem là đang active.
- Spawn `ESTIMATED` có dấu `~` để phân biệt với expiry exact.
- Spawn `UNKNOWN`/không có expiry hiển thị `--:--`, không tự đặt default duration.
- Overlay hiển thị tối đa một số item nhỏ, ưu tiên item sắp hết hạn để panel không phình ra.
- Khi runtime chưa có snapshot hoặc bị disconnect/error, overlay hiển thị trạng thái chờ/unavailable và không giữ số liệu cũ như dữ liệu hiện hành.

Nice-to-have, chưa đưa vào lần này:

- Chọn một spawn cụ thể trên bản đồ.
- Bộ lọc theo species/IV/shiny.
- Progress bar, màu cảnh báo và notification khi còn dưới ngưỡng.

### 4. Edge cases

- `expiresAtEpochMs == null`: giữ `remainingMillis == null`, hiển thị `--:--`.
- `expiresAtEpochMs <= now`: `CountdownService` clamp về `0L`; UI lọc item đã expired khỏi nhóm active.
- `expiryConfidence == ESTIMATED`: số vẫn được tính từ timestamp nhưng phải có marker `~`.
- Snapshot rỗng: hiển thị “không có Pokémon” thay vì blank panel.
- Nhiều spawn: sort expiry sớm nhất trước, unknown ở cuối, giới hạn số dòng.
- Tên species chứa ký tự phân cách hoặc rất dài: state repository phải encode an toàn, formatter phải cắt để không phá layout overlay.
- Người dùng bật overlay trước khi headless service có dữ liệu: hiển thị trạng thái chờ và cập nhật khi state dùng chung xuất hiện.
- Service/process restart: state cũ không được coi là live; headless service clear state khi khởi động, stop hoặc gặp lỗi bridge.
- Clock nhảy/lệch: countdown dựa trên `System.currentTimeMillis()` và timestamp expiry cùng epoch; giá trị âm được clamp về zero trong core.
- Runtime live hiện đang map expiry chưa biết thành `UNKNOWN`; do đó overlay có thể hiển thị `--:--` cho dữ liệu thật cho đến khi adapter cung cấp expiry. Không được suy diễn một despawn time exact từ `firstSeenAtEpochMs`.

### 5. Ràng buộc

Technical:

- `core` không được phụ thuộc Android/UI. Logic tính countdown tiếp tục nằm ở `CountdownService` và được tái sử dụng bởi formatter.
- Structured runtime là nguồn sự thật duy nhất cho nearby; overlay không đọc protobuf, không dùng screenshot và không tự query game.
- Hai service chạy trong cùng app nhưng có lifecycle riêng, vì vậy cần một state bridge app-local an toàn giữa headless service và overlay. `SharedPreferences` phù hợp với payload nhỏ và không cần thêm dependency.
- UI update chạy main thread mỗi 1 giây; việc lưu snapshot chỉ gồm metadata countdown, không copy payload protobuf lớn.

Business/design:

- Overlay hiện là panel dark, text monospace, width khoảng 230dp; countdown cần compact và không phá joystick.
- Backward compatibility: nếu runtime không có expiry, behavior cũ vẫn giữ nguyên, chỉ thêm dòng trạng thái.

### 6. Các phương án đã cân nhắc

1. Overlay gọi trực tiếp `StructuredAutomationController`: không phù hợp vì controller thuộc headless service và không có binder/API ổn định.
2. Overlay gọi HTTP `GET /v1/status`: tạo coupling vòng qua loopback, status hiện tại cũng không mang danh sách spawn, và polling thêm socket không cần thiết.
3. Singleton in-memory: đơn giản nhưng mất state khi process/service restart và khó phân biệt state stale.
4. State repository app-local bằng `SharedPreferences`: payload nhỏ, lifecycle rõ, overlay chỉ đọc, dễ clear khi disconnect; đây là phương án được chọn.

### 7. Tương tác với feature hiện có

`StructuredAutomationController` đã tạo `AutomationSnapshot` từ `PogoGameAdapter`; chỉ cần giữ nearby snapshot mới nhất trong tick status. `HeadlessAutomationEngine` publish state đó cho repository. Automation planner/runner không đổi behavior; countdown chỉ là read-only presentation.

`JoystickOverlayService` đang có `automationSummaryView`, nên thêm một `countdownView` cùng style và một runnable 1 giây. Drag, teleport, speed, settings và notification không đổi.

### 8. Dependencies

- Phụ thuộc vào `NearbySnapshot`, `NearbySpawn`, `SpawnExpiryConfidence` và `CountdownService` hiện có.
- Không cần API endpoint mới, thư viện thứ ba hay thay đổi bridge protocol.
- Để thấy số countdown thật trong production, version-specific runtime sau này vẫn phải cung cấp `expiresAtEpochMs`; lần này chỉ hoàn thiện data/UI path và hành vi honest cho expiry unknown.

### 9. Rủi ro

- Nếu không clear state khi bridge mất kết nối, overlay có thể hiển thị số cũ khiến user tưởng runtime vẫn live. Giảm thiểu bằng clear lúc service start/stop và engine error/disabled.
- Nếu duplicate logic tính thời gian giữa core và UI, số hiển thị có thể lệch hoặc không nhất quán. Giảm thiểu bằng API `CountdownService.forExpiry` dùng chung.
- Nếu hiển thị toàn bộ nearby list, overlay sẽ che game/joystick. Giảm thiểu bằng giới hạn 3 dòng và ưu tiên expiry sớm.

### 10. Tiêu chí nghiệm thu sơ bộ

Các tiêu chí chi tiết được chuẩn hóa ở bảng bên dưới. Tính năng được coi là hoàn thành khi core test pass, app compile pass và overlay có đủ trạng thái known/estimated/unknown/unavailable theo các tiêu chí đó.

---

## Acceptance Criteria (from spec)

> Source: không tìm thấy `docs/newspec/**` hoặc `docs/specs/**` cho feature này; yêu cầu từ ticket/user được chuyển thành tiêu chí **inferred — needs BA confirm**.

| ID | Rule / Requirement | Formula / Expected | Acceptance note |
|----|--------------------|--------------------|-----------------|
| AC-1 | Tính remaining cho expiry đã biết | `remaining = max(expiresAtEpochMs - nowEpochMs, 0)` | Dùng `CountdownService`; giá trị expired là `0`, không âm. |
| AC-2 | Hiển thị exact countdown trên overlay | `remainingMillis != null` → `MM:SS` | Dòng có tên Pokémon và tự refresh khoảng 1 giây. |
| AC-3 | Đánh dấu estimated | `expiryConfidence == ESTIMATED` → prefix `~` | User biết số là ước tính, không nhầm với exact. |
| AC-4 | Giữ unknown là unknown | `expiresAtEpochMs == null` → `--:--` | Không fabricate duration từ `firstSeenAtEpochMs`. |
| AC-5 | Ưu tiên và giới hạn danh sách | sort known expiry tăng dần, unknown cuối, tối đa 3 dòng | Overlay giữ kích thước nhỏ, item sắp hết hạn được thấy trước. |
| AC-6 | Hết hạn không còn active | `expiresAtEpochMs <= now` → không render trong nhóm active | Core vẫn trả `isExpired=true`, overlay không giữ item `00:00` vô thời hạn. |
| AC-7 | Trạng thái không có dữ liệu | chưa có snapshot → `waiting/unavailable`; snapshot rỗng → `no Pokémon` | Không hiển thị snapshot cũ sau service restart/disconnect/error. |
| AC-8 | Không ảnh hưởng automation hiện có | planner/runner/action flow không đổi | Feature chỉ đọc nearby state và render overlay. |
| AC-9 | Verification | `./gradlew test` và `./gradlew :app:assembleDebug` pass | Bao phủ core countdown, formatter và compile Android. |

- Requirement “tính countdown” của user → AC-1, AC-2, AC-3, AC-4.
- Requirement “có hiển thị trên overlay” của user → AC-5, AC-6, AC-7.
- Bảo toàn architecture/runtime hiện có → AC-8, AC-9.

> Lưu ý: AC-1..AC-9 là phân tích ban đầu cho despawn countdown; sau khi user làm rõ, bộ tiêu chí áp dụng là CD-1..CD-7 ở Section 11.

---

## Synthesis

### Key Insight

Cooldown cần có hai nguồn tính độc lập: teleport gần nhất từ vị trí hiện tại, hoặc action đánh dấu vị trí gần nhất. Khoảng cách và `readyAt` phải được tính ở domain service; overlay chỉ giảm remaining theo đồng hồ hiện tại.

### Recommended Approach

Thêm `GeoMath.distanceMeters`, `TeleportCooldownService` và hai mode trong `core`. `JoystickLocationController` tạo candidate cho mode `CURRENT_POSITION` sau mỗi teleport publish thành công. Headless structured runner ghi nhận action `spin`/`catch`/`berry` gần nhất vào app-local repository để overlay tính mode `LAST_ACTIVE`. Overlay có nút chuyển mode, render `MM:SS`/`HH:MM:SS` mỗi giây, và gắn nhãn đây là safety estimate; không tự động chặn action.

### Risks to Watch

- Bảng cooldown cộng đồng không phải contract chính thức của Niantic và có thể thay đổi.
- Cooldown server còn phụ thuộc action đánh dấu vị trí mà app chưa quan sát được.
- Timer có thể gây hiểu nhầm nếu được coi là cơ chế chặn action thay vì chỉ safety estimate.

### Open Questions

- ~~Countdown đang cần cho despawn nearby hay cooldown của action?~~ → Resolved in Section 11: cần cooldown sau khi đổi location/teleport, không phải despawn nearby.
- Ngưỡng cảnh báo màu/notification khi còn ít giây? Chưa có yêu cầu; để sau.
- Các câu hỏi về runtime expiry và số dòng nearby countdown không còn thuộc scope sau khi chốt cooldown teleport.

---

## Section 11 — Làm rõ phạm vi: cooldown sau khi teleport

Người dùng xác nhận countdown cần hiển thị thời gian chờ sau khi dịch chuyển location theo rule cooldown thường dùng trong Pokémon GO spoofing, không phải thời gian despawn của nearby spawn. Nhánh triển khai nearby countdown ở trên được xem là hướng phân tích ban đầu và không phải acceptance cho feature này.

### Quy tắc được chọn

- Khoảng cách tính bằng khoảng cách địa lý giữa vị trí trước và sau teleport, dùng `GeoMath`/Haversine.
- Chỉ teleport không tự chứng minh một cooldown server-side; cooldown trong game thường gắn với action đánh dấu vị trí trước đó. Vì repo không quan sát được toàn bộ action account/server, overlay phải gọi đây là `estimated cooldown`/`safety cooldown`, không tuyên bố là trạng thái server chính thức.
- Với bản triển khai đầu tiên, dùng bảng bracket bảo thủ, làm tròn lên bracket kế tiếp khi khoảng cách nằm giữa hai mốc. Mốc tối đa là 2 giờ; khoảng cách rất ngắn có thể là `0:00`/không cần chờ.
- Fixture đang dùng: `1 km=1 min`, `2 km=2 min`, `4 km=6 min`, `5 km=6 min`, `10 km=8 min`, `25 km=15 min`, `50 km=24 min`, `100 km=30 min`, `250 km=46 min`, `500 km=64 min`, `750 km=82 min`, `1,000 km=100 min`, `1,200 km=115 min`, `1,300 km+=120 min`.
- Countdown bắt đầu sau khi mock-location publish thành công, dựa trên `teleportAtEpochMs + cooldownDurationMs`; mỗi lần teleport mới thay thế timer hiện tại.
- Di chuyển bằng joystick liên tục không nên reset timer ở mỗi tick 50ms; chỉ thao tác teleport đột ngột tạo một candidate cooldown trong phạm vi feature này.

### Acceptance criteria mới (thay cho AC-1..AC-9 ở phần trên)

| ID | Rule / Requirement | Formula / Expected | Acceptance note |
|----|--------------------|--------------------|-----------------|
| CD-1 | Tính khoảng cách teleport | `distanceKm = distance(previousPoint, targetPoint)` | Cùng một điểm hoặc chênh lệch rất nhỏ không tạo cooldown đáng kể. |
| CD-2 | Tính thời lượng theo bracket | `cooldownMs = table.ceil(distanceKm)` và clamp tối đa `120 min` | Dùng bảng cấu hình rõ ràng, không hard-code rải trong overlay. |
| CD-3 | Tính thời điểm hết cooldown | `readyAt = publishAt + cooldownMs` | Timer không bị ảnh hưởng bởi số lần overlay refresh. |
| CD-4 | Hiển thị trên overlay | `remaining = max(readyAt - now, 0)` dạng `HH:MM:SS` hoặc `MM:SS` | Hiển thị khoảng cách và marker `estimated` để user hiểu đây là safety timer. |
| CD-5 | Refresh và kết thúc | refresh khoảng 1 giây; `remaining == 0` → “Ready” | Không giữ timer cũ sau khi hết hạn. |
| CD-6 | Edge cases | chưa có vị trí trước / teleport lỗi → không tạo timer mới; target trùng → `0ms`/Ready | Không hiển thị số giả nếu chưa tính được distance hoặc publish thất bại. |
| CD-7 | Overlay lifecycle | `readyAt`/distance được lưu cùng last location để timer không bị mất khi overlay restart | Countdown là read-only presentation, không tự chặn game action. |

### Điều chỉnh kiến trúc

Nguồn dữ liệu phải nằm ở `JoystickLocationController.teleport`, vì đây là nơi biết cả điểm cũ, target và thời điểm publish mock location. Controller phát `state.teleportCooldown`; `JoystickOverlayService` render state đó mỗi giây. Không cần đưa state qua headless structured runtime hoặc `NearbySnapshot`.

### Rủi ro và giả định mới

- Bảng cooldown cộng đồng không phải contract public chính thức của Niantic và các nguồn không hoàn toàn đồng nhất; cần coi đây là safety estimate. Nếu user muốn một chart cụ thể, chart đó phải được chốt thành fixture/test.
- Cooldown server thực tế còn phụ thuộc action trước đó và timestamp action, thông tin app hiện chưa biết. Vì vậy implementation không được tự động gửi action hoặc hứa “an toàn tuyệt đối”; chỉ hiển thị timer cảnh báo.

---

## Section 12 — Hai mode: current position và last active

Người dùng yêu cầu triển khai cả hai cách tính cooldown trên cùng overlay.

### Định nghĩa mode

- `CURRENT_POSITION`: khi teleport từ `previousPoint` tới `destination` thành công, tính `distance(previousPoint, destination)` và bắt đầu candidate từ thời điểm publish thành công. Joystick movement không reset candidate này.
- `LAST_ACTIVE`: lưu vị trí và thời điểm action game gần nhất có khả năng đánh dấu location. Các action hiện được nối vào nguồn này là `Spin`, `Catch` và `UseBerry`; phase `STARTED`/`COMPLETED`/`INDETERMINATE` chỉ ghi một lần cho mỗi command. Khi render, tính `distance(lastActivePoint, currentPosition)` nhưng giữ `readyAt = lastActiveAt + cooldown(distance)`. Teleport không reset `lastActiveAt`.
- Nếu chưa có action last active hoặc chưa biết current position, mode `LAST_ACTIVE` hiển thị inactive/waiting thay vì dựng số giả. Nếu timer đã hết, hiển thị `Ready`.
- Overlay có nút chuyển qua lại giữa hai mode và lưu lựa chọn trong `SharedPreferences`.

### Acceptance criteria bổ sung

| ID | Rule / Requirement | Formula / Expected | Acceptance note |
|----|--------------------|--------------------|-----------------|
| CD-8 | Chọn mode current position | `readyAt = teleportPublishAt + table(distance(previousPoint, destination))` | Candidate mới chỉ được tạo sau publish thành công. |
| CD-9 | Ghi nhận last active | action `spin`/`catch`/`berry` có phase đã có khả năng chạy → lưu `point`, `action`, `activeAtEpochMs` | Dedupe theo `commandId`; state dùng chung giữa headless và overlay. |
| CD-10 | Tính mode last active | `readyAt = lastActiveAt + table(distance(lastActivePoint, currentPosition))` | Teleport không làm mới timer; vị trí hiện tại chỉ thay đổi khoảng cách cần chờ. |
| CD-11 | Hiển thị mode | overlay cho phép toggle và giữ mode sau restart | Label phải nói rõ `Current position` hoặc `Last active`. |
| CD-12 | Thiếu dữ liệu | thiếu last active/current position → inactive hoặc waiting | Không suy diễn action từ teleport. |
| CD-13 | Không chặn game | cooldown chỉ là read-only estimate | Automation/action flow hiện tại không bị chặn tự động. |

### Điều chỉnh kiến trúc

`StructuredAutomationController` giữ player position mới nhất từ observation và phát callback khi action game có khả năng đã chạy. `LastActiveLocationRepository` lưu state nhỏ bằng `SharedPreferences`; `JoystickOverlayService` đọc repository mỗi giây và gọi `TeleportCooldownService.forLastActive`. Candidate current-position vẫn nằm trong `JoystickLocationController` và được overlay persist như trước.

### Giới hạn cần biết

Nguồn action hiện tại là structured automation bridge. Nếu sau này repo có đường thao tác thủ công ngoài runner, đường đó cũng phải gọi `LastActiveLocationRepository.record` thì mode `LAST_ACTIVE` mới biết action đó. Bảng thời gian vẫn là community estimate, không phải xác nhận cooldown server-side.
