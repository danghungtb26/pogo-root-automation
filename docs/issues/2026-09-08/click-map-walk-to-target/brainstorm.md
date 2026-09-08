# Brainstorm: Click map để walk tới điểm trong Pokémon GO

**Type:** feature / architecture / ux
**Date:** 2026-09-08

---

## Analysis

### 1. Tính năng này giải quyết vấn đề gì?

Người vận hành muốn chọn một điểm đến bằng thao tác click/tap trên bản đồ rồi để nhân vật di chuyển tới đó theo kiểu walk, thay vì phải kéo joystick thủ công hoặc nhập latitude/longitude vào dialog teleport.

Có hai ý nghĩa kỹ thuật khác nhau cần tách ra:

1. **Chọn điểm trên bản đồ do controller sở hữu** (map trong app/overlay hoặc marker nearby đã có tọa độ), sau đó controller mô phỏng walk tới `GeoPoint`. Đây là hướng phù hợp với kiến trúc hiện tại và có thể làm được.
2. **Tap trực tiếp lên bản đồ 3D bên trong Pokémon GO**, đọc pixel tap rồi suy ra tọa độ địa lý của điểm đó. Đây là một bài toán khác: cần biết camera center/zoom/bearing, phép chiếu của client Unity, trạng thái map hiện tại và một input path được client chấp nhận. Repo hiện chưa có các dữ liệu/binding này.

Trong tài liệu này, “walk” được hiểu là cập nhật mock location liên tục theo thời gian tới target, không phải teleport một lần và cũng không phải tìm đường theo đường phố thực tế.

### 2. Ai được hưởng lợi?

Người vận hành PoGo Root Automation trên thiết bị/emulator root, đang dùng built-in joystick và muốn chọn vị trí nhanh hơn. Tính năng chủ yếu phục vụ thao tác thủ công có kiểm soát; không nên tự động coi mọi điểm được click là một lệnh automation game hoặc tự động mở encounter/catch.

Ở trạng thái hiện tại, user có thể:

- dùng `JoystickPadView` để điều khiển hướng và strength;
- dùng teleport dialog để nhập tọa độ;
- nhận dữ liệu nearby có `GeoPoint` trong `NearbySpawn` nhưng chưa có map picker để chọn marker;
- không có màn hình map trong controller và không có đường đọc tọa độ từ pixel của Pokémon GO.

### 3. Các use case cốt lõi

**Must-have cho phiên bản đầu:**

- Hiển thị một map picker do controller sở hữu, hoặc tối thiểu cho phép chọn một nearby marker đã có tọa độ.
- Tap một điểm hợp lệ → tạo `GeoPoint` đích và hiển thị preview khoảng cách/hướng.
- Bấm `Walk`/xác nhận → location controller di chuyển liên tục từ vị trí hiện tại tới target bằng mock location.
- Có `Stop`, `Cancel` và khả năng chọn target mới; chọn target mới phải hủy hoặc thay thế hành trình cũ rõ ràng.
- Khi tới trong bán kính tolerance, tự dừng và phát trạng thái `arrived`.
- Hiển thị rõ `provider not ready`, thiếu vị trí ban đầu, lỗi publish và trạng thái đang walk.

**Nice-to-have, chưa nên gộp vào phiên bản đầu:**

- vẽ đường đi theo road network;
- tự động dừng ở PokéStop/Gym/nearby spawn;
- route nhiều waypoint;
- click trực tiếp trên bản đồ 3D của Pokémon GO;
- tự động trigger encounter/catch sau khi tới nơi.

### 4. Các edge case

