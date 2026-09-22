# Brainstorm: Kotlin chỉ phục vụ overlay/UI

**Loại:** architecture
**Ngày phân tích đầu tiên:** 2026-09-22
**Phạm vi game/build/ABI:** rà source framework hiện tại; chưa xác minh runtime trên device. Không bổ sung binding game.

> Cập nhật phạm vi: người dùng cho phép Kotlin sở hữu fake location, teleport
> và walk-to-location. Các kết luận ở lượt đầu yêu cầu chuyển movement khỏi
> Kotlin đã bị thay thế bởi **Tiếp nối 1** cuối tài liệu. Bằng chứng code vẫn
> dùng được; việc movement nằm trong Kotlin không còn là vi phạm kiến trúc.

## Bước 1 — Yêu cầu và phạm vi

- Yêu cầu người dùng: xem Kotlin hiện còn nắm việc gì; đích mong muốn là Kotlin
  chỉ vẽ overlay/nhận thao tác UI, không sở hữu logic xử lý với process Pokémon GO.
- Trigger phân tích: tài liệu vừa cập nhật vẫn giao control loop, runtime
  lifecycle, config synchronization và movement cho Kotlin
  (`docs/ARCHITECTURE.md:110`, `AGENTS.md:43`).
- Phạm vi: kiểm tra code đang được app gọi, phân biệt với class chỉ còn tồn tại;
  liệt kê công việc UI, transport/Android plumbing và logic runtime/gameplay.
- Đầu ra: bằng chứng theo file/method, khoảng cách với yêu cầu và hướng tách
  trách nhiệm. Đây là lượt review kiến trúc, chưa triển khai refactor runtime.
- Ràng buộc: không viết/sửa test, giữ guards, freshness và fail-closed; không
  sử dụng device/ADB để thay cho việc đọc source.
- Ưu tiên: trách nhiệm rõ ràng, không hai nơi điều phối gameplay, duy trì hành
  vi khi chuyển từng phần. Không có yêu cầu thay ngôn ngữ render Android.
- Điểm cần phân biệt: bridge nhận/gửi dữ liệu, lưu setting và Android location
  provider không đồng nghĩa gọi API PoGo. Tuy vậy, chúng cũng vượt nghĩa đen
  “chỉ vẽ UI”; sẽ ghi rõ phần giữ lại dưới dạng adapter và phần cần chuyển owner.
- Tài liệu catch/spin-native cũ chỉ bao phủ một feature; chủ đề này kiểm tra toàn
  bộ boundary Kotlin nên dùng issue riêng.

**Kết luận bước 1:** lấy yêu cầu mới làm kiến trúc đích; mô tả ownership trước đó
chỉ dùng làm ảnh chụp hiện trạng, không mặc định đã đáp ứng “Kotlin chỉ UI”.

## Bước 2 — Bằng chứng từ reverse

Không áp dụng cho lượt audit này: không xác định class/method/field/RVA hay
implementation của Pokémon GO. Câu hỏi cần trả lời là framework Kotlin đang
gọi gì, nên bằng chứng cần lấy từ entry point/call site của framework.

- Không tra hay gọi thử method game; không suy luận game behavior từ tên method.
- Bộ reverse mặc định được AGENTS chỉ định là PoGo 0.427.0, version code
  2026082702, arm64-v8a; lượt này không đối chiếu artifact/build đang chạy.
- Nếu phương án sau này thay location bằng game binding, đó là việc mới cần
  reverse và xác minh riêng; không coi audit này là bằng chứng cho binding đó.

**Kết luận bước 2:** có thể kiểm tra boundary ngôn ngữ/process bằng source
framework, không phụ thuộc reverse hoặc device evidence.

## Bước 3 — Function/method, param và hành vi

Các symbol dưới đây đều là **framework**, không phải method game. Trạng thái
**Đã xác nhận từ source** chỉ xác nhận code/call site, không khẳng định đã chạy
trên device trong lượt review này. Đường dẫn `app/...` trong bảng viết tắt cho
`app/src/main/java/dev/pogoroot/automation/...`; `pogo/...` viết tắt cho
`game-adapter/pogo/src/main/kotlin/dev/pogoroot/automation/pogo/...`.

### Control loop và readiness — đang được service khởi tạo

| Owner / signature | Param và nguồn | Hành vi / kết quả / lỗi | Bằng chứng |
|---|---|---|---|
| `HeadlessAutomationService.onCreate()` | Không param; callback Android | Tạo bridge, coordinator, router, navigation receiver, engine, HTTP server; gọi `engine.start()` | `app/service/HeadlessAutomationService.kt:47`, `:92` |
| `HeadlessAutomationEngine.private fun runLoop()` | Không param; dùng repository và state của engine | Worker lặp đọc config, quyết định idle/run, retry STOP lỗi sau 700 ms, chạy cycle theo interval. Trả `Unit` khi loop kết thúc | `app/engine/HeadlessAutomationEngine.kt:89` |
| `RuntimeLifecycleCoordinator.fun ensureRunning(config: HeadlessAutomationConfig): Result<BridgeEvent.RuntimeReady>` | `config`: snapshot từ repository, chứa enabled/armed/revision và feature settings | Connect, nhận biết đổi session, gửi START, giữ state RUNNING, kiểm tra ready rồi sync ba config; lỗi qua `recordFailure` | `app/runtime/RuntimeLifecycleCoordinator.kt:56` |
| `RuntimeLifecycleCoordinator.fun ensureIdle(): Result<Unit>` | Không param; dùng state/bridge hiện tại | Quyết định gửi STOP, reset state/cache; lỗi trả qua `Result` | Cùng file `:92` |
| `RuntimeLifecycleCoordinator.fun pushRuntimeConfigs(config: HeadlessAutomationConfig): Result<Unit>` | Snapshot từ UI/API qua engine | Chỉ gửi khi RUNNING và managed ready; ngoài điều kiện thì no-op | Cùng file `:156` |
| `CatchSpinConfigDispatcher.fun sync(config: HeadlessAutomationConfig, armed: Boolean): Result<Unit>` | `config`: repository; `armed`: ý định người dùng | Map config, dedupe theo session/nội dung, gửi config và chỉ ghi applied cache sau thành công; không có ready thì fail | `app/runtime/CatchSpinConfigDispatcher.kt:20` |

