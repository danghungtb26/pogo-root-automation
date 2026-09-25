# Brainstorm: Chính sách force cho auto walk tới tọa độ

**Loại:** feature
**Ngày phân tích đầu tiên:** 2026-09-25
**Phạm vi game/build/ABI:** Thiết kế kiến trúc cho point candidate native → Kotlin fake-location; chưa gắn vào một flow PoGo cụ thể

**Làm rõ sau brainstorm:** Người dùng xác nhận mong muốn ban đầu là Kotlin lưu candidate đã nhận, rồi location controller tick theo thời gian và dịch chuyển fake location từng bước tới candidate. Vì vậy route không có lease/renewal; freshness chỉ kiểm lúc nhận candidate. B không-force bị bỏ qua thì A tiếp tục tới local arrival/terminal. UI sau này cũng có thể nhập tọa độ và tạo route owner `USER` qua cùng coordinator/executor/tick path; không cần native bridge hay vòng movement riêng. Làm rõ này thay thế các đề xuất lease tạm thời bên dưới. Khi lập brainstorm, flow fort-walk được để cho phase tích hợp; kết quả migration thực tế được ghi ở cuối tài liệu.

## Bổ sung 1 — Làm rõ phạm vi thiết kế

Theo làm rõ của người dùng, lượt này chỉ thiết kế kiến trúc cho luồng candidate tọa độ và policy `force`. Luồng auto-fort hiện tại được đọc để hiểu boundary/lease và các điểm coupling; không phải đích triển khai hay contract bắt buộc cho thiết kế mới.

- Thiết kế ở mức trách nhiệm giữa producer candidate, Kotlin admission/priority policy, active navigation state và location executor.
- Xác định semantics cho active target, `force`, owner/source, route lifetime, terminal events, session/freshness và kết quả accept/reject.
- Chưa sửa hoặc chốt cách sửa `RuntimeAutoFortNavigation`, `RuntimeNavigationPayload`, encoder/decoder, hoặc việc native chọn/phát fort target.
- Việc tích hợp và sửa lại flow fort-walk theo kiến trúc được chọn là phần việc sau; khi làm phần đó sẽ đánh giá lại contract/version và chi tiết lifecycle theo source lúc ấy.

## Bước 1 — Yêu cầu và phạm vi

### Yêu cầu đã biết

- Tính năng điều khiển việc đi bộ tới một tọa độ điểm; phần thực thi movement/fake-location dự kiến tiếp tục ở Kotlin.
- Native có thể gửi một tọa độ candidate làm mục tiêu đi bộ.
- Payload dự kiến có thêm cờ `force`.
- Câu hỏi chính sách: khi Kotlin đang đi tới candidate A rồi nhận candidate B với `force=false`, Kotlin có nên bỏ qua B và tiếp tục đi tới A không?
- Cần xác định riêng hành vi khi không có mục tiêu đang chạy, khi `force=true`, và khi request bị stale hoặc xung đột với walk do người dùng khởi tạo.

### Phạm vi lần phân tích này

- Dùng request native→Kotlin và trạng thái walk hiện có làm bằng chứng về coupling/constraints cho thiết kế.
- Định nghĩa trách nhiệm kiến trúc của candidate admission và `force`, vẫn giữ native làm chủ quyết định gameplay, Kotlin làm chủ movement.
- Đề xuất semantics và tiêu chí chấp nhận ở mức thiết kế; chưa chọn chi tiết migration của auto-fort, chưa triển khai code hay thay đổi protocol.

### Bằng chứng kiến trúc đã biết

