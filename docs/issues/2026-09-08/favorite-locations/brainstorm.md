# Brainstorm: Lưu và thao tác với danh sách favorite location

**Type:** feature / architecture / ux
**Date:** 2026-09-08

---

## Analysis

### 1. Tính năng này giải quyết vấn đề gì?

Người dùng muốn lưu các tọa độ thường dùng thành một danh sách `favorite location`, sau đó mở danh sách để xem nhanh và chọn thao tác thay vì phải nhập latitude/longitude lại trong dialog `Change location`.

Phạm vi người dùng mô tả cho phiên bản đầu là:

- có danh sách địa điểm đã lưu;
- mỗi item có tên, tọa độ, khoảng cách tới vị trí hiện tại và cooldown;
- click item mở đúng hai lựa chọn `Teleport` và `Walk`.

Để danh sách có giá trị sử dụng, tính năng cũng cần một đường tối thiểu để tạo favorite (lưu vị trí hiện tại hoặc nhập tọa độ) và xóa item đã lưu. Tuy nhiên không cần làm map picker, tìm kiếm địa chỉ, đồng bộ cloud hay chỉnh sửa route trong phiên bản đầu.

Có một điểm cần định nghĩa rõ: `cooldown` trong code hiện tại là safety estimate, không phải trạng thái cooldown server-side chính thức của Pokémon GO. Với từng favorite, cách hiển thị dễ hiểu nhất trong v1 là **cooldown ước tính nếu teleport từ vị trí hiện tại tới điểm đó ngay bây giờ**, được suy ra từ khoảng cách. Nếu người dùng muốn “thời gian cooldown còn lại của lần action gần nhất” thì đó là một semantics khác và cần dùng thêm `LastActiveLocationRepository`.

### 2. Ai được hưởng lợi?

Đối tượng là người vận hành PoGo Root Automation trên thiết bị/emulator root, đã dùng built-in joystick/mock-location provider và thường quay lại một số địa điểm cố định.

Hiện tại người dùng phải:

- mở shortcut `Teleport`;
- nhập thủ công `latitude, longitude`;
- tự nhớ tên địa điểm và tự ước lượng khoảng cách/cooldown.

Favorite list giảm thao tác nhập lại và cung cấp một màn hình kiểm tra trước khi đổi vị trí. Đây là tính năng app-local cho một user/device; chưa có nhu cầu multi-account, phân quyền hay server backend.

### 3. Các use case cốt lõi

#### Must-have

1. Mở dialog `Favorite Locations` từ shortcut overlay hoặc từ `MainActivity`.
2. Xem toàn bộ favorite đã lưu; mỗi item có tên, latitude/longitude, distance tới vị trí hiện tại và cooldown estimate.
3. Tạo favorite bằng ít nhất một trong hai cách:
   - lưu vị trí hiện tại với tên do người dùng nhập;
   - nhập một cặp tọa độ hợp lệ với tên.
4. Click một item → mở action sheet/dialog có đúng hai lựa chọn `Teleport` và `Walk`.
5. `Teleport` gửi target về location service hiện có và đi qua `JoystickLocationController.teleport`.
6. `Walk` gửi target về cùng location service và đi qua `JoystickLocationController.walkTo`.
7. Cho phép xóa favorite để quản lý danh sách; thao tác xóa cần xác nhận nếu dùng gesture dễ bấm nhầm.
8. Refresh distance/cooldown khi dialog mở và định kỳ khoảng một giây trong lúc dialog đang hiển thị.

#### Nice-to-have, chưa đưa vào v1

- sắp xếp theo tên, khoảng cách hoặc lần dùng gần nhất;
- đánh dấu favorite đang được dùng / target đang walk;
- sửa tên và tọa độ inline;
- import/export JSON;
- map preview hoặc mở tọa độ trong ứng dụng bản đồ;
- nhóm theo khu vực;
- đồng bộ giữa thiết bị.

### 4. Các edge case

