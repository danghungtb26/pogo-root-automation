# Phase 1 — Core model và bridge contract

Plan: [Tổng quan](00-overview.md)

Điều kiện bắt đầu: T-001 contract đạt Verify; T-002 xác nhận source map.
Điều kiện hoàn tất: core có model/policy độc lập Android; Kotlin và C++ encode/decode cùng một point-candidate payload/version.

Sau mỗi task, kiểm tra diff và kết quả Verify, cập nhật ngay khối details dưới task trước khi chuyển tiếp. Chỉ tick `[x]` khi Verify đạt. Không tạo hoặc sửa test code theo `AGENTS.md`; dùng test hiện có và walkthrough state transition.

- [x] **T-003** **[core]** *(coder)* — Thêm model/policy thuần game-build trong `core/src/main/kotlin/dev/pogoroot/automation/core/location/WalkCandidate.kt` và `WalkCandidatePolicy.kt` [new]: biểu diễn candidate ID/GeoPoint/force, active owner/generation và quyết định accept/ignore/replace/reject. Policy nhận thời gian/sequence qua param để kiểm freshness lúc admission, không đọc clock, Android, bridge hoặc game state. Route accepted không hết hạn; same-ID/target giữ nguyên route không restart. **AC:** AC-01…AC-06. **Phụ thuộc:** T-001. **Verify:** `./gradlew :core:test`; rà state matrix idle, A+B false, A+B true, same-ID/target, USER owner, stale terminal; không tạo/sửa test.

  <details>
  <summary>Đã hoàn thành — T-003</summary>

  - Tạo `WalkCandidate.kt`: `NativeWalkCandidate`, `ActiveWalkRoute`, owner, decision outcomes và target-correlated terminal model.
  - Tạo `WalkCandidatePolicy.kt`: pure `submit`, `terminate`; explicit `nowNanos`, session/sequence/generation; kiểm fresh/stale, coordinate, ID và sequence. Admission idle nhận candidate; B non-force giữ A; same-ID/target trả `UNCHANGED` không đổi generation; force chỉ replace native route với generation tăng; USER owner luôn thắng; terminal sai session/ID/generation được bỏ qua. Route không có expiry/lease.
  - Review matrix bằng source walkthrough: IDLE→ACCEPTED; A+B false→IGNORED_BUSY (A giữ nguyên); A+B true→REPLACED (generation mới); same ID/target→UNCHANGED (executor không restart); USER+B force→REJECTED_OWNER_CONFLICT; late terminal A khi B active→IGNORED_STALE_TERMINAL; invalid/stale candidate không đổi route.
  - Sau làm rõ của người dùng, xóa `expiresAtNanos`, lease duration/validation và decision `RENEWED`/`EXPIRED` khỏi model/policy; active candidate được giữ tới terminal/local arrival hoặc owner/session/provider transition. Kotlin generation áp dụng cho cả route USER/NATIVE và không reset giữa session để chặn callback cũ.
  - Verify follow-up trên model sau thay đổi lifetime: `./gradlew test assembleDebug --rerun-tasks` exit 0; `:core:compileKotlin` thành công (`:core:test` NO-SOURCE); `git diff --check` sạch.
  - Verify: `./gradlew test assembleDebug` exit 0; log báo `:core:compileKotlin` thành công và `:core:test NO-SOURCE` (module hiện không có test source). State matrix đã được source-walkthrough. `git diff --check` exit 0; source files 67 và 205 dòng. Không tạo hoặc sửa test code.

  </details>