- **Chưa có vị trí hiện tại:** không thể walk theo khoảng cách hữu hạn; UI phải yêu cầu user set vị trí ban đầu hoặc dùng GPS/mock location hiện tại trước.
- **Mock provider chưa sẵn sàng:** không bắt đầu walk và không giả vờ rằng target đã được nhận.
- **Target trùng vị trí hiện tại:** trả về `arrived`/`Ready`, không phát các location update vô ích.
- **Target rất gần:** dùng tolerance thực tế, ví dụ 5–10 m, để tránh dao động qua lại do sai số tọa độ.
- **User tap liên tiếp:** target mới thay thế target cũ theo một state transition duy nhất; không tạo nhiều executor chạy song song.
- **User bấm Stop trong lúc publish:** tick tiếp theo phải publish speed bằng 0 hoặc dừng cập nhật an toàn; không để task cũ tiếp tục di chuyển.
- **Service restart:** hành trình đang chạy không nên tự resume nếu chưa có policy rõ ràng; mặc định nên stop và yêu cầu xác nhận lại.
- **Xoay màn hình / map camera thay đổi:** một tap chỉ hợp lệ khi dùng cùng camera/projection snapshot tại thời điểm tap.
- **Tap ngoài map hoặc lên control/overlay:** không tạo target; gesture của joystick và menu phải không bị nuốt.
- **Map load lỗi/offline:** vẫn cho phép chọn nearby marker hoặc nhập tọa độ nếu dữ liệu có sẵn; không hiển thị map rỗng như thể tap được.
- **Crossing ±180° longitude:** `GeoMath.distanceMeters` đã xử lý normalize longitude, nhưng map projection và renderer cũng phải xử lý nhất quán.
- **Latitude/longitude không hợp lệ:** reject trước khi đưa vào controller.
- **Game loading/disconnect/runtime binding mất:** location controller có thể vẫn publish mock location, nhưng UI phải phân biệt “đang walk theo location” với “Pokémon GO đã nhận vị trí”.
- **Walk không phải road routing:** đường thẳng địa lý có thể đi qua sông/tòa nhà; nếu user cần “đi theo đường” thì phải thêm routing dependency và scope khác.
- **Click trực tiếp trên Pokémon GO:** map có thể đang zoom/rotate/tilt, marker có thể nằm trên terrain; không thể dùng một công thức pixel→GPS cố định.

### 5. Các ràng buộc

#### Technical

- Repo đã có `GeoPoint` và `GeoMath.destination`/`distanceMeters` trong `core`.
- `JoystickLocationController` hiện publish vị trí mỗi 50 ms, tính vận tốc từ `maxSpeedKmh` và strength, rồi gọi `RootMockLocationProvider` cho GPS + network test providers. Đây là nền tảng tốt để thêm một target-following mode.
- `AutomationAction.MoveTo(target, mode)` và `MovementMode.WALK` đã tồn tại; `BridgePayloadCodec` cũng encode/decode được action này. Tuy nhiên đây mới là contract, chưa phải movement executor hoạt động.
- `PogoGameAdapter`/`BridgeBackedPogoActionExecutor` yêu cầu capability `MOVE` và build fingerprint được allowlist. Zygisk probe hiện announce zero capabilities và trả `binding_not_implemented` cho mọi command. Vì vậy không thể coi `MoveTo` qua runtime bridge là đường chạy được ở phiên bản hiện tại.
- `RootMockLocationProvider` là đường location độc lập với game runtime. Vì vậy walk controller có thể thực hiện ở tầng app/location mà không cần `MOVE` capability của POGO runtime, miễn là user đã bật built-in joystick/mock provider.
- `scripts/structured-only-source-guard.sh` cấm screen capture và input injection kiểu `screencap`/`input tap`/`input swipe`. Đây là ràng buộc kiến trúc hiện tại, không nên phá chỉ để prototype click pixel trên game.
- `app` chưa có `MapView`, `Mapbox`, `osmdroid`, Google Maps hay dependency map/tile nào. `INTERNET` đã có trong manifest nhưng không đồng nghĩa đã có nguồn tile hoặc license/API key.

#### Product/UX

- Cần gọi đúng trạng thái: `Walk to target` là mô phỏng location có kiểm soát, không phải bảo đảm nhân vật đi theo đường thật.
- Teleport cooldown hiện là safety estimate và read-only; walk liên tục không nên reset cooldown ở mỗi tick 50 ms.
- Overlay đang được thiết kế compact để không che Pokémon GO. Một map toàn màn hình có thể phù hợp trong controller Activity; một map lớn trong overlay sẽ xung đột với joystick và touch event.
- Tính năng cần fail-closed ở các trạng thái thiếu tọa độ, thiếu provider hoặc target không hợp lệ.

### 6. Các phương án đã cân nhắc

#### Phương án A — Map picker của controller + walk controller (khuyến nghị)

Controller có một map UI độc lập hoặc một map screen mở từ shortcut. Tap trên map trả về `GeoPoint`; UI gửi target cho một `WalkToController` đặt cạnh `JoystickLocationController`. Có thể bắt đầu bằng marker nearby hoặc map renderer đơn giản, sau đó mới chọn tile provider.

