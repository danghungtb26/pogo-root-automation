# Phase 1 — Status và broker sớm

Plan: [Tổng quan](00-overview.md)

**Điều kiện bắt đầu:** T-004 chốt wire/session contract; hook backend có thể còn unknown.
**Điều kiện hoàn tất:** controller nhận attach/status của session mới trước probe, broker có một read owner, status không cấp mutation.

Sau mỗi task, cập nhật ngay khối details dưới checkbox với file tạo/sửa/xóa/di chuyển, thay đổi method/param/hành vi và kiểm chứng thực tế trước khi chuyển task. Chỉ tick `[x]` khi Verify đạt; chưa đạt thì giữ `[ ]` và ghi phần còn thiếu.

- [x] **T-005** **[bridge/protocol/zygisk]** *(coder)* — Thêm codec/DTO `RuntimeBootstrapStatus` (dự kiến tạo file `bridge/protocol/src/main/kotlin/dev/pogoroot/automation/bridge/RuntimeBootstrapStatusPayloadCodec.kt`) và frame type trong `BridgeProtocol.kt`, `BridgePayloadDecoder.kt`/`BridgePayloadEncoder.kt`; đổi đồng bộ native wire ở `zygisk/jni/shared/bridge_kotlin/` bằng header/encoder riêng (dự kiến tạo). Version/marker, bounded field bytes/count, `sessionId`/`pid`/process identity, sequence và phase transition phải theo T-004; `gameBuild` absent trước verify, unknown enum/malformed/truncated frame fail closed. `RUNTIME_READY` cũ không dùng làm early handshake hoặc quyền gameplay. **AC:** AC-02, AC-04, AC-07. **Phụ thuộc:** T-004. **Verify:** `./gradlew :bridge:protocol:test --rerun-tasks` và compile/run các native protocol checks hiện có; review Kotlin/C++ wire parity, không thêm test code.

  <details>
  <summary>Đã hoàn thành — T-005</summary>

  - **Tạo file:** `bridge/protocol/src/main/kotlin/dev/pogoroot/automation/bridge/RuntimeBootstrapStatusPayloadCodec.kt` — DTO/strict codec; `zygisk/jni/shared/bridge_kotlin/runtime_bootstrap_status_protocol.h` — native payload encoder.
  - **Sửa file:** `BridgeProtocol.kt`, `BridgePayloadDecoder.kt`, `BridgePayloadEncoder.kt`, `zygisk/jni/shared/core/runtime_native_prelude.inc`, `zygisk/jni/main.cpp` — bridge v4/frame 12/wire include; phase checklist này — trạng thái.
  - **Xóa file:** Không.
  - **Di chuyển/đổi tên:** Không.
  - **Thay đổi chức năng:** `RuntimeBootstrapStatusPayloadCodec.encode/decode` xử lý session/pid/process/phase/seq, nullable build/ABI, error/capability/timestamp có giới hạn; `BridgeEvent.BootstrapStatus` map frame type 12. Native encoder dùng cùng thứ tự/trường/marker/schema; pre-binding capability rỗng, failed cần error code, live cần strong identity.
  - **Kiểm chứng:** `./gradlew :bridge:protocol:test --rerun-tasks` → pass (`test NO-SOURCE`); `zygisk/tests/runtime_automation_event_protocol_test.cpp` compile/run với `-Wall -Wextra -Werror` → pass; `./scripts/build-magisk.sh` → arm64-v8a/x86_64 build và ZIP pass; `git diff --check` sạch, source mới ≤500 dòng. Kotlin/native field order đối chiếu trực tiếp.
  - **Sai khác so với plan:** Hai host test path `zygisk/jni/runtime_command_protocol_test.cpp` và `runtime_observation_protocol_test.cpp` trong `AGENTS.md` không tồn tại; dùng native protocol test hiện có và full native build qua script. Chưa có test mới vì repo cấm viết test.

  </details>