Các method coordinator/dispatcher là instance, có synchronization; engine gọi
trên worker riêng. Native vẫn tự discovery và bật module; Kotlin vẫn quyết định
thời điểm START, STOP, gửi config và trạng thái readiness của phía controller.

### Xử lý game-state payload — còn trong đường đọc live

| Owner / signature | Param và nguồn | Hành vi / kết quả / lỗi | Bằng chứng |
|---|---|---|---|
| `RuntimeObservationRouter.fun tick(): Result<RuntimeObservationTick>` | Không param; đọc bridge | Gọi `source.refresh()`, lấy capability, lọc seq, route event, map target/navigation; disconnect/reset khi binding lost/lỗi | `app/runtime/observation/RuntimeObservationRouter.kt:40` |
| `BridgePogoRuntimeSource.fun refresh(): Result<Unit>` | Không param; event queue từ bridge | Sort seq, kiểm tra session/identity, gọi `consume`, đưa event vào pending queue | `pogo/BridgePogoRuntime.kt:161` |
| `BridgePogoRuntimeSource.private fun consumeObservation(event: BridgeEvent.ObservationEvent)` | `event`: message hợp session/identity; có type, payload/version và timestamp | Decode/cache nearby, encounter, forts, inventory, catch-spin bundle; có nhánh suy lifecycle OVERWORLD/ENCOUNTER từ observation; giữ lỗi decode | Cùng file `:226` |

Router tạo `PogoGameAdapter(source, actionExecutor = null)` tại dòng 33.
Không có executor mutation được gắn vào live router. Tuy nhiên raw-game-state
interpretation vẫn thực sự được gọi qua `source.refresh()`, kể cả router chỉ
dùng event để hiển thị. Không được kết luận tất cả code adapter này là code chết.

### Movement — Kotlin đang tính toán và thực thi

| Owner / signature | Param và nguồn | Hành vi / kết quả / lỗi | Bằng chứng |
|---|---|---|---|
| `NativeNavigationReceiver.fun receive(payload: RuntimeNavigationPayload, observedAtNanos: Long)` | `payload`: native walk/stop/arrived; thời gian quan sát monotonic ns từ bridge | Reject timestamp quá hạn/tương lai, lease 5 giây; gửi command sang bus, tạo toast; `Unit` | `app/location/NativeNavigationReceiver.kt:18` |
| `JoystickOverlayService.private fun applyPendingMapTarget()` | Không param; repository + config + controller | Kiểm tra enabled/provider/point, lấy và xóa target rồi `controller.walkTo` | `app/overlay/JoystickOverlayService.kt:417` |
| `JoystickOverlayService.private fun applyAutoFortNavigationCommand()` | Không param; latest command từ bus | Gọi `walkTo` hoặc `stopWalking`; code xử lý navigation nằm ngay trong service overlay | Cùng file `:432` |
| `JoystickLocationController.fun walkTo(target: GeoPoint, toleranceMeters: Double = JoystickLocationState.DEFAULT_WALK_TOLERANCE_METERS)` | Target từ fort/map tap/favorites; tolerance mặc định 8 m, hữu hạn ≥ 0; tọa độ hợp lệ | Tính khoảng cách, quyết định ARRIVED/WALKING, giữ target và state; thiếu vị trí hiện tại thì ERROR; `Unit` | `app/location/JoystickLocationController.kt:106` |
| `JoystickLocationController.private fun tick()` | Không param; dùng state, elapsed time, joystick strength và speed | Tick 50 ms; tính bước bằng `WalkPlanner.step`/`GeoMath.destination`, quyết định arrival, publish vị trí | Cùng file `:74`, `:222` |
| `JoystickLocationController.fun teleport(point: GeoPoint)` | Điểm từ thao tác UI; lat/lon hợp lệ | Cập nhật state, gọi sink khi ready, tính teleport cooldown sau publish thành công | Cùng file `:175` |
| `RootMockLocationProvider.override fun publish(point: GeoPoint, speedMetersPerSecond: Float, bearingDegrees: Float): Result<Unit>` | Điểm, vận tốc m/s, hướng độ từ controller | Ghi GPS/network test provider của Android; yêu cầu provider đã start và tọa độ hợp lệ | `app/location/RootMockLocationProvider.kt:55` |

`JoystickOverlayService` tạo controller/provider tại dòng 111. Movement chạy
trên executor của controller; overlay xử lý UI trên main thread. Đây là logic
đang dùng, vượt render UI, nhưng đi qua Android location chứ không invoke
Unity/PoGo trực tiếp. Cooldown ở đây là mô hình app hiển thị, không phải bằng
chứng cooldown authoritative từ server game.

### UI, config, transport và phần còn tồn tại nhưng chưa thấy caller live

- `RuntimeAutomationEventPayload.toAutomationEvent(): AutomationEvent?`
  (`app/runtime/observation/RuntimeAutomationEventMapper.kt:9`): extension nhận
  native payload, tạo text/type cho toast; unknown trả null. Đây là presentation.