Ưu điểm: không cần hiểu camera nội bộ của Pokémon GO, không cần screen capture/input injection, tái sử dụng mock-location path hiện có, test được bằng unit test. Nhược điểm: user không click trực tiếp vào đúng bản đồ đang nhìn trong Pokémon GO và cần thêm map renderer/provider.

#### Phương án B — Chọn nearby marker rồi walk

Tái sử dụng `NearbySpawn.position`/`FortSnapshot.position` để user chọn một entity trong danh sách/overlay. Đây là vertical slice nhỏ nhất và gần như không cần map tile. Có thể xem là bước 1 trước khi có map đầy đủ.

Ưu điểm: đúng dữ liệu structured mà repo đã đọc được, ít phụ thuộc UI map. Nhược điểm: không chọn được một điểm tự do không có marker.

#### Phương án C — Tap trực tiếp bản đồ Pokémon GO

Cần runtime binding version-specific để đọc player/map camera state hoặc xây một phép chiếu tương thích; cần một input path để đưa tap vào client; sau đó xác nhận target bằng observation mới. Nếu map là Unity/IL2CPP, mọi offset/type/camera API có thể đổi theo build.

Ưu điểm: UX đúng với ý tưởng “click map trong PKM”. Nhược điểm: chi phí và độ giòn rất cao, phụ thuộc build, dễ sai tọa độ khi camera thay đổi, mâu thuẫn với structured-only guard hiện tại, và chưa có binding movement capability hoạt động. Không nên là scope đầu tiên.

#### Phương án D — Nhập tọa độ/teleport rồi walk một đoạn

Tái sử dụng teleport dialog hiện có để chọn target, sau đó thay vì publish một lần thì chạy walk controller. Đây là phương án kiểm chứng movement engine mà chưa cần map dependency.

Ưu điểm: triển khai và kiểm thử nhanh. Nhược điểm: chưa giải quyết trải nghiệm click map; vẫn cần UI target state và cancel/re-target.

### 7. Tương tác với feature hiện có

- `JoystickOverlayService` hiện quản lý `JoystickLocationController`, shortcut menu, joystick pad, teleport dialog, cooldown và scan result overlays. Không nên nhét map đầy đủ vào `JoystickPadView`; joystick pad nên chỉ giữ nhiệm vụ điều hướng trực tiếp.
- Có thể thêm shortcut `Map target`/`Walk to target` mở một Activity hoặc một panel riêng. Nếu map là Activity, thao tác tap không tranh touch với Pokémon GO; nếu map là overlay, cần quyết định khi nào overlay nhận touch và khi nào trả touch cho game.
- `JoystickLocationController` hiện có hai command chính: `setJoystick` và `teleport`. Cần thêm state machine target movement hoặc một lớp điều phối riêng, không nên để map view tự gọi `sink.publish`.
- `GeoMath` đã đủ cho chuyển động theo đường tròn lớn đơn giản và khoảng cách; cần bổ sung logic `stepTowards`/`walkTo` có thể test độc lập.
- `NearbySnapshot` đã có player position và spawn position. Nếu map picker hiển thị nearby markers, state này nên là nguồn structured duy nhất, không đọc lại payload trong UI.
- `MoveTo(WALK)` trong core hiện được planner/bridge mô hình hóa nhưng chưa nối vào built-in mock location. Cần quyết định rõ v1 là app-side location movement hay runtime-side game action; hai đường này không được trộn ngữ nghĩa.
- Nếu sau này muốn automation tự walk tới spawn, `AutomationCoordinator`/`AutomationRunner` phải có policy, capability, cancellation, stale-observation và action result semantics. Không nên phát sinh từ một tap UI mà không qua serialized execution boundary.

### 8. Dependencies

**Cho hướng khuyến nghị A/B/D:**

- API/state cho `WalkTarget` gồm target, source, startedAt, current distance, status (`IDLE`, `WALKING`, `ARRIVED`, `STOPPED`, `ERROR`).
- `WalkToController` hoặc mở rộng `JoystickLocationController` với target mode, tốc độ, tolerance, cancel và single-writer tick loop.
- Map renderer hoặc trước mắt là nearby/coordinate picker. Nếu dùng map tiles online, cần chọn provider, attribution, cache/offline policy và license/API key.
- Đồng bộ current position: ưu tiên vị trí cuối cùng controller đã publish; nếu có `NearbySnapshot.playerPosition`, cần quy định nguồn nào là authoritative và cách xử lý lệch.
- Unit test cho geo step, arrival, cancel, re-target, provider failure và time drift; device smoke test với Pokémon GO đang foreground/background.