- **Danh sách rỗng:** hiển thị empty state có hướng dẫn `Add favorite`; không hiển thị màn hình trắng.
- **Tên rỗng:** reject khi lưu; trim khoảng trắng. Đề xuất giới hạn 1–80 ký tự để row không phá layout.
- **Tên trùng:** cho phép vì tên chỉ là label; mỗi record phải có `id` riêng. Có thể cảnh báo tùy chọn nhưng không được dùng tên làm khóa.
- **Tọa độ không hợp lệ:** reject latitude ngoài `[-90, 90]`, longitude ngoài `[-180, 180]`, giá trị `NaN`/vô cực hoặc input không parse được.
- **Antimeridian:** dùng `GeoMath.distanceMeters`; không tự trừ longitude theo số học đơn giản.
- **Chưa có current location:** vẫn render tên/tọa độ; distance và cooldown hiển thị `—`; `Walk` phải disabled hoặc báo không thể walk nếu controller chưa có vị trí bắt đầu. `Teleport` chỉ khả dụng khi provider đã sẵn sàng.
- **Provider chưa sẵn sàng / overlay chưa chạy:** không coi việc gửi Intent là teleport thành công. UI phải báo cần start built-in joystick/cho phép overlay.
- **Teleport hoặc walk khi đang walk:** `JoystickLocationController` hiện dừng target walk khi teleport hoặc joystick được điều khiển. Chính sách v1 nên giữ nguyên: thao tác mới thay thế hành trình cũ và UI báo rõ.
- **Joystick đang được kéo trong lúc thao tác:** mọi lệnh location phải vẫn qua single writer của `JoystickLocationController`; không để dialog tự gọi `sink.publish`.
- **Click liên tiếp:** command mới nhất được xử lý theo thứ tự; không tạo nhiều scheduled task. Có thể debounce click trong UI, nhưng correctness phải nằm ở service/controller.
- **Cooldown đang chạy:** hiển thị remaining/estimate nhưng không tự giả định server đã sẵn sàng. Với v1, cooldown là read-only cảnh báo; không tự chặn action trừ khi sau này user yêu cầu policy chặn.
- **Cooldown vượt thời gian / đổi system clock:** tính bằng `max(readyAt - now, 0)`; không hiển thị số âm.
- **Service restart:** favorite vẫn còn vì được lưu bền; target đang walk không tự resume nếu chưa có policy riêng.
- **Xóa item đang được chọn:** chỉ xóa dữ liệu favorite; không tự dừng một hành trình đang dùng cùng tọa độ trừ khi target identity được liên kết rõ ràng.
- **Dữ liệu lưu hỏng sau update:** bỏ qua record lỗi và giữ các record hợp lệ; không crash dialog/service. Nếu toàn bộ payload hỏng, reset về empty state và có thể ghi log.
- **Dialog bị dismiss/background:** dừng timer refresh khi dialog không còn hiển thị; dữ liệu được đọc lại khi mở lần sau.
- **Orientation/configuration change:** danh sách và dialog không làm mất dữ liệu đã lưu; draft của form thêm favorite phải được xử lý rõ nếu dialog bị recreate.
- **Nhiều favorite cùng tọa độ:** distance/cooldown giống nhau nhưng tên và `id` khác; không tự gộp trong v1.

### 5. Các ràng buộc

#### Technical

- `core` đã có `GeoPoint`, `GeoMath.distanceMeters` và `TeleportCooldownService`; phần tính distance/cooldown nên dùng lại, không nhân bản trong UI.
- `JoystickLocationController` đã có `teleport`, `walkTo`, `stopWalking`, state `providerReady` và state walk. Đây là đường thực thi có sẵn cho hai action.
- `JoystickOverlayService` đang sở hữu controller và là single writer của mock location. Dialog favorite không được khởi tạo location sink riêng.
- Vị trí hiện tại được controller persist qua `OverlayPositionStore`; dialog có thể đọc snapshot gần nhất từ store để render. Đây là dữ liệu có thể trễ tối đa khoảng một giây, nên UI nên coi là `last known position`.
- Các repository hiện dùng `SharedPreferences` cho payload nhỏ. Danh sách favorite có thể lưu dưới một JSON array trong namespace riêng, với schema version và id ổn định. Chưa cần thêm Room/SQLite cho số lượng nhỏ.
- App chưa có UI framework/dependency map; điều này không ảnh hưởng vì yêu cầu hiện tại chỉ là list tọa độ.
- Callback của dialog chỉ là thao tác UI. `JoystickOverlayService` phải kiểm tra `providerReady`, target hợp lệ và trạng thái controller trước khi gọi action; phản hồi thành công/thất bại cần được thể hiện bằng Toast hoặc state có thể đọc lại.
- Không cần thay đổi `bridge/protocol`, `game-adapter` hoặc Zygisk runtime cho v1. `Teleport`/`Walk` ở đây là app-side mock-location movement.