- `AutomationConfigRepository.update(transform: (HeadlessAutomationConfig) ->
  HeadlessAutomationConfig): HeadlessAutomationConfig`
  (`app/config/AutomationConfig.kt:133`): callback từ UI/API, tăng revision,
  persist setting. `toRuntimeTransferConfig()` còn hardcode `keepUnknownIv`,
  `keepLegendary`, `keepMythical` thành true (`AutomationPolicyBridge.kt:69`).
  Đây là default policy trong Kotlin, dù native mới thực hiện quyết định transfer.
- `AutoFortNavigationBus.publish(command: AutoFortNavigationCommand, expiresAt:
  Long)` (`app/location/AutoFortNavigationBus.kt:18`): giữ command, timeout lease
  bằng Handler; hết hạn phát Stop. Cần bảo toàn guard này khi chuyển owner.
- `JoystickAutoStartCoordinator.sync()` (`app/service/JoystickAutoStartCoordinator.kt:31`):
  dựa UsageStats foreground + overlay permission để start/stop joystick service.
  Đây là Android/UI lifecycle, không phải kiểm tra Unity readiness.
- Tìm call site trong `app/src/main`, `core/src/main`, `game-adapter/pogo/src/main`
  không thấy app khởi tạo `AutomationRunner`, `AutomationCoordinator`,
  `BridgeBackedPogoActionExecutor`; chúng vẫn tồn tại trong source/library.
  Không gọi chúng là vòng gameplay đang chạy.
- Không thấy caller production cho `ScanResultRepository.recordEncounter()`
  hoặc khởi tạo `RuntimeStatusRepository` trong `app/src/main` ở snapshot này.
  Overlay có đọc kết quả scan; không đủ bằng chứng nói Kotlin scanner còn chạy.

**Kết luận bước 3:** Kotlin chưa chỉ UI: control loop/readiness/config sequencing,
raw observation interpretation và movement đang nằm trong đường thực thi app.
Live catch/spin/discard/transfer executor không được gắn vào router Kotlin.

## Bước 4 — Luồng hành vi và điểm tích hợp

### Luồng đang tồn tại — đã xác nhận từ call site framework

```text
Overlay/settings/HTTP API
  → lưu config / đổi enabled
  → HeadlessAutomationEngine.runLoop()
  → AutomationCycle.run(config)
  → RuntimeLifecycleCoordinator.ensureRunning(config)
  → connect / START / chờ managed ready / gửi 3 config
  → RuntimeBridgeClient → socket → companion → runtime native

Native observation/event
  → RuntimeBridgeClient event queue
  → RuntimeObservationRouter.tick()
  → BridgePogoRuntimeSource.refresh() / consumeObservation()
  → decode + cache game snapshot + suy lifecycle
  → toast/status hoặc navigation/map target
  → JoystickOverlayService → JoystickLocationController.tick()
  → WalkPlanner / GeoMath → RootMockLocationProvider → Android location
```

- `AutomationCycle.run(config: HeadlessAutomationConfig): Unit` tại
  `app/src/main/java/dev/pogoroot/automation/engine/AutomationCycle.kt:16`
  nhận repository snapshot từ engine; chỉ gọi router sau `strongIdentityVerified`.
- `RuntimeBridgeClient.connect(): Result<BridgeEvent.RuntimeReady>` tại
  `app/src/main/java/dev/pogoroot/automation/root/RuntimeBridgeClient.kt:79`
  đăng ký UID bằng root, nối abstract socket, tạo reader và đợi ready. Đây là
  IPC/bootstrap, không attach/hook trực tiếp process game.
- `RuntimeBridgeClient.startRuntime(): Result<Unit>` tại cùng file `:133`
  gửi START; `stopRuntime()` tại `:139` gửi STOP; chúng không tự invoke game.
- Native `bool runtime_managed_configs_ready()` (không param) tại
  `zygisk/jni/shared/runtime/module/runtime_managed_config.inc:2` chỉ true khi
  đủ ba config có cùng revision > 0. Đây là config gate phía native.
- Native `void sync_auto_enabled_modules(ProbeContext &context)` tại
  `zygisk/jni/shared/runtime/module/runtime_module_autostart.inc:40` nhận
  context của runtime, kiểm tra config/map ready rồi bật/tắt module. Nó là
  nơi quyết định activation feature thực tế; Kotlin không thay thế guard này.
- `bool activate_runtime(ProbeContext &context, const char *request_id)` tại
  `zygisk/jni/shared/runtime/control/runtime_control.inc:49` nhận context và
  ID tương quan của request; START khởi động readiness retry và reset config.
  `stop_runtime(...)` tại `:103` dừng/reset. Không được xóa luồng gửi lại config
  ở Kotlin trước khi đã có owner mới vì START hiện làm mất mirror cũ.
- `run_runtime_catch_spin_observer_tick(ObserverTickContext &tick)` tại
  `zygisk/jni/modules/catch_spin/coordinator.inc:237` dùng tick context native,
  xử lý pending result/delay/snapshot và chọn catch/spin trong cùng runtime.
  Đây là source framework native, không phải chứng minh ABI của một method game.
- `RuntimeMainThreadBridge.java` (`app/.../root/RuntimeMainThreadBridge.java:14`)
  chỉ có `public static boolean post(long token)` và native callback; token
  opaque, không chứa quyết định gameplay. Native đăng ký callback tại
  `zygisk/jni/shared/runtime/mainthread/runtime_main_thread_bridge.inc:384`.
  Dù file ở `app/`, helper được load vào process game; không được nhầm nó với
  Kotlin controller hoặc xóa theo vị trí thư mục đơn thuần.

### Khoảng cách và rủi ro migration

