# Contract kiến trúc: point-walk candidate

**Trạng thái:** contract generic đã được tích hợp vào producer/consumer fort-walk; device scenarios còn chờ BlueStacks Air 1
**Ngày:** 2026-09-25
**Phạm vi:** candidate tọa độ native → admission/ownership Kotlin → fake-location executor

## Ranh giới trách nhiệm

| Thành phần | Trách nhiệm | Không chịu trách nhiệm |
|---|---|---|
| Native candidate producer | Chọn tọa độ theo game state/eligibility; phát một candidate có identity và terminal khi cần dừng/xác nhận đến nơi | Tính bước fake-location, quyết định location owner hoặc điều khiển Android provider |
| Bridge ingress | Kiểm version, frame/payload bounds, runtime identity, session, capability, sequence và timestamp | Chọn tọa độ hay quyết định candidate nào thắng |
| Kotlin admission coordinator | Kiểm freshness/ownership; quyết định nhận, bỏ qua, thay thế, kết thúc; giữ active route trong RAM đến terminal/local arrival | Đọc raw game state, chọn candidate gameplay hay phát gameplay action |
| Kotlin location executor | Validate tọa độ lần cuối, tính walk step, cập nhật mock provider, dừng local walk và báo local arrival | Chọn candidate tiếp theo hoặc xác nhận server/game-side arrival |
| Native gameplay policy | Xác nhận readiness/eligibility và quyết định action dựa trên state game | Coi local Kotlin arrival là bằng chứng action/game arrival |

Candidate/route là state trong RAM theo runtime session, không persist. Kotlin nhận một candidate hợp lệ thì giữ route active và location controller tiếp tục tick từng bước tới target; không cần native gửi heartbeat/renewal để duy trì walk. Kotlin là authority duy nhất cho active location route và provider. Không thêm transport khác.

UI có thể mở rộng để người dùng nhập tọa độ trực tiếp. Input đó là route `USER`: UI parse/validate latitude/longitude rồi gọi `WalkCandidateCoordinator.userWalkTo(target)` (hoặc adapter hiện có `WalkCandidateLocationActions.userWalkTo`). Route đi qua cùng owner arbitration, executor, tick và mock-location writer như favorite/map walk; không đi qua native bridge và không tạo loop di chuyển riêng. Tọa độ nhập tay không được persist mặc định; lưu thành favorite nếu người dùng chủ động yêu cầu lưu.

## Dữ liệu và identity

- `candidateId`: ID ổn định cho một native route intent. Mỗi route mới phải có ID chưa từng dùng trong runtime session hiện tại; producer không tái sử dụng ID sau stop/arrival. Kotlin giữ tombstone session-scoped cho route từng được active và fail-closed nếu ID đó xuất hiện lại; proposal từng bị ignore trước khi active không bị tombstone. Gửi lại cùng ID/target khi route đang active là idempotent và không restart tick; dùng cùng ID với target khác bị từ chối. Target/force là dữ liệu proposal. ID rỗng hoặc quá giới hạn bị từ chối.
- `force`: chỉ cho phép proposal mới thay một active route có owner `NATIVE`; không vượt freshness, capability, session hoặc owner guard.
- `sessionId` và `sequence`: thuộc runtime event/frame hiện có; candidate mới phải khớp session đang ready và có sequence tăng nghiêm ngặt. Session mới reset mọi native-owned route trước khi nhận event của session mới.
- `generation`: Kotlin tăng đơn điệu mỗi lần location route được nhận/thay, gồm cả USER route; generation không reset khi session đổi để callback location cũ không thể khớp route mới. Native terminal trên wire mang session + candidate ID duy nhất; receiver chỉ gắn terminal với generation khi ID còn đúng active route. Policy sau đó kiểm tra session + ID + generation. ID không được tái sử dụng trong session nên late terminal không thể kết thúc route khác; không cần acknowledgement để native biết generation Kotlin.
- `observedAtNanos`: bridge timestamp dùng để kiểm freshness tại thời điểm nhận candidate. Sau khi candidate qua guards và được nhận, timestamp không còn hạn chế thời lượng route. Clock monotonic, thời gian tương lai hoặc quá cũ đều fail closed khi admission; không có lease hay renewal.
- `target`: latitude/longitude hữu hạn và trong miền hợp lệ. Kiểm tra lại ở ingress/policy boundary; lỗi không dừng hoặc thay một route khác bằng tọa độ sai.
- `owner`: `NATIVE` cho route được admission coordinator nhận; `USER` cho joystick, teleport đang thao tác, favorite/manual walk, map target hoặc tọa độ người dùng nhập trong UI. Không suy owner từ vị trí hiện tại. Nguồn cụ thể (typed/favorite/map/joystick) có thể dùng cho UI/diagnostic, nhưng quyền arbitration vẫn là `USER`.