- `docs/ARCHITECTURE.md:7-12` giao automation/gameplay runtime cho native, còn Kotlin sở hữu fake location và `walk-to-location`.
- `docs/issues/2026-09-22/kotlin-ui-only-boundary/brainstorm.md:91` ghi nhận `NativeNavigationReceiver.receive(payload, observedAtNanos)` cùng kiểm tra độ cũ/lease và forward navigation intent.
- `docs/issues/2026-09-22/kotlin-ui-only-boundary/checklists/phase-3-kotlin-cutover.md:87,94-95` mô tả lease native 5 giây, luồng `NativeNavigationReceiver` → `AutoFortNavigationBus` → controller, map-target TTL 30 giây và manual input arbitration.
- Các tài liệu trên là baseline cần đối chiếu với source hiện tại; chúng chưa xác nhận ý nghĩa của cờ `force`.

### Giả định và câu hỏi còn mở

- Theo làm rõ của người dùng, candidate/force ở đây là contract kiến trúc tổng quát; không đồng nhất với contract auto-fort hiện có. Mapping hoặc thay đổi luồng fort-walk sẽ phân tích ở lượt sau.
- Quyết định hiện tại: `force` chỉ thay native-owned route; không ghi đè location do USER chọn. Candidate hợp lệ được nhận khi không có native route active; B không-force khi A active bị bỏ qua và A tiếp tục tới arrival/terminal.
- Kiến trúc định nghĩa active route bằng owner/id/generation/session, không chỉ suy từ `JoystickLocationController.walkTarget`.

## Bước 2 — Bằng chứng từ reverse

- Phạm vi theo `AGENTS.md`: reverse output được pin cho Pokémon GO `0.427.0`, version code `2026082702`, ABI `arm64-v8a`; các file extract hiện có trong `reverse/pogo-0.427.0/classes/`.
- Đã tìm các extract với nhóm từ khóa `navigation`, `autowalk`, `walk-to`, `map-target`, `targetlocation`, `latitude`, `longitude`: không có kết quả trong thư mục classes.
- Danh sách extract hiện có gồm `DynamicTappablesService.cs`, `IDynamicTappablesService.cs`, `WildMapPokemon.cs`, `MapEntityCell.cs`, `DynamicTappables.cs`, `IDynamicTappable.cs`, `TapGesture.cs`, `MapPokemon.cs`; chưa thấy symbol biểu đạt navigation hay candidate tọa độ.
- Không tra dump nén: câu hỏi hiện tại là chính sách Kotlin nhận/giữ/thay mục tiêu sau khi đã có tọa độ; chưa cần xác định method PoGo nào tạo ra tọa độ. Nếu source framework cho thấy cần xác minh nguồn gameplay của candidate, sẽ ghi rõ giới hạn thay vì suy ra method game.
- Kết luận bước này: chưa có bằng chứng reverse nào quyết định được ý nghĩa `force`; đây là policy/contract ở boundary native→Kotlin. Việc không thấy symbol trong curated extracts không chứng minh game không có API di chuyển hoặc target-selection.
- Chưa xác minh: nguồn native sẽ tạo candidate trong flow tích hợp tương lai; điểm này không chặn thiết kế generic và sẽ được tra khi sửa luồng fort-walk.

## Bước 3 — Function/method, param và hành vi

Các method dưới đây là framework Kotlin hiện có, không phải method PoGo. Chúng chỉ làm baseline để nhận diện trách nhiệm/coupling; thiết kế mới không bắt buộc sửa đúng các method này. Reverse chưa cung cấp symbol để truy ra caller game cụ thể.

### `RuntimeNavigationPayloadCodec.decode` — đã xác nhận từ source Kotlin

- Nguồn: `bridge/protocol/src/main/kotlin/dev/pogoroot/automation/bridge/RuntimeNavigationPayloadCodec.kt:7-35`; protocol version `1`.
- Payload hiện có `kind: RuntimeNavigationKind` (`STOP`, `WALK`, `ARRIVED`), `fortId: String`, `target: GeoPoint`, `reason: String`; chưa có `force` hoặc source/priority.
- Param: `payload: ByteArray`; codec kiểm tra size, marker, enum kind, string bounds, latitude/longitude hữu hạn và trong miền hợp lệ, id khác rỗng cho WALK/ARRIVED, không có trailing bytes.
- Kết quả: `Result<RuntimeNavigationPayload>`; chỉ decode/validate, không chọn target hay quyết định thay candidate.
- Giới hạn: chữ ký native wire tương ứng cần ghép ở bước 4; không suy rằng thêm field Kotlin một mình là tương thích protocol.