#### Product/UX

- Vì list được mở từ float menu, v1 có thể dùng `AlertDialog` overlay với `ListView` cuộn được; không cần rời Pokémon GO. Nếu số lượng favorite tăng lớn, có thể chuyển sang `Activity` ở phase sau.
- Row nên ưu tiên thông tin theo thứ tự: tên → tọa độ → distance → cooldown. Giá trị unavailable phải có label rõ ràng, không dùng `0` gây hiểu nhầm.
- `Teleport` và `Walk` phải là action rõ ràng trong dialog; không chạy ngay khi click row nếu chưa có bước xác nhận/action selection.
- Cooldown nên ghi `estimate`/`ước tính`. Không gọi đó là cooldown server chính thức.
- Khoảng cách nên hiển thị m hoặc km tùy ngưỡng, nhưng quy tắc format phải thống nhất và test được.
- Tính năng không tự động trigger encounter/catch/spin sau khi tới nơi.

### 6. Các phương án đã cân nhắc

#### Phương án A — Dialog overlay + repository SharedPreferences + controller hiện có (khuyến nghị)

Tạo `FavoriteLocation`/`FavoriteLocationRepository`, một dialog list và thêm shortcut `Favorites`. Dialog chỉ đọc/hiển thị; `JoystickOverlayService` thực thi action bằng controller hiện có.

Ưu điểm: đúng lifecycle overlay hiện tại, tách persistence khỏi executor, tận dụng được `teleport`/`walkTo`, không cần dependency mới/manifest entry và dễ test parser/repository/domain math. Nhược điểm: dialog có diện tích hữu hạn và current location hiển thị là last persisted point thay vì snapshot trực tiếp.

#### Phương án B — Nhét list vào một overlay panel

Mở một `WindowManager` overlay lớn chứa danh sách và action.

Ưu điểm: không rời Pokémon GO. Nhược điểm: dễ che game, phức tạp touch/focus/keyboard, tranh chấp với joystick và dialog hiện có. Không phù hợp với yêu cầu list có nhiều trường.

#### Phương án C — Lưu từng favorite thành nhiều key trong `OverlayPositionStore`

Mở rộng store hiện tại để có các key indexed kiểu `favorite_0_*`.

Ưu điểm: ít file mới. Nhược điểm: khó xóa/reorder/migrate, dễ tạo record rỗng khi update một phần và làm store vị trí/cooldown phình sai trách nhiệm. Không khuyến nghị.

#### Phương án D — Room/SQLite

Tạo database cho favorite locations.

Ưu điểm: phù hợp khi có search, nhóm, lịch sử, sync hoặc hàng nghìn record. Nhược điểm: thêm dependency/schema/migration không cần thiết cho một list nhỏ app-local. Chỉ nên nâng cấp khi yêu cầu mở rộng.

### 7. Tính năng này tương tác với các feature hiện có như thế nào?

#### Luồng đọc

Dialog favorite đọc `FavoriteLocationRepository`, đọc current point từ `OverlayPositionStore`, rồi dùng `GeoMath.distanceMeters` và `TeleportCooldownService` để tạo view model từng row. Không lưu distance/cooldown đã tính vì cả hai đều thay đổi theo thời gian.

#### Luồng action

Click row trong dialog → action dialog `Teleport`/`Walk` → `JoystickOverlayService` → kiểm tra `providerReady`/current point → `JoystickLocationController.teleport` hoặc `walkTo` → state callback/persist current point/cooldown.

Điều này bảo đảm joystick, teleport thủ công và favorite action dùng chung một writer. `teleport` đang tự hủy walk hiện tại; `walkTo` sẽ thay target cũ; hành vi này cần được document trong UI.

#### Shortcut/navigation

Thêm icon item `Favorites` vào `ShortcutMenuView`; callback gọi `showFavoriteLocationsDialog()` trong `JoystickOverlayService`. Có thể thêm nút ở `MainActivity` nếu muốn mở list khi overlay chưa chạy, nhưng action `Teleport`/`Walk` vẫn cần provider/overlay sẵn sàng.

#### Cooldown

Không sửa semantics cooldown hiện tại. Per-row forecast dùng khoảng cách hiện tại → favorite và `cooldownMillisForDistance`; current countdown của lần teleport/action gần nhất vẫn là state riêng. Nếu hiển thị remaining theo `LAST_ACTIVE`, cần chốt UX vì đó là “cooldown tính từ action gần nhất tới target”, không phải cooldown cố định của favorite.

