# Danh mục refactor: Kotlin UI + location, native PoGo runtime

Ngày đối chiếu: 2026-09-22. Đây là inventory sau khi thực hiện plan
[checklists/00-overview.md](checklists/00-overview.md), không còn là danh sách
việc dự kiến.

## 1. Boundary đã chốt

- Kotlin giữ overlay/settings, persistence, client IPC, Android service/API và
  fake location/joystick/teleport/walk-to-location.
- Native giữ readiness, desired-state reconciliation, build/capability guards,
  game-state interpretation, target selection, module activation và gameplay.
- App gửi một full `RuntimeDesiredState` có revision; `RuntimeUiStatus` phân biệt
  received/applied/ready. Receipt không được diễn giải là action game hoàn tất.
- Native chọn fort/map target; Kotlin thực hiện movement với session/freshness/
  lease. Local arrival không cấp quyền spin/catch.
- Không viết/sửa/xóa/disable test. `game-adapter/*` vẫn giữ trong repository để
  build/test độc lập nhưng không nằm trong APK UI-only.

## 2. Quy ước đường dẫn

| Alias | Thư mục thực tế |
|---|---|
| `A/` | `app/src/main/java/dev/pogoroot/automation/` |
| `C/` | `core/src/main/kotlin/dev/pogoroot/automation/core/` |
| `B/` | `bridge/protocol/src/main/kotlin/dev/pogoroot/automation/bridge/` |
| `P/` | `game-adapter/pogo/src/main/kotlin/dev/pogoroot/automation/pogo/` |
| `N/` | `zygisk/jni/` |

## 3. Thành phần giữ lại và owner

| Thành phần | Owner/trách nhiệm sau refactor |
|---|---|
| `A/overlay/*`, `MainActivity.kt` | Render, input, settings, toast và foreground visibility |
| `A/location/*`, `C/location/*`, `RootMockLocationProvider.kt` | Joystick, teleport, walk, geo/arrival và Android mock-provider lifecycle |
| `A/data/*`, `A/config/AutomationConfig.kt` | SharedPreferences, favorites, map-target TTL và durable user config |
| `A/service/RuntimeUiAutomationFacade.kt` | Poll kết nối nhẹ, map config thành desired state, gửi revision và route location |
| `A/root/RuntimeUiClient.kt`, `RuntimeBridgeClient.kt` | Authenticated IPC, desired-state submit, reconnect và event/status delivery |
| `A/runtime/RuntimeUiStateStore.kt`, `RuntimeUiEventRouter.kt` | Validate envelope, lưu UI state, route event/diagnostic/map/navigation; không gameplay |
| `A/service/AutomationControlServer.kt` | HTTP compatibility facade; không tự chạy engine/coordinator |
| `A/events/*`, `RuntimeAutomationEventMapper.kt` | Presentation/event mapping |
| `N/modules/*`, `N/shared/runtime/*` | Native readiness, reconciliation, gameplay, outcome guards và bridge |
| `B/*Runtime*PayloadCodec.kt` | Versioned wire contracts và DTO validation |

## 4. Đã bỏ khỏi production path

| Trách nhiệm cũ | Kết quả thực tế |
|---|---|
| App loop quyết định START/STOP, readiness retry và sync ba config | `HeadlessAutomationEngine.kt`, `AutomationCycle.kt`, `HeadlessAutomationStatusReporter.kt` đã xóa; facade chỉ gửi desired snapshot |
| App decode/cache nearby, encounter, fort, inventory | `RuntimeObservationRouter.kt`, `RuntimeObservationTick.kt` và `RuntimeStatusRepository.kt` đã xóa; router mới chỉ nhận DTO/status/event |
| Suy lifecycle từ raw POGO payload | Không còn app `game-adapter:pogo` consumer; native công bố `RuntimeUiStatus` |
| Dựng core gameplay policy/planner/runner | `AutomationCoordinator.kt`, `AutomationRunner.kt`, `CatchPlanner.kt`, `ModuleActionPlanner.kt`, planner encounter, `AutomationPolicy.kt`, `ActionExecution*.kt` đã xóa hoặc tách model còn consumer |
| App dependency vào POGO adapter/protobuf | `app/build.gradle.kts` chỉ giữ joystick AAR, `:core`, `:bridge:protocol`; vendor files và game-adapter modules vẫn giữ trong repo |
| Scan matcher xử lý encounter trong app | `ScanResultRepository` chỉ còn bounded summary record/read/clear |

## 5. Compatibility có lý do

Các file sau vẫn tồn tại nhưng không có production caller sau cutover:

- `A/runtime/RuntimeLifecycleCoordinator.kt` và ba config dispatcher;
- `A/root/RuntimeControlBridge.kt`;
- `A/config/AutomationPolicyBridge.kt` với các `toRuntime*Config()`;
- `C/automation/AutomationAction.kt` và các type action mà bridge/adapter còn dùng;
- `B/RuntimeSessionManager.kt` vì vẫn là generic transport/session guard;
- `game-adapter/api`, `game-adapter/pogo`, `game-adapter/fake` cho consumer/test
  ngoài APK.

`RuntimeLifecycleCoordinatorTest` là test compatibility của trình tự cũ. Không
sửa test để loại bỏ implementation; không mô tả test này là bằng chứng native
desired-state mới. Việc xóa compatibility group hoàn toàn là phạm vi riêng.

## 6. Contract live

| Contract | Owner/hành vi |
|---|---|
| `RuntimeDesiredState` | Kotlin serialize lựa chọn được hỗ trợ; native giữ RAM-only desired snapshot, expiry tối đa 30 giây và revision monotonic |
| `RuntimeUiStatus` | Native phát lifecycle/capability/module status, desired/applied revision; broker replay duy nhất status mới nhất |
| `RuntimeUiEventRouter` | Chặn session/identity/sequence/timestamp/size sai; bỏ qua raw nearby/encounter/fort/inventory |
| Map target/navigation | Native chỉ phát khi capability/guard; Kotlin giữ map target TTL 30 giây và navigation lease 5 giây |
| Protocol | Kotlin/C++ cùng protocol version 3; unsupported/malformed/mixed identity fail closed |

Settings UI chưa có native consumer (`berryMode`, throw preference, snapshot,
loop pacing, v.v.) vẫn persist/API-compatible nhưng không được mapper gửi như
gameplay semantics. `mapTapWalkEnabled` là gate app-owned; nó không bật binding.

## 7. Persistence và disconnect

- `headless_automation` là durable source of truth của user config.
- Native desired/applied mirrors, pointers, pending actions, readiness và
  cooldown là session/process RAM-only.
- Root `runtime.status`/`controller.uids` chỉ là diagnostics/auth metadata.
- Session mới làm native/app reset state session; facade gửi lại desired snapshot
  mới nhất. Broker không queue/replay gameplay command; chỉ cache status UI.
- Thiếu binding, exact build, capability, freshness, lease hoặc expiry đều fail
  closed; không dùng screenshot, `input tap`/`input swipe` hay filesystem queue.

## 8. Bằng chứng và giới hạn

- Focused/full Gradle, app tests, sáu native host checks và multi-ABI package đã
  đạt; `git diff --check` sạch tại các mốc kiểm chứng. Test source không đổi.
- T-022 device matrix chưa chạy: máy hiện tại không có `BlueStacks Air 1`; xem
  [`verification.md`](verification.md). Không báo device pass từ static review.
- `discard`/`transfer` và các binding/action vẫn chịu capability gates hiện có;
  cleanup không mở thêm capability hoặc giả lập outcome.