### `NativeNavigationReceiver.receive` / `reset` — đã xác nhận từ source Kotlin

- Nguồn: `app/src/main/java/dev/pogoroot/automation/location/NativeNavigationReceiver.kt:11-53`.
- `receive(payload: RuntimeNavigationPayload, observedAtNanos: Long)`: reject thời điểm <=0, ở tương lai hoặc quá lease 5 giây bằng cách gọi `reset`, tức gửi `Stop`; `ARRIVED` chỉ phát UI event rồi return; `WALK` chuyển thành `AutoFortNavigationCommand.WalkTo(fortId,target)`, `STOP` thành `Stop(reason)`, rồi gửi command với expiry `observedAtNanos + LEASE_NS`.
- `reset(reason: String = "runtime disconnected")`: gửi `Stop(reason)` với expiry `0` và cập nhật `lastCommand`.
- Không có kiểm tra active target hay `force`; mọi WALK hợp lệ đều được gửi xuống bus. `lastCommand` chỉ dùng tránh lặp thông báo UI, không chặn command.
- Receiver chỉ giữ `lastCommand` để so sự kiện; nó không là owner bền vững của candidate đang được controller thực hiện.

### `AutoFortNavigationBus.publish` / `register` — đã xác nhận từ source Kotlin

- Nguồn: `app/src/main/java/dev/pogoroot/automation/location/AutoFortNavigationBus.kt:9-55`.
- `publish(command: AutoFortNavigationCommand, expiresAt: Long)`: thay `latestCommand` bằng command mới; thông báo listener nếu command khác; WALK tạo expiry timer. Cùng command vẫn cập nhật generation/expiry nhưng không gọi listener.
- `register(commandListener: (AutoFortNavigationCommand) -> Unit)`: lưu listener, hết hạn thì chuyển WALK thành STOP, rồi replay `latestCommand`.
- Bus là process-local handoff/lease, không thực hiện policy chọn candidate. `force=false` cần được xử lý trước khi `publish` nếu yêu cầu là giữ nguyên candidate/lease cũ.

### `JoystickLocationController.walkTo` / `stopWalking` / `tick` — đã xác nhận từ source Kotlin

- Nguồn: `app/src/main/java/dev/pogoroot/automation/location/JoystickLocationController.kt:15-39,41-51,90-165,220-317`.
- `walkTo(target: GeoPoint, toleranceMeters: Double = 8.0)`: validate coordinate/tolerance; nếu có current point và ngoài tolerance thì ghi `walkTarget=target`, `walkStatus=WALKING`, đặt speed/joystick strength về 0. Không kiểm tra target hiện tại, nên lệnh mới thay target cũ. Nếu đã trong tolerance thì ghi `ARRIVED` và xóa target; nếu current point null thì ghi `ERROR`.
- `stopWalking()`: xóa target và dừng speed/strength; không kiểm tra owner/source của walk.
- `tick()`: nếu có `walkTarget`, dùng `WalkPlanner.step` để cập nhật điểm mỗi tick và xóa target khi đến tolerance; nếu không có walk target thì có thể chạy theo joystick strength.
- State chỉ có `walkTarget`/`walkStatus`, không phân biệt target native, map target hay target người dùng. `setJoystick` và `teleport` cũng xóa walk target, vì vậy controller hiện chưa có owner/priority để force có thể áp dụng riêng cho native walk.

### `JoystickOverlayService.applyAutoFortNavigationCommand` — đã xác nhận từ source Kotlin

