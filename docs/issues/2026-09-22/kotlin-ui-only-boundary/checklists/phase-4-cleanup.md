# Phase 4 — Xóa logic cũ và gỡ dependency khỏi app

Plan: [Tổng quan](00-overview.md)

Điều kiện bắt đầu: T-014 và T-015 đạt; source inventory đã cập nhật; chưa xóa compatibility mà test cũ dùng.
Điều kiện hoàn tất: file/symbol chết đã xóa có chứng cứ; app không phụ thuộc POGO adapter/raw decoder; phần compatibility còn giữ được khai báo rõ.

Sau mỗi task, cập nhật ngay khối details dưới checkbox với file tạo/sửa/xóa/di chuyển, thay đổi method/param/hành vi và kiểm chứng thực tế trước khi chuyển task. Chỉ tick [x] khi Verify đạt; chưa đạt thì giữ [ ] và ghi phần còn thiếu. Không viết/sửa/xóa/disable test. Alias A/, B/, C/, P/, N/ được định nghĩa trong [inventory](../refactor-inventory.md#2-quy-ước-đường-dẫn).

- [x] **T-016** **[app]** *(android)* — Xóa engine/cycle/status reporter sau chuyển consumer. **AC:** AC-01, AC-RF-01, AC-07. **Phụ thuộc:** T-014, T-015. **Verify:** không còn reference production tới ba class; test app và assemble đạt, API status vẫn compile.

  Xóa `A/engine/HeadlessAutomationEngine.kt`, `AutomationCycle.kt`, `HeadlessAutomationStatusReporter.kt` sau khi chuyển `HeadlessAutomationStatus` DTO/HTTP status fields sang model presentation đã chốt. Không xóa `AutomationRunState`/`CatchSpinArmState` chỉ vì nằm trong cùng folder; giữ/make UI-intent state khi còn consumer.
  Gỡ scheduler/error retry/readiness orchestration khỏi service thay vì đổi tên loop rồi giữ logic. Liệt kê toàn bộ caller đã sửa trong details.
  Giữ `RuntimeLifecycleCoordinator`, ba dispatchers, `RuntimeControlBridge` và mapping cần cho existing tests nhưng production không khởi tạo/gọi. Test code không thay đổi; không ghi đã xóa vật lý nhóm compatibility.

  <details>
  <summary>Đã thực hiện — T-016</summary>

  - Xóa `app/src/main/java/dev/pogoroot/automation/engine/HeadlessAutomationEngine.kt`, `AutomationCycle.kt` và `HeadlessAutomationStatusReporter.kt`; HTTP status DTO/presentation đã chuyển sang `RuntimeUiAutomationStatus` ở T-013 nên không còn cần `HeadlessAutomationStatus`.
  - Không sửa/xóa `AutomationRunState.kt`, `CatchSpinArmState.kt`, `RuntimeLifecycleCoordinator.kt`, ba config dispatchers hoặc `RuntimeControlBridge.kt`; nhóm sau vẫn là compatibility source cho `RuntimeLifecycleCoordinatorTest`, không được khởi tạo trong production service/API.
  - Verification: `rg` trên `app/src/main/java` và `app/src/test` không còn symbol engine/cycle/status reporter; compatibility refs còn đúng trong `RuntimeLifecycleCoordinatorTest` và coordinator/dispatcher sources; `./gradlew :app:testDebugUnitTest :app:assembleDebug --rerun-tasks` → BUILD SUCCESSFUL (49 tasks); `git diff --check` sạch; không đổi test source.

  </details>

- [x] **T-017** **[app]** *(android)* — Xóa raw-game-state glue không còn dùng trong app. **AC:** AC-02, AC-RF-01, AC-07. **Phụ thuộc:** T-016. **Verify:** `rg`/review không còn app production import POGO adapter/GameBuild; app tests/compile đạt.

  Xóa `A/runtime/observation/RuntimeObservationRouter.kt`, `RuntimeObservationTick.kt` và `A/root/RuntimeStatusRepository.kt` khi caller bằng 0; router mới T-012 thay toàn bộ production consumer. Không xóa `zygisk/module/bin/runtime-status.sh` phục vụ scripts.
  Xóa `toCorePolicy()` và `BerryMode.toCoreBerryType()` nếu chỉ dùng cho extension đã nghỉ; giữ các mapping compatibility trong `AutomationPolicyBridge.kt`.
  Xóa `ScanResultRepository.recordEncounter()` và field matcher/import raw encounter nếu không có consumer. Giữ summary DTO, `record/read/clear`, bounded/dedupe UI store và list render. Không tự xóa `ScanResultSummary.fromEncounter` nếu adapter/library còn dùng.
  Rà `RuntimeBridgeClient.send(AutomationCommand)` và action-executor glue app đã nghỉ; bỏ callable production path sau kiểm caller. Không xóa cả bridge action contracts/native module handlers chỉ vì UI không dùng.

  <details>
  <summary>Đã thực hiện — T-017</summary>

  - Xóa `app/src/main/java/dev/pogoroot/automation/runtime/observation/RuntimeObservationRouter.kt`, `RuntimeObservationTick.kt` và `app/src/main/java/dev/pogoroot/automation/root/RuntimeStatusRepository.kt`; production caller đã chuyển sang `RuntimeUiEventRouter`/native status ở T-012/T-013, status shell parser không còn caller.
  - Sửa `RuntimeBridgeClient.kt`: bỏ implement/import `RuntimeBridge`, bỏ `send(BridgeEvent.AutomationCommand)` khỏi app client vì không còn production caller; giữ `receiveEvents()` cho `RuntimeUiClient`, desired-state transport, explicit diagnostic và legacy `RuntimeControlBridge` methods cho compatibility coordinator/tests. Bridge `RuntimeBridge` contract vẫn giữ trong `bridge/protocol` vì `game-adapter/pogo` còn compile-time consumer.
  - Sửa `AutomationPolicyBridge.kt`: xóa `toCorePolicy()` và `BerryMode.toCoreBerryType()` không có caller; giữ ba `toRuntime*Config()` mappings cho coordinator/dispatcher compatibility. Sửa `ScanResultRepository.kt`: xóa `ScanMatcher`, raw `EncounterSnapshot`, `recordEncounter()`; giữ bounded summary `record/read/clear` và UI list consumers.
  - Verification: `rg` trên `app/src/main`/`app/src/test` không còn raw router/tick/status repository, `toCorePolicy`, `recordEncounter`, POGO adapter/GameBuild imports hoặc app `.send(AutomationCommand)` caller; lần compile đầu phát hiện và sửa một annotation `override` còn sót ở `receiveEvents()`, sau đó `./gradlew :app:testDebugUnitTest :app:compileDebugKotlin --rerun-tasks` → BUILD SUCCESSFUL (33 tasks); `git diff --check` sạch; không đổi test source.

  </details>

- [x] **T-018** **[core]** *(coder)* — Dọn planner/runner chết theo symbol-level dependency map. **AC:** AC-06, AC-RF-01, AC-07. **Phụ thuộc:** T-017. **Verify:** focused core/adapter/bridge build/test đạt; không consumer bị mất, không test diff.

  Candidate xóa: `C/automation/AutomationRunner.kt`, planner trong `AutomationCoordinator.kt`, `CatchPlanner.kt`, `automation/modules/EncounterActionPlanner.kt`, `ModuleActionPlanner.kt`, `ActionExecutionValidation.kt` khi không còn caller repo.
  Tách `AutomationSnapshot` sang file model riêng cùng package nếu `ActionExecution`/adapter vẫn dùng. Rà return type/helper/export từng file; giữ DTO/enums/constants/action contract dùng bởi UI, bridge, fake/pogo adapter. Không xóa `C/location`, `C/time`, `GeoPoint`, `AutoFortNavigationCommand`.
  Rà `B/RuntimeSessionManager.kt`: chỉ xóa nếu mới không dùng và không consumer; nếu dùng cho generic transport guards thì giữ, bỏ reasoning gameplay nếu có. Candidate còn consumer phải ghi quyết định giữ và nguồn trong inventory.
  Không xóa toàn module `game-adapter/*`, không đổi test để hợp với cleanup, không blanket-delete folder.

  <details>
  <summary>Đã thực hiện — T-018</summary>

  - Xóa core dead planner/runner: `AutomationRunner.kt`, `AutomationCoordinator.kt` (kèm `AutomationSnapshot` chỉ dùng bởi planner), `CatchPlanner.kt`, `automation/modules/EncounterActionPlanner.kt`, `ModuleActionPlanner.kt`, `ActionExecutionValidation.kt` và `AutomationPolicy.kt`.
  - Tách các consumer còn sống khỏi file execution cũ: tạo `ActionRequest.kt` cho contract mà `game-adapter/api` và `game-adapter/pogo` submit; tạo `RuntimeIdentity.kt` cho `bridge/protocol` identity conversion; tạo `AutomationTimingConstants.kt` giữ các hằng số `DEFAULT_*_SETTLE_DELAY_MS`/`MAX_SETTLE_DELAY_MS` mà app config còn dùng. Không xóa `AutomationAction`/outcome/throw/berry enums, action codec hoặc location/time models.
  - Symbol review: không còn production consumer của planner/runner/AutomationPolicy/AutomationSnapshot; `ActionRequest` còn api/pogo callers và `RuntimeIdentity` còn bridge caller, đúng dependency map. Không đổi test source.
  - Verification: `./gradlew :core:compileKotlin :bridge:protocol:compileKotlin :game-adapter:api:compileKotlin :game-adapter:fake:compileKotlin :game-adapter:pogo:compileKotlin :app:compileDebugKotlin --rerun-tasks` → BUILD SUCCESSFUL (15 tasks); `./gradlew :core:test :bridge:protocol:test :game-adapter:api:test :game-adapter:fake:test :game-adapter:pogo:test --rerun-tasks` → BUILD SUCCESSFUL (16 tasks; bridge/pogo test task NO-SOURCE); `git diff --check` sạch. Không có sai khác so với plan ngoài việc tách DTO sống để giữ adapter/bridge callers.

  </details>

- [x] **T-019** **[build]** *(coder)* — Gỡ POGO/protobuf dependency khỏi runtime APK có kiểm chứng. **AC:** AC-02, AC-RF-01. **Phụ thuộc:** T-017, T-018. **Verify:** `debugRuntimeClasspath` không còn :game-adapter:pogo/:api và POGOProtos; app và các module còn giữ build/test đạt.

  Sửa `app/build.gradle.kts`: bỏ app `implementation` vào `:game-adapter:pogo` và `:game-adapter:api` khi T-017 đã loại imports; thay wildcard app/libs bằng tập runtime còn thực sự dùng hoặc exclude chính xác.
  Rà bytecode/metadata/callers của local libraries và `./gradlew :app:dependencies --configuration debugRuntimeClasspath` trước khi bỏ protobuf; nếu thư viện UI còn cần một dependency thì ghi nguồn và giữ phần đó, không kết luận chỉ từ việc không có import trực tiếp.
  Giữ vendor files, CI checksums, `THIRD_PARTY_NOTICES`, `game-adapter/pogo` compileOnly và settings module declarations. Gỡ library khỏi app không đồng nghĩa xóa khỏi repository.
  Không tạo module lớn hoặc chuyển language để né boundary; Kotlin app vẫn phụ thuộc core/bridge cho DTO/location.

  <details>
  <summary>Đã thực hiện — T-019</summary>

  - Sửa `app/build.gradle.kts`: bỏ `implementation(project(":game-adapter:api"))`, `implementation(project(":game-adapter:pogo"))`, direct `protobuf-java` và wildcard toàn bộ `app/libs`; giữ duy nhất `libs/virtualjoystick-1.10.1.aar` vì `JoystickPadView.kt` import `io.github.controlwear.virtual.joystick.android.JoystickView`.
  - Không xóa vendor files: `POGOProtos-2.60.8.jar`, `bcpkix-jdk15on-1.60.jar`, `bcprov-jdk15on-1.60.jar`, `parser-1.6.0.aar` vẫn trong `app/libs` cho checksum/vendor/adapter repository; `game-adapter/*` và settings module declarations vẫn giữ.
  - Dependency evidence: trước sửa `debugRuntimeClasspath` có `:game-adapter:api`, `:game-adapter:pogo` và `com.google.protobuf:protobuf-java:3.23.0`; sau sửa chỉ còn `:core`, `:bridge:protocol`, Kotlin stdlib và không có POGO/protobuf.
  - Verification: `./gradlew :app:compileDebugKotlin :app:testDebugUnitTest :core:test :bridge:protocol:test :game-adapter:api:test :game-adapter:fake:test :game-adapter:pogo:test --rerun-tasks` → BUILD SUCCESSFUL (35 tasks; core/adapter/bridge test tasks không có source được ghi nhận); app compile và UI joystick dependency vẫn resolve; `git diff --check` sạch; không đổi test source.

  </details>