**Nếu theo hướng C:**

- Build fingerprint cụ thể của Pokémon GO và version-scoped Unity/IL2CPP binding.
- Dữ liệu camera center, scale/zoom, bearing, tilt, viewport/insets tại đúng thời điểm tap.
- Phép biến đổi từ screen point → world/geographic point và validation bằng observation.
- Client-owned input/action binding, capability `MOVE` và flow result; hiện các phần này chưa tồn tại trong probe runtime.
- Bộ test riêng cho zoom, rotate, orientation, map loading, overlay overlap và game update.

### 9. Rủi ro

- **Sai mô hình tọa độ:** map picker có thể hiển thị marker đúng nhưng walk target lệch nếu camera/projection snapshot không cùng thời điểm. Với click trực tiếp POGO, rủi ro này là cao nhất.
- **Nhầm “walk” với đường đi thật:** đường thẳng địa lý sẽ không tránh vật cản hay tuân theo đường phố; UI và acceptance phải nói rõ.
- **Race condition trong location writer:** joystick, teleport, walk target và service stop có thể cùng ghi location. Cần một executor/single writer và priority policy rõ ràng.
- **Game nhận vị trí khác controller:** Android mock provider publish thành công không chứng minh Pokémon GO đã cập nhật map/server. Cần hiển thị `location published` tách khỏi `game observed position`.
- **Runtime bridge chưa mutation-ready:** đưa `MoveTo` vào UI rồi submit qua bridge sẽ chỉ bị reject ở probe hiện tại. V1 phải không phụ thuộc capability này hoặc disable rõ ràng.
- **Map dependency/licensing:** tile server miễn phí không phải mặc định là được dùng production; cần chốt provider trước khi cam kết UI map.
- **Account/game safety:** walk/spoof location có thể vi phạm điều khoản Pokémon GO. Project đã ghi rõ không có anti-detection/Play Integrity bypass; feature phải giữ nguyên giới hạn đó.

### 10. Tiêu chí nghiệm thu

Chưa tìm thấy spec riêng trong `docs/newspec/**` hoặc `docs/specs/**`; các tiêu chí dưới đây là **inferred — needs BA/user confirm**. Đặc biệt, AC-1 cố ý khóa scope phiên bản đầu ở controller-owned target; click trực tiếp trên bản đồ POGO được ghi nhận là phase sau.

## Acceptance Criteria (from spec)

> Source: không có spec màn hình/feature riêng; suy ra từ yêu cầu người dùng và code hiện tại — **inferred — needs BA/user confirm**.

| ID | Rule / Requirement | Formula / Expected | Acceptance note |
|----|--------------------|--------------------|-----------------|
| AC-1 | Chọn target | Một tap/marker hợp lệ tạo đúng một `GeoPoint` với latitude `[-90, 90]`, longitude `[-180, 180]` | V1 dùng controller-owned map, nearby marker hoặc coordinate picker; chưa yêu cầu đọc pixel từ map Pokémon GO. |
| AC-2 | Có vị trí bắt đầu | `currentPoint != null` và mock provider ready trước khi start | Thiếu một trong hai điều kiện → không bắt đầu, UI hiển thị lý do. |
| AC-3 | Walk liên tục | Mỗi tick publish target position với `speed <= maxSpeedKmh`; bước di chuyển không nhảy trực tiếp tới target | Dùng cùng single-writer loop với location controller; không gọi `sink.publish` từ UI. |
| AC-4 | Đến nơi và dừng | `distance(current, target) <= tolerance` → trạng thái `ARRIVED`, speed `0`, strength `0` | Tolerance cần chốt, đề xuất 5–10 m cho v1. Không dao động quanh target. |
| AC-5 | Stop/re-target | Stop hủy hành trình; tap target mới hủy target cũ và chỉ còn một hành trình active | Không có hai scheduled task cùng publish vị trí. |
| AC-6 | Lỗi/lifecycle | provider publish lỗi, service stop hoặc target invalid → trạng thái `ERROR`/`STOPPED`, không tiếp tục walk ngầm | Restart mặc định không resume target cũ nếu chưa có policy. |
| AC-7 | Tách location và game observation | UI phân biệt `location published` với `game observed position`; không tuyên bố game đã tới nếu chỉ mock provider thành công | Giảm false confidence khi Pokémon GO load chậm/disconnect. |
| AC-8 | Không regression joystick/teleport | Joystick, teleport, cooldown, overlay shortcut vẫn hoạt động; mọi location write đi qua policy writer | Walk không reset cooldown ở mỗi tick; teleport khi đang walk phải có policy explicit. |
| AC-9 | Không screen automation trong v1 | Không thêm screen capture, `input tap`, pixel coordinate inference hoặc bypass structured-only guard | Đây là ràng buộc của kiến trúc hiện tại. |
| AC-10 | Bridge capability honesty | Không submit `MoveTo` qua POGO bridge khi runtime chưa announce `MOVE`/chưa allowlist build; hiển thị disabled/unavailable | Probe hiện tại trả `binding_not_implemented`; v1 app-side walk có thể chạy độc lập. |
| AC-11 | Verification | Unit tests cho geo step/arrival/cancel/re-target; `./gradlew test`, `./gradlew :app:assembleDebug` và device smoke test pass | Device test cần kiểm tra location update, game map refresh và overlay touch. |
| AC-12 | Phase 2 direct POGO map click | Chỉ pass khi có version-scoped camera/projection/input binding và test sai số target trên nhiều zoom/rotation/orientation | Không nên coi AC-12 là điều kiện để ship v1. |