#### Không ảnh hưởng

Không cần thay đổi automation planner/runner, nearby reducer, bridge protocol, game adapter, runtime binding hay Zygisk. Favorite location không tự biến thành automation target.

### 8. Các dependencies

- **Domain model:** `FavoriteLocation(id, name, point, createdAtEpochMs)`; chỉ `id`, `name`, `point` là bắt buộc cho v1.
- **Persistence:** `FavoriteLocationRepository` app-local, payload JSON có `schemaVersion`, validate khi đọc, atomic replace khi ghi.
- **UI:** dialog favorite + `ListView`/adapter; empty state; dialog thêm favorite; action dialog `Teleport`/`Walk`; delete affordance.
- **Navigation:** icon shortcut mới trong `ShortcutMenuView`; không cần manifest declaration mới cho v1.
- **Command boundary:** callback của dialog gọi các method trên chính `JoystickOverlayService`/controller. Nếu Activity được thêm sau này thì Intent vẫn phải quay về service; command không được bypass controller.
- **Dynamic display:** current point từ `OverlayPositionStore`, periodic refresh khoảng 1 giây, formatter cho tọa độ/distance/cooldown.
- **Tests:** repository CRUD/validation/migration, distance formatting, cooldown forecast, action routing, provider-not-ready, re-target và regression của joystick/teleport.

Không có third-party dependency bắt buộc. Không cần API endpoint, map tile hay binding version-specific.

### 9. Các rủi ro

- **Cooldown bị hiểu sai:** user có thể nghĩ số hiển thị là xác nhận server. Giảm thiểu bằng label `estimated`, tooltip/message và không dùng nó làm bằng chứng an toàn tuyệt đối.
- **Race giữa dialog và location tick:** dialog có thể hiển thị action thành công trước khi provider publish xong. Giảm thiểu bằng service là nơi duy nhất thực thi, state callback rõ ràng và Toast sau khi nhận kết quả.
- **Current location stale:** distance có thể lệch khi user đang joystick. Giảm thiểu bằng refresh 1 giây, ghi `last known`, và tính lại ngay trước action trong service nếu cần.
- **Persistence payload lỗi/migration:** JSON thủ công dễ hỏng nếu schema thay đổi. Giảm thiểu bằng schema version, `runCatching`, record-level validation và test migration.
- **Action khi service chưa chạy:** start service không đồng nghĩa provider đã ready. UI phải hiển thị unavailable/retry, không âm thầm bỏ command.
- **UX mở rộng quá scope:** map, search địa chỉ, routing hoặc sync sẽ làm tăng chi phí không cần thiết. Giữ v1 là danh sách tọa độ app-local.
- **Account/game safety:** mock location/teleport/walk có thể vi phạm điều khoản Pokémon GO. Tính năng không nên bổ sung anti-detection hoặc bypass khác ngoài phạm vi repo.

### 10. Acceptance criteria

Các điều kiện cụ thể được chuẩn hóa ở bảng bên dưới. Tính năng v1 được coi là hoàn thành khi các điều kiện đó pass và các câu hỏi về cooldown/action availability đã được chốt.

## Acceptance Criteria (from spec)

> Source: ticket/requirement người dùng — “lưu được favorite location”, “xem list”, “xem được tọa độ, tên, cooldown, distance”, click item có `teleport` và `walk`. Không tìm thấy spec riêng trong `docs/newspec/**` hoặc `docs/specs/**` → các tiêu chí dưới đây là **inferred — needs BA confirm**.