Route được nhận một lần và tồn tại trong RAM tới một terminal condition: Kotlin xác nhận local arrival, native gửi STOP/ARRIVED khớp route, candidate force mới thay route, thao tác USER chiếm location, provider/executor lỗi, hoặc runtime session bị reset/disconnect. Candidate mới không-force khi A active bị bỏ qua; A tiếp tục tick tới target. Candidate không được replay sau khi bị ignore/reject.

## Candidate decisions

Admission trả một `CandidateDecision` nội bộ để coordinator và diagnostics phân biệt kết quả. Contract hiện không yêu cầu gửi acknowledgement về native hay đưa quyết định vào UI.

| Decision | Ý nghĩa / state effect |
|---|---|
| `ACCEPTED` | Candidate hợp lệ được nhận khi không có native route active; tạo generation rồi gọi executor |
| `IGNORED_BUSY` | Candidate khác không-force tới khi native route active; giữ nguyên route đang chạy |
| `REPLACED` | Candidate force hợp lệ thay native route active; vô hiệu generation trước, commit route mới nguyên tử rồi cập nhật executor |
| `UNCHANGED` | Candidate ID/target/session khớp active native route; giữ route và generation, không restart executor |
| `REJECTED_OWNER_CONFLICT` | Có route USER; candidate native bị từ chối dù force=true |
| `REJECTED_STALE` | Timestamp/sequence/session không hợp lệ hoặc event cũ |
| `REJECTED_INVALID` | Version, kind, ID, coordinate, capability hoặc payload validation lỗi |
| `IGNORED_STALE_TERMINAL` | Terminal wire không khớp session + unique candidate ID active, hoặc callback nội bộ không khớp generation; state không đổi |
| `COMPLETED` / `CANCELLED` | Terminal đúng route active; xóa active native route và dừng fake movement tương ứng |

Reject/ignore không được tự stop hoặc thay đổi route active. Executor/provider failure kết thúc route native hiện hành ở Kotlin theo cùng identity, dừng provider an toàn và không tự thử candidate khác. Local arrival sinh terminal `COMPLETED` cho local route/UI; nó không gửi gameplay action và không tự khẳng định PoGo đã tới nơi.

## Wire projection v1

`ObservationType.POINT_WALK_CANDIDATE = 12`, observation payload version `1`; Kotlin capability token là `POINT_WALK_CANDIDATE`. Session, event sequence, observed timestamps, runtime identity và capability vẫn thuộc bridge envelope/status, không lặp trong inner payload.

Payload big-endian: `u32 magic = 0x504F4743` (`POGC`), `u32 kind` (`1=WALK`, `2=STOP`, `3=ARRIVED`), `u32 flags` (`bit 0 = force`, mọi bit khác zero-invalid), `u32 candidateIdByteLength`, UTF-8 candidate ID (1…128 bytes); `WALK` có thêm `f64 latitude`, `f64 longitude`. `STOP`/`ARRIVED` không có tọa độ và flags phải zero. Payload phải kết thúc đúng sau field cuối; unknown version/kind/flag, malformed UTF-8, size/string/coordinate sai hoặc trailing bytes fail closed. Terminal wire identity là envelope session + unique candidate ID; Kotlin gắn generation nội bộ chỉ khi ID còn là route active.

## State transitions

