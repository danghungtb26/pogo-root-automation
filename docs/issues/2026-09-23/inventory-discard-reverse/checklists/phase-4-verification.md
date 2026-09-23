# Phase 4 — Kiểm chứng host và thiết bị

Plan: [Tổng quan](00-overview.md).

Điều kiện bắt đầu: implementation T-005–T-015 đạt kiểm tra tại chỗ. Điều kiện hoàn tất: focused checks, build và runtime evidence cần thiết đạt; case chưa quan sát được ghi riêng, không tự chuyển thành PASS.

Sau mỗi task, cập nhật ngay details dưới checkbox với file tạo/sửa/xóa/di chuyển, thay đổi method/param/hành vi và kiểm chứng thực tế trước khi chuyển task. Chỉ tick `[x]` khi Verify đạt; chưa đạt giữ `[ ]` và ghi phần còn thiếu.

Phase này mô tả công việc khi triển khai/kiểm thử được giao; **không chạy thiết bị chỉ vì đang viết plan**. Không tạo/sửa test code. Không tạo harness/script tạm, raw ADB, cache mutation hoặc request cố ý sai để ép error path.

- [x] **T-016** **[zygisk]** *(reviewer)* — Rà diff theo các AC và chạy focused checks có sẵn dưới đây. Bổ sung bảng coverage theo task: host mock kiểm gì; điều gì chỉ codewalk; điều gì chờ runtime. Kiểm declaration/include sau T-004, toàn bộ source đã chạm <=500 dòng, `git diff --check`; xác nhận không có test source được thêm/sửa. Nếu test mock hiện tại không tương thích production guard mới, ghi lỗi và dependency xử lý theo chỉ dẫn người dùng, không bỏ guard hay tự sửa test để PASS. **AC:** AC-02–AC-15 trong phạm vi static/host, không thay device proof. **Phụ thuộc:** T-015. **Verify:** các test hiện có liên quan pass, review không còn nhánh dùng 6 param/fabricated context/double rollback/reset barrier/curated capacity; từng AC có đúng mức bằng chứng và kết quả thực tế, regression chưa giải quyết giữ task mở.

  <details>
  <summary>Đã hoàn thành — T-016</summary>

  Coverage thực tế:

  | Kiểm tra | Kết quả | Mức bằng chứng |
  |---|---|---|
  | `runtime_discard_filter_factory_test.cpp` | PASS | delegate factory/GC root mock only; không phủ game binding |
  | `runtime_discard_preparation_test.cpp` | PASS | row/class/id/count/recyclable/set and no-invoke guards |
  | `runtime_readiness_retry_test.cpp` | PASS | generic readiness retry policy |
  | `./gradlew test assembleDebug` | PASS | Kotlin/protocol/build, no native runtime proof |
  | `ANDROID_NDK=/Users/admin/Library/Android/sdk/ndk/28.2.13676358 ./scripts/build-magisk.sh` | PASS | arm64-v8a + x86_64 compile/link/package |
  | `git diff --check` | PASS | whitespace only |
  | source line limit | PASS | touched native files <=500 lines |

  Code review xác nhận không còn arg thứ sáu, synthetic set fallback, framework
  rollback hoặc `NewCount` cache write. Success/reconcile, owner generation,
  cache baseline/no-delta và lifecycle maintenance vẫn là code/runtime gaps;
  chúng không được nâng thành PASS ở đây. Không thêm/sửa test source.

  </details>

Lệnh focused đối chiếu `.github/workflows/ci.yml` và AGENTS.md; chạy binary sau compile, không chỉ báo compile thành công:

```bash
c++ -std=c++17 -Wall -Wextra -Werror -Izygisk/jni \
  zygisk/tests/runtime_discard_filter_factory_test.cpp -o /tmp/runtime_discard_filter_factory_test
/tmp/runtime_discard_filter_factory_test
c++ -std=c++17 -Wall -Wextra -Werror -Izygisk/jni \
  zygisk/tests/runtime_discard_preparation_test.cpp -o /tmp/runtime_discard_preparation_test
/tmp/runtime_discard_preparation_test
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
./gradlew :bridge:protocol:test --rerun-tasks
```

Khi lập plan, không tìm thấy `zygisk/jni/runtime_command_protocol_test.cpp` và `runtime_observation_protocol_test.cpp` mà AGENTS.md dẫn tới. Không dùng hai lệnh compile đường dẫn thiếu đó như checks có sẵn. Nếu wire definitions/codecs thay đổi, T-016 phải xác định coverage thực của test hiện còn trong repo/CI và ghi khoảng trống; không tạo lại test trái chính sách. `rg --files bridge/protocol` hiện cũng không liệt kê source test: nếu Gradle task trả `NO-SOURCE` thì ghi đúng không có test chạy, không báo như codec behavior đã được kiểm chứng. Các build/check không phủ hành vi phải được bổ sung bằng review và runtime evidence tương ứng trước T-021.

Hai discard test hiện có không chứng minh list signature, expiration guard mới, GC lifetime, actual Promise ordering hay reconciliation. Không gán các AC đó là pass nhờ mock.

