# Phase 2 — Native sở hữu runtime và config application

Plan: [Tổng quan](00-overview.md)

Điều kiện bắt đầu: T-006 đạt, phase 0 đã chốt semantics; chưa bật cả owner Kotlin/native cho cùng session.
Điều kiện hoàn tất: native xử lý full desired state đến applied/ready hoặc error; status có nguồn native; reconnect không tái chạy mutation cũ.

Sau mỗi task, cập nhật ngay khối details dưới checkbox với file tạo/sửa/xóa/di chuyển, thay đổi method/param/hành vi và kiểm chứng thực tế trước khi chuyển task. Chỉ tick [x] khi Verify đạt; chưa đạt thì giữ [ ] và ghi phần còn thiếu. Không viết/sửa/xóa/disable test. Alias A/, B/, C/, P/, N/ được định nghĩa trong [inventory](../refactor-inventory.md#2-quy-ước-đường-dẫn).

- [x] **T-007** **[zygisk]** *(native)* — Thêm kho desired-state trong RAM và đường nhận intent. **AC:** AC-01, AC-04, AC-RF-03. **Phụ thuộc:** T-006. **Verify:** source review chứng minh intent chưa ready không invoke IL2CPP/mutation; build multi-ABI đạt.

  Dự kiến tạo `N/shared/runtime/control/runtime_desired_state.inc` với parser handler do `N/modules/core/module.inc` sở hữu. API đề xuất — chưa tồn tại: `bool accept_runtime_desired_state(ProbeContext& context, const RuntimeDesiredStateRequest& request)`; context từ command loop, request từ parser T-005.
  Sửa `runtime_control.inc` routing và `main.cpp` include. Broker phải auth/session-check trước forward; handler validate identity/expiry/revision/data trước lưu.
  Tách desired snapshot khỏi applied feature configs, không để `activate_runtime` reset mất intent vừa nhận. Không persist native config ra disk. Duplicate idempotent; stale/conflicting snapshot từ chối theo T-003; chỉ giữ latest complete snapshot theo session.

  <details>
  <summary>Đã thực hiện — T-007</summary>

  - Tạo `zygisk/jni/shared/runtime/control/runtime_desired_state.inc`: thêm `accept_runtime_desired_state(ProbeContext&, const pogo_runtime_desired_state::Request&)`, RAM-only `State` snapshot theo session, modular revision gate, duplicate idempotent, stale/conflict rejection, TTL/identity validation và `desired_state_received` receipt ở phase `ACCEPTED`. Snapshot API chỉ copy dữ liệu trong mutex để T-008 reconcile dùng; không persist ra disk.
  - Sửa `zygisk/jni/shared/bridge_kotlin/runtime_desired_state_protocol.h`: thêm control action wire `9` cho `DESIRED_STATE_SET` (giữ `7` cho legacy transfer CONFIG_SET).
  - Sửa `zygisk/jni/modules/core/module.inc`: runtime-core sở hữu action `7` và gọi handler; sửa tham số để chuyển nguyên frame vào receiver.
  - Sửa `zygisk/jni/shared/runtime/control/runtime_control.inc` và `zygisk/jni/main.cpp`: route desired snapshot trước CONFIG_SET/gameplay, giữ broker hiện có làm lớp auth/session-check trước forward. `accept_runtime_desired_state` chỉ parse/validate/mutex-copy/socket receipt; không gọi `RuntimeBinding`, IL2CPP, map probe, mutation hay module enable/apply. `activate_runtime`/`stop_runtime` không reset store này vì store tách khỏi ba applied-config mirror hiện tại.
  - Build: `ANDROID_NDK="/Users/admin/Library/Android/sdk/ndk/28.2.13676358" ./scripts/build-magisk.sh` → BUILD thành công cho `arm64-v8a` và `x86_64`, tạo `build/pogo-root-automation-magisk-multiabi.zip`. Lần compile đầu bắt được hai lỗi ownership/context trong source; đã sửa rồi chạy lại thành công. `git diff --check` sạch; không thêm/sửa/xóa test.

  </details>

- [x] **T-008** **[zygisk]** *(native)* — Chuyển trình tự START/readiness/apply/STOP vào native reconciliation. **AC:** AC-01, AC-04, AC-06, AC-RF-03. **Phụ thuộc:** T-007. **Verify:** native có một owner và observer không thấy partial revision; existing readiness host test + multi-ABI build đạt.

  Dự kiến tạo `N/shared/runtime/control/runtime_desired_state_reconcile.inc`; API đề xuất — chưa tồn tại: `void reconcile_runtime_desired_state(ProbeContext& context)` được command/map-readiness tick gọi, không worker managed mới tùy tiện.
  Sửa `runtime_control.inc`, `runtime_map_maintenance.inc`, `shared/runtime/module/runtime_managed_config.inc`, `runtime_module_autostart.inc`, `modules/{catch_spin,transfer,discard}/config.inc` để reuse validation/apply helpers mà không phải giả lập ba command từ Kotlin.
  STOP/disable chặn dispatch mới và xử lý pending outcome theo cơ chế hiện có; stale completion không được override generation/config mới. Đợi exact identity/build/capability/map-ready rồi publish config đồng bộ dưới revision gate; không chỉ ghi ba mutex lần lượt trong khi observer vẫn đọc.
  Native giữ default policy đúng hành vi hiện có; không bật feature thiếu binding, không thay thread/owner game. Việc START/STOP legacy còn cần đến đâu theo T-003/T-010, không sinh owner thứ hai.

  <details>
  <summary>Đã thực hiện — T-008</summary>

  - Tạo `zygisk/jni/shared/runtime/control/runtime_desired_state_reconcile.inc`: thêm `reconcile_runtime_desired_state(ProbeContext&)` và direct mirror apply cho catch/discard/transfer. Reconcile chỉ commit snapshot enabled sau khi native đã active, còn hạn, đúng build và map-live readiness; sau đó dùng `sync_auto_enabled_modules` hiện có để gate capability/availability. Khi managed binding chưa sẵn sàng thì không apply/enable.
  - Yêu cầu `enabled=false` disable module bằng native và ghi cả ba config mirror trong một commit. Khi expiry, native fail-closed bằng cách reset module/config mirror nhưng giữ desired RAM snapshot; revision mới sẽ reconcile lại. `activate_runtime`/`stop_runtime` chỉ invalidate applied revision, không reset desired intent.
  - Сүрьеэгүй partial revision-ээс хамгаалах: `main.cpp`-д recursive applied-config commit mutex нэмсэн; `modules/{catch_spin,discard,transfer}/config.inc`-ийн snapshot/reset/legacy apply helper-үүд ижил lock хэрэглэнэ. Ингэснээр observer snapshot болон reconcile-ийн гурван mirror нэг revision-ээс өөр төлөв харахгүй.
  - `shared/runtime/control/runtime_map_maintenance.inc` одоо map readiness tick бүрт native reconcile дуудна; нэмэлт managed worker/хоёр дахь orchestration owner үүсгээгүй. Desired action wire `9` болгож, legacy transfer CONFIG_SET-ийн wire `7` мөргөлдөхөөс хамгаалсан.
  - Verification: `c++ -std=c++17 -Wall -Wextra -Werror -Izygisk/jni zygisk/tests/runtime_readiness_retry_test.cpp -o /tmp/runtime_readiness_retry_test && /tmp/runtime_readiness_retry_test` амжилттай. `ANDROID_NDK="/Users/admin/Library/Android/sdk/ndk/28.2.13676358" ./scripts/build-magisk.sh` → arm64-v8a/x86_64 build болон multi-ABI ZIP амжилттай; `git diff --check` цэвэр. Test source өөрчлөөгүй.

  </details>

- [x] **T-009** **[zygisk]** *(native)* — Phát status/event đủ cho UI từ state native. **AC:** AC-02, AC-05, AC-RF-02. **Phụ thuộc:** T-008. **Verify:** mọi status field có nguồn và unknown semantics; nhận/apply/ready khác nhau; host telemetry tests hiện có + native build đạt.

  Dự kiến tạo `N/shared/bridge_kotlin/runtime_ui_status_bridge.inc`, sửa `host/runtime_capabilities.inc`, `shared/runtime/observation/runtime_observation_senders.inc` và relevant control/module notification hooks. Status lấy từ native owner T-008; app không suy lifecycle từ NEARBY/ENCOUNTER nữa.
  Giữ `AUTOMATION_EVENT`, `NAVIGATION`, `MAP_TARGET` và IDs/freshness; throw diagnostic chuẩn hóa thành UI event/DTO theo T-003. Không đọc thêm game object chỉ để làm giàu UI.
  Phân biệt module registered với configured/ready; lỗi thiếu config/module phải hiển thị đúng, không đợi Kotlin coordinator mới cho phát status. Scan/result source chưa có thì báo unknown/unavailable; không thêm capture/scanner pipeline.

  <details>
  <summary>Đã thực hiện — T-009</summary>

  - Tạo `zygisk/jni/shared/bridge_kotlin/runtime_ui_status_bridge.inc`: `send_runtime_ui_status(ProbeContext&)` dựng status trực tiếp từ `RuntimeBinding`, `g_runtime_active`, desired snapshot, applied revision, config mirrors và module registry. Lifecycle phân biệt attached-idle/diagnostic-pending/binding-ready/applying/ready/error; module phân biệt registered/configured/ready/disabled/error và chỉ báo revision khi mirror đã configured.
  - Sửa `runtime_native_prelude.inc`, `runtime_bridge_protocol.inc`, `runtime_bridge_broker.inc`: thêm internal status magic, bounded length-prefixed forwarding và bridge message type `RUNTIME_STATUS`. Broker thay placeholder session bằng session đã auth của `BrokerContext` trước khi phát frame protocol 3; không để native process đoán session của companion.
  - Status capability list được dựng từ binding flags hiện có và sort/unique trước encode. `AUTOMATION_EVENT`, `NAVIGATION`, `MAP_TARGET` và observation IDs/freshness không bị thay đổi; status không đọc game object mới chỉ để làm già enriched UI. Status gửi sau START/STOP/DIAGNOSTIC, map-live transition và reconcile/expiry.
  - Verification: readiness host test và `runtime_automation_event_protocol_test` compile/run đều exit 0. `ANDROID_NDK="/Users/admin/Library/Android/sdk/ndk/28.2.13676358" ./scripts/build-magisk.sh` → arm64-v8a/x86_64 và multi-ABI ZIP thành công; `git diff --check` sạch. Không sửa test source.

  </details>

- [x] **T-010** **[zygisk]** *(native)* — Nối broker/status replay và xử lý reconnect theo lifecycle contract. **AC:** AC-04, AC-05, AC-RF-02. **Phụ thuộc:** T-002, T-009. **Verify:** broker tuân theo ma trận disconnect, auth/session được giữ, queue hữu hạn và không replay gameplay; build native đạt.

  Sửa `N/shared/bridge_kotlin/runtime_bridge_broker.inc`, `runtime_bridge_protocol.inc` và `shared/bridge_appproc/runtime_companion.inc` nếu cần để forward command/status mới, handshake đúng version và trả latest native status khi reconnect.
  Tách metadata/session ownership ở companion với readiness/gameplay ở runtime. Không chuyển IL2CPP hoặc feature policy vào broker.
  Giải quyết drain/backpressure khi không có UI theo policy T-002: nếu retain runtime state thì event buffers phải bounded; chỉ snapshot/status được resync, không replay command gameplay. Old control/config paths bị retire hoặc normalize vào cùng native owner theo T-003, không để writer cạnh tranh.
  Nếu policy app-kill còn chưa chốt, giữ task này chưa đạt và tiếp tục phần độc lập; không tự chọn continue/auto-resume. Native runtime mất channel phải giữ fail-closed behavior đã xác minh.

  <details>
  <summary>Đã thực hiện — T-010</summary>

  - Sửa `zygisk/jni/shared/core/runtime_native_prelude.inc`: `BrokerContext` giữ tối đa một `latest_runtime_ui_status` snapshot; không có command/gameplay queue.
  - Sửa `zygisk/jni/shared/bridge_kotlin/runtime_bridge_protocol.inc` và `runtime_bridge_broker.inc`: broker nhận internal status bounded theo hard message limit, cache latest status, inject lại session của broker khi forward và replay snapshot ngay sau `RUNTIME_READY` khi controller reconnect. UID auth, runtime session check trước forward và fail-closed khi runtime-fd disconnect vẫn được giữ.
  - Reconnect chỉ khôi phục UI status mới nhất; không replay command/result/observation mutation. Khi native runtime channel mất, broker không tiếp tục coi status cũ là tín hiệu gameplay authoritative.
  - Verification: `ANDROID_NDK="/Users/admin/Library/Android/sdk/ndk/28.2.13676358" ./scripts/build-magisk.sh` → arm64-v8a/x86_64 ZIP thành công; `./gradlew :bridge:protocol:test :bridge:protocol:build --rerun-tasks` → BUILD SUCCESSFUL (test NO-SOURCE, không đổi test source). Lần chạy đầu gặp lỗi permission của Gradle cache sandbox; đã rerun với escalation chỉ cho cache access. `git diff --check` sạch.

  </details>