- Nguồn: `app/src/main/java/dev/pogoroot/automation/overlay/JoystickOverlayService.kt:456-474`.
- Nhận `WalkTo` thì gọi trực tiếp `controller.walkTo(command.target)`; nhận `Stop` thì gọi `controller.stopWalking()`; không có nhánh kiểm tra force hoặc quyền sở hữu mục tiêu.
- `AutoFortNavigationCommand.WalkTo(val fortId: String, val target: GeoPoint)` và `Stop(val reason: String)` tại `core/src/main/kotlin/dev/pogoroot/automation/core/automation/AutoFortNavigationCommand.kt:6-8`; hiện thiếu cờ force và metadata source.

- Kết luận bước này: với implementation hiện tại, WALK B sẽ thay WALK A khi tới overlay/controller. Muốn `force=false` giữ A thì cần policy gate và cách nhận diện active native target trước `walkTo`; chỉ thêm boolean vào payload mà không thay receiver/bus/command flow không tạo hành vi giữ candidate.
- Chưa xác minh ở bước này: event router, service lifecycle và native encoder/emitter; sẽ ghép luồng và lease ở bước 4.

## Bước 4 — Luồng hành vi và điểm tích hợp

### Luồng hiện tại đã xác nhận

1. Native `RuntimeAutoFortNavigation.update(snapshot)` giữ target hiện tại nếu còn hợp lệ; khi không còn target thì tự chọn fort đủ điều kiện. Khi tới trong bán kính 12 m, nó phát `ARRIVED`, xóa target rồi có thể phát tiếp `WALK` cho target mới trong cùng lần update. Nguồn: `zygisk/jni/modules/catch_spin/auto_fort_navigation.h:23-63,66-73`.
2. `update_runtime_navigation` encode và phát từng command; nhánh busy/disabled phát `STOP`. Coordinator gọi `pause_runtime_navigation_if_busy` trước guard và gọi `update_runtime_navigation` sau khi đọc snapshot thành công. Nguồn: `zygisk/jni/modules/catch_spin/navigation.inc:3-31`; `zygisk/jni/modules/catch_spin/coordinator.inc:328-355,357-379`.
3. Native payload hiện là version 1, mang kind/fort id/latitude/longitude/reason. App router chỉ nhận observation đã khớp runtime identity, sequence và freshness tổng quát; navigation sau đó được decode và chuyển cho receiver. Nguồn: `zygisk/jni/shared/bridge_kotlin/runtime_navigation_protocol.h:5-46`; `app/src/main/java/dev/pogoroot/automation/runtime/observation/RuntimeUiEventRouter.kt:73-99,128-142,153-172`.
4. Router ghi latest navigation vào UI state, rồi gọi `NativeNavigationReceiver.receive`; facade poll native events mỗi 500 ms. Nguồn: `app/src/main/java/dev/pogoroot/automation/service/RuntimeUiAutomationFacade.kt:48-58,63-71,121-138`; `app/src/main/java/dev/pogoroot/automation/runtime/RuntimeUiStateStore.kt:58-60,75-76`.
5. Receiver gắn native lease 5 giây và publish bus; overlay đăng ký listener, replay command mới nhất; `WalkTo` gọi `controller.walkTo`, `Stop` gọi `controller.stopWalking`. Nguồn: `app/src/main/java/dev/pogoroot/automation/location/NativeNavigationReceiver.kt:18-47,50-52`; `app/src/main/java/dev/pogoroot/automation/location/AutoFortNavigationBus.kt:18-49`; `app/src/main/java/dev/pogoroot/automation/overlay/JoystickOverlayService.kt:74-79,111,456-474`.

### Hệ quả đối với force=false

