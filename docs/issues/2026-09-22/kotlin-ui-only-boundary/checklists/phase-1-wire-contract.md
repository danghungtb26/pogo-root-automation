# Phase 1 — Bổ sung contract hai phía

Plan: [Tổng quan](00-overview.md)

Điều kiện bắt đầu: phase 0 hoàn tất; `bridge-contract.md` là nguồn schema; production vẫn dùng đường cũ.
Điều kiện hoàn tất: codec Kotlin/C++ đồng bộ, không activate gameplay bằng command mới trước native owner.

Sau mỗi task, cập nhật ngay khối details dưới checkbox với file tạo/sửa/xóa/di chuyển, thay đổi method/param/hành vi và kiểm chứng thực tế trước khi chuyển task. Chỉ tick [x] khi Verify đạt; chưa đạt thì giữ [ ] và ghi phần còn thiếu. Không viết/sửa/xóa/disable test. Alias A/, B/, C/, P/, N/ được định nghĩa trong [inventory](../refactor-inventory.md#2-quy-ước-đường-dẫn).

- [x] **T-004** **[bridge/protocol]** *(coder)* — Thêm desired-state/status DTO và codec Kotlin theo contract. **AC:** AC-02, AC-05, AC-RF-03. **Phụ thuộc:** T-003. **Verify:** `./gradlew :bridge:protocol:test :bridge:protocol:build --rerun-tasks` đạt; review codec khớp schema, không Android/POGOProtos/game parser.

  Dự kiến tạo `B/RuntimeDesiredStatePayloadCodec.kt`, `B/RuntimeUiStatusPayloadCodec.kt` và tách model/helper nếu cần; sửa `BridgeProtocol.kt`, `BridgePayloadDecoder.kt`, `BridgePayloadEncoder.kt` cho message đã chốt.
  API đề xuất — chưa tồn tại: `RuntimeDesiredStatePayloadCodec.encode(request: RuntimeDesiredStateRequest): Result<ByteArray>` / `decode(payload: ByteArray): Result<RuntimeDesiredStateRequest>`; status tương tự với `RuntimeUiStatus`. Definition đầy đủ param/field theo T-003, không dùng Android `HeadlessAutomationConfig` làm wire type.
  Giữ size/version/string/enum/finite-number validation, session/seq envelope, unknown handling fail closed. Không đổi/remove wire IDs cũ đang dùng bởi compatibility code. Không viết test; test task có NO-SOURCE phải ghi đúng.

  <details>
  <summary>Đã hoàn thành — T-004</summary>

  - **Tạo file:** `bridge/protocol/src/main/kotlin/dev/pogoroot/automation/bridge/RuntimeDesiredStatePayloadCodec.kt` và `RuntimeUiStatusPayloadCodec.kt` — DTO, enum, encode/decode, field validation và hard limits theo contract.
  - **Sửa file:** `BridgeProtocol.kt` — nâng frame protocol lên 3 và thêm `BridgeEvent.RuntimeUiStatus`; `BridgePayloadDecoder.kt`/`BridgePayloadEncoder.kt` — route `RUNTIME_STATUS=10` qua status codec; `BridgePayloadCodecSupport.kt` — strict boolean helper.
  - **Xóa file:** Không.
  - **Di chuyển/đổi tên:** Không.
  - **Thay đổi chức năng:** Thêm `RuntimeDesiredStateRequest` full native-supported snapshot với marker RDST; thêm `RuntimeUiStatus`/module lifecycle/state với marker RUST; giữ message IDs cũ, reject marker/schema/trailing bytes/boolean/limit/finite-number sai và tách receipt/status khỏi gameplay outcome. Không import Android, POGOProtos hoặc game parser.
  - **Kiểm chứng:** `./gradlew :bridge:protocol:test :bridge:protocol:build --rerun-tasks` → `BUILD SUCCESSFUL`, 6 tasks executed; `git diff --check` → sạch; `wc -l` các source mới/sửa → tất cả dưới 500 dòng; test source không đổi. Lần chạy đầu phát hiện helper scope compile lỗi, đã sửa và chạy lại đạt.
  - **Sai khác so với plan:** Không; native parser/encoder và production wiring để T-005 trở đi, nên T-004 chưa claim desired command hoạt động runtime.

 </details>

- [x] **T-005** **[zygisk]** *(native)* — Thêm wire parser/encoder native đối xứng. **AC:** AC-04, AC-RF-03. **Phụ thuộc:** T-003. **Verify:** review byte order/field order/limits khớp T-004; build multi-ABI bằng script đạt.

  Dự kiến tạo `N/shared/bridge_kotlin/runtime_desired_state_protocol.h`, `runtime_ui_status_protocol.h` và helper `.inc` nếu cần; nối include trong `N/main.cpp` đúng thứ tự. API đề xuất — chưa tồn tại: `bool parse_desired_state(const std::vector<uint8_t>& command, RuntimeDesiredStateRequest* out)` và encoder status; types/fields theo contract T-003.
  Chỉ parse/encode, không gọi managed object, không bật module. Preserve broker envelope và version guards. Mọi source ≤500 dòng.
  Build: `ANDROID_NDK="$HOME/Library/Android/sdk/ndk/27.1.12297006" ./scripts/build-magisk.sh`. Chưa tuyên bố command mới hoạt động trên game chỉ từ build.

  <details>
  <summary>Đã hoàn thành — T-005</summary>

  - **Tạo file:** `zygisk/jni/shared/bridge_kotlin/runtime_desired_state_protocol.h` — parser RDST, full native-supported state, strict bool/f64/count/string/revision guards; `runtime_ui_status_protocol.h` — RUST status DTO và encoder theo schema.
  - **Sửa file:** `zygisk/jni/main.cpp` — include hai protocol header; `zygisk/jni/shared/core/runtime_native_prelude.inc` — nâng `kBridgeProtocolVersion` từ 2 lên 3 để frame mismatch fail closed.
  - **Xóa file:** Không.
  - **Di chuyển/đổi tên:** Không.
  - **Thay đổi chức năng:** Native parser/encoder dùng big-endian, marker/schema RDST/RUST, field order và limits khớp T-004/bridge-contract; parser chỉ parse/validate, status encoder chỉ tạo payload, chưa gọi managed object/bật module/reconcile. Wire IDs không đổi.
  - **Kiểm chứng:** `ANDROID_NDK="/Users/admin/Library/Android/sdk/ndk/28.2.13676358" ./scripts/build-magisk.sh` → build/package thành công arm64-v8a và x86_64, tạo `build/pogo-root-automation-magisk-multiabi.zip`; `wc -l` hai header → 163/182 dòng; `git diff --check` → sạch; test source không đổi. Lần chạy với NDK 27.1 theo plan không có thư mục NDK, đã dùng NDK 28.2 hiện có theo CI.
  - **Sai khác so với plan:** Không về scope; do NDK 27.1 không tồn tại nên dùng NDK 28.2.13676358 đã cài và build chính thức vẫn đạt. Command mới chưa được activate, sẽ do T-007 sở hữu.

 </details>

- [x] **T-006** **[bridge/protocol]** *(reviewer)* — Review tương thích protocol và tách receipt khỏi outcome. **AC:** AC-04, AC-05, AC-RF-03. **Phụ thuộc:** T-004, T-005. **Verify:** bảng đối chiếu từng field/branch và APK-module compatibility được ghi trong `../bridge-contract.md`; không mất guard cũ.

  Đối chiếu Kotlin/C++ trực tiếp: frame/payload version, max lengths, unsigned IDs, timestamps/expiry, identity/session correlation, duplicate/conflict, state chưa biết. Kiểm tra command mới không bị parser legacy nhận nhầm.
  Rà ba loại ACK/status: nhận desired snapshot, applied revision, module ready; gameplay event/outcome vẫn độc lập. Rà older/newer peers không thể silently chạy đường cũ.
  Chạy host protocol test có sẵn liên quan theo phase 5 nếu header dùng chung bị ảnh hưởng. Không tạo malformed-input harness hoặc test mới; ghi rõ nhánh mới chưa có automated coverage và sẽ cần review + device evidence.

  <details>
  <summary>Đã hoàn thành — T-006</summary>

  - **Tạo file:** Không.
  - **Sửa file:** `bridge/protocol/src/main/kotlin/dev/pogoroot/automation/bridge/BridgePayloadEncoder.kt` — từ chối encode `BridgeEvent.RuntimeStatus` legacy dưới protocol 3; `docs/issues/2026-09-22/kotlin-ui-only-boundary/bridge-contract.md` — ghi rõ RuntimeUiStatus/RUST là status path duy nhất và compatibility boundary.
  - **Xóa file:** Không.
  - **Di chuyển/đổi tên:** Không.
  - **Thay đổi chức năng:** Review đối chiếu Kotlin/C++ common prefix, protocol 3, message IDs 4/5/10, marker RDST/RUST, big-endian field order, 4 MiB/64 KiB/64/8 limits, strict bool, finite IV, identity/session/expiry/sequence và duplicate/stale/conflict semantics. Receipt ACCEPTED, desired/applied/ready status và gameplay outcome được tách; unknown/mixed peer fail closed. Desired parser chưa được route vào gameplay ở task này; T-007 sẽ dispatch trước legacy gameplay parser.
  - **Kiểm chứng:** `./gradlew :bridge:protocol:test :bridge:protocol:build --rerun-tasks` → `BUILD SUCCESSFUL`, test/build task đạt (test hiện `NO-SOURCE`); `rg` static đối chiếu constant/marker/field names → khớp contract; `git diff --check` → sạch. Không tạo malformed harness/test mới.
  - **Sai khác so với plan:** Không; coverage tự động cho codec mới chưa có theo rule không viết test, nên bằng chứng hiện tại là codec/build/static review; device/negative binary evidence để T-022.

 </details>