| ID | Rule / Requirement | Formula / Expected | Acceptance note |
|----|--------------------|--------------------|-----------------|
| AC-1 | Lưu favorite bền vững | App restart → vẫn đọc được record có `id`, `name`, `latitude`, `longitude` | Đáp ứng yêu cầu “lưu favorite location”; payload lỗi không làm crash. |
| AC-2 | Validate favorite | `latitude ∈ [-90, 90]`, `longitude ∈ [-180, 180]`, cả hai finite, `name.trim()` không rỗng | Input invalid không được ghi. |
| AC-3 | Hiển thị danh sách | Mỗi record hợp lệ render đúng một row | Empty list có empty state và hướng dẫn thêm item. |
| AC-4 | Hiển thị dữ liệu cơ bản | Row hiển thị `name`, `latitude`, `longitude` với format ổn định | Đáp ứng trực tiếp yêu cầu xem tên và tọa độ; không làm tròn mất khả năng nhận diện điểm. |
| AC-5 | Tính distance động | `distanceMeters = GeoMath.distanceMeters(currentPoint, favorite.point)` | Refresh khi current point thay đổi; thiếu current point → `—`, không hiển thị `0`. |
| AC-6 | Hiển thị cooldown | `estimatedCooldown = TeleportCooldownService.cooldownMillisForDistance(distanceMeters / 1000)` khi có current point | Gắn nhãn `estimate`; distance zero/không có current point có trạng thái rõ ràng. |
| AC-7 | Chọn action | Click row mở action dialog có `Teleport` và `Walk`, chưa tự thực thi khi chỉ click row | Hai action phải target đúng favorite được chọn. |
| AC-8 | Teleport routing | Chọn `Teleport` → callback vào `JoystickOverlayService` → `JoystickLocationController.teleport(favorite.point)` | Không để dialog gọi trực tiếp `sink.publish`; provider chưa ready → báo lỗi/unavailable. |
| AC-9 | Walk routing | Chọn `Walk` → command vào `JoystickOverlayService` → `JoystickLocationController.walkTo(favorite.point)` | Thiếu current point/provider → không start walk; có target mới → target cũ bị thay thế. |
| AC-10 | Single writer / regression | Joystick, manual teleport và favorite action không tạo concurrent location writer | Không có hai scheduled task cùng publish; teleport/joystick đang có phải giữ behavior hiện tại. |
| AC-11 | Quản lý tối thiểu | User có thể xóa một favorite; sau khi xác nhận, item biến mất và không xuất hiện sau restart | Edit/reorder không bắt buộc cho v1. |
| AC-12 | Lifecycle và refresh | Dialog refresh khi mở và khoảng mỗi 1 giây khi đang hiển thị; timer dừng khi dismiss/service destroy | Distance/cooldown không bị đóng băng trên màn hình. |
| AC-13 | Fail-closed | Parse error, record lỗi, provider error hoặc command thất bại → thông báo trạng thái; không tuyên bố teleport/walk thành công | Giữ nguyên fail-closed behavior của project. |
| AC-14 | Verification | Unit tests cho repository/validation/format/cooldown/action routing; `git diff --check`; Gradle test/build relevant pass | Device smoke test kiểm tra overlay, service, mock location và action dialog. |

Mapping requirement → acceptance:

- “Lưu được favorite location” → AC-1, AC-2, AC-11.
- “Chỉ cần xem list” → AC-3, AC-4, AC-5, AC-6.
- “Xem được tọa độ, tên, cooldown, distance” → AC-4, AC-5, AC-6.
- “Click vào có option teleport và walk” → AC-7, AC-8, AC-9.
- Không làm hỏng location control hiện tại → AC-10, AC-13, AC-14.

---

## Synthesis

### Key Insight

Làm được với kiến trúc hiện tại và không cần đụng tới Pokémon GO runtime: favorite chỉ là dữ liệu app-local, còn Teleport/Walk tái sử dụng controller đang có. Điểm quan trọng nhất là không để dialog tự publish location; mọi action phải được service xử lý để joystick, teleport thủ công và favorite dùng chung một single writer. `distance` và cooldown không nên lưu cố định trong favorite vì phải tính lại theo vị trí hiện tại và thời gian hiện tại.

### Recommended Approach

Chọn Phương án A: `FavoriteLocationRepository` dùng `SharedPreferences` + dialog `ListView` mở từ icon `Favorites` trong float menu. V1 làm CRUD tối thiểu (thêm/xóa), list hiển thị name/coordinates/distance/cooldown estimate, click row mở `Teleport`/`Walk`, rồi gọi controller từ overlay service. Cooldown trong row nên được ghi rõ là “ước tính nếu teleport ngay”, còn cooldown hiện tại của hệ thống vẫn là safety estimate read-only. Chưa cần map, Room, cloud sync hoặc bridge/Zygisk change.

### Risks to Watch

- Người dùng hiểu cooldown estimate là cooldown server chính thức.
- Dialog gọi action trước khi mock-location provider ready hoặc render vị trí đã stale.
- JSON persistence và action routing tạo lỗi im lặng sau restart hoặc race với joystick/teleport.

### Open Questions

