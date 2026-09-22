# Phase 0 — Chốt invariants và contract

Plan: [Tổng quan](00-overview.md)

Điều kiện bắt đầu: đọc brainstorm Tiếp nối 1–2, inventory và AGENTS; chưa chạy thao tác device chỉ vì có plan.
Điều kiện hoàn tất: T-001–T-003 đạt Verify; schema, danh sách setting và disconnect semantics có nguồn; phần thiếu evidence chặn đúng task phụ thuộc.

Sau mỗi task, cập nhật ngay khối details dưới checkbox với file tạo/sửa/xóa/di chuyển, thay đổi method/param/hành vi và kiểm chứng thực tế trước khi chuyển task. Chỉ tick [x] khi Verify đạt; chưa đạt thì giữ [ ] và ghi phần còn thiếu. Không viết/sửa/xóa/disable test. Alias A/, B/, C/, P/, N/ được định nghĩa trong [inventory](../refactor-inventory.md#2-quy-ước-đường-dẫn).

- [x] **T-001** **[docs]** *(reviewer)* — Chốt inventory caller/export và baseline kiểm chứng. **AC:** AC-RF-01, AC-07. **Phụ thuộc:** Không. **Verify:** inventory ghi đủ production/test consumer, export còn dùng và baseline thực tế; không có test diff.

  Rà `A/service/*`, `A/engine/*`, `A/runtime/*`, `A/root/*`, `A/config/AutomationPolicyBridge.kt`, `C/automation/*`, `P/BridgePogoRuntime.kt`, build files và `.github/workflows/ci.yml`; cập nhật `../refactor-inventory.md` khi có thay đổi mới.
  Lập bảng từng setting: UI key → native consumer hiện có → giữ/persist-only/unsupported. Đặc biệt `loopIntervalMs`, build allowlist, throw/berry/snapshot settings không được âm thầm nhận semantics mới.
  Ghi ràng buộc `RuntimeLifecycleCoordinatorTest` và toàn bộ dependency compatibility cần giữ. Lưu baseline `./gradlew :app:testDebugUnitTest --rerun-tasks` và focused tasks tồn tại; nếu task AGP khác tên, xác nhận bằng `./gradlew :app:tasks --all`. Lỗi môi trường ghi rõ, không đổi build/test để che lỗi. Baseline không chứng minh contract native mới.

  <details>
  <summary>Đã hoàn thành — T-001</summary>

  - **Tạo file:** Không.
  - **Sửa file:** `docs/issues/2026-09-22/kotlin-ui-only-boundary/refactor-inventory.md` — bổ sung caller/export baseline, test compatibility, bảng setting → consumer → trạng thái contract mới và giới hạn evidence.
  - **Xóa file:** Không.
  - **Di chuyển/đổi tên:** Không.
  - **Thay đổi chức năng:** Không; đây là inventory/review, không thay đổi source runtime hoặc test.
  - **Kiểm chứng:** `./gradlew :app:testDebugUnitTest --rerun-tasks` → `BUILD SUCCESSFUL`, 33 task executed; `git diff --name-only -- app/src/test` → không có test diff; `rg` toàn repo → xác nhận production callers, compatibility consumers và dependency app hiện tại.
  - **Sai khác so với plan:** Không.

  </details>

- [x] **T-002** **[zygisk]** *(native)* — Chốt lifecycle khi STOP, reconnect và process chết. **AC:** AC-04, AC-LOC-04, AC-RF-02. **Phụ thuộc:** T-001. **Verify:** ma trận lifecycle trong inventory ghi owner, reset/retain, hậu quả và nguồn cho từng case; các quyết định sản phẩm chưa rõ được nêu riêng.

  Rà `N/shared/runtime/control/runtime_control.inc:activate_runtime/stop_runtime/runtime_command_channel_loop`, `N/shared/bridge_kotlin/runtime_bridge_broker.inc:handle_controller`, `A/service/HeadlessAutomationService.kt:onDestroy`, `A/engine/HeadlessAutomationEngine.kt:shutdown`.
  Phân biệt UI ẩn, service explicit stop, socket tạm mất, app bị kill, game chết/đổi session, STOP trong lúc diagnostic/config pending. Ghi rõ broker hiện chỉ drain runtime khi có controller và nguy cơ backpressure.
  Mặc định bảo toàn behavior có bằng chứng, không biến refactor thành tính năng chạy độc lập khi app chết. Đề xuất policy mới (nếu bắt buộc) phải tách rõ; T-010 phụ thuộc phần policy đã chốt. Không tự force-stop bằng ADB. Khi cần live evidence mà script hiện có không hỗ trợ, ghi thiếu và tiếp tục phần độc lập; không tạo harness/test mới.

  <details>
  <summary>Đã hoàn thành — T-002</summary>

  - **Tạo file:** Không.
  - **Sửa file:** `docs/issues/2026-09-22/kotlin-ui-only-boundary/refactor-inventory.md` — thêm ma trận lifecycle STOP/reconnect/process death, owner/reset/retain/hậu quả, và các giới hạn evidence/backpressure.
  - **Xóa file:** Không.
  - **Di chuyển/đổi tên:** Không.
  - **Thay đổi chức năng:** Không; source review xác nhận explicit STOP reset native module/config state, game death reset runtime session state, controller disconnect hiện chưa tự STOP và broker không có bounded replay queue.
  - **Kiểm chứng:** `rg`/`sed` trên `runtime_control.inc`, `runtime_bridge_broker.inc`, `runtime_companion.inc`, `HeadlessAutomationService.kt`, `HeadlessAutomationEngine.kt`, `RuntimeBridgeClient.kt` → đối chiếu các nhánh stop, fd HUP, session creation, cleanup và reconnect; `git diff --check` → sạch.
  - **Sai khác so với plan:** Không; policy auto-STOP khi app kill được ghi là chưa được giao, nên giữ behavior hiện tại và đặt guard/revision/expiry mới ở T-003/T-008.

  </details>

- [x] **T-003** **[bridge/protocol]** *(coder)* — Chốt schema desired state/status và completion semantics. **AC:** AC-01, AC-02, AC-05, AC-RF-03. **Phụ thuộc:** T-001, T-002. **Verify:** tạo `../bridge-contract.md` với schema, version/marker, field order/type/unit, limits, compatibility matrix và lifecycle table; không còn chỗ implementation phải đoán.

  Đề xuất — chưa tồn tại: `RuntimeDesiredState` gồm enabled, revision và snapshot setting được native hỗ trợ; request bọc session/request ID, identity/expiry. `RuntimeUiStatus` gồm native lifecycle/capabilities/module states, desired/applied revision và error; transport state của app tách riêng.
  Định nghĩa nhận intent trước managed-ready chỉ lưu RAM; native độc lập xác minh exact build/ABI/binding/capability trước apply/mutation. Không mượn config ACK để báo action thành công.
  Chốt duplicate request, cùng revision cùng/khác nội dung, stale revision/expiry, STOP ưu tiên hơn apply đang đợi, config revision wrap, reconnect cùng/khác session, default policy và config chưa được hỗ trợ. Full snapshot phải publish đồng bộ; observer không được dùng nửa bộ config.
  Chọn protocol negotiation/version phù hợp hiện trạng decoder hai phía: mixed APK/module fail closed, không tự fallback sang Kotlin coordinator. Không chốt wire ID tùy ý trong task code. Event giữ automation/map-target/navigation; throw diagnostic có transport DTO thuần bridge. Scan chưa có producer thì giữ unavailable, không tạo feature mới.

  <details>
  <summary>Đã hoàn thành — T-003</summary>

  - **Tạo file:** docs/issues/2026-09-22/kotlin-ui-only-boundary/bridge-contract.md — nguồn schema desired-state/status hai phía, framing/version/marker, field order/type/unit, limits, revision/duplicate/expiry, receipt-vs-outcome, lifecycle và compatibility matrix.
  - **Sửa file:** Không.
  - **Xóa file:** Không.
  - **Di chuyển/đổi tên:** Không.
  - **Thay đổi chức năng:** Không; contract chốt protocol 3 dùng wire IDs hiện có (COMMAND=4, RUNTIME_STATUS=10), desired marker RDST, status marker RUST, full native-supported snapshot và native-owned status semantics.
  - **Kiểm chứng:** rg/sed trên BridgeProtocol.kt, payload codec/decoder/encoder, runtime_native_common.inc, các native config protocol và lifecycle source → đối chiếu version 2 hiện tại, hard limit 4 MiB, string/count guards, marker legacy, field consumers và fail-closed frame reader; review link/schema trong bridge-contract.md → không còn field order/receipt semantics cần implementation đoán.
  - **Sai khác so với plan:** Protocol 3 được chọn để mixed APK/module fail closed; wire message IDs cũ được giữ, không thêm fallback hoặc wire ID mới. Timestamps/status schema là native snapshot; unsupported UI settings vẫn persist-only theo T-001.

 </details>