| Phần | Đánh giá so với yêu cầu | Rủi ro nếu chỉ xóa code Kotlin |
|---|---|---|
| Engine + lifecycle coordinator | Vượt UI; đang sở hữu trình tự vận hành runtime | Runtime không nhận START/config hoặc không STOP đúng khi có lỗi |
| Adapter raw game-state | Vượt UI; source đọc live vẫn parse/cache/suy state | Mất status/event hoặc bỏ sót session/identity/seq checks nếu bỏ toàn bộ adapter |
| Movement + map target trong overlay | Vượt UI; có state machine và tác động location | Mất walk/teleport, lease timeout hoặc để provider sống sau stop |
| Settings/persistence/IPC | Hạ tầng phục vụ UI; có thể làm mỏng | Tạo hai config authority hoặc mất request/result correlation khi đổi owner |
| Toast/render/foreground visibility | Phù hợp UI | Chuyển sang game process làm tăng phụ thuộc Android/Unity không cần thiết |
| Planner/executor cũ không gắn live | Code tồn dư cần tách/giảm dependency | Xóa theo tên có thể làm hỏng model/codec dùng chung; phải kiểm tra caller |

Không phát hiện đường controller Kotlin trực tiếp giữ pointer game hay gọi
IL2CPP trong call chain đã kiểm tra. Tuy nhiên “không hook trực tiếp” chưa đáp
ứng yêu cầu “chỉ UI”: orchestration và diễn giải game state vẫn nằm ở Kotlin.

**Kết luận bước 4:** ba phần cần thay ownership rõ nhất là runtime orchestration,
raw observation processing và movement execution; native gameplay coordinator
đã có. Không cần di chuyển lại catch/spin executor từ Kotlin vì live router
hiện không gắn executor đó.

## Bước 5 — Hướng xử lý

### Kiến trúc đích theo yêu cầu người dùng

```text
Kotlin overlay/UI
  - render state, settings, toast
  - nhận thao tác và gửi ý định người dùng
             ↕ client IPC mỏng
Native/backend
  - nhận desired config và lệnh người dùng
  - lifecycle/readiness/config application/retry
  - game-state interpretation và toàn bộ gameplay
  - movement state machine; phát state đã chuẩn bị cho UI
             ↕ adapter hệ điều hành nếu cần
Android APIs / Pokémon GO runtime binding theo đúng boundary
```

Kotlin vẫn cần lifecycle Android để hiển thị overlay, nhận input và trao đổi
message. “Chỉ UI” không có nghĩa bỏ kiểm tra frame lỗi, peer/session hoặc bỏ
timeout an toàn. Những kiểm tra transport này phải còn ở boundary, nhưng
Kotlin không tự dựng OVERWORLD/ENCOUNTER hoặc chọn action từ raw game state.

| Hạng mục | Hướng xử lý đề xuất | Lý do / điều kiện |
|---|---|---|
| `HeadlessAutomationEngine.runLoop`, `RuntimeLifecycleCoordinator.ensureRunning/ensureIdle` | Chuyển state machine vận hành sang native/backend. App gửi ý định enable/disable và nhận trạng thái thực tế | Native là owner readiness/retry và thứ tự apply config; UI không poll để điều phối tiến trình game |
| Config dispatchers + `toRuntimeTransferConfig()` | Client chỉ gửi config người dùng và nhận ACK; backend chịu trách nhiệm đợi ready, apply đồng bộ, default policy và phản hồi revision | Không để vòng sống game phụ thuộc trình tự ba dispatcher trong Kotlin; vẫn giữ một config authority |
| `BridgePogoRuntimeSource.consumeObservation`, router/adapter | Native gửi dữ liệu đã chuẩn bị cho UI: status, event, dòng scan, vị trí/cooldown hiển thị; app chỉ decode contract đó | Loại game snapshot cache, protobuf fallback, suy lifecycle và dependency `game-adapter:pogo` khỏi đường UI |
| `JoystickLocationController`, map-target/navigation trong overlay | Chuyển tính bước đi, arrival, ưu tiên joystick/auto-walk/teleport và cooldown calculation sang backend movement; UI chỉ gửi input | Đáp ứng UI-only cả trên đường location. Chưa chọn host/process cuối cùng của backend movement |
| `RootMockLocationProvider` | Tách thành adapter Android thuần: thực thi lệnh location đã quyết định, báo success/error, giữ cleanup/freshness | Đây là Android API, không game binding. Nếu yêu cầu tuyệt đối mọi Kotlin ngoài UI biến mất, cần thiết kế cầu nối native ↔ Android riêng; không thể chỉ copy code vào Zygisk game module |
| `RuntimeBridgeClient` | Giữ vai trò client mỏng: connect, serialize, correlation, nhận state/event; bỏ quyết định managed lifecycle khỏi client | IPC là hạ tầng cho UI; không thêm kênh gameplay thứ hai |
| Render, settings, favorites, overlay position, toast | Giữ Kotlin; repository setting/UI có thể giữ theo persistence hiện tại | Lưu lựa chọn người dùng không phải game execution. Chưa có yêu cầu đổi kho dữ liệu, không tự thêm native database |
| Core planner/runner/action executor không có live caller | Tách khỏi dependency của UI rồi đánh giá dọn code tồn dư | Không nhầm tồn tại source với runtime đang hoạt động; không viết/sửa test để thực hiện cleanup |

Tên “native/backend movement”, “dữ liệu đã chuẩn bị cho UI” là **đề xuất chưa
có implementation/contract mới**. Lượt này không tạo API giả, method game hay
RVA. Signature/wire fields mới cần xác định trong thiết kế migration sau khi
chốt host của movement; API hiện có được ghi tại bước 3–4.