- Yêu cầu “click map rồi walk tới đó” → AC-1, AC-3, AC-4, AC-5.
- Yêu cầu không di chuyển sai hoặc chạy ngầm → AC-2, AC-6, AC-8.
- Yêu cầu phù hợp project hiện tại → AC-7, AC-9, AC-10, AC-11.
- Nếu user xác nhận bắt buộc click trực tiếp map trong Pokémon GO → AC-12 trở thành scope bắt buộc và cần chốt lại kiến trúc/runtime binding trước khi triển khai.

---

## Synthesis

### Key Insight

Repo đã có phần khó của “di chuyển location liên tục” ở tầng mock provider: `JoystickLocationController` publish 20 Hz, `GeoMath` tính destination và `RootMockLocationProvider` phát GPS/network test locations. Phần còn thiếu không phải là “walk” nói chung mà là target selection và target-following state machine. Ngược lại, click trực tiếp pixel trên bản đồ Pokémon GO cần dữ liệu camera/projection và input binding mà repo hiện cố ý chưa có; `MoveTo(WALK)` trong protocol chỉ là contract, còn probe runtime vẫn zero-capability và reject command.

### Recommended Approach

Nên làm theo hai phase. Phase 1 xây `MapTargetPicker`/nearby marker picker của controller và `WalkToController` dùng chung single writer với joystick/teleport; ưu tiên test bằng coordinate picker hoặc `NearbySpawn.position` trước khi thêm map tiles. UI phải có start/stop/re-target, tolerance, trạng thái provider và phân biệt “đã publish location” với “game đã quan sát vị trí”. Phase 2 chỉ nghiên cứu click trực tiếp trên map Pokémon GO sau khi có build fingerprint, camera/projection observation và client-owned input binding; không nên dùng screen capture/input injection để lách kiến trúc hiện tại.

### Risks to Watch

- Nếu để joystick, teleport và walk cùng publish độc lập, location sẽ bị race và target không đáng tin.
- Nếu UI gọi walk qua POGO bridge ngay bây giờ, command sẽ bị reject vì runtime chưa có `MOVE` capability; nên giữ v1 ở app-side mock-location path.
- Nếu gọi tính năng là “đi tới điểm trên map” nhưng thực tế đi theo đường thẳng, user có thể kỳ vọng road routing; cần chốt terminology và tolerance từ đầu.

### Open Questions

- ~~Người dùng muốn click trên **map của controller/overlay** hay bắt buộc click trực tiếp trên **bản đồ đang hiển thị bên trong Pokémon GO**?~~ → Resolved in Section 11: bắt buộc direct tap trên map Pokémon GO.
- “Walk” có nghĩa là đường thẳng GPS với tốc độ cấu hình, hay phải bám theo đường phố/route thực tế?
- Khi đang walk mà user dùng joystick/teleport, thao tác nào thắng: dừng walk, chuyển mode ngay, hay yêu cầu xác nhận?
- Vị trí hiện tại nên lấy từ `JoystickLocationController` đã publish hay từ `NearbySnapshot.playerPosition` khi hai nguồn lệch nhau?
- ~~Có chấp nhận phase 1 không cần map tiles, chỉ chọn nearby marker/nhập target, để kiểm chứng walk engine trước không?~~ → Resolved in Section 11: target selection phải bắt đầu từ direct POGO map tap; map picker của controller chỉ là fallback/debug harness.
- Direct POGO map click đã được chọn; vẫn cần chốt **build/version Pokémon GO mục tiêu**, ABI/device và mức sai số tọa độ chấp nhận được.