`IDLE` không có active native route. USER route được theo dõi như owner riêng, có quyền ưu tiên. `ACTIVE(NATIVE, A, session, generation)` là route native duy nhất mà executor có thể chạy; sau admission, location controller tick dần tới target mà không cần candidate lặp lại. Mỗi hàng là một event; event không đạt guard bị từ chối và không side-effect lên route khác.

| State trước | Input và guard | Owner/route lifetime/result | State sau và side effect |
|---|---|---|---|
| `IDLE`, không có USER route | Candidate mới hợp lệ, capability/session/sequence/freshness/coordinate đều đạt; `force` bất kỳ | Kotlin nhận owner `NATIVE`, generation mới; `ACCEPTED` | `ACTIVE(NATIVE, candidate)`; sau commit gọi executor với target; controller tick tới target |
| `ACTIVE(NATIVE, A)` | Candidate B có ID khác, `force=false`, guards đạt | A giữ owner, generation và target; B=`IGNORED_BUSY` | `ACTIVE(A)` tiếp tục tick tới target |
| `ACTIVE(NATIVE, A)` | Candidate B có ID/target/session giống A và event còn fresh | A giữ owner/generation/target; `UNCHANGED` | `ACTIVE(A)`; executor không restart hoặc reset tick |
| `ACTIVE(NATIVE, A)` | Candidate B khác A, `force=true`, mọi guard đạt | Commit B với generation mới rồi vô hiệu A; `REPLACED` | `ACTIVE(B)`; executor nhận B sau commit. Terminal A không còn hiệu lực |
| `ACTIVE(USER, U)` | Candidate native, force bất kỳ | USER giữ owner; `REJECTED_OWNER_CONFLICT`; candidate không chiếm location | `ACTIVE(USER, U)` không đổi |
| `IDLE` hoặc route khác | User gửi tọa độ đã nhập và validate hợp lệ | Tạo owner `USER`, generation mới; dùng cùng location executor | `ACTIVE(USER, U)`; controller tick từng bước tới target |
| `ACTIVE(NATIVE, A)` | Candidate stale/future, sequence lùi, sai session/capability hoặc invalid | `REJECTED_STALE`/`REJECTED_INVALID`; không nhận B và không đổi A | A tiếp tục tick tới target |
| `ACTIVE(NATIVE, A)` | `STOP`/`ARRIVED` wire khớp session + unique candidate ID hoặc local arrival xác nhận target trong tolerance | Terminal đúng route; `CANCELLED` hoặc `COMPLETED`; chỉ kết thúc A | Dừng fake movement A rồi `IDLE`; local arrival chỉ UI/location |
| `ACTIVE(NATIVE, B)` | STOP/ARRIVED wire của A không khớp unique ID hoặc callback A có generation cũ | `IGNORED_STALE_TERMINAL`; không đổi route/owner | B tiếp tục `ACTIVE` |
| Bất kỳ native state | Session disconnect/reset hoặc runtime identity đổi | Invalidate native generation; dừng native-owned movement fail-closed | `IDLE`; không khôi phục từ persistence |
| `ACTIVE(NATIVE, A)` | User bắt đầu joystick/teleport/favorite/map walk | User input preempt; vô hiệu A generation; native route không được force lại nếu USER còn active | `ACTIVE(USER, U)`; controller vẫn là single location writer |
| `ACTIVE(USER, U)` | User explicit stop/replace hoặc manual target kết thúc | User location controller cập nhật owner; candidate native chỉ được nhận từ event mới còn fresh | `IDLE` hoặc `ACTIVE(USER, U2)`; không replay candidate đã bị reject |
| Bất kỳ state | Invalid payload/version/capability/coordinate hoặc location provider không sẵn sàng | `REJECTED_INVALID` hoặc executor failure; không cho native chiếm/tiếp tục movement | Giữ USER route nếu an toàn; nếu native route không thể thực thi thì dừng route native và về `IDLE` |