- Nếu Kotlin bỏ qua B mà không gọi bus, bus tiếp tục giữ A nhưng expiry timer ban đầu vẫn chạy; sau 5 giây không có lệnh A mới, bus sẽ phát `STOP`. Do native hiện phát target đã chọn ở mỗi scan, A chỉ được gia hạn nếu native vẫn phát A; nếu native đã chuyển sang B thì yêu cầu “tiếp tục A” không được đảm bảo.
- Nếu Kotlin coi mọi native WALK hợp lệ (kể cả B bị từ chối) là gia hạn A, app có thể giữ A sống dù native đã đổi lựa chọn. Điều đó cần được định nghĩa như policy rõ ràng, và không áp dụng cho `STOP`, stale/session reset hoặc candidate hết hiệu lực.
- Native có thể phát `ARRIVED(A)` rồi `WALK(B)` trong cùng update. Hiện ARRIVED chỉ tạo UI event; nó không kết thúc/đánh dấu target A trong controller. Một gate chỉ dựa trên `controller.snapshot().walkTarget != null` có thể nhầm A còn hoạt động và từ chối B, dù native đã kết thúc A.
- Kotlin walk state không mang source/owner, trong khi map target/favorite/native đều có thể gọi cùng `walkTo`; vì vậy chỉ `force` boolean chưa đủ để ngăn native ghi đè thao tác manual hoặc để STOP native chỉ dừng walk của native.

### Các boundary cần giữ trong thiết kế kiến trúc

- Producer/native chọn candidate và quyết định tính hợp lệ theo game state; Kotlin không tự chọn candidate từ raw game state.
- Kotlin có một policy boundary để nhận/từ chối/thay candidate theo `force`, current owner và active state; location executor chỉ thực hiện target đã được chấp nhận.
- Candidate identity, active-target generation, nguồn sở hữu, active-route lifetime và terminal result cần có semantics rõ để không nhầm request mới với route đang chạy.
- Runtime identity/sequence/freshness/session guards vẫn thuộc contract; chi tiết gắn chúng vào fort-walk được để lượt tích hợp sau.
- Cadence native/thiết bị chưa khảo sát vì generic Kotlin walk dùng tick controller hiện có; lượt tích hợp fort-walk sẽ xác minh event ordering/lifecycle theo runtime.

## Bước 5 — Hướng xử lý

Đây là hướng kiến trúc, không phải task list để sửa luồng auto-fort hiện tại. Tên API bên dưới minh họa semantics; chưa chốt wire schema, class placement hay version.

### Đánh giá câu hỏi

Với kiến trúc mong muốn, Kotlin nhận candidate A một lần, giữ A là active route trong RAM và controller tiếp tục tick fake location dần tới target. Candidate B với `force=false` bị bỏ qua trước khi chạm executor, nên A không bị ghi đè và tiếp tục tới local arrival/terminal. Không cần native renew A. Lease 5 giây chỉ là baseline của flow fort-walk legacy hiện tại và sẽ được đánh giá lại khi flow đó được migrate.

### Hướng kiến trúc khuyến nghị — tách candidate khỏi active navigation

1. Phân biệt **candidate** (tọa độ đề xuất) với **active route** (candidate đã được Kotlin nhận và giao location executor). Native chịu trách nhiệm candidate/gameplay eligibility; Kotlin admission và fake-location execution.
2. Đặt admission policy trong Kotlin ở boundary logic giữa ingress và location executor. State tối thiểu: `candidateId`, target, owner/source (`NATIVE`/`USER`), active generation và session. Route được giữ trong RAM tới local arrival, native terminal, force replacement, USER preemption, provider failure hoặc session reset.
3. API dự kiến — **Đề xuất, chưa tồn tại; chỉ minh họa contract kiến trúc**:

   ```kotlin
   data class NativeWalkCandidate(
       val candidateId: String,
       val target: GeoPoint,
       val force: Boolean,
   )

   fun submitCandidate(candidate: NativeWalkCandidate, observedAtNanos: Long): CandidateDecision
   ```

   Param `candidateId` dùng để tương quan candidate và terminal result; `target` là tọa độ đã qua validation; `force` chỉ định quyền thay active candidate theo policy; `observedAtNanos` chỉ dùng kiểm freshness khi nhận. Một decision result nên phân biệt tối thiểu accepted, ignored-busy, replaced, unchanged, rejected-stale/invalid và rejected-owner-conflict; vị trí trả kết quả (native ack hay Kotlin diagnostic/UI) còn là quyết định kiến trúc mở.
