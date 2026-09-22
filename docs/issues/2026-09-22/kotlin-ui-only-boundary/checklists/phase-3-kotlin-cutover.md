# Phase 3 — Kotlin client/UI cutover, giữ location

Plan: [Tổng quan](00-overview.md)

Điều kiện bắt đầu: contract và native owner T-010 đạt; có rollback build trước cutover.
Điều kiện hoàn tất: service/API không gọi engine/coordinator/POGO adapter; location/UI còn hoạt động; chỉ một native owner.

Thứ tự thực thi trong phase: T-011 → T-012 và T-015 → T-013 → T-014.
T-015 phải cung cấp mapper trước khi service cutover. Các API/router cũ chỉ
giữ tạm cho source chưa chuyển còn compile, không chạy đồng thời hai owner;
gỡ phần tạm ở T-016/T-017. Không deploy artifacts giữa chừng chưa cutover đủ.

Sau mỗi task, cập nhật ngay khối details dưới checkbox với file tạo/sửa/xóa/di chuyển, thay đổi method/param/hành vi và kiểm chứng thực tế trước khi chuyển task. Chỉ tick [x] khi Verify đạt; chưa đạt thì giữ [ ] và ghi phần còn thiếu. Không viết/sửa/xóa/disable test. Alias A/, B/, C/, P/, N/ được định nghĩa trong [inventory](../refactor-inventory.md#2-quy-ước-đường-dẫn).

- [x] **T-011** **[app]** *(android)* — Thu gọn RuntimeBridgeClient thành transport và desired-intent client. **AC:** AC-01, AC-04, AC-05. **Phụ thuộc:** T-010. **Verify:** client không quyết định managed readiness/START→CONFIG sequence; `:app:assembleDebug` đạt và source guards được review.

  Dự kiến tạo `A/root/RuntimeUiClient.kt`; sửa `RuntimeBridgeClient.kt` để thực hiện contract mới. API đề xuất — chưa tồn tại: `submitDesiredState(state: RuntimeDesiredState): Result<DesiredStateReceipt>` và listener status/event theo T-003; desired settings do service cung cấp, session/identity/request ID/expiry do transport bọc.
  Connect/reconnect chỉ handshake/peer/session + gửi latest desired snapshot idempotent, không gọi START/DIAGNOSTIC để chạy readiness từ Kotlin. Reader/request timeout và outbound write serialization vẫn có; callback UI chuyển đúng main thread.
  Chưa xóa interface/method legacy khi service/engine cũ còn cần compile; giữ tạm chữ ký và implementation cũ đến T-013 chuyển caller, rồi T-016/T-017 gỡ phần hết dùng. API mới không gọi lại coordinator hay fallback khi protocol không tương thích. `RuntimeControlBridge` mà fake test cũ dùng vẫn giữ.
  Receipt chỉ xác nhận request; applied/native readiness nhận từ native status. Failure clear session-bound caches/navigation; không requeue gameplay. File client đã gần 500 dòng nên tách transport reader/correlation helper khi cần.

  <details>
  <summary>Đã thực hiện — T-011</summary>

  - Sửa `app/src/main/java/dev/pogoroot/automation/root/RuntimeBridgeClient.kt` (493 dòng): thêm `submitDesiredState(RuntimeDesiredState)` với request/session/identity/expiry do transport bọc, chờ duy nhất receipt phase `ACCEPTED`; thêm event listener cho status/event. Các START/STOP/DIAGNOSTIC và CONFIG_SET API legacy vẫn giữ tạm để caller cũ compile nhưng không được API mới gọi ngầm.
  - Tạo `app/src/main/java/dev/pogoroot/automation/root/RuntimeUiClient.kt`: facade UI-only giữ latest desired snapshot để resend idempotent khi reconnect, marshal status/event callback lên main looper, clear session-bound status khi session/failure đổi và không queue gameplay. `currentStatus()` đọc native status; không có readiness state machine hay fallback coordinator.
  - Verification/source review: `RuntimeUiClient` không import/call `RuntimeLifecycleCoordinator`, `HeadlessAutomationEngine`, `GameCapability`, `START` hoặc `DIAGNOSTIC`; receipt không bị coi là applied/ready. `./gradlew :app:compileDebugKotlin --rerun-tasks` và `./gradlew :app:assembleDebug --rerun-tasks` đều BUILD SUCCESSFUL (chỉ warning deprecated Android API hiện có). Không đổi test source.

  </details>

- [x] **T-012** **[app]** *(android)* — Tạo UI/status router trực tiếp để thay observation router. **AC:** AC-02, AC-04, AC-LOC-02. **Phụ thuộc:** T-011. **Verify:** router mới không import `dev.pogoroot.automation.pogo`/`GameCapability`; mapper/navigation tests có sẵn và app compile đạt.

  Dự kiến tạo `A/runtime/observation/RuntimeUiEventRouter.kt` và `A/runtime/RuntimeUiStateStore.kt`. API đề xuất — chưa tồn tại: `accept(event: RuntimeUiEvent): Unit` và `snapshot(): RuntimeUiState`; DTO/schema theo T-003, không dựng `BridgePogoRuntimeSource/PogoGameAdapter`. Router cũ còn tạm để engine chưa chuyển compile; service chỉ chọn router mới ở T-013, router cũ bị xóa ở T-017.
  Chuyển các guard session, pid/process/package/fingerprint, monotonic seq, payload version/size và stale data từ adapter sang boundary nhận message. Không bỏ guard vì không còn adapter; không suy lifecycle từ raw observation hoặc local connection.
  Route automation events tới mapper/sink hiện có; status lấy native; map target chỉ khi native advertise READ_MAP_TARGET + payload hợp lệ; navigation giữ 5s lease. Reset khi đổi session/binding lost. Raw nearby/encounter/fort/inventory không parse/cache trong app.
  Giữ `RuntimeAutomationEventMapper` và test nguyên trạng nếu contract không buộc đổi; throw diagnostic dùng bridge DTO T-003/T-009. `RuntimeObservationTick` chưa xóa cho đến T-017 sau khi callers chuyển.

  <details>
  <summary>Đã thực hiện — T-012</summary>

  - Tạo `app/src/main/java/dev/pogoroot/automation/runtime/RuntimeUiStateStore.kt`: session-scoped UI DTO/state, native status, automation event, map target, navigation lease và throw diagnostic; raw nearby/encounter/fort/inventory không được lưu.
  - Tạo `app/src/main/java/dev/pogoroot/automation/runtime/observation/RuntimeUiEventRouter.kt`: consume `RuntimeUiClient` events, guard session/pid/process/package/fingerprint, monotonic observation sequence, status timestamp, payload size/version và stale observation; map target chỉ route khi `READ_MAP_TARGET` advertised; navigation giữ 5s lease/reset; session/binding loss reset.
  - Tạo `bridge/protocol/.../RuntimeThrowDiagnosticPayloadCodec.kt` để throw diagnostic DTO nằm ở bridge boundary, không phụ thuộc POGO adapter. `RuntimeAutomationEventMapper` hiện có được reuse; raw game-state payloads chỉ bỏ qua.
  - Sửa native capability/status builders để advertise `READ_MAP_TARGET` từ `map_pokemon_tap_event_verified`; không thêm game-object read. Router mới không import `dev.pogoroot.automation.pogo` hoặc `GameCapability`, không gọi START/DIAGNOSTIC và local navigation arrival không dispatch gameplay.
  - Verification: `./gradlew :app:testDebugUnitTest --rerun-tasks` → BUILD SUCCESSFUL (33 tasks, test source không đổi); `runtime_auto_fort_navigation_test` compile/run exit 0; app compile thành công và `git diff --check` sạch.

  </details>

- [x] **T-015** **[app]** *(android)* — Chuẩn bị mapper settings cho client mới, tách khỏi policy/dispatcher legacy. **AC:** AC-05, AC-RF-03, AC-07. **Phụ thuộc:** T-011. **Verify:** SharedPreferences keys/data và test compatibility giữ nguyên; mapper mới không dựng AutomationPolicy hoặc quyết định game state; app compile đạt.

  Dự kiến tạo `A/config/RuntimeDesiredStateMapper.kt`; map `HeadlessAutomationConfig` thành `RuntimeDesiredState` đã chốt ở T-003. T-013 sẽ nối UI/API submit path vào mapper này; task này chưa bật thêm đường điều khiển song song.
  `AutomationConfigRepository` vẫn là durable source, config revision được giữ; không tạo native persisted config. Native T-008 nhận trách nhiệm default policy, mapper không tự thêm filter game ẩn.
  Giữ mapping `toRuntimeCatchSpinConfig/toRuntimeTransferConfig/toRuntimeDiscardConfig` mà legacy tests cần; chúng không được dùng trên production path mới. `toCorePolicy` và helper chết sẽ được xóa ở T-017.
  Giữ setting/query chưa có native consumer theo bảng T-001: persist-only/deprecated/unsupported được công bố đúng; không silently bật throw/berry/scan. `loopIntervalMs` không còn điều phối gameplay.

  <details>
  <summary>Đã thực hiện — T-015</summary>

  - Tạo `app/src/main/java/dev/pogoroot/automation/config/RuntimeDesiredStateMapper.kt`: map `HeadlessAutomationConfig` → `RuntimeDesiredState` với cùng config revision, native-owned catch/spin/discard/transfer fields và sorted discard limits. Mapper không dựng `AutomationPolicy`, không đọc game state và không tự bật throw/berry/scan.
  - Các setting persist-only/unsupported (`mapTapWalkEnabled`, throw/curve/encounter/snapshot/berry/toasts/loop interval và structured allowlist) không xuất hiện trong desired wire; legacy `toRuntimeCatchSpinConfig/toRuntimeTransferConfig/toRuntimeDiscardConfig` chưa xóa vì dispatcher cũ còn compile, chưa được production path mới gọi.
  - SharedPreferences repository/key/revision logic không đổi. `./gradlew :app:compileDebugKotlin --rerun-tasks` → BUILD SUCCESSFUL; không thêm/sửa test.

  </details>

- [x] **T-013** **[app]** *(android)* — Chuyển service và HTTP API khỏi engine/coordinator cũ. **AC:** AC-01, AC-05, AC-RF-02. **Phụ thuộc:** T-012, T-015. **Verify:** service/API production không khởi tạo/gọi legacy engine/coordinator/dispatchers; app test/assemble đạt; endpoint/intent checklist có mapping mới.

  Sửa `A/service/HeadlessAutomationService.kt`, `AutomationControlServer.kt`, `MainActivity.kt` và call sites từ overlay/boot nếu cần. Giữ service class/component, intents, port 127.0.0.1:8765 và các endpoint health/status/start/stop/config/runtime-diagnostic.
  Service nối client, mapper T-015 và router T-012; gửi desired snapshot sau đọc prefs; thay `engine.activate/deactivate/pushRuntimeConfigs/snapshot` bằng facade user intent/status. START/STOP của người dùng vẫn có ý nghĩa nhưng service không chạy state machine để quyết định lúc native ready. Manual diagnostic chỉ forward request read-only.
  Status JSON phân biệt desired enabled với applied/ready; không tạo thành công giả từ config ACK. Giữ field/query đang có khi có thể, deprecated trường không còn consumer phải được ghi, không đổi semantics điều khiển qua script.
  Gỡ `pendingModuleLoadStatuses` phụ thuộc configsApplied coordinator khi native status T-009 đã đủ. Giữ Android foreground service, boot, joystick auto-start, notification; onDestroy/explicit stop tuân T-002. Không để cả old engine và new client cùng hoạt động.

  <details>
  <summary>Đã thực hiện — T-013</summary>

  - Tạo `app/src/main/java/dev/pogoroot/automation/service/RuntimeUiAutomationFacade.kt`: service-owned facade duy nhất đọc `AutomationConfigRepository`, map bằng `RuntimeDesiredStateMapper`, submit desired snapshot trên một executor tuần tự, drain `RuntimeUiEventRouter`, expose native status/applied revision/readiness cho API và reset navigation khi session/disconnect. `start/stop/config` chỉ đổi durable desired intent; `requestRuntimeDiagnostic()` là forward read-only explicit, không gọi START/STOP hay coordinator.
  - Sửa `HeadlessAutomationService.kt`: bỏ khởi tạo/gọi `HeadlessAutomationEngine`, `RuntimeLifecycleCoordinator`, `RuntimeObservationRouter`, `RuntimeBridgeClient` và `pendingModuleLoadStatuses`; giữ foreground service, boot/intent actions, port/API, joystick auto-start, notification và onDestroy cleanup. ACTION_ENABLE/DISABLE/SYNC gửi desired state qua facade; native status thay thế `configsApplied` gate.
  - Sửa `AutomationControlServer.kt`: giữ nguyên `/health`, `/v1/health`, `/v1/status`, `/v1/start`, `/v1/stop`, `/v1/config`, `/v1/runtime/diagnostic`, query mapping và port `127.0.0.1:8765`; status JSON lấy `enabled` từ desired config nhưng `runtimeControlState/modules/lifecycle/mutationPermission/suspended/error` từ native UI status, không biến receipt thành applied/ready.
  - Sửa `MainActivity.kt`: hiển thị master state từ persisted desired config, không đọc `AutomationRunState`. Không xóa legacy engine/coordinator/dispatchers vì T-016/T-017 còn dùng để compile; production service/API không tham chiếu chúng.
  - Verification: source guard trên service/API/MainActivity không còn `HeadlessAutomationEngine`, `RuntimeLifecycleCoordinator`, `RuntimeObservationRouter`, config dispatchers, `START_RUNTIME`, `STOP_RUNTIME`, `configsAppliedToNative` hoặc `pendingModuleLoadStatuses`; `./gradlew :app:compileDebugKotlin --rerun-tasks` → BUILD SUCCESSFUL; `./gradlew :app:testDebugUnitTest --rerun-tasks` → BUILD SUCCESSFUL (33 tasks); `./gradlew :app:assembleDebug --rerun-tasks` → BUILD SUCCESSFUL (43 tasks, 6m42s); `git diff --check` sạch; không đổi test source.

  </details>

- [x] **T-014** **[app]** *(android)* — Nối lại UI/location với router mới và giữ ngoại lệ fake location. **AC:** AC-LOC-01, AC-LOC-02, AC-LOC-03, AC-LOC-04. **Phụ thuộc:** T-013. **Verify:** navigation receiver tests cũ đạt; review chứng minh local arrival không dispatch gameplay; app compile đạt, device postconditions ở T-022.

  Giữ `A/location/JoystickLocationController.kt`, `RootMockLocationProvider.kt`, `NativeNavigationReceiver.kt`, `AutoFortNavigationBus.kt`, `C/location/*`, `C/time/*` và repositories. Rewire event sinks/service callback khi router mới đổi type, không port thuật toán movement sang native.
  `JoystickOverlayService` tiếp tục delegate walk/teleport/joystick đến location controller. Nếu file touched vượt 500 dòng, tách handoff thành `A/location/LocationIntentRouter.kt` (dự kiến tạo); không trộn game target selection vào helper này.
  Giữ map-target TTL mặc định 30s, native navigation lease 5s, stop/cleanup và manual input arbitration; phân biệt navigation native cần session với walk/favorite do người dùng chọn. Không để mất native session dừng sai manual flow nếu không phải policy hiện có đã chốt.
  Scan UI/repository vẫn render summary đã có hoặc trạng thái unavailable; không gọi `ScanMatcher` để xử lý raw encounter và không tạo native scanner mới.

  <details>
  <summary>Đã thực hiện — T-014</summary>

  - Không phát sinh source change mới: T-013 đã nối `RuntimeUiEventRouter` trong `RuntimeUiAutomationFacade` tới `MapTargetRepository` và `NativeNavigationReceiver`; không cần port hay sửa thuật toán location.
  - Review wiring: map target chỉ được lưu khi native status advertise `READ_MAP_TARGET`, sau đó `JoystickOverlayService` đọc TTL 30 giây và gọi `JoystickLocationController.walkTo`; native navigation đi qua lease 5 giây → `NativeNavigationReceiver` → `AutoFortNavigationBus` → controller. `ARRIVED` chỉ phát UI feedback, local `WalkStatus.ARRIVED` chỉ kết thúc movement; không có callback nào dispatch gameplay hoặc chọn fort mới.
  - Giữ nguyên joystick/teleport/favorite walk, speed/bearing/step calculation, mock provider cleanup và manual input arbitration. `JoystickLocationController.kt` 322 dòng, `RootMockLocationProvider.kt` 154, `NativeNavigationReceiver.kt` 53, `AutoFortNavigationBus.kt` 57, `JoystickOverlayService.kt` 499 — đều dưới giới hạn 500 dòng.
  - Verification: `./gradlew :app:testDebugUnitTest --tests dev.pogoroot.automation.runtime.NativeNavigationReceiverTest --rerun-tasks` → BUILD SUCCESSFUL (33 tasks); app compile/assemble T-013 đã đạt; `git diff --check` sạch; không đổi test source.

  </details>
