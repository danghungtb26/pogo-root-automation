# Phase 5 — Cutover và kiểm chứng

Plan: [Tổng quan](00-overview.md)

**Điều kiện bắt đầu:** T-015 lifecycle/watchdog, T-018 controller và mọi backend ABI được mở đã qua gate tương ứng.
**Điều kiện hoàn tất:** host/source checks, startup matrix, cleanup sau matrix và final build/docs đều đạt; unsupported ABI giữ capability đóng.

Sau mỗi task, cập nhật ngay khối details dưới checkbox với file tạo/sửa/xóa/di chuyển, thay đổi method/param/hành vi và kiểm chứng thực tế trước khi chuyển task. Chỉ tick `[x]` khi Verify đạt; chưa đạt thì giữ `[ ]` và ghi phần còn thiếu.

- [ ] **T-019** **[build]** *(reviewer)* — Kiểm checkpoint tích hợp trước device: `./gradlew :bridge:protocol:test --rerun-tasks`, focused app tests hiện có, `./gradlew test assembleDebug --rerun-tasks`, hai host native protocol tests trong `AGENTS.md`, `ANDROID_NDK="$HOME/Library/Android/sdk/ndk/27.1.12297006" ./scripts/build-magisk.sh`, shell syntax và `git diff --check`. Kiểm non-Markdown source ≤500 dòng, ZIP có loader/core đúng cả ABI + manifest, một native gameplay scheduler và không có Kotlin runtime orchestration. **AC:** AC-01–AC-10 (source/build gate, chưa thay live verification). **Phụ thuộc:** T-009–T-018. **Verify:** lệnh thực tế pass và artifact/package inspection khớp; thiếu tool/SDK/NDK ghi trong details, không coi Gradle là native verification.

  <details>
  <summary>Đã chạy một phần host checkpoint; T-019 còn mở</summary>

  - **Kiểm chứng đã chạy:** `./gradlew test assembleDebug --rerun-tasks` pass (52 tasks); `./scripts/build-magisk.sh` pass arm64-v8a/x86_64 bằng NDK mặc định 28.2.13676358; `bash -n scripts/build-magisk.sh scripts/package-magisk.sh` và `git diff --check` pass. Hai host test hiện có `zygisk/tests/runtime_automation_event_protocol_test.cpp` và `runtime_readiness_retry_test.cpp` compile/run với `-Wall -Wextra -Werror` pass. Không có source non-Markdown nào >500 dòng trong cây app/bridge/zygisk/scripts được rà. ZIP có hai Zygisk `.so`, hai core `.so`, manifest từng ABI và external `coreArtifacts` với SHA-256.
  - **Bổ sung sau native split:** Source review xác nhận `zygisk/loader/main.cpp` chỉ ghép companion/broker, còn `zygisk/jni/main.cpp` build vào core và là owner duy nhất của gameplay scheduler. `app/build.gradle.kts` theo dõi cả `zygisk/loader/` để phát hiện ZIP cũ. `./scripts/build-magisk.sh` và `git diff --check` pass sau split; package kiểm core export và hash. Không có live proof core đã load trên target.
  - **Còn thiếu:** T-009–T-018 chưa qua đủ Verify/lifecycle gate, nên checkpoint tích hợp chưa đạt. Hai path test `zygisk/jni/runtime_command_protocol_test.cpp` và `runtime_observation_protocol_test.cpp` ghi trong `AGENTS.md` không tồn tại; NDK 27.1.12297006 ghi trong checklist không có trên máy. Chưa chạy target/device matrix.

  </details>

- [ ] **T-020** **[zygisk/app]** *(reviewer)* — Thực hiện **manual handoff** trên đúng `BlueStacks Air 1`, chọn/kiểm bằng scripts repo rồi kiểm ma trận trong [brainstorm](../brainstorm.md#bước-6--tiêu-chí-chấp-nhận-và-xác-minh): controller-first, game-first, controller kill/restart, game kill/restart, delayed library, duplicate `dlopen`, missed init, unsupported build/ABI/translation, core hash/ABI mismatch. Ghi trong `../runtime-verification.md` (dự kiến tạo) mỗi scenario: game build/ABI/translation, session ID, phase+sequence+timestamp, error/capability, config revision, mutation/action postcondition; không suy action completed từ ACK. Chỉ dùng `scripts/*.sh` để cài/smoke/diagnostics; khi run fail mới dùng `./scripts/logcat-full.sh` và `./scripts/collect-binding-diagnostics.sh`; không đọc log run thành công. **AC:** AC-01–AC-10. **Phụ thuộc:** T-019. **Verify:** tất cả scenario áp dụng có quan sát thực tế và đạt expected result; backend chưa verify ghi unsupported/capability đóng, không tick nếu AC mục tiêu còn thiếu.

  <details>
  <summary>Chưa thực hiện — T-020</summary>

  Chưa có thay đổi hoặc kết quả kiểm chứng. Agent cập nhật khối này ngay sau khi xử lý task, trước khi sang task tiếp theo.

  </details>

- [ ] **T-021** **[zygisk]** *(native)* — Sau T-020, xóa đường monolith/polling-primary không còn caller trong `zygisk/jni/main.cpp`, `shared/core/runtime_module.inc`, `shared/runtime/control/runtime_binding_probe.inc` và build scripts; giữ diagnostic watchdog có giới hạn, một gameplay owner/core, một reactor broker và toàn bộ auth/identity/freshness guards. Ghi file xóa/di chuyển và rollback artifact trong notes; không xóa compatibility code đang còn caller. **AC:** AC-02, AC-06, AC-09, AC-10. **Phụ thuộc:** T-020. **Verify:** `rg` không còn production caller của poll-primary hoặc duplicate command loop, native build/ZIP hai ABI pass, source ≤500 dòng và `git diff --check` sạch.

  <details>
  <summary>Chưa thực hiện — T-021</summary>

  Chưa có thay đổi hoặc kết quả kiểm chứng. Agent cập nhật khối này ngay sau khi xử lý task, trước khi sang task tiếp theo.

  </details>

- [ ] **T-022** **[docs/build]** *(reviewer)* — Sau cleanup, cập nhật `docs/ARCHITECTURE.md`, `zygisk/README.md`, `reverse/README.md` chỉ khi evidence mới thay đổi index, `../bootstrap-contract.md` và `../runtime-verification.md` theo kiến trúc thực tế/ABI support/errors; chạy lại focused Kotlin/native checks liên quan, `./gradlew test assembleDebug --rerun-tasks`, `ANDROID_NDK="$HOME/Library/Android/sdk/ndk/27.1.12297006" ./scripts/build-magisk.sh`, `git diff --check`. Ghi trạng thái capability chưa verified rõ; không tuyên bố `LIVE_READY` cho backend thiếu device evidence. **AC:** AC-01–AC-10. **Phụ thuộc:** T-021. **Verify:** tài liệu khớp source/package và lệnh cuối pass; mỗi task hoàn thành có notes thực tế, không còn dependency bị chặn nhưng báo hoàn thành.

  <details>
  <summary>Chưa thực hiện — T-022</summary>

  Chưa có thay đổi hoặc kết quả kiểm chứng. Agent cập nhật khối này ngay sau khi xử lý task, trước khi sang task tiếp theo.

  </details>