- [x] **T-004** **[bridge/protocol]** *(coder)* — Thêm payload/version mới trong `bridge/protocol/src/main/kotlin/dev/pogoroot/automation/bridge/PointWalkCandidatePayloadCodec.kt` [new] và observation discriminator/capability constant trong `BridgeProtocol.kt`. Payload bám T-001: candidate ID, tọa độ, `force`; giữ navigation v1 hiện tại để phase fort-walk sau tự migrate. Validate size, string bounds, coordinate finite/range, version/kind và trailing bytes. **AC:** AC-01, AC-03, AC-06, AC-07. **Phụ thuộc:** T-001. **Verify:** `./gradlew :bridge:protocol:test`; code review format Kotlin/C++ tương ứng trong contract; không thêm test cases.

  <details>
  <summary>Đã hoàn thành — T-004</summary>

  - Sửa `BridgeProtocol.kt`: thêm `POINT_WALK_CANDIDATE(12)` và capability token `POINT_WALK_CANDIDATE`; `NAVIGATION(11)`/navigation codec v1 giữ nguyên.
  - Tạo `PointWalkCandidatePayloadCodec.kt` v1: big-endian magic/kind/flags/UTF-8 ID/optional coordinate; WALK, STOP, ARRIVED; validate payload version/size, ID 1…128 bytes, strict UTF-8, canonical flags, terminal field rules, finite/ranged coordinates và trailing bytes. Thêm encode/decode để có một biểu diễn wire rõ ràng.
  - Cập nhật `candidate-contract.md` với byte layout, discriminator/capability và phân chia identity: native dùng session + unique-per-session candidate ID trên terminal; Kotlin giữ generation cho timer/provider callback, và coordinator sẽ giữ tombstone cho ID đã từng active. Producer không tái sử dụng ID trong session.
  - Verify: `./gradlew test assembleDebug` exit 0; log báo `:bridge:protocol:compileKotlin` thành công và `:bridge:protocol:test NO-SOURCE` (module hiện không có test source). Kotlin/C++ byte layout được đối chiếu trong T-005. `git diff --check` exit 0; codec 110 dòng, `BridgeProtocol.kt` 407 dòng. Không sửa test code.

  </details>

- [x] **T-005** **[zygisk]** *(native)* — Thêm C++ candidate encoder/forwarder trong `zygisk/jni/shared/bridge_kotlin/runtime_walk_candidate_protocol.h` và `runtime_walk_candidate_bridge.inc` [new], rồi route discriminator/version/capability qua `runtime_bridge_broker.inc` theo include/order trong `main.cpp`. Không đổi `RuntimeAutoFortNavigation` hoặc gửi candidate từ fort-walk ở phase này. Giữ existing bridge frame, identity/session/sequence, payload limit và companion forwarding. **AC:** AC-06, AC-07. **Phụ thuộc:** T-002, T-004. **Verify:** `ANDROID_NDK="$HOME/Library/Android/sdk/ndk/27.1.12297006" ./scripts/build-magisk.sh`; chạy host protocol test hiện có chỉ khi T-002 xác nhận đúng path/relevance; không tạo test code.

  <details>
  <summary>Đã hoàn thành — T-005</summary>

  - Tạo `runtime_walk_candidate_protocol.h` và `runtime_walk_candidate_bridge.inc`; native payload v1 match Kotlin layout: POGC, kind 1/2/3, flags, bounded ID, optional coordinates, observation envelope type 12/version 1.
  - Sửa `main.cpp` include order (header trước protocol `.inc`; candidate forwarder sau navigation forwarder và trước broker), `runtime_bridge_broker.inc` dispatch type/version, và `runtime_ui_status_bridge.inc` + `host/runtime_capabilities.inc` quảng bá `POINT_WALK_CANDIDATE` chỉ khi build identity exact. Không sửa `RuntimeAutoFortNavigation`/fort producer.
  - Header candidate dùng validator riêng `valid_point_walk_coordinate` với `std::isfinite` và miền latitude/longitude; không gọi `valid_navigation_coordinate` và không phụ thuộc declaration từ `auto_fort_navigation.h`. Đã rà include order hiện hành: candidate protocol được include trước fort header nhưng vẫn độc lập.
  - Wire review: C++ `append_u32`/`append_f64`/`append_string` big-endian khớp Kotlin `DataOutputStream`; forwarder giữ runtime session, sequence, timestamps, pid/process/build identity, giới hạn bridge và frame hiện có.
  - Verify trước đó: lần chạy với NDK 27.1 nêu trong checklist dừng do thư mục không tồn tại; sau đó NDK 28.2 build arm64-v8a + x86_64 và tạo ZIP thành công (exit 0). Verify lại trên tree hiện tại bằng `ANDROID_NDK="$HOME/Library/Android/sdk/ndk/28.2.13676358" ./scripts/build-magisk.sh`: exit 0, cấu hình cả hai ABI, Ninja báo `no work to do` ở cả arm64-v8a và x86_64, script xác nhận/gắn ZIP `build/pogo-root-automation-magisk-multiabi.zip`. Đây là incremental verification, không tuyên bố compile lại object vì không có source nào cần rebuild. T-002 xác nhận không có host test candidate-specific nên không chạy test không liên quan. `git diff --check` exit 0; các file native thay đổi dưới 500 dòng. Không tạo/sửa test.

  </details>