---

## Section 11 — Chốt hướng direct tap trên bản đồ Pokémon GO

Người dùng xác nhận muốn tap trực tiếp trên bản đồ 3D đang hiển thị trong Pokémon GO, sau đó hệ thống lấy target địa lý và walk tới đó. Vì vậy controller-owned map không còn là đường sản phẩm chính; nó chỉ nên tồn tại như một debug harness để test `WalkToController` khi runtime binding chưa hoàn tất.

### 11.1. Kiến trúc nên chọn

Không nên bắt đầu bằng screenshot rồi tự đo pixel trên ảnh. Direct tap nên đi theo pipeline structured trong target process:

```text
User tap trên map Pokémon GO
        |
        v
Version-scoped Unity/IL2CPP tap observer
        |
        +--> xác nhận tap nằm trên map, không phải UI
        +--> chụp cùng frame: screen point + viewport + camera/map state
        |
        v
Runtime-side screen -> GeoPoint conversion
        |
        v
Bridge Observation: MAP_TAP / MAP_TARGET
        |
        v
Controller nhận target + xác nhận state
        |
        v
Single-writer WalkToController
        |
        v
RootMockLocationProvider publish location đều đặn
```

### 11.2. Vì sao nên convert `screen -> GeoPoint` ở runtime?

Có hai cách triển khai phép chiếu:

1. **Runtime-side conversion — khuyến nghị:** trong process Pokémon GO, dùng camera hiện tại và transform địa lý nội bộ của build để đổi `(screenX, screenY)` thành `GeoPoint`, sau đó chỉ gửi `GeoPoint` và metadata validation qua bridge.
2. **Controller-side unprojection:** runtime gửi camera matrix/view-projection, viewport, map origin/scale và state cần thiết; controller tự unproject. Cách này làm contract lớn hơn và dễ sai nếu map dùng transform riêng, terrain/height hoặc floating origin.

Runtime-side conversion giảm số lượng dữ liệu build-specific phải chảy qua bridge và giữ logic Unity/IL2CPP ở adapter version tương ứng. Tuy vậy, nó chỉ khả thi nếu tìm được camera/map transform hoặc một hàm client-owned đã thực hiện chuyển đổi. Nếu chỉ lấy được Android screen size mà không lấy được Unity camera/map coordinate system thì không đủ để tính tọa độ.

### 11.3. Các lớp cần xây

#### A. Read-only binding discovery

Mở rộng binding probe hiện có theo hướng read-only để trả lời từng câu hỏi, không vội bật mutation:

- exact package/version code/name, build fingerprint, ABI và translation layer;
- `libil2cpp.so`/`libunity.so` và trạng thái exported IL2CPP API;
- assembly/type candidates liên quan tới map, camera, input, touch/pointer, world/geospatial transform;
- khả năng gọi được Unity main-thread API an toàn;
- map state có tồn tại ổn định khi vào overworld hay chỉ sau khi map load xong.

`docs/M2_NEARBY_BINDING.md` đã quy định đúng nguyên tắc: adapter phải version-scoped, không reuse offset/signature chỉ vì version number giống nhau, và artifact binary/derived signature không commit vào repo.

#### B. Tap observer trong Pokémon GO

Tap observer cần bắt **event thật** trong target process, không phát sinh tap mới. Dữ liệu tối thiểu của event:

- `tapId`/sequence và timestamp monotonic;
- `screenX`, `screenY` theo cùng coordinate space với Unity camera;
- viewport width/height, orientation và insets nếu có;
- map interaction state: map loaded, overworld, UI layer hit, pointer id;
- camera/map snapshot hoặc `GeoPoint` đã convert;
- binding/build fingerprint.

Tap trên overlay, dialog, encounter, gym menu hoặc UI khác phải bị loại trước khi phát `MAP_TAP`. Nếu không phân biệt được map hit với UI hit, target phải bị từ chối thay vì đoán.

Hiện chưa biết class/method cụ thể của Pokémon GO build nào sẽ cung cấp event này. Đây là phần cần device spike; không nên đặt tên binding giả trong core. Nếu hook được Unity input ở một boundary ổn định, đó là preferred path. Nếu chỉ hook Android `MotionEvent` bên ngoài mà không giữ được cùng camera state, độ tin cậy sẽ thấp hơn.