Terminal xử lý idempotent: terminal trùng route đã kết thúc không có tác động. Native STOP/ARRIVED wire mang envelope session + candidate ID duy nhất; Kotlin tìm route active cùng session/ID rồi gắn generation hiện hành trước khi gọi policy. Location tick và provider callback phải khớp route generation/revision hiện hành trước side effect. Nếu native đổi session, session cũ không thể mở lại route. Nếu event mới không thể được phân biệt với session hiện hành, ingress từ chối.

## Policy invariants

1. Tại một thời điểm chỉ có một writer mock-location: `JoystickLocationController`/location executor hiện hữu.
2. Candidate không-force được nhận khi idle; candidate B không-force không thay A; A tiếp tục đến terminal/local arrival.
3. Force thay native-owned route atomically; không chiếm USER route hoặc vượt qua guard.
4. Native stop/arrival wire được tương quan bằng session + unique candidate ID; Kotlin location/provider callbacks cũng phải khớp generation nội bộ.
5. Invalid/stale events không publish target và không dừng route khác.
6. Kotlin local arrival chỉ kết thúc fake movement và cập nhật location/UI state; native tự quyết định game-side eligibility.
7. Không có implicit replay sau reject, session reset, provider failure hoặc USER preemption; native cần gửi event mới còn fresh.

## Quyết định mở được giữ theo default

- **Q-01 — Force so với USER:** default đã chọn cho implementation này là `force=true` không hủy route `USER`; AC-04 giữ nguyên. Thao tác USER tiếp tục được ưu tiên.
- **Q-02 — Native acknowledgement:** chưa thêm acknowledgement. Các quyết định trên chỉ nội bộ Kotlin/diagnostics; nếu producer sau này cần phản ứng theo accept/reject, phải thiết kế feedback riêng và version hóa.
- Candidate decision không phải gameplay result. `ACCEPTED` không khẳng định provider đã đi tới nơi; `COMPLETED` chỉ khẳng định Kotlin hoàn tất local walk.
- Observation router giữ cursor sequence tăng nghiêm ngặt và tiêu thụ sequence ngay sau khi envelope qua identity/session/freshness/order guards, trước payload decode và admission. Vì vậy payload sai version, candidate bị reject hoặc bị ignore không được replay; policy nhận sequence của observation trước đó làm cursor so sánh, còn ingress giữ sequence hiện tại kể cả khi admission từ chối.

## Tách biệt với fort-walk

Đây là contract tổng quát cho tọa độ đã có; T-008/T-009 đã migrate native producer và Kotlin consumer fort-walk sang contract này. Native vẫn chọn fort theo game state, giữ pause/resume policy, và phát target-correlated `STOP`/`ARRIVED`; Kotlin admission/coordinator giữ một route đã nhận và location controller tick fake-location tới target. Không thêm PoGo binding. Legacy `NAVIGATION(11)` codec/receiver còn trong source để tương thích regression test, nhưng không còn live dispatch location.

## Implementation map đã xác nhận trong checkout

Các đường dẫn dưới đây được đối chiếu với source, `.github/workflows/ci.yml` và `zygisk/tests/`. Candidate files nền tảng được thêm trong T-003…T-007, producer/consumer fort-walk được tích hợp trong T-008/T-009.

