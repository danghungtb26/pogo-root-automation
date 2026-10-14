# Phase 4 — Controller hội tụ theo session

Plan: [Tổng quan](00-overview.md)

**Điều kiện bắt đầu:** status/early broker T-007 có contract ổn định; lifecycle gate có thể đang chờ verification.
**Điều kiện hoàn tất:** service sống qua game chưa mở/socket transient, UI hiển thị phase thật, reconnect resubmit full desired snapshot mà không arm/replay action.

Sau mỗi task, cập nhật ngay khối details dưới checkbox với file tạo/sửa/xóa/di chuyển, thay đổi method/param/hành vi và kiểm chứng thực tế trước khi chuyển task. Chỉ tick `[x]` khi Verify đạt; chưa đạt thì giữ `[ ]` và ghi phần còn thiếu.

- [ ] **T-016** **[app]** *(android)* — Biến `RuntimeUiClient`/`RuntimeBridgeClient.connect()` và `HeadlessAutomationService` thành connection supervisor: socket unavailable/game chưa chạy/session turnover là transient với bounded backoff, không `stopSelf()` do connect sớm thất bại; vẫn giữ nhánh stop do user `ACTION_STOP_SERVICE` và điều kiện module install không hợp lệ. Đổi return contract `connect()` từ `Result<BridgeEvent.RuntimeReady>` sang session handle/`RuntimeBootstrapStatus` (**đề xuất — chưa tồn tại**) để không đợi verified build; cập nhật mọi caller/submission path dùng session ID đã authenticate, còn `RUNTIME_READY` chỉ compatibility. Reader reset state khi EOF và reconnect với auth UID, sequence/freshness mới. Nếu file Kotlin có nguy cơ vượt 500 dòng, tách supervisor ra file mới thay vì phình `RuntimeBridgeClient.kt`. **AC:** AC-08; hỗ trợ AC-03/AC-06. **Phụ thuộc:** T-005, T-007. **Verify:** `./gradlew test assembleDebug --rerun-tasks`; controller mở khi game chưa chạy giữ service và trạng thái waiting, khi game mở sau tự hội tụ, không spam retry; không thêm/sửa test code.

  <details>
  <summary>Đã triển khai handshake/retry trên host; T-016 còn mở</summary>

  - **Thay đổi:** `RuntimeBridgeClient.connectSession()` nhận bootstrap status của broker mà không chờ `RUNTIME_READY`; `RuntimeUiClient.connect()` và `RuntimeUiEventRouter.ensureSession()` dùng `sessionId/pid/processName` từ status. `RuntimeBridgeClient.connect()` cũ vẫn phục vụ compatibility code. `RuntimeUiAutomationFacade.pollRuntime()` retry 0.5–8 giây sau lỗi kết nối; `HeadlessAutomationService` giữ foreground service khi runtime mất, vẫn dừng theo user action/module install invalid. Reader clear socket/session khi reconnect; store xóa ready/observation cũ khi session đổi. Desired snapshot trước `BUILD_VERIFIED` dùng fingerprint rỗng; Kotlin/native codec và native identity guard chấp nhận rỗng chỉ dưới gate, không dựng fingerprint giả.
  - **Kiểm chứng:** `./scripts/build-magisk.sh` pass arm64-v8a/x86_64; `./gradlew :bridge:protocol:compileKotlin :app:compileDebugKotlin --rerun-tasks` và `./gradlew test assembleDebug --rerun-tasks` pass; `git diff --check` pass.
  - **Còn thiếu:** startup/reconnect thật trên `BlueStacks Air 1` chưa chạy; chưa tick Verify.

  </details>