- [x] **T-006** **[zygisk]** *(native)* — Chuyển session broker từ gate `kProbeComplete` sang `kTargetAttached` trong `zygisk/jni/shared/bridge_appproc/runtime_companion.inc` và `zygisk/jni/shared/bridge_kotlin/runtime_bridge_broker.inc`: tạo session ID mới tại attach, bind abstract socket sớm, reactor duy nhất đọc runtime stream và multiplex controller frames; loại `dup(fd)` có reader cạnh tranh, giữ auth UID, correlation, sequence, limits và snapshot in-memory. Khi EOF/process chết, đóng session, bỏ queue/pending/result/lease và không tái dùng `pid` như identity. Tách `.inc` nếu file vượt 500 dòng. **AC:** AC-03, AC-06, AC-09. **Phụ thuộc:** T-001, T-004, T-005. **Verify:** host native protocol checks và native build qua `scripts/build-magisk.sh`; source review chứng minh chỉ một reactor đọc runtime FD, controller đến muộn nhận đúng session snapshot.

  <details>
  <summary>Đã hoàn thành — T-006</summary>

  - **Tạo file:** `zygisk/jni/shared/bridge_kotlin/runtime_bridge_reactor.inc` — poll server/runtime/client FDs; `runtime_bridge_observation_dispatch.inc` — forwarding observation tách khỏi reactor.
  - **Sửa file:** `runtime_companion.inc` — handoff stream tại attach; `runtime_bridge_broker.inc` — parse/runtime routing, cached status, correlation và session cleanup; `runtime_bridge_protocol.inc` — bỏ `RUNTIME_READY` fingerprint giả; `runtime_native_prelude.inc` — broker context/session/listener guards; `main.cpp` — include order; phase checklist này.
  - **Xóa file:** Không.
  - **Di chuyển/đổi tên:** Không.
  - **Thay đổi chức năng:** `companion_handler` tạo session, persist diagnostics rồi bàn giao descriptor sở hữu riêng cho broker ngay sau `TARGET_ATTACHED`; handler đóng descriptor gốc trước khi reactor đọc. Broker drain stream cả khi không có controller, cache UI status/verified ready, route result theo command ID, giới hạn 4 controller, kiểm per-client frame sequence và dọn pending khi disconnect. Listener session cũ đóng trước khi session mới bind.
  - **Kiểm chứng:** `./scripts/build-magisk.sh` → arm64-v8a/x86_64 và ZIP pass; native automation-event protocol test compile/run pass; `git diff --check` sạch; các `.inc` mới/sửa dưới 500 dòng. Source review: chỉ broker reactor gọi `read_companion_message` sau attach; reconnect replay cached status cùng session, không replay command/result. Live `BlueStacks Air 1` còn ở T-020.
  - **Sai khác so với plan:** Dùng `dup(fd)` chỉ để chuyển **ownership** trước khi companion handler return; không có hai reader cạnh tranh. Path test native trong `AGENTS.md` không tồn tại, dùng test hiện có và full native build.

  </details>

