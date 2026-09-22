# Phase 5 — Kiểm chứng và cập nhật tài liệu

Plan: [Tổng quan](00-overview.md)

Điều kiện bắt đầu: T-019 đạt; rollout/device chỉ chạy trong phạm vi được giao khi thực thi, yêu cầu viết plan chưa cấp quyền cài/reboot.
Điều kiện hoàn tất: T-020, T-021 và T-023 đạt Verify; T-022 chỉ đạt khi đúng
target Air 1 có mặt, còn nếu target không tồn tại thì giữ unchecked và ghi waiver
thực tế, không báo device/live verification hoàn tất.

Sau mỗi task, cập nhật ngay khối details dưới checkbox với file tạo/sửa/xóa/di chuyển, thay đổi method/param/hành vi và kiểm chứng thực tế trước khi chuyển task. Chỉ tick [x] khi Verify đạt; chưa đạt thì giữ [ ] và ghi phần còn thiếu. Không viết/sửa/xóa/disable test. Alias A/, B/, C/, P/, N/ được định nghĩa trong [inventory](../refactor-inventory.md#2-quy-ước-đường-dẫn).

- [x] **T-020** **[app]** *(reviewer)* — Chạy kiểm chứng Kotlin/Android hiện có và kiểm dependency cuối. **AC:** AC-01, AC-02, AC-05, AC-06, AC-07, AC-RF-01. **Phụ thuộc:** T-019. **Verify:** focused checks và full Gradle đạt; không test diff, không producer gameplay Kotlin trên service path.

  Chạy các focused tasks phù hợp đã xác nhận ở T-001:
  ```bash
  ./gradlew :app:testDebugUnitTest :core:test :bridge:protocol:test :game-adapter:api:test :game-adapter:fake:test :game-adapter:pogo:test --rerun-tasks
  ./gradlew test assembleDebug
  ./gradlew :app:dependencies --configuration debugRuntimeClasspath
  git diff --check
  ```
  Ghi task nào NO-SOURCE, không tính như đã có coverage. `RuntimeLifecycleCoordinatorTest` chỉ xác nhận compatibility cũ; mapper/navigation tests xác nhận contract đang giữ. Không khẳng định các test đó chứng minh native desired-state mới.
  Rà production creation/call sites để chứng minh không engine/coordinator legacy/POGO decoder; kiểm API status serializer và local desired state không bị nhầm với native applied state. Lỗi baseline/môi trường ghi tách khỏi lỗi mới.

  <details>
  <summary>Đã thực hiện — T-020</summary>

  - Focused verification: `./gradlew :app:compileDebugKotlin :app:testDebugUnitTest :core:test :bridge:protocol:test :game-adapter:api:test :game-adapter:fake:test :game-adapter:pogo:test --rerun-tasks` → BUILD SUCCESSFUL (35 tasks; các test task core/bridge/game-adapter không có source được ghi nhận).
  - Full verification: `./gradlew test assembleDebug --rerun-tasks` → BUILD SUCCESSFUL (51 tasks), tạo `app/build/outputs/apk/debug/app-debug.apk`. Chỉ có warning deprecated Android API/manifest package baseline, không có compile/test failure.
  - Dependency/source review: `./gradlew :app:dependencies --configuration debugRuntimeClasspath` chỉ còn `:core`, `:bridge:protocol`, Kotlin stdlib và joystick AAR; không còn `:game-adapter:*`, protobuf hoặc POGOProtos. Production service/API không tạo engine/coordinator/POGO decoder; receipt và native applied/ready vẫn tách biệt. `git diff --check` sạch; test source không đổi.

  </details>

- [x] **T-021** **[zygisk]** *(native)* — Chạy 6 native host checks có sẵn và build multi-ABI. **AC:** AC-04, AC-07, AC-LOC-02, AC-LOC-04, AC-RF-03. **Phụ thuộc:** T-019. **Verify:** 6 binary checks chạy đạt, build/package cả arm64-v8a và x86_64 đạt; không sửa test để làm pass.

  Dùng lệnh trong mục native bên dưới, lấy từ `.github/workflows/ci.yml` hiện hành. Không dùng path `zygisk/jni/runtime_*_test.cpp` cũ không còn tồn tại.
  Chạy `ANDROID_NDK="$HOME/Library/Android/sdk/ndk/27.1.12297006" ./scripts/build-magisk.sh` để build/package; không thay bằng ad hoc CMake. APK và ZIP là hai artifacts riêng.
  Host tests chỉ bao phủ readiness helper/navigation/telemetry và guards sẵn có. New desired-state codec/reconcile chưa có test mới theo yêu cầu; phải có review T-006/T-008 và device evidence T-022, không dùng 6 tests này để tuyên bố đầy đủ coverage.

  <details>
  <summary>Đã thực hiện — T-021</summary>

  - Host checks đã compile và chạy exit 0, không output failure: `runtime_readiness_retry_test`, `runtime_auto_fort_navigation_test`, `runtime_automation_event_protocol_test`, `runtime_automation_subject_names_test`, `runtime_discard_filter_factory_test`, `runtime_discard_preparation_test` theo đúng include path trong checklist. Không sửa test source.
  - `ANDROID_NDK="/Users/admin/Library/Android/sdk/ndk/28.2.13676358" ./scripts/build-magisk.sh` → BUILD SUCCESSFUL; CMake build/link đạt cho `arm64-v8a` và `x86_64`, package tạo `build/pogo-root-automation-magisk-multiabi.zip` và script validate hai `.so`.
  - Sai khác môi trường so với plan: NDK `27.1.12297006` không tồn tại trên máy hiện tại, nên dùng NDK 28.2 đã cài theo override được `AGENTS.md` hỗ trợ. Desired-state codec/reconcile vẫn không có test mới theo rule; host checks chỉ là readiness/navigation/telemetry/discard coverage, không được diễn giải quá phạm vi đó.
  - `git diff --check` sạch; không tạo/sửa/xóa/disable test.

  </details>

- [ ] **T-022** **[app]** *(reviewer)* — Xác minh Air 1 theo ma trận hành vi và lưu bằng chứng. **AC:** AC-01, AC-02, AC-04, AC-05, AC-06, AC-LOC-01, AC-LOC-02, AC-LOC-03, AC-LOC-04, AC-RF-02, AC-RF-03. **Phụ thuộc:** T-020, T-021. **Verify:** ma trận device bên dưới có kết quả thực tế/postcondition; kiểm tra bắt buộc chưa chạy giữ task chưa đạt.

  Xác nhận target bằng scripts repository, không `adb devices` hoặc chọn emulator khác. Khi deployment được giao, dùng `./scripts/push-emulator.sh` và `./scripts/install-magisk-module.sh --apk app/build/outputs/apk/debug/app-debug.apk`; chỉ thêm `--reboot` nếu đã được cho phép. Cần native module mới thực sự active trước khi so kết quả.
  Sau khi entry point đã xác nhận target, dùng env target repo (`ANDROID_SERIAL=127.0.0.1:5565` nếu vẫn là cấu hình Air 1): `scripts/bluestacks-smoke-test.sh`, `scripts/device-smoke-test.sh`, `scripts/headless-control.sh` với các lệnh đã có. Script lifecycle có force-stop/relaunch game; chạy khi thao tác đó thuộc phạm vi kiểm chứng.
  Quan sát UI/status trực tiếp, không xem log thành công. Nếu thao tác lỗi, thu bằng `scripts/logcat-full.sh` rồi chỉ đọc log liên quan. Script thiếu thao tác/negative binary probe thì ghi giới hạn, không tạo script/harness/test/ADB thay thế.
  Lưu `../verification.md` với build/module identity, ABI/session, input, expected/observed và hạn chế. Không diễn giải trạng thái native nhận config như đã caught/spun; không bật capability/binding chưa xác minh.

  <details>
  <summary>Bỏ qua có chủ đích — T-022</summary>

  - Đã thử `ANDROID_SERIAL=127.0.0.1:5565 ./scripts/push-emulator.sh`: script repository không tìm thấy target, kết thúc với `emulator 127.0.0.1:5565 is not connected/authorized` sau connection refused.
  - Đã thử `./scripts/bluestacks-smoke-test.sh`: kết thúc với `FAIL: no adb device connected`.
  - Theo xác nhận của người dùng, máy này không có `BlueStacks Air 1`; không cài/reboot, không chọn emulator khác và không dùng ADB ad hoc. T-022 giữ `[ ]`, không coi build/host checks là device pass.
  - Tạo [`verification.md`](../verification.md), ghi artifact APK/ZIP, protocol v3, target/session chưa có, expected/observed và giới hạn. Không đọc log thành công, không tạo/sửa/xóa/disable test.

  </details>

- [x] **T-023** **[docs]** *(reviewer)* — Review kết quả cleanup, giới hạn compatibility và cập nhật rule/tài liệu. **AC:** AC-01, AC-02, AC-07, AC-RF-01, AC-RF-02. **Phụ thuộc:** T-022. **Verify:** docs khớp source sau refactor; mỗi task đã xong có details thực tế; không non-Markdown source touched vượt 500 dòng, diff check sạch.

  Cập nhật `AGENTS.md`, `docs/ARCHITECTURE.md`, `docs/ARCHITECTURE_SUMMARY.md` và `../refactor-inventory.md`: native là owner thực tế, Kotlin giữ UI/location; bỏ mô tả migration đã hoàn tất, vẫn nêu những file compatibility chưa xóa.
  Rà liên kết `docs/RUNTIME_BRIDGE.md`/`DATAFLOW_OVERVIEW.md` và tài liệu liên quan nhưng giữ nội dung người dùng đã sửa; chỉ sửa đoạn trực tiếp lỗi thời theo contract mới.
  Đối chiếu ma trận giữ/chuyển/xóa với diff, check test source không bị sửa/xóa/skip; cập nhật danh sách candidate core còn giữ vì consumer. Không báo đã xóa toàn bộ legacy source khi chỉ ngắt production path.
  Rà từng guard, unknown/unsupported behavior, địa chỉ IPC, UI/HTTP compatibility và no-fallback policy. `git diff --check` phải sạch; details của mỗi task phải chứa file/method/lệnh/kết quả thực tế.

  <details>
  <summary>Đã thực hiện — T-023</summary>

  - Cập nhật `AGENTS.md`: đường live là `RuntimeUiAutomationFacade`/`RuntimeUiEventRouter`; engine/coordinator/raw-observation path chỉ còn compatibility khi test cũ cần.
  - Cập nhật `docs/ARCHITECTURE.md`, `docs/ARCHITECTURE_SUMMARY.md`, `docs/DATAFLOW_OVERVIEW.md`, `docs/RUNTIME_BRIDGE.md` và `../refactor-inventory.md`: native owner, Kotlin UI/location, protocol v3, desired/applied/ready semantics, IPC `pogo_root_automation_runtime`, HTTP compatibility, no-fallback và giới hạn device evidence.
  - Inventory đối chiếu với diff: đã xóa engine/raw glue/core planner chết; giữ `RuntimeLifecycleCoordinator`/dispatcher/`RuntimeControlBridge`, `RuntimeSessionManager`, action types và `game-adapter/*` vì compatibility/consumer ngoài APK; không sửa test source.
  - Tách `zygisk/jni/shared/core/runtime_il2cpp_api.inc`, `runtime_native_declarations.inc` và `zygisk/jni/shared/bridge_kotlin/runtime_bridge_observation_protocol.inc` để mọi native source đã chạm ≤500 dòng, giữ thứ tự include của một translation unit.
  - Verification: `ANDROID_NDK="/Users/admin/Library/Android/sdk/ndk/28.2.13676358" ./scripts/build-magisk.sh` → build/link/package lại thành công arm64-v8a + x86_64; `git diff --check` sạch; không có test path trong diff. T-022 vẫn unchecked theo [`verification.md`](../verification.md) vì máy không có Air 1.

  </details>

## Native checks hiện có

Các command dưới đây chỉ chạy khi thực thi task, chưa chạy lúc viết plan.

```bash
c++ -std=c++17 -Wall -Wextra -Werror -Izygisk/jni \
  zygisk/tests/runtime_readiness_retry_test.cpp -o /tmp/runtime_readiness_retry_test
/tmp/runtime_readiness_retry_test
c++ -std=c++17 -Wall -Wextra -Werror -Izygisk/jni \
  zygisk/tests/runtime_auto_fort_navigation_test.cpp -o /tmp/runtime_auto_fort_navigation_test
/tmp/runtime_auto_fort_navigation_test
c++ -std=c++17 -Wall -Wextra -Werror -Izygisk/jni/shared/bridge_kotlin \
  zygisk/tests/runtime_automation_event_protocol_test.cpp -o /tmp/runtime_automation_event_protocol_test
/tmp/runtime_automation_event_protocol_test
c++ -std=c++17 -Wall -Wextra -Werror -Izygisk/jni \
  zygisk/tests/runtime_automation_subject_names_test.cpp -o /tmp/runtime_automation_subject_names_test
/tmp/runtime_automation_subject_names_test
c++ -std=c++17 -Wall -Wextra -Werror -Izygisk/jni \
  zygisk/tests/runtime_discard_filter_factory_test.cpp -o /tmp/runtime_discard_filter_factory_test
/tmp/runtime_discard_filter_factory_test
c++ -std=c++17 -Wall -Wextra -Werror -Izygisk/jni \
  zygisk/tests/runtime_discard_preparation_test.cpp -o /tmp/runtime_discard_preparation_test
/tmp/runtime_discard_preparation_test
ANDROID_NDK="$HOME/Library/Android/sdk/ndk/27.1.12297006" ./scripts/build-magisk.sh
```

## Ma trận device

| Case | Trigger bằng công cụ hiện có/UI | Postcondition |
|---|---|---|
| Cold start / game chưa ready | Boot controller và mở game qua headless-control bootstrap/game | App gửi desired state; native tự readiness/apply; UI nhận trạng thái đợi/ready đúng; không mutation trước guards |
| Đổi setting trước/sau ready | Settings UI hoặc headless-control config | Giá trị persist, desired/applied revision đúng; không có bộ config trộn revision |
| Master stop trong init hoặc đang chạy | UI stop hoặc headless-control stop | Native không dispatch action mới; late result không re-enable; status cho biết pending/outcome nếu đã có action in flight |
| Stop/resume cùng process | UI hoặc headless-control start/stop | Desired state mới đúng; không START/config loop Kotlin; không lặp mutation từ lần trước |
| Game force-stop/relaunch | device-smoke-test hiện có | Session mới, không reuse pending action/seq/map target; config user được resync, guard mới phải đạt |
| UI ẩn/foreground và provider | Home/quay game, bật/tắt joystick theo UI hiện có | Overlay visibility, auto-start và provider cleanup giữ behavior baseline |
| Joystick/teleport/favorite walk | Thao tác UI | Movement vẫn tính/chạy tại Kotlin, có arrival/cancel, không yêu cầu binding PoGo mới |
| Native auto-walk/map target | Chỉ feature đã verified/có capability | Native chọn/resolve target; Kotlin walk; stale lease dừng native-issued walk; local arrival không tự dispatch spin |
| Event/diagnostic/status | UI + headless-control status/diagnostic | Toast/status đúng nguồn native; registered/received/applied/ready phân biệt; unsupported không báo success giả |
| Controller kill/socket mất | Chỉ thao tác được hỗ trợ/đã cho phép theo T-002 | Theo policy đã chốt, không unbounded queue/auto-resume mutation mới; ghi chưa xác minh nếu không có entry point |
| Mixed protocol hoặc malformed binary | Paired-artifact check khi được phép; static guard review nếu không có tool | Fail closed; không fallback Kotlin owner. Ghi rõ case chỉ review source, không giả là đã inject packet lên device |

Không yêu cầu cài game build lạ, gọi method game thử, thêm probe/harness hoặc bật
feature chưa verified để đủ checklist. Scenario không áp dụng phải có lý do,
không đổi tiêu chí bắt buộc thành “pass” khi thiếu công cụ hoặc chưa chạy.