### Các lựa chọn thực sự

1. Giữ native gameplay + Kotlin controller như hiện tại: ít thay đổi nhưng
   **không đạt yêu cầu UI-only**; không chọn làm kiến trúc đích.
2. Kotlin UI + client/persistence/Android adapter mỏng; native/backend sở hữu
   toàn bộ policy/state machine: hướng phù hợp để loại logic PoGo khỏi Kotlin
   mà vẫn dùng Android API qua adapter rõ ràng. Cần công nhận adapter Android
   là phần hạ tầng, không gán quyền quyết định movement cho adapter đó.
3. Kotlin chỉ render/input; chuyển cả persistence/transport/location glue sang
   native qua JNI/IPC: sát nghĩa đen hơn nhưng phạm vi lớn hơn, cần migration
   dữ liệu và xử lý Android permission/process lifecycle. Không cần chọn ngay
   phương án này chỉ để bỏ gameplay logic; cũng không thể tuyên bố đã đạt nó
   nếu vẫn giữ movement/controller Kotlin.

Hướng đề xuất là chuyển owner theo thứ tự: backend nhận desired state/config
→ native status/UI event contract → bỏ app lifecycle/game adapter loop → tách
movement khỏi overlay → giảm dependency/code cũ. Ở mỗi phần chỉ một owner
được chạy. Khi thay một phần, chỉ bỏ đường cũ sau khi đường mới giữ được STOP,
stale/session checks và postcondition; rollback phải trả lại owner cũ, không
bật cả hai đường cùng lúc.

**Kết luận bước 5:** chỉnh rule để “Kotlin chỉ UI” là đích bắt buộc, đồng thời
ghi những trách nhiệm đang còn trong Kotlin là phần cần chuyển. Chưa chỉnh
runtime, chưa đổi storage và chưa khẳng định đã đạt boundary mới.

## Bước 6 — Tiêu chí chấp nhận và xác minh

### Tiêu chí chấp nhận

Các tiêu chí migration dưới đây là **đề xuất từ yêu cầu người dùng**, chưa phải
kết quả đã đạt của source hiện tại.

| ID | Nguồn yêu cầu | Điều kiện đầu vào | Hành vi / postcondition | Method liên quan | Cách kiểm chứng |
|---|---|---|---|---|---|
| AC-01 | Kotlin chỉ overlay/UI | Người dùng bật/tắt feature | Kotlin gửi intent; native/backend tự giữ state và trình tự runtime; không có vòng app quyết định START/STOP theo game readiness | `HeadlessAutomationEngine.runLoop`, `RuntimeLifecycleCoordinator.ensureRunning/ensureIdle` | Rà call graph sau migration; script smoke hiện có |
| AC-02 | Không xử lý logic với PoGo process | Native phát nearby/encounter hoặc đổi lifecycle | UI nhận model hiển thị; không parse protobuf game, giữ game snapshot để lập luận hoặc suy OVERWORLD/ENCOUNTER | `RuntimeObservationRouter.tick`, `BridgePogoRuntimeSource.consumeObservation` | Rà import/dependency/call site; so status với event native |
| AC-03 — đã thay thế | Yêu cầu UI-only trước ngoại lệ location | Joystick/walk/teleport/map tap | Không còn yêu cầu chuyển movement sang native; áp dụng AC-LOC-01 bên dưới | `JoystickLocationController.tick/walkTo/teleport`, `JoystickOverlayService.applyPendingMapTarget` | Xem Tiếp nối 1 |
| AC-04 | Giữ fail-closed | Disconnect, đổi session, config không đủ hoặc sai revision, navigation hết hạn | Không replay mutation cũ, không chạy module thiếu config/readiness, không tiếp tục location bằng lệnh quá hạn | Native `runtime_managed_configs_ready`, `sync_auto_enabled_modules`; `AutoFortNavigationBus` hiện tại | Test có sẵn nếu phù hợp; device smoke cho lifecycle, không viết/sửa test |
| AC-05 | UI vẫn dùng được | Thay setting rồi restart/reconnect | Giữ một nguồn config bền vững, không mất lựa chọn; status phân biệt desired/applied/ready | Repository + config client/backend thay dispatcher | Review migration; dùng control script/config và xác minh state |
| AC-06 | Một owner gameplay | Backend feature đang chạy | Không gắn lại runner/action executor Kotlin vào production; event catch chỉ phục vụ hiển thị | Router `actionExecutor = null`; native coordinator | Tìm caller trong production sources |
| AC-07 | Rule không viết test | Thực hiện migration | Không thêm/sửa test code; chỉ chạy kiểm chứng có sẵn, build và diff check | Quy định `AGENTS.md` | Review diff và chạy lệnh hiện có |

### Kiểm chứng của lượt review và kiểm chứng tương lai

- Lượt này đọc source, lần theo call site, tìm các class/caller đang và chưa
  được dùng, đối chiếu native config/module gates. Không chạy device, không
  gọi game method, không viết test và không sửa runtime source.
- Chạy `git diff --check` cho thay đổi tài liệu. Không dùng build/test để khẳng
  định boundary đã đổi vì lượt này chưa triển khai migration.
- Sau implementation: dùng `./gradlew test assembleDebug` và test hiện có phù
  hợp; native build qua `scripts/build-magisk.sh` với override NDK của repo;
  device check qua `scripts/device-smoke-test.sh`,
  `scripts/bluestacks-smoke-test.sh` và `scripts/headless-control.sh` theo options
  có sẵn. Không thay bằng raw ADB hay script tự tạo, không viết/sửa test.