- **Nguồn tạo favorite trong v1:** chỉ `Save current location`, hay cần cả nhập tọa độ thủ công? Khuyến nghị hỗ trợ cả hai vì dialog tọa độ đã tồn tại.
- **Định nghĩa cooldown hiển thị trong row:** cooldown estimate nếu teleport ngay (khuyến nghị), hay remaining theo last active action, hay cần hiển thị cả hai?
- Khi provider/overlay chưa chạy, nút `Teleport`/`Walk` nên disabled, hay bấm vào sẽ hướng dẫn start joystick rồi retry?
- Giới hạn số favorite và giới hạn độ dài tên là bao nhiêu? Đề xuất không quá 100–200 item và tên tối đa 80 ký tự cho v1.
- Có cần sửa tên/tọa độ không? `Add` và xóa đã được chốt cho v1; edit để phase sau nếu người dùng xác nhận.
- Khi thao tác favorite trong lúc đang joystick/walk, có cần dialog xác nhận thay thế hành trình hiện tại không, hay giữ chính sách hiện tại “lệnh mới nhất thắng”?

## Section 11 — Chốt UI: mở danh sách bằng dialog từ float menu

Người dùng xác nhận muốn thêm một icon vào float/shortcut menu; khi bấm icon đó thì hiển thị danh sách favorite trong dialog. Quyết định này thay thế hướng `FavoriteLocationsActivity` toàn màn hình cho v1.

### 11.1. Luồng UI đề xuất

1. `ShortcutMenuView` có thêm shortcut `Favorites` (biểu tượng `★` hoặc `▣`).
2. Callback của shortcut gọi `showFavoriteLocationsDialog()` trong `JoystickOverlayService`, tương tự cách service đang gọi `showTeleportDialog()`.
3. Dialog dùng title `Favorite Locations`, một `ListView` có adapter và vùng empty state. Mỗi row hiển thị tên, tọa độ, distance và cooldown estimate.
4. Bấm một row không teleport ngay; mở action dialog nhỏ gồm `Teleport`, `Walk` và `Cancel`.
5. Bấm `Teleport`/`Walk` gọi controller từ service hiện tại, nên không cần Intent trung gian và không tạo thêm location writer.
6. Dialog list có nút `Add` để lưu current location/nhập tọa độ và affordance xóa item. Nút `Add` là một phần của v1, không còn là tùy chọn phase sau.

### 11.2. Ràng buộc kỹ thuật của dialog overlay

- Dùng `ContextThemeWrapper` và `AlertDialog.Builder` như `showTeleportDialog()`; đặt window type bằng `overlayWindowType()` để dialog hiển thị trên ứng dụng game.
- `ListView` phải có chiều cao tối đa theo màn hình và scroll khi có nhiều item; không tạo một dialog cao hơn viewport.
- Service cần giữ reference tới dialog đang mở và dismiss khi `onDestroy()` để tránh window leak.
- Tận dụng `cooldownTick` hiện có để gọi adapter refresh khoảng mỗi giây; khi dialog đã dismiss thì không refresh view đã bị detach.
- Khi click action, tính lại current point/provider readiness ngay trong callback trước khi gọi controller. Giá trị hiển thị trong row có thể đã trễ một tick.
- Vì dialog nhận focus/touch, chỉ hiển thị khi shortcut menu đã đóng hoặc giữ menu ở trạng thái không tương tác để tránh click xuyên.

### 11.3. Trade-off và giới hạn

Dialog phù hợp với yêu cầu “chỉ xem list” và giúp thao tác nhanh mà không rời Pokémon GO. Nút `Add` nên đặt ở toolbar/header của dialog để luôn nhìn thấy, còn danh sách vẫn cuộn độc lập. Đổi lại, không nên nhồi thêm map, filter phức tạp hoặc hàng trăm row vào dialog; nếu favorite phát triển thành một màn hình quản lý lớn thì chuyển sang `FavoriteLocationsActivity` là bước nâng cấp hợp lý.

### 11.4. Acceptance bổ sung cho UI dialog