- [x] **T-017** **[build]** *(native)* — Kiểm môi trường SDK/NDK/CMake/Ninja/canonical Zygisk header theo script; chạy full Gradle và native build/package riêng. Ghi source revision/diff identity và mode Calibration của artifact dùng ở T-018; không phát hành artifact calibration như production. Override NDK dưới đây là đường dẫn được dự án hướng dẫn, chưa bảo đảm đang tồn tại; nếu thiếu dùng override được script hỗ trợ khi có môi trường hợp lệ, không dựng quy trình build thay thế. **AC:** hỗ trợ AC-02, AC-14, AC-16 và DoD build. **Phụ thuộc:** T-016. **Verify:** `./gradlew test assembleDebug` và `scripts/build-magisk.sh` thành công; APK/ZIP đúng đường dẫn, script xác nhận ZIP có arm64-v8a và x86_64; compile x86_64 không có nghĩa binding AArch64 được phép trên game x86_64. Ghi command/output kết quả, SDK thiếu hoặc build fail thì giữ `[ ]`.

  <details>
  <summary>Đã hoàn thành — T-017</summary>

  - `./gradlew test assembleDebug` → `BUILD SUCCESSFUL`.
  - `./scripts/build-magisk.sh` → CMake/Ninja compile and link PASS for
    `arm64-v8a` and `x86_64`; package artifact exists at
    `build/pogo-root-automation-magisk-multiabi.zip` and APK at
    `app/build/outputs/apk/debug/app-debug.apk`.
  - The artifact is **Calibration/unsupported for production mutation**:
    `kDiscardExecutionEnabled=false`; no device evidence is implied by the
    successful compile/package. T-018–T-020 remain required.

  </details>

```bash
./gradlew test assembleDebug
ANDROID_NDK=/Users/admin/Library/Android/sdk/ndk/28.2.13676358 ./scripts/build-magisk.sh
```

Artifacts: `app/build/outputs/apk/debug/app-debug.apk`, `build/pogo-root-automation-magisk-multiabi.zip`. Không coi Gradle là build Zygisk, không commit output build.

- [ ] **T-018** **[zygisk]** *(native)* — Cài artifact calibration và xác nhận **BlueStacks Air 1** bằng entry point repo trong phạm vi kiểm thử được giao. Đối chiếu package/version code, ELF build/ABI thực chạy, session; không suy identity từ serial hoặc reverse folder. Bắt đầu bằng passive owner/type/thread/layout survey sau reverse. Sau khi guard tĩnh/owner đạt, mới thực hiện bước tạo model local có kiểm soát và refresh nếu cần, vẫn chưa RecycleItem. Kiểm 5 param binding, filter delegates, service khi bag UI chưa mở, row/expiration/set cùng generation, managed roots và cache baseline; đối chiếu concrete Promise/layout có thể xác minh trước mutation. Ghi scalar diagnostics qua status hiện có, không pointer IPC. **AC:** AC-02, AC-03, AC-04, AC-07, AC-16. **Phụ thuộc:** T-017. **Verify:** có bảng preflight với PASS/FAIL/unknown cho từng guard, sai identity hoặc thiếu context không gọi mutation; cold launch với bag UI đóng không crash/fallback null. Script không xuất đủ evidence thì ghi blocker/khả năng diagnostic cần thêm, không đánh dấu binding verified chỉ vì smoke/probe PASS.

  <details>
  <summary>Chưa thực hiện — T-018</summary>

  Đã thử đúng entry point `ANDROID_SERIAL=127.0.0.1:5565
  ./scripts/bluestacks-smoke-test.sh`; kết quả thực tế là
  `FAIL: no adb device connected`. Vì vậy chưa install/push/probe và chưa có
  runtime identity, owner/lifetime, five-param ABI, baseline hoặc cold-launch
  evidence. Không dùng raw ADB để bypass; T-018 cần chạy lại khi BlueStacks
  Air 1 online.

  </details>

Đọc option hiện hành của script trước khi dùng. `127.0.0.1:5565` là target cấu hình hiện tại, phải xác nhận đúng instance qua workflow, không tự chuyển instance:

```bash
ANDROID_SERIAL=127.0.0.1:5565 ./scripts/bluestacks-smoke-test.sh
ANDROID_SERIAL=127.0.0.1:5565 ./scripts/push-emulator.sh
ANDROID_SERIAL=127.0.0.1:5565 ./scripts/install-magisk-module.sh --apk app/build/outputs/apk/debug/app-debug.apk
ANDROID_SERIAL=127.0.0.1:5565 ./scripts/headless-control.sh status
ANDROID_SERIAL=127.0.0.1:5565 ./scripts/headless-control.sh diagnostic
ANDROID_SERIAL=127.0.0.1:5565 ./scripts/binding-probe-test.sh
```