- [ ] **T-017** **[app]** *(android)* — Route `RuntimeBootstrapStatus` ở `app/src/main/java/dev/pogoroot/automation/runtime/observation/RuntimeUiEventRouter.kt`, `app/src/main/java/dev/pogoroot/automation/runtime/RuntimeUiStateStore.kt`, `app/src/main/java/dev/pogoroot/automation/service/RuntimeUiAutomationFacade.kt` và notification của `HeadlessAutomationService.kt` sang `WAITING_FOR_GAME`, `ATTACHING`, `INITIALIZING`, `VERIFYING`, `LIVE_READY`, `FAILED`; phase `FAILED_*` hiển thị error code/recoverable, không tự diễn giải raw game state. Pre-build status chỉ so session/pid/process/sequence/freshness; khi strong identity xuất hiện mới so exact build fingerprint cho live status/observation. `RuntimeUiStatus.ready && strongIdentityVerified` vẫn là display/action guard, không lấy phase transport làm quyền mutation. **AC:** AC-04, AC-08. **Phụ thuộc:** T-005, T-016. **Verify:** Gradle app tests/build hiện có; manual UI trên `BlueStacks Air 1` thấy waiting/attach/fail/live theo source status, stale/wrong-session status bị bỏ.

  <details>
  <summary>Đã triển khai route/display trên host; T-017 còn mở</summary>

  - **Thay đổi:** `RuntimeUiEventRouter` kiểm session, sequence, phase tiến và timestamp hợp lệ cho bootstrap replay; chỉ nhận `RUNTIME_READY` khi phase `LIVE_READY`, strong identity, PID/process/build/capability khớp và còn fresh. `RuntimeUiStateStore` giữ bootstrap trong session, xóa ready/native status/observation khi phase không live hoặc session đổi. `RuntimeUiAutomationFacade.snapshot()` map phase thành `WAITING_FOR_GAME`, `ATTACHING`, `INITIALIZING`, `VERIFYING`, `LIVE_READY`, `FAILED`, trình bày error code/recoverable; mutation guard vẫn dựa trên native ready + strong identity. `HeadlessAutomationService` cập nhật notification khi trạng thái đổi.
  - **Kiểm chứng:** `./gradlew test assembleDebug --rerun-tasks` pass; source ≤500 dòng; `git diff --check` pass.
  - **Còn thiếu:** UI phase và reject stale/wrong-session trên `BlueStacks Air 1` chưa quan sát; chưa tick Verify.

  </details>

- [ ] **T-018** **[app/zygisk]** *(android)* — Sửa `RuntimeUiAutomationFacade`/`RuntimeUiClient` để mỗi session mới hoặc reconnect cùng session gửi **full** desired snapshot/revision từ `AutomationConfigRepository`; ACK chỉ ghi nhận nhận lệnh, applied status từ native mới xác nhận apply. Session turnover xóa stale UI/location handoff, không replay gameplay command cũ, không auto-arm; native reconcile giữ guard config/lease/arm và không kế thừa pending action của process cũ. Kotlin vẫn chỉ điều khiển fake-location theo exception của `AGENTS.md`. **AC:** AC-04, AC-06, AC-08. **Phụ thuộc:** T-006, T-007, T-016. **Verify:** focused app/bridge Gradle checks và runtime matrix controller kill/reopen/game restart: revision áp đúng session, action command cũ không phát lại, gameplay chỉ arm sau user intent mới.

  <details>
  <summary>Đã triển khai resubmit/disarm trên host; T-018 còn mở</summary>

  - **Thay đổi:** `RuntimeUiClient` giữ full desired snapshot mới nhất; reconnect cùng session resubmit snapshot, session mới xóa status cũ và ép `catchSpinArmed=false` trước khi gửi, `RuntimeUiAutomationFacade` tiếp tục lấy revision từ `AutomationConfigRepository` và disarm khi router báo đổi/mất session. `RuntimeBridgeClient` chỉ gửi desired state với session hiện tại từ bootstrap; ACK chỉ là receipt, không mở mutation. Broker/runtime vẫn có session/revision/expiry/arm guards.
  - **Kiểm chứng:** `./gradlew test assembleDebug --rerun-tasks` và build native hai ABI pass; không sửa test code.
  - **Còn thiếu:** matrix controller kill/reopen/game restart và applied-status trên `BlueStacks Air 1` chưa chạy; chưa tick Verify.

  </details>