- Game binding/device postcondition chỉ được coi là xác minh sau lượt chạy
  thích hợp trên Air 1; audit source này không thay bằng chứng đó.

### Kết luận và câu hỏi còn mở

**Đã xác nhận:** live Kotlin còn UI/persistence/IPC, engine/lifecycle/config
sequencing, raw observation interpretation, location execution và HTTP control
API. Không thấy app gắn Kotlin gameplay executor; native đang giữ gameplay.
Do đó source hiện tại **chưa đạt yêu cầu Kotlin chỉ overlay/UI**.

**Đã cập nhật tài liệu:** `AGENTS.md` ghi kiến trúc đích UI-only và đánh dấu
control/movement hiện tại là migration work; `docs/ARCHITECTURE.md` phân biệt
hiện trạng với đích, dẫn tới audit này. Không trình bày cập nhật rule như thể
runtime đã được refactor.

**Chưa quyết định trong phạm vi review:** host/process của backend movement;
mức chấp nhận adapter Android mỏng còn viết bằng Kotlin; có chuyển persistence
setting sang backend hay giữ app như hiện tại; chính sách khi toàn bộ app UI
bị kill. Không tự suy ra native phải tiếp tục mutation khi mất controller.
Các điểm này cần được chốt trong thiết kế migration; không chặn kết luận về
trách nhiệm Kotlin hiện tại.

## Tiếp nối 1 — Cho phép Kotlin sở hữu fake location

### Bước 1 — Yêu cầu được làm rõ

- Người dùng cho phép Kotlin xử lý fake location, teleport và walk-to-location.
- Đích mới: Kotlin phục vụ UI **và location controller Android**. Giới hạn
  không sở hữu logic process Pokémon GO vẫn giữ nguyên.
- Thay thế yêu cầu chuyển `JoystickLocationController`, `WalkPlanner` và
  `RootMockLocationProvider` sang native chỉ vì chúng không phải render UI.
- Phạm vi lần này là cập nhật rule/kiến trúc và đánh giá lại kết luận đã có;
  không refactor runtime hoặc viết/sửa test.
- Bước 2–4 dùng lại bằng chứng framework ở lượt đầu: movement tính toán tại
  `JoystickLocationController` và ghi Android test providers; không có binding
  game mới nên không cần reverse/device pass bổ sung.

### Bước 5 — Ranh giới sau ngoại lệ

| Phần việc | Owner sau yêu cầu mới |
|---|---|
| Overlay, input, settings, toast, state hiển thị | Kotlin |
| Joystick, fake location, teleport, walk-to-coordinate/favorite | Kotlin `app/location/` cùng thuật toán thuần `core/location/` |
| Tính tốc độ/hướng/bước đi, local arrival/stop, ưu tiên input location | Kotlin location controller; overlay delegate, không trộn thuật toán vào view |
| Android mock-provider start/stop/cleanup | Kotlin `RootMockLocationProvider` |
| Chọn Pokémon/fort hoặc quyết định pause/resume vì game state | Native |
| Map tap thật → tọa độ với camera/frame hợp lệ | Native binding đã xác minh; Kotlin thực hiện walk sau capability/freshness checks |
| Quyết định catch/spin/discard/transfer, game-side range/readiness/outcome | Native |
| Giữ config/UI persistence và client IPC mỏng | Kotlin; không giao lại quyền diễn giải raw game state |

Kotlin có thể tính đã đến tọa độ theo khoảng cách hình học. Điều đó không cấp
quyền spin/catch và không chứng minh game đã nhận vị trí mới; native vẫn kiểm
tra điều kiện game. Cooldown ước tính cho location UI cũng không thay thế
gameplay cooldown authoritative.

Native-issued navigation vẫn có session/freshness/lease và xử lý mất kết nối.
Manual joystick/teleport/walk không cần một binding PoGo mới; việc hủy/chuyển
mode phải do location controller quản lý rõ ràng, không để hai nguồn cùng ghi
vị trí. Rule vẫn cấm screenshot/input-tap fallback cho map-target binding.

**Kết luận thay thế:** bỏ movement execution khỏi danh sách bắt buộc chuyển
sang native. Những phần còn cần tách khỏi Kotlin là runtime orchestration,
readiness/config application sequencing và raw game-state processing. Bằng
chứng code của lượt đầu không đổi; cách đánh giá location thay đổi do yêu cầu
người dùng đã được làm rõ.

Đã cập nhật `AGENTS.md` và `docs/ARCHITECTURE.md` theo boundary này, bao gồm
map-tap walk được phép chia native-resolve/Kotlin-move. Không đổi runtime code
hay nơi persist data.

### Bước 6 — Tiêu chí và kết luận thay thế

| ID | Nguồn yêu cầu | Đầu vào | Hành vi / postcondition | Kiểm chứng |
|---|---|---|---|---|
| AC-LOC-01 | Người dùng cho phép Kotlin fake location/teleport/walk | Input joystick hoặc target tọa độ/favorite | Kotlin được validate, tính movement, giữ state và ghi Android mock location; không bắt buộc chuyển code này sang native | Đối chiếu `JoystickLocationController`, `WalkPlanner`, `RootMockLocationProvider` với rule |
| AC-LOC-02 | Giữ giới hạn logic PoGo | Native chọn fort hoặc phát map target hợp lệ | Kotlin nhận walk/stop có guards; không tự đọc game snapshot để chọn mục tiêu tiếp theo | Review call site/native navigation contract |
| AC-LOC-03 | Một owner gameplay | Kotlin báo local ARRIVED | Không tự gọi catch/spin; native kiểm tra game-side location/range/readiness trước hành động | Review caller/executor production |
| AC-LOC-04 | Giữ hành vi location an toàn hiện có | Stop/provider failure/stale native navigation hoặc đổi mode | Giữ cleanup, native navigation lease và một nguồn ghi location tại một thời điểm | Review source; smoke/test có sẵn khi có implementation change |