4. Policy gợi ý:
   - Không có native target active: nhận candidate dù force false.
   - Candidate cùng ID/target với route active: giữ nguyên route, không restart tick (`UNCHANGED`). Cùng ID nhưng target khác là invalid.
   - Candidate khác, force false: bỏ candidate mới, giữ A và tiếp tục tick tới target; không cần heartbeat hay gia hạn.
   - Candidate khác, force true: thay A bằng B nếu current owner là `NATIVE`; force không tự ghi đè joystick/walk/favorite do user. User input tiếp tục là quyền ưu tiên và thao tác explicit của user mới chiếm quyền.
5. Terminal (`stop`, `arrived`) phải tương quan đúng `candidateId`/generation/session để terminal cũ không kết thúc route mới. Local arrival/provider callback cũng phải thuộc generation hiện hành. Local arrival chỉ là postcondition fake-location và UI; không chứng minh game-side arrival hay dispatch gameplay.
6. Thiết kế phải giữ freshness, capability/identity, validation, provider readiness, session reset và fail-closed. Policy `force` không được tự cấp quyền vượt qua guard an toàn hoặc quyền owner khác.

### Ranh giới trách nhiệm đề xuất

| Thành phần | Sở hữu | Không sở hữu |
|---|---|---|
| Native candidate producer | Chọn candidate theo game state/eligibility; phát identity/version và lifecycle intent | Fake-location execution hoặc cách Kotlin điều khiển provider |
| Bridge ingress | Payload validation, runtime identity, session, sequence và timestamp/freshness | Chọn target hoặc quyết định ưu tiên candidate |
| Kotlin admission policy | `force`, owner arbitration, active target/generation, accept/reject/replace/terminal decisions | Đọc raw game state hay chọn candidate gameplay |
| Kotlin location executor | Coordinate validation, path step/speed, mock-provider write, local movement state/arrival | Tự chọn candidate tiếp theo hoặc xác nhận PoGo đã tới nơi |
| Native gameplay policy | Dựa trên game-side state để quyết định hành động sau navigation | Dựa riêng vào Kotlin local `ARRIVED` như bằng chứng game-side arrival |

### State transition tối thiểu cần định nghĩa

| State/event | Quyết định kiến trúc | State sau xử lý |
|---|---|---|
| `IDLE` + candidate hợp lệ, force bất kỳ | Accept | Candidate thành active |
| `ACTIVE(A)` + candidate `B`, `force=false` | Ignore B; không đổi target | `ACTIVE(A)` tiếp tục tick tới arrival/terminal |
| `ACTIVE(A)` + candidate `B`, `force=true` và owner cho phép | Replace atomically; vô hiệu generation của A | `ACTIVE(B)` |
| `ACTIVE(A)` + terminal A hợp lệ | Hoàn tất/hủy A | `IDLE` |
| `ACTIVE(B)` + terminal cũ của A | Ignore stale terminal | `ACTIVE(B)` |
| `USER` owner + candidate native | Reject theo policy ưu tiên user | User route giữ nguyên |

### Phần để lượt sau

Sau khi chốt kiến trúc generic, lượt tích hợp riêng sẽ rà và sửa lại flow fort-walk: semantics `WALK/STOP/ARRIVED`, việc native giữ hay đổi target, target correlation, cách route Kotlin tiếp tục tới target sau candidate one-shot, và cập nhật encoder/decoder native-Kotlin đồng bộ. Không buộc giữ lease semantics của legacy flow.

### Vì sao chọn hướng này

- Chỉ có Kotlin giữ state cần để bảo vệ movement đang chạy; native vẫn sở hữu target selection/gameplay eligibility, theo `docs/ARCHITECTURE.md:7-12` và boundary đã rà ở bước 4.
- Phân tách producer, admission policy và executor giữ game decision ở native, arbitration/movement ở Kotlin; controller không đọc game state.
- Không biến `force` thành quyền ghi đè mọi location input; owner cần là dữ liệu của active route, không suy ra từ tọa độ.
- Freshness được kiểm tại ingress/admission; sau khi nhận, Kotlin giữ route và tự tick tới terminal, tránh phụ thuộc native gửi lại target để duy trì walk.