| ID | Rule / Requirement | Formula / Expected | Acceptance note |
|----|--------------------|--------------------|-----------------|
| AC-15 | Mở từ float menu | Bấm icon `Favorites` → một dialog list xuất hiện trên overlay | Không cần mở Activity riêng trong v1. |
| AC-16 | List có thể cuộn | Số row vượt chiều cao dialog → `ListView` scroll được | Không cắt mất row cuối, không che toàn bộ game ngoài vùng dialog. |
| AC-17 | Click không chạy nhầm action | Bấm row → action dialog; chỉ bấm `Teleport` hoặc `Walk` mới gọi controller | `Cancel`/dismiss không đổi location. |
| AC-18 | Dialog lifecycle | Service destroy/dismiss → dialog được đóng và refresh callback không truy cập view cũ | Không window leak/crash do callback sau dismiss. |
| AC-19 | Có nút thêm favorite | Bấm `Add` trong dialog list → form thêm favorite xuất hiện | Nút luôn khả dụng kể cả khi danh sách đang rỗng. |
| AC-20 | Lưu từ form Add | Nhập tên + tọa độ hợp lệ → record được lưu và xuất hiện ngay trong list | Có thể prefill current location; input invalid/blank name không được lưu. |

Các acceptance mới này bổ sung cho AC-3, AC-4, AC-5, AC-6 và AC-7; không thay đổi semantics của AC-8, AC-9 và AC-10.

### 11.5. Cập nhật quyết định

- ~~List phải mở bằng `FavoriteLocationsActivity` toàn màn hình.~~ → Resolved: v1 mở bằng dialog từ icon `Favorites` trong float menu.
- ~~Navigation cần manifest declaration cho Activity.~~ → Resolved: v1 không cần Activity/manifest entry mới cho list.
- ~~Favorite list cần có nút Add hay không.~~ → Resolved: v1 có nút `Add` ngay trong dialog list.
- Các câu hỏi về cách Add (chỉ lưu current location hay cho nhập tọa độ), cooldown semantics, provider chưa sẵn sàng, giới hạn dữ liệu và chính sách thay thế walk vẫn còn mở.

## Section 12 — Bổ sung nút Add trong dialog favorite

Người dùng xác nhận dialog danh sách phải có thêm nút `Add`. Đây là thao tác tạo favorite chính thức của v1, không chỉ là placeholder cho phase sau.

### 12.1. Luồng Add đề xuất

1. Dialog `Favorite Locations` có nút `Add` ở header/toolbar hoặc nút nổi bật phía cuối phần header, không đặt lẫn vào một row favorite.
2. Bấm `Add` mở form nhỏ gồm `Name` và `Latitude, longitude`.
3. Nếu controller có current point, prefill tọa độ hiện tại; user vẫn được sửa để lưu một điểm khác.
4. Bấm `Save` validate name/tọa độ theo AC-2, tạo `FavoriteLocation` với `id` mới và ghi qua `FavoriteLocationRepository`.
5. Save thành công → đóng form, reload adapter và hiển thị item mới ngay; save thất bại → giữ form và hiển thị lỗi tại field phù hợp.
6. Hủy/dismiss form → không thay đổi danh sách.

### 12.2. Quyết định phạm vi

- V1 hỗ trợ cả `Save current location` thông qua tọa độ prefill và nhập tọa độ thủ công trong cùng form; không cần thêm một màn hình riêng.
- Tên là label, không dùng làm unique key; tên trùng vẫn được phép.
- `Add` không tự teleport/walk sau khi lưu. Người dùng phải click row rồi chọn action rõ ràng.
- Sau khi thêm item, distance/cooldown được tính lại từ current point hiện tại; không lưu các giá trị đã tính vào record.

### 12.3. Acceptance bổ sung

AC-19 và AC-20 bổ sung cho AC-1, AC-2 và AC-3; chúng xác nhận nút `Add` hoạt động cả ở empty state và không tạo action location ngoài ý muốn.

## Section 13 — Bổ sung icon xóa trên từng favorite

Người dùng xác nhận mỗi row cần có thêm icon xóa. Icon nên nằm ở mép phải row, có `contentDescription` dạng `Delete {name}`, và là control độc lập với vùng click mở action `Teleport`/`Walk`.

### 13.1. Luồng xóa đề xuất

1. Bấm icon xóa của một row → không mở action dialog và không đổi location.
2. Hiển thị confirm dialog, ví dụ `Delete “{name}”?`, với `Cancel` và `Delete`.
3. Nếu xác nhận, gọi `FavoriteLocationRepository.delete(id)` theo `id` ổn định, không xóa theo tên hoặc theo index của list.
4. Xóa thành công → đóng confirm dialog, reload adapter, item biến mất ngay; nếu không còn item thì hiển thị empty state.
5. Hủy hoặc dismiss confirm dialog → giữ nguyên favorite và vị trí hiện tại.

### 13.2. Ràng buộc và edge case