AC-03 cũ bị thay thế bởi AC-LOC-01. Các kết luận bắt buộc chuyển movement và
câu hỏi host của native movement trong lượt đầu không còn áp dụng. Giữ rule
không viết/sửa test và các giới hạn runtime/gameplay khác.

**Kết luận hiện hành:** kiến trúc đích là **Kotlin UI + Android location
controller; native Pokémon GO runtime + gameplay**. `JoystickLocationController`,
`WalkPlanner`/`GeoMath` và `RootMockLocationProvider` phù hợp boundary mới.
Kotlin vẫn còn control/runtime và raw game-state interpretation cần thu gọn
theo audit trước; ngoại lệ location không mở rộng sang các phần này.

Lượt cập nhật chỉ sửa tài liệu/rule; kiểm tra bằng `git diff --check`, không
build, viết/sửa test hoặc thao tác device. Chính sách khi cả app bị kill chưa
được thay đổi bởi ngoại lệ location.

## Tiếp nối 2 — Tổng hợp refactor và lập plan

### Bước 1 — Phạm vi triển khai cần lập kế hoạch

- Yêu cầu mới: tổng hợp thông tin cần triển khai, chỉ rõ việc/code cần xóa và
  viết plan theo phase; chưa yêu cầu thực thi refactor.
- Boundary áp dụng là Tiếp nối 1: giữ UI, persistence, IPC mỏng và toàn bộ
  fake-location/teleport/walk hợp lệ ở Kotlin; native sở hữu PoGo runtime/gameplay.
- Dùng template refactor; tái sử dụng bằng chứng source ở lượt audit, bổ sung
  dependency/caller, kiểm tra hiện có và native contract cần thay trước cleanup.
- Không viết/sửa test. Cần kiểm tra test hiện có trước khi hứa xóa class cũ;
  không bỏ test hoặc làm tắt kiểm chứng để dọn dependency.
- Đầu ra: `refactor-inventory.md` và `checklists/00-overview.md` cùng các phase.
  Mỗi task có dependency, Verify và details để agent ghi thay đổi thực tế.
- Không thay binding game, không chuyển movement/storage sang native, không
  tự suy ra automation phải tiếp tục khi process controller bị kill.

### Bước 2 — Phạm vi bằng chứng

Dùng lại kết luận không cần reverse ở lượt đầu: đây là refactor framework,
không thêm/sửa signature/RVA/field của Pokémon GO. Native boundary/callback
hiện có phải được giữ; nếu phát sinh yêu cầu binding mới phải tách discovery.

### Bước 3 — Dependency và contract bổ sung

Đã đối chiếu bổ sung source framework và call site; không phải bằng chứng
implementation game:

- `app/build.gradle.kts:24`: app kéo toàn bộ JAR/AAR trong `app/libs`, protobuf,
  `game-adapter:api` và `game-adapter:pogo`. Chỉ bỏ import router chưa loại được
  dependency/library khỏi APK; cần task Gradle riêng, vẫn giữ các vendor file
  mà CI/script kiểm checksum và `game-adapter:pogo` dùng compileOnly.
- `RuntimeObservationRouter.kt:32`: là consumer live chính của POGO adapter;
  `RuntimeStatusRepository.kt:6` còn import `GameBuild` nhưng không thấy caller
  production. Đây là hai điểm cần xử lý trước khi gỡ app → adapter dependencies.
- `RuntimeLifecycleCoordinatorTest.kt:20` và fake bridge `:98` dùng trực tiếp
  coordinator, ba dispatcher, `RuntimeControlBridge` và mapping config cũ.
  Xóa tất cả file này ngay sẽ làm test hiện có không compile. Theo rule không
  viết/sửa test, plan mặc định loại chúng khỏi call graph production, giữ mã
  compatibility hiện có không được mở rộng. Không xóa/disable test để báo pass.
- `RuntimeSessionManager`, `AutomationRunner`, `AutomationCoordinator`,
  `BridgeBackedPogoActionExecutor`, `toCorePolicy`, `recordEncounter` không có
  caller app live trong lần tìm này. Nhưng core model/action vẫn bị bridge
  và adapter import, nên không thể xóa trọn `core/automation` hay `core/model`.
- Native `activate_runtime(context, request_id)` tại
  `zygisk/jni/shared/runtime/control/runtime_control.inc:49` reset ba config
  khi host bắt đầu; `stop_runtime` tại `:103` cũng reset. Lưu desired config
  mới phải tách khỏi applied mirror để tránh bị START xóa trước khi apply.
- `apply_runtime_catch_spin_config(context, command)` tại
  `zygisk/jni/modules/catch_spin/config.inc:79` kiểm runtime-active,
  pid/process/package, fingerprint và expiry. Protocol hiện tại không cho
  Kotlin gửi config trước managed readiness; phải có đường nhận desired intent
  chỉ lưu dữ liệu rồi native tự apply sau guards, không bỏ fingerprint gate.
- `runtime_managed_configs_ready()` hiện đòi đủ ba config cùng revision > 0.
  Gửi nửa bộ config hoặc activate từ ACK chưa đủ là sai completion semantics.
- Broker `handle_controller(fd, context)` tại
  `zygisk/jni/shared/bridge_kotlin/runtime_bridge_broker.inc:1` kiểm peer auth,
  session; lỗi/HUP controller trả về accept loop (`:191`, `:254`). Không thấy
  STOP tự động ở đoạn này. Không được khẳng định app-kill đã dừng native;
  policy và backpressure khi không có client cần task discovery riêng.