### Rủi ro và cần chốt trước triển khai

- Hành vi đã chốt mặc định cho `force=true` là chỉ thay native-owned target; route USER vẫn được ưu tiên.
- Cần chốt accept/reject feedback có cần gửi native/UI hay chỉ giữ diagnostic nội bộ; active-route lifetime và quyền hủy manual route đã được định nghĩa.
- Protocol hiện tại là bằng chứng baseline, không phải schema đích đã chốt. Khi sửa fort-walk, cập nhật Kotlin/native cùng lúc, giữ size/identity/sequence/freshness và fail-closed khi version lạ.
- Thứ tự ARRIVED/STOP/WALK của native hiện tại là chi tiết phải thiết kế lại trong lượt sửa fort-walk sau, không phải constraint buộc kiến trúc generic dùng nguyên thứ tự đó.

## Bước 6 — Tiêu chí chấp nhận và xác minh

Các tiêu chí dưới đây nghiệm thu semantics kiến trúc; chúng chưa yêu cầu thay đổi cụ thể trong auto-fort code.

### Tiêu chí chấp nhận

Các AC dưới đây gồm yêu cầu trực tiếp của người dùng và các guard suy ra từ boundary Kotlin/native hiện hành. Policy giữ USER priority; route one-shot được giữ tới terminal/local arrival, không dùng renewal.

| ID | Nguồn yêu cầu | Điều kiện đầu vào | Hành vi / postcondition | Method liên quan | Cách kiểm chứng |
|---|---|---|---|---|---|
| AC-01 | Người dùng | Không có native target active; candidate hợp lệ, `force=false` | Kotlin admission nhận candidate; executor được giao target, native vẫn chọn candidate | Candidate admission → location executor | Review state transition idle → active và ownership |
| AC-02 | Người dùng | A là native target active; B khác A, hợp lệ, `force=false` | Kotlin bỏ qua B; A giữ active route và tiếp tục tick từng bước tới local arrival/terminal; không cần renew | Candidate admission và active-route lifecycle | Walkthrough A→B và controller tick tiếp tục với target A |
| AC-03 | Đề xuất từ yêu cầu | A native active; B khác A, hợp lệ, `force=true` | Policy atomically thay A bằng B; terminal/generation cũ của A không thể stop B | Candidate admission, active-route generation | Walkthrough A replace B rồi gửi late STOP(A) |
| AC-04 | Ràng buộc manual arbitration; policy đề xuất | Active owner là USER (joystick, favorite/map walk hoặc UI nhập tọa độ); native gửi B với force true hoặc false | Native candidate bị từ chối; route người dùng tiếp tục tới khi user dừng/thay đổi | Candidate coordinator; `JoystickLocationController` | Review user entrypoints và owner transitions |
| AC-05 | Đề xuất từ yêu cầu | Candidate trùng ID/target với native active | Không restart route/tick; active route giữ nguyên | Admission policy, active route | Review duplicate candidate và route generation |
| AC-06 | Ràng buộc freshness/session; đề xuất target correlation | STOP/ARRIVED(A) đến sau khi B đã active | Chỉ kết thúc A nếu id/generation/session còn khớp; terminal cũ không dừng B hay USER movement | Active-route lifecycle contract | Walkthrough A→B→late STOP(A), A→ARRIVED→B, session reset |
| AC-07 | Ràng buộc protocol hiện hành | Payload sai version, coordinate ngoài miền, identity sai, sequence lùi, stale/future timestamp | Không nhận candidate; native-owned movement hiện hành không bị target invalid ghi đè | `RuntimeUiEventRouter`, codec, receiver | Dùng các guard hiện có; source review và focused existing receiver test |
| AC-08 | `AC-LOC-03` tại `docs/issues/2026-09-22/kotlin-ui-only-boundary/brainstorm.md:378` | Kotlin local arrival hoặc walk state đổi sang ARRIVED | Chỉ dừng fake movement/cập nhật UI; không dispatch catch/spin hay xác nhận game-side arrival | `JoystickLocationController.tick`; native gameplay policy | Review production callers và native action eligibility |
| AC-09 | Làm rõ mới của người dùng | Người dùng nhập tọa độ hợp lệ trong UI và chọn auto walk | UI tạo route USER qua coordinator/executor hiện có; cùng tick và single mock-location writer; native `force` không chiếm route này | UI coordinate input → `WalkCandidateLocationActions.userWalkTo` → `WalkCandidateCoordinator.userWalkTo` | Review validation, owner arbitration và không có writer/loop thứ hai |