- Tách listener của icon xóa khỏi listener của row để bấm xóa không đồng thời mở `Teleport`/`Walk`.
- Hai lần bấm nhanh hoặc hai dialog xóa cùng mở phải chỉ xóa record đúng `id`; thao tác xóa record đã biến mất được coi là no-op an toàn.
- Xóa favorite không tự dừng walk đang chạy tới cùng tọa độ trong v1; walk state thuộc controller, còn favorite chỉ là dữ liệu lưu. Nếu cần dừng, user dùng control `Stop` riêng.
- Xóa một item không làm thay đổi cooldown global, current location hoặc các favorite khác.

### 13.3. Acceptance bổ sung

| ID | Rule / Requirement | Formula / Expected | Acceptance note |
|----|--------------------|--------------------|-----------------|
| AC-21 | Có icon xóa | Mỗi row hợp lệ có control xóa riêng, có accessibility label | Icon không bị nhầm với vùng click chọn action. |
| AC-22 | Xóa có xác nhận | Bấm icon → confirm; chỉ `Delete` mới gọi repository delete | `Cancel`/dismiss không thay đổi dữ liệu. |
| AC-23 | Xóa đúng record và refresh | `delete(id)` → row biến mất ngay; restart không đọc lại item | Tên trùng vẫn xóa đúng item được chọn; empty state xuất hiện khi list rỗng. |

AC-21–AC-23 bổ sung cho AC-3, AC-7 và AC-11; không thay đổi hành vi Teleport/Walk hoặc cooldown.

## Section 14 — Tách UI khỏi `JoystickOverlayService`

Người dùng yêu cầu không để toàn bộ UI tập trung trong `app/src/main/java/dev/pogoroot/automation/overlay/JoystickOverlayService.kt`. Refactor đã được thực hiện theo hướng service giữ lifecycle, state và location orchestration; mỗi nhóm view/dialog có file riêng.

### 14.1. Phân chia file

- `MainOverlayView.kt`: float button, shortcut grid, joystick panel, drag/layout của main overlay.
- `CooldownOverlayView.kt`: badge cooldown và drag/persist vị trí.
- `ScanResultOverlays.kt`: hai scan widget hundo/shiny và drag/layout.
- `TeleportLocationDialog.kt`: dialog nhập tọa độ teleport thủ công.
- `FavoriteLocationsDialog.kt`: list favorite, Add và điều phối child dialog.
- `FavoriteLocationActionDialog.kt`: chọn `Teleport`/`Walk`.
- `FavoriteLocationEditorDialog.kt`: form Add và validate input.
- `FavoriteLocationDeleteDialog.kt`: confirm xóa.
- `FavoriteLocationAdapter.kt`: row favorite và icon xóa.

`JoystickOverlayService.kt` còn giữ: tạo service/repository/controller, nối callback, xử lý automation toggle, tính cooldown state, persist current point và notification. File giảm từ khoảng 1.009 dòng sau khi thêm feature xuống khoảng 425 dòng sau refactor.

### 14.2. Acceptance bổ sung

| ID | Rule / Requirement | Formula / Expected | Acceptance note |
|----|--------------------|--------------------|-----------------|
| AC-24 | Tách trách nhiệm UI | Mỗi nhóm view/dialog của overlay nằm ở file/class riêng; service không trực tiếp dựng favorite/cooldown/scan view | Giảm độ dài service mà không đổi flow người dùng. |
| AC-25 | Giữ nguyên orchestration | UI gọi callback về service; service vẫn là nơi gọi `JoystickLocationController` | Không tạo location writer mới, không bypass controller. |
| AC-26 | Không regression sau refactor | `./gradlew test assembleDebug` và `git diff --check` pass | Device smoke test vẫn cần kiểm tra overlay/dialog thực tế nếu có thiết bị. |

### 14.3. Cập nhật quyết định

- ~~Tất cả UI có thể để trong `JoystickOverlayService.kt`.~~ → Resolved: tách theo từng nhóm view/dialog và adapter.
- Service vẫn là lifecycle/orchestration owner; không chuyển state machine location sang view class.

### 13.4. Cập nhật quyết định

- ~~Favorite chỉ có Add, chưa cần delete control.~~ → Resolved: mỗi row có icon xóa và confirm dialog trong v1.
- Câu hỏi “có cần sửa tên/tọa độ không” vẫn còn mở; edit chưa thuộc scope nếu người dùng chưa yêu cầu.