| Boundary | File hiện hữu và vai trò | File dự kiến / include dependency |
|---|---|---|
| Core policy | `WalkCandidate.kt` và `WalkCandidatePolicy.kt` thêm ở T-003; `GeoPoint.kt` là tọa độ domain | Pure admission, ownership, one-shot active-route lifetime và terminal correlation |
| Kotlin payload | `BridgeProtocol.kt` giữ discriminator; `RuntimeNavigationPayloadCodec.kt` và type 11 còn để regression compatibility | `PointWalkCandidatePayloadCodec.kt` dùng cho live fort route; type 11 bị router bỏ qua cho location |
| Kotlin ingress | `RuntimeUiAutomationFacade` sở hữu `RuntimeUiEventRouter`; router kiểm session/runtime identity, observation sequence, freshness và giới hạn message rồi dispatch MAP_TARGET/candidate | `NativeWalkCandidateReceiver.kt` (T-006); candidate callback vào `WalkCandidateCoordinator` |
| Location | `JoystickLocationController` ghi mock location và tick target | `WalkCandidateCoordinator.kt` giữ owner/lifecycle; `WalkCandidateLocationActions.kt` là executor cho native candidate và USER walk/favorite/map/typed-coordinate entry |
| Native payload/forwarder | `runtime_observation_protocol.h` định nghĩa observation envelope; `runtime_navigation_protocol.h` định nghĩa observation 11/version 1; `runtime_navigation_bridge.inc` đóng runtime event envelope | `runtime_walk_candidate_protocol.h` và `runtime_walk_candidate_bridge.inc` đã thêm ở T-005 cạnh navigation tương ứng |
| Native broker | `runtime_bridge_broker.inc` parse envelope rồi dispatch theo discriminator/version; `runtime_bridge_observation_protocol.inc` định nghĩa parser/forwarding helpers | Candidate discriminator branch đã thêm; `main.cpp` include protocol headers trước `.inc`, bridge `.inc` sau envelope helpers và trước broker. Giữ thứ tự này và một translation unit |
| Native fort feature | `RuntimeAutoFortWalkCandidateProducer` trong `auto_fort_navigation.h`; `navigation.inc` phát candidate; `modules/catch_spin/coordinator.inc` gọi update/pause trong feature tick | Giữ fort target selection và game eligibility native; mỗi target phát một WALK ID, rồi candidate-correlated STOP/ARRIVED kết thúc route |

### Wiring và test/build entrypoints

- APK live path: `HeadlessAutomationService.onCreate` tạo `NativeWalkCandidateReceiver` và `RuntimeUiAutomationFacade`; facade tạo `RuntimeUiEventRouter` với callback candidate/reset. Candidate callback vào coordinator. Generic user walks gọi `WalkCandidateLocationActions.userWalkTo(target)`; UI nhập tọa độ sau này có thể reuse API đó sau validation. `AutoFortNavigationBus` đã xóa; `NativeNavigationReceiver` không được production service tham chiếu.
- Capability có runtime meaning được quảng bá trong `zygisk/jni/shared/bridge_kotlin/runtime_ui_status_bridge.inc` (`runtime_ui_status_capabilities`) và baseline `zygisk/jni/host/runtime_capabilities.inc`. Observation discriminator/version routing nằm riêng ở `runtime_bridge_broker.inc`; candidate protocol chỉ được xử lý khi ingress session ready và capability contract cho phép.
- App regression test tồn tại: `app/src/test/java/dev/pogoroot/automation/runtime/NativeNavigationReceiverTest.kt`; focused command từ checklist: `./gradlew :app:testDebugUnitTest --tests dev.pogoroot.automation.runtime.NativeNavigationReceiverTest --rerun-tasks`.
- Kotlin modules có task hiện hành `./gradlew :core:test`, `./gradlew :bridge:protocol:test`; full CI chạy `./gradlew test assembleDebug`.
- Native host tests thực tế trong `.github/workflows/ci.yml` là `zygisk/tests/runtime_readiness_retry_test.cpp`, `runtime_auto_fort_navigation_test.cpp`, `runtime_automation_event_protocol_test.cpp`, `runtime_automation_subject_names_test.cpp`, `runtime_discard_filter_factory_test.cpp`, và `runtime_discard_preparation_test.cpp`. CI biên dịch/chạy từng file với `c++ -std=c++17 -Wall -Wextra -Werror -Izygisk/jni` (protocol test riêng dùng include path bridge theo workflow). `runtime_auto_fort_navigation_test.cpp` là focused regression đã có cho T-008/T-009.
- `AGENTS.md` nhắc `zygisk/jni/runtime_command_protocol_test.cpp` và `runtime_observation_protocol_test.cpp`; cả hai không tồn tại trong checkout và không được gọi. Không có candidate-specific native host test hiện tại; không thêm test code theo repo rule. `./scripts/build-magisk.sh` là entrypoint native package build được yêu cầu cho phase.
- Sau migration, `JoystickOverlayService.kt` có 464 dòng và `catch_spin/coordinator.inc` 474 dòng; cả hai dưới giới hạn 500.