#### C. Projection/geo conversion

Có thể implement theo thứ tự ưu tiên:

1. Gọi transform/map API nội bộ của client nếu nó đã có hàm screen/world hoặc world/geo.
2. Dùng `Camera.ScreenPointToRay`/tương đương trên Unity main thread, raycast vào map plane/terrain, rồi dùng map world-origin/scale để đổi world point sang lat/lon.
3. Chỉ khi hai cách trên không có, mới gửi matrix/state về controller để unprojection; đây là fallback khó kiểm chứng hơn.

Không được dùng một công thức phẳng kiểu “mỗi pixel = X mét” cố định. Công thức đó sẽ sai khi zoom, bearing, pitch, orientation, device density, safe area, floating origin hoặc map tile origin thay đổi.

#### D. Bridge contract

Nên thêm một observation/event có version riêng, ví dụ `MAP_TAP` hoặc `MAP_TARGET`, thay vì nhét dữ liệu vào `NEARBY`:

- `tapId`, `observedAtEpochMs`, `observedAtElapsedNs`;
- `screenX/screenY` để debug;
- `targetLatitude/targetLongitude` nếu conversion thành công;
- `playerLatitude/playerLongitude` tại cùng snapshot;
- `mapStateVersion`/`cameraSnapshotId`;
- `confidence` hoặc `conversionError`;
- runtime identity/build fingerprint.

Bridge phải coi event là observation, không phải mutation command. Như vậy direct tap không cần phá structured-only source guard và không cần gửi `input tap` từ controller.

#### E. Target handoff và walk

`StructuredAutomationController` nhận map event và ghi target vào một state bridge app-local hoặc IPC nhỏ. `JoystickOverlayService`/location owner đọc target rồi gọi API target movement của location controller. Cần giữ một single writer cho mọi lệnh `joystick`, `teleport`, `walk` và `stop`.

Trong v1, nên walk bằng `RootMockLocationProvider` hiện có thay vì submit `AutomationAction.MoveTo` qua POGO bridge. Lý do là runtime probe hiện announce zero capabilities và `runtime_command_channel_loop` vẫn trả `binding_not_implemented`; `MoveTo(WALK)` mới chỉ là protocol/domain contract. Sau này nếu client-owned `MOVE` binding được verified, có thể thêm backend thứ hai nhưng không được làm thay đổi semantics của target tap.

### 11.4. Spike plan trước khi viết feature hoàn chỉnh

Spike nên có output kiểm chứng được sau từng bước:

1. **Fingerprint:** chạy binding diagnostics trên đúng device/emulator, ghi package/version/ABI/translation layer; chọn một build duy nhất.
2. **Metadata survey:** xác nhận IL2CPP API/assembly survey hoạt động và lập danh sách candidate map/camera/input. Không commit dump nhạy cảm hoặc binary artifact.
3. **Tap telemetry:** bắt tap thật và phát event read-only gồm screen coordinate, viewport, state/sequence; chưa walk.
4. **Map-hit filtering:** chứng minh tap trên map được nhận, tap trên UI/encounter bị loại, orientation/rotation không làm đảo trục.
5. **Known-point calibration:** dùng một điểm/marker có `GeoPoint` biết trước hoặc điểm player hiện tại; đo sai số theo zoom, bearing, pitch và nhiều vị trí màn hình.
6. **Runtime conversion:** chỉ khi calibration đạt ngưỡng mới phát `GeoPoint` qua bridge.
7. **Location-only walk:** nối target giả/target telemetry vào `WalkToController`, kiểm chứng publish 20 Hz, arrival, stop, re-target.
8. **End-to-end:** tap POGO map → event → target → mock location → map reload/nearby observation xác nhận vị trí mới.

Nếu bước 3 không làm được với một binding ổn định, direct tap không nên chuyển sang suy đoán từ screenshot. Khi đó blocker là runtime binding chứ không phải UI/location code.

### 11.5. Acceptance criteria bổ sung cho scope direct POGO

Các tiêu chí dưới đây **inferred — needs BA/user confirm** và bổ sung/thay thế phần “phase 2” trước đó:

| ID | Rule / Requirement | Formula / Expected | Acceptance note |
|----|--------------------|--------------------|-----------------|
| DMC-1 | Version-scoped runtime | Chỉ enable feature khi package/version/ABI/build fingerprint khớp adapter đã verified | Unknown/ambiguous build → disabled, không đoán offset hoặc projection. |
| DMC-2 | Nhận tap thật | Một tap trên map tạo tối đa một `MAP_TAP` với sequence/timestamp/viewport hợp lệ | Không dùng screenshot polling hoặc `input tap` injection. |
| DMC-3 | Phân biệt map và UI | Tap ngoài map, loading, encounter, dialog hoặc overlay → không tạo target | Fail closed nếu hit-test state không rõ. |
| DMC-4 | Cùng snapshot | Tap point và camera/map state dùng cùng frame/sequence | Không ghép tap hiện tại với camera snapshot cũ sau zoom/pan/rotate. |
| DMC-5 | Projection đúng | `screen point -> GeoPoint` ổn định qua zoom/bearing/pitch/orientation trong sai số đã chốt | Cần calibration fixture và sai số mét cụ thể; chưa chốt mặc định. |
| DMC-6 | Bridge observation | `MAP_TAP/MAP_TARGET` đi qua protocol versioned, giữ runtime identity và conversion error | Không trộn vào nearby payload hoặc bypass session validation. |
| DMC-7 | Walk target | Target hợp lệ được handoff tới single-writer walk controller; publish từng bước, không teleport thẳng tới target | Dùng mock-location backend ở phase đầu. |
| DMC-8 | Arrival/stop/re-target | `distance <= tolerance` → stop; stop và tap mới hủy target cũ | Không có hai movement loop cùng ghi provider. |
| DMC-9 | Game confirmation | `location published` và `game observed position` là hai trạng thái riêng | Chỉ báo “game đã tới” sau observation mới hoặc timeout rõ ràng. |
| DMC-10 | Runtime fail-safe | Mất binding, bridge, camera state hoặc conversion → dừng/không start walk và hiển thị lỗi | Không sử dụng target cũ hoặc target stale. |
| DMC-11 | Verification | Unit test projection contract/validation, protocol test, binding smoke test và end-to-end device test pass | Bao phủ map load, UI hit, zoom, rotation, orientation, service restart. |
| DMC-12 | Không anti-detection | Không thêm root hiding, Play Integrity bypass, anti-cheat evasion hoặc logic che giấu mock location | Giữ đúng scope và giới hạn đã ghi trong README. |

### 11.6. Điều chỉnh synthesis

**Key Insight cập nhật:** Direct map tap có thể làm được về mặt kiến trúc, nhưng điểm khó nhất là tạo một **version-scoped read-only tap/camera/geo binding** trong process Pokémon GO. `GeoMath` và mock provider chỉ giải quyết nửa sau là di chuyển; chúng không thể suy ra tọa độ từ pixel nếu không biết Unity camera và map transform tại cùng thời điểm.

**Recommended Approach cập nhật:** Chấp nhận direct POGO map click là scope chính, nhưng triển khai theo spike read-only trước: fingerprint → tap telemetry → map/UI hit filtering → same-frame projection → calibration. Ưu tiên runtime-side conversion thành `GeoPoint`, phát `MAP_TARGET` qua bridge, rồi handoff sang app-side single-writer walk trên `RootMockLocationProvider`; chỉ dùng `MoveTo(WALK)` runtime-side sau khi `MOVE` binding thực sự được verified.

**Risks to Watch cập nhật:**

- Một game update hoặc khác ABI/translation layer có thể làm hỏng toàn bộ tap/camera binding; phải fail closed theo exact fingerprint.
- Projection có thể đúng ở tâm map nhưng sai ở mép màn hình hoặc khi pitch/rotate; cần calibration nhiều điểm, không chỉ một demo.
- Tap event có thể đến khi map/UI state đang chuyển frame; thiếu snapshot đồng bộ phải được xem là conversion failure.

**Open Questions cập nhật:**

- ~~Người dùng muốn controller-owned map hay direct POGO map click?~~ → Đã chốt direct POGO map click.
- Exact Pokémon GO build/version, thiết bị/ABI (physical ARM64 hay BlueStacks x86_64/translation) là gì?
- Có thể chấp nhận runtime-side conversion trả target từ tap, hay bắt buộc controller phải tự tính từ camera matrices?
- Sai số tối đa cho target là bao nhiêu mét ở tâm map và mép màn hình?
- “Walk” v1 là đường thẳng GPS theo tốc độ cấu hình hay cần route theo road network?
- Khi user tap target mới/joystick/teleport trong lúc walk, policy ưu tiên nào được chốt?