### Kế hoạch xác minh kiến trúc và phần việc sau

- Thiết kế: review state machine idle/active/replaced/completed/cancelled; kiểm owner, force priority, candidate correlation, terminal idempotency, result visibility, stale/session/provider failure và single location writer. UI typed-coordinate là một entrypoint USER dùng chung execution path.
- Tích hợp fort-walk ở lượt sau: đối chiếu kiến trúc đã chốt với native selector, bridge schema/encoder, Kotlin router/receiver và location service; khi thay protocol cập nhật hai phía đồng bộ. Test hiện có `app/src/test/java/dev/pogoroot/automation/runtime/NativeNavigationReceiverTest.kt:18-73` cover decode/kinds/malformed payload, chưa cover candidate policy force/owner.
- Không cần xác minh binding PoGo để chốt policy generic cho tọa độ đã có. Khi thay target selection/arrival của fort-walk, rà game-side lifecycle riêng; local Kotlin ARRIVED không chứng minh PoGo đã tới nơi.
- Generic candidate path được triển khai ở T-003…T-007; T-008/T-009 sau đó chuyển producer fort-walk sang observation type 12 và gỡ live Kotlin `NAVIGATION(11)` dispatch/lease bus. Native selector và game-state pause/arrival policy vẫn ở `catch_spin`; Kotlin admission/coordinator giữ một active route và controller tick mock-location. Không thêm UI tọa độ nhập tay; API USER `userWalkTo(target)` sẵn để reuse. Chi tiết nằm trong [candidate contract](candidate-contract.md) và [phase 3](checklists/phase-3-fort-walk.md).
- Verify sau tích hợp: focused legacy receiver regression, full Gradle suite, sáu native host checks và multi-ABI Magisk build đều pass; runtime scenario chưa chạy vì `bluestacks-smoke-test.sh` báo `no adb device connected` cho target cấu hình. Không tạo/sửa test code.

### Kết luận và câu hỏi còn mở

- Trả lời ở mức kiến trúc: **được**, Kotlin nhận A một lần, giữ candidate làm active route và tick mock location dần về A. Candidate B với `force=false` có thể bị bỏ qua trong khi A tiếp tục; khi idle thì candidate hợp lệ được nhận. Không cần lease/renewal.
- Khuyến nghị kiến trúc: tách producer candidate, Kotlin admission/priority, active-route ownership/lifecycle và location executor; `force` áp dụng lên active candidate theo owner policy; terminal phải target/session-correlated. UI có thể bổ sung nhập tọa độ bằng cách gọi cùng `USER` route path, không cần native protocol.
- Fort-walk hiện dùng one-shot candidate lifecycle; `NAVIGATION(11)`/lease receiver chỉ còn compatibility code để test hiện có và không dispatch live location. Native gửi terminal đúng candidate ID trước candidate kế tiếp; Kotlin giữ route đã nhận tới terminal/local arrival/provider/session/user transition.
- Còn mở ở mức kiến trúc: candidate decision có cần phản hồi cho native/UI hay chỉ giữ diagnostic nội bộ.