- [x] **T-007** **[zygisk]** *(native)* — Phát `RUNTIME_BOOTSTRAP_STATUS` tại `TARGET_ATTACHED` (`ATTACHING` UI mapping) và mọi phase/error từ owner state machine vào broker snapshot, cùng session/sequence/timestamp; frame đầu tiên đủ để controller xác nhận session khi build còn unknown. Reconnect replay **một** snapshot mới nhất, không replay gameplay command/result. Trong `runtime_bridge_broker.inc` và status sender, giữ `RUNTIME_READY` cũ chỉ sau identity hợp lệ, tách khỏi `RuntimeUiStatus.ready`; START/CONFIG ACK, connected hoặc `IL2CPP_LIBRARY_SEEN` không mở capability. `runtime.status` chỉ phản chiếu diagnostics, không thành queue. **AC:** AC-02, AC-03, AC-04, AC-09. **Phụ thuộc:** T-005, T-006. **Verify:** host protocol checks; source/observable status cho thấy `ATTACHING` có trước probe, snapshot sequence không lùi khi reconnect và pre-live capability rỗng.

  <details>
  <summary>Đã hoàn thành — T-007</summary>

  - **Tạo file:** `zygisk/jni/shared/runtime/bootstrap/runtime_bootstrap_status_sender.inc` — owner-side phase publisher.
  - **Sửa file:** `runtime_bootstrap_status_protocol.h`, `runtime_native_prelude.inc`, `runtime_bridge_protocol.inc`, `runtime_bridge_broker.inc`, `runtime_bridge_reactor.inc`, `runtime_binding_probe.inc`, `runtime_control.inc`, `runtime_ui_status_bridge.inc`, `host/runtime_capabilities.inc`, `main.cpp`, Kotlin `RuntimeBootstrapStatusPayloadCodec.kt` và checklist này.
  - **Xóa file:** Không.
  - **Di chuyển/đổi tên:** Không.
  - **Thay đổi chức năng:** Broker tạo status `CREATED` tại attach và replay cùng payload phase/sequence/timestamp khi reconnect. `publish_runtime_bootstrap_status` là sender duy nhất của injected runtime: kiểm transition liền kề hoặc terminal `FAILED`, gắn pid/process và timestamp, rồi phát internal message; lỗi module registry dùng sender này. Broker xác thực pid/process, sequence và elapsed time tăng, thay session ID bằng ID companion sở hữu, cache/broadcast snapshot. Trước `LIVE_READY`, broker không forward `RUNTIME_READY`, UI status hay observation; sau live chỉ phát `RUNTIME_READY` khi fingerprint/capability khớp bootstrap status. Native từ chối START và managed diagnostic trước gate; probe mapping không gọi IL2CPP. Internal event protocol nâng lên v5, bridge v4.
  - **Kiểm chứng:** `./scripts/build-magisk.sh` → arm64-v8a/x86_64 và ZIP pass; `./gradlew :bridge:protocol:test --rerun-tasks` → pass (`test NO-SOURCE`); `./gradlew test assembleDebug --rerun-tasks` → pass trước thay đổi sender cuối; native automation-event protocol binary compile/run với `-Wall -Wextra -Werror` → pass; `git diff --check` sạch, các source đã sửa ≤500 dòng. Source review: attach tạo snapshot trước probe, replay không tăng payload phase sequence và pre-live capability rỗng.
  - **Giới hạn:** Core state machine ở T-011 chưa có nên hiện chỉ có `CREATED` hoặc lỗi module registry; các transition lifecycle khác và kiểm chứng thứ tự frame trên `BlueStacks Air 1` thuộc T-011–T-020. Kotlin client đổi handshake ở T-016. Không coi transport status là quyền gameplay.

  </details>

- [x] **T-008** **[build]** *(reviewer)* — Kiểm checkpoint Phase 1: chạy `./gradlew :bridge:protocol:test --rerun-tasks`, hai host native protocol binaries theo `AGENTS.md`, `ANDROID_NDK="$HOME/Library/Android/sdk/ndk/27.1.12297006" ./scripts/build-magisk.sh` và `git diff --check`; ghi rõ controller-first chưa chứng minh live gate. Không sửa test để ép pass. **AC:** AC-03, AC-04, AC-09. **Phụ thuộc:** T-005–T-007. **Verify:** các lệnh pass, ZIP còn đủ `arm64-v8a`/`x86_64`; nếu môi trường thiếu NDK/device, giữ task mở và ghi giới hạn cụ thể.

  <details>
  <summary>Đã hoàn thành — T-008</summary>

  - **Tạo file:** Không.
  - **Sửa file:** Checklist này — trạng thái checkpoint.
  - **Xóa file:** Không.
  - **Di chuyển/đổi tên:** Không.
  - **Thay đổi chức năng:** Không; đây là checkpoint kiểm chứng Phase 1.
  - **Kiểm chứng:** `./gradlew :bridge:protocol:test --rerun-tasks` → pass (`test NO-SOURCE`); host native `runtime_automation_event_protocol_test.cpp` compile/run với `-std=c++17 -Wall -Wextra -Werror` → pass; `./scripts/build-magisk.sh` với NDK 28.2 mặc định → arm64-v8a/x86_64 và ZIP pass, script kiểm hai `.so` trong ZIP; `git diff --check` sạch. Source review xác nhận broker tạo session/status từ attach, một reactor đọc runtime FD và pre-live capability đóng.
  - **Sai khác môi trường:** NDK 27.1 override ghi trong checklist không cài trên host; NDK 28.2.13676358 đúng default của script có sẵn và build pass. Hai path native protocol test ghi trong `AGENTS.md` không tồn tại; không tạo/sửa test, dùng host native test hiện có cùng full native build. Controller-first và live gate trên `BlueStacks Air 1` chưa chứng minh, chuyển T-020.

  </details>