- `scripts/headless-control.sh` đang cung cấp bootstrap/status/start/stop/
  diagnostic/config/game; HTTP facade phải giữ endpoint và khả năng vận hành.
  Smoke hiện có không phải công cụ gửi malformed binary command.
- CI hiện chạy 6 host tests trong `zygisk/tests/`, khác các path native test
  cũ ghi trong AGENTS. Chọn command từ source/CI thực tế; không viết test mới.

**Kết luận:** cần contract desired config/status và native owner trước khi
cắt Kotlin loop; cần phân biệt xóa file thật, gỡ dependency APK và mã cũ chỉ
giữ cho compatibility với kiểm chứng hiện có.

### Bước 4 — Luồng chuyển đổi và điều kiện xóa

Luồng đích: UI/API → persist snapshot có revision → client gửi desired state
theo session qua IPC → native nhận intent trong RAM → native tự START/readiness,
apply đủ config và bật module → native phát status/event → Kotlin render hoặc
thực hiện navigation qua location controller hiện có.

Contract mới là đề xuất của framework, chưa tồn tại: desired state chứa
`enabled`, revision và config feature người dùng; status phân biệt đã nhận,
đã áp dụng và runtime/module ready. Intent có thể được nhận trước managed ready,
nhưng không được apply/mutate trước exact identity/build/capability guards.
Mọi wire ID/version/field order phải được ghi chính thức ở task contract.

Thứ tự thay bắt buộc: xác nhận invariants → contract hai phía → native owner
→ thin Kotlin client/router → service/API cutover → xóa logic/dependency cũ.
Không để cả coordinator Kotlin và native cùng quản lý host/config một phiên.

Điều kiện xóa:

- Engine/cycle/status reporter cũ: service và HTTP không còn tham chiếu, DTO
  dùng chung đã được chuyển sang model trình bày/status mới.
- POGO adapter khỏi app: router mới vẫn bảo toàn auth/session/identity/sequence,
  capability, payload version, native navigation lease và map-target TTL.
- Parser raw/diagnostic cũ: event cần cho UI có wire presentation thay thế;
  không chỉ ngừng decode rồi âm thầm mất toast/status.
- Planner/core cũ: lập danh sách symbol export cùng consumer. Ví dụ
  `AutomationSnapshot` nằm chung file `AutomationCoordinator.kt`; tách DTO
  nếu còn consumer trước khi xóa planner. Không xóa nguyên folder.
- Coordinator/dispatchers/interface được test cũ gọi: loại khỏi production
  call graph, giữ compatibility implementation hiện có; chưa xóa vật lý trong
  plan này. Không tạo test-support copy, không viết lại test hay skip test.
- Vendor JAR: gỡ khỏi runtime dependency của app khi không còn dùng, nhưng giữ
  file và module adapter trong repo vì CI/vendor script/compileOnly còn cần.

Luồng HTTP, service intents và SharedPreferences keys được giữ tương thích.
`loopIntervalMs` không còn được dùng để điều phối game; có thể giữ key/query
deprecated để không phá config cũ, không tái sử dụng làm gameplay scheduler.
Không tự kích hoạt scan/throw feature chưa sẵn sàng chỉ để lấp UI contract.

### Bước 5 — Danh mục refactor

Đã tổng hợp tại [refactor-inventory.md](refactor-inventory.md): bảng giữ lại,
việc bỏ khỏi Kotlin, xóa file thật, gỡ dependency APK, compatibility cho test,
contract/native cần bổ sung và discovery gates.

Quyết định phạm vi: giữ coordinator/dispatcher cũ chỉ để các test hiện có còn
compile/chạy; loại toàn bộ production caller của chúng. Không mở rộng chúng,
không viết test-support copy và không xóa/disable test. Tách rõ giới hạn này
để không báo đã xóa vật lý toàn bộ runtime Kotlin.

Không xóa vendor artifacts hoặc các module adapter chỉ vì app không còn dùng.
Không đổi storage, gameplay binding, thuật toán location hoặc thêm scanner.

### Bước 6 — Plan và kiểm chứng

Đã viết [plan triển khai](checklists/00-overview.md), gồm 6 phase và 23 task:

1. Inventory, lifecycle invariants và chốt wire contract.
2. Codec/DTO Kotlin và C++ đồng bộ.
3. Native nhận desired state, tự readiness/apply/STOP và phát status.
4. Kotlin client/router/settings mapper, chuyển service/HTTP và giữ location.
5. Xóa source không còn caller, gỡ dependency app, công khai compatibility.
6. Kiểm chứng hiện có, device scenarios và cập nhật tài liệu theo code thực tế.

Đã rà dependency để mapper T-015 có trước service cutover T-013; client/router
mới được chuẩn bị trước khi xóa đường cũ, từng task vẫn có điều kiện compile.
Sau cutover chỉ native giữ quyền điều phối runtime. Các lớp cũ cần cho test
chỉ được giữ vì quyết định compatibility của plan, không còn caller production.

Mọi task vẫn `[ ]`, có AC, dependency, Verify và khối details để ghi thay đổi
thực tế ngay sau thực thi. Discovery về disconnect/app-kill, schema wire và
setting chưa có native consumer có task chịu trách nhiệm, không được đoán.

Kiểm chứng lúc lập tài liệu: 23 ID duy nhất và đủ details, dependency không
chu trình, 28 liên kết nội bộ inventory/checklists trỏ đúng file/anchor;
`git diff --check` đạt. Chưa sửa runtime/test code, chưa chạy build/test hoặc
thao tác thiết bị; kết quả triển khai và device verification vẫn chưa có.