Không tự thêm `--reboot`. Probe có thể launch game, status có thể bootstrap app; install/smoke/probe không phải toàn bộ read-only. Nếu script cần restart ngoài phạm vi đã giao, ghi dependency thực tế. Khi thao tác thất bại mới dùng `collect-binding-diagnostics.sh`/`logcat-full.sh` và đọc log liên quan; thành công đánh giá từ structured status/result/UI postcondition.

- [ ] **T-019** **[zygisk]** *(native)* — Chạy controlled action theo calibration contract T-014, bắt đầu một manual discard lượng nhỏ trên item người kiểm thử chủ động chọn, recyclable/context/count hợp lệ. Xác nhận action identity, một invoke/request route 137 qua evidence sẵn có; game prediction không làm phát success; response và cache reconcile khớp T-002. Sau đó mới bật auto cho một item trong cùng session đã chứng minh; đo current count và current limit tại invoke, amount bằng excess. Đối chiếu arithmetic 100→70/limit50 bằng codewalk và, nếu tái hiện được hợp lệ, runtime; không chỉnh memory hay request sai để dựng số liệu. Chạy bag UI đóng và trạng thái không vượt limit. **AC:** AC-05, AC-06, AC-08, AC-11, AC-16. **Phụ thuộc:** T-018. **Verify:** actual successful discard có correlated result và postcondition cache đáng tin; không lặp request trước reconcile; auto giữ đúng limit theo count tại invoke; count<=limit không mutation. Count cuối có thể chịu gameplay khác nên không chỉ so before-minus-amount; ghi hoạt động đồng thời và không nhận telemetry/count predicted là server proof. Nếu action/Promise/cache contract runtime khác discovery, đóng gate và quay về task bị ảnh hưởng.

  <details>
  <summary>Chưa thực hiện — T-019</summary>

  Chưa thực hiện controlled manual/auto discard: đây là mutation trên game
  state và cần T-018 preflight plus một item/amount nhỏ do người kiểm thử chủ
  động chọn. Emulator hiện offline, và chưa có ủy quyền xóa item cụ thể. Không
  bịa route 137, correlated Promise result hoặc cache postcondition; task vẫn
  mở.

  </details>

- [ ] **T-020** **[zygisk]** *(reviewer)* — Thực hiện và ghi matrix lifecycle/outcome dưới đây qua UI/control workflow hiện có và diagnostic T-014. Bắt buộc runtime: config/toggle khi pending nếu có cửa sổ quan sát hợp lệ, reconnect, cold launch/relaunch với UI bag đóng và baseline mới. Dùng `scripts/device-smoke-test.sh` chỉ trong scope force-stop/relaunch đã được giao. Với response2/3, transport error, no-delta, GC loss hoặc queue timeout khó sinh hợp lệ, kết hợp bằng chứng binary + codewalk và ghi runtime “chưa quan sát”; không bịa pass, không dùng request sai hoặc fault injector mới. **AC:** AC-09, AC-10, AC-11, AC-12, AC-13, AC-16. **Phụ thuộc:** T-019. **Verify:** mọi case có expected/actual/evidence level; không duplicate invoke, double rollback, clear unknown do reset hoặc callback cũ hoàn tất action mới. Các scenario runtime bắt buộc đạt; nếu postcondition còn dựa giả định chưa chứng minh, giữ task mở và T-021 bị chặn. Review các case hiếm không đồng nghĩa đã tái hiện live.

  <details>
  <summary>Chưa thực hiện — T-020</summary>

  Chưa có runtime lifecycle matrix vì T-018 chưa chạy và T-019 chưa được
  phép/không thể chạy. Codewalk đã giữ unknown barrier cho
  timeout/transport/layout và không reset active Promise qua config, nhưng
  reconnect, cold relaunch, owner change, result 2/3 và no-delta chưa được
  quan sát trên thiết bị. T-021 bị chặn cho tới khi các case bắt buộc có
  evidence thực tế.

  </details>

| Nhóm | Expected để ghi evidence |
|---|---|
| Config revision unrelated, thay limit, disable→enable lúc pending | Request cũ vẫn tương quan; không queue mới tới khi an toàn; candidate chưa invoke dùng/reject theo revision mới |
| Disable-all, STOP, socket disconnect | Không admission mới; drain hoặc giữ unknown có ownership rõ; reconnect không xóa barrier tùy tiện |
| Owner/scene đổi, process relaunch | Không dereference handle/pointer cũ; generation mới và baseline mới trước admission; không persist pending |
| Queue trễ/timeout, Promise mất target/sai shape/deadline | Phân biệt chưa invoke đã chứng minh với outcome unknown; không tự retry qua cadence/toggle |
| Result2/3 và item khác hợp lệ | Game rollback; suppression theo bằng chứng; fairness chỉ sau cache an toàn |
| Unset/transport rejection | Không coi local rollback là server non-execution; giữ unknown tới khi có recovery proof hợp lệ |
| Success nhưng refresh pending/no-delta | Success giữ đúng nghĩa, module chưa Ready khi thiếu reconcile; không ghi NewCount vào cache |
| Manual+auto/catch/spin/transfer gần nhau | Recheck admission trên main thread; tối đa một mutation được nhận theo policy |
