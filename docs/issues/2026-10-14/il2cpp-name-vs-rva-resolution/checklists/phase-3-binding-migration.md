# Phase 3 — Binding migration và capability gates

Plan: [Tổng quan](00-overview.md)

Điều kiện bắt đầu: `VBR-103` và `VBR-203` hoàn tất; exact build
identity và verified method contract có sẵn. `VBR-003` chỉ là dependency bắt
buộc cho RVA equality guard, không cho descriptor migration.

Điều kiện hoàn tất: năm critical callsite dùng verified descriptor,
capability gates phân biệt build/method/owner/outcome; RVA mismatch không bao giờ
fallback sang direct call.

Sau mỗi task, cập nhật ngay khối details dưới checkbox với file tạo/
sửa/xóa/di chuyển, thay đổi method/param/hành vi và kiểm chứng thực
tế trước khi chuyển task. Chỉ tick `[x]` khi Verify đạt; chưa đạt
thì giữ `[ ]` và ghi phần còn thiếu.

## Task

- [x] **VBR-301** **[zygisk]** *(native)* — Thay lookup
  `MapPokemon.TryCapture` trong `direct_map_bindings.inc` bằng
  `VBR-202` + expectation `VBR-203`; lưu verified descriptor/reason trong
  `RuntimeBinding`. `direct_map_actions.inc` tiếp tục gọi
  `il2cpp_runtime_invoke` trên main thread; không thay argument construction cho
  tới khi `VBR-004` chốt exact `PokeballThrow` layout. `direct_catch_verified`
  không được true chỉ vì descriptor non-null. **AC:** `AC-VBR-03`–
  `AC-VBR-05`, `AC-VBR-07`. **Phụ thuộc:** `VBR-004`, `VBR-103`,
  `VBR-203`. **Verify:** Native build; static call path
  expectation → unique/token verified descriptor → `runtime_invoke`; mismatch
  giữ direct catch unavailable và log structured reason.

  <details>
  <summary>Đã hoàn thành — VBR-301</summary>

  - **Tạo file:** Không.
  - **Sửa file:** `runtime_method_contract.h`/`runtime_method_resolver.inc` — thêm exact image/class resolution và `kClassUnavailable`; `runtime_native_prelude.inc` — lưu `map_pokemon_try_capture_descriptor`; `modules/catch_spin/direct_map_bindings.inc` — thay name/type lookup bằng 0.433.0 expectation + verified resolver, log structured result và bỏ helper cũ; chính checklist này — cập nhật task.
  - **Xóa file:** Không.
  - **Di chuyển/đổi tên:** Không.
  - **Thay đổi chức năng:** `MapPokemon` phải resolve duy nhất từ exact `holo-game.dll` + namespace/class; `TryCapture` pointer chỉ được publish khi unique full signature và token `0x06013ab5` khớp. `direct_catch_verified` còn yêu cầu exact build, spawn getter, AR-values class và catch-proto class; descriptor non-null riêng lẻ không đủ.
  - **Kiểm chứng:** Static call path là profile 0.433.0 → exact image/class → `verify_runtime_method` → stored descriptor → `map_pokemon_try_capture` → existing main-thread `api.runtime_invoke`. Zero/ambiguous/token/type/class mismatch để pointer null và log reason/count. Không có cast evidence RVA thành callable address. `ANDROID_NDK=/Users/admin/Library/Android/sdk/ndk/28.2.13676358 ./scripts/build-magisk.sh` đạt arm64-v8a/x86_64 và package; lần build đầu chỉ ra helper two-parameter cũ không còn dùng, đã xóa rồi build lại sạch.
  - **Sai khác so với plan:** Không đổi argument construction; static `PokeballThrow` 12-byte layout từ VBR-004 được giữ nguyên. Promise/result và authoritative postcondition vẫn là gate riêng, chưa claim device verification.

  </details>
- [x] **VBR-302** **[zygisk]** *(native)* — Migrate
  `GetPokemon`/`ReleasePokemon` trong `runtime_probe_service_owners.inc` và
  transfer binding, cùng `GetItemCount`/`RecycleItem` trong discard binding,
  sang verified descriptor expectation. Thay substring/name-arity guard bằng
  exact strong identity cho mutation entry; substring survey có thể giữ chỉ
  để diagnostic. Giữ `runtime_invoke`, exception out, main-thread scheduling
  và owner generation. `ItemData`/Promise contract thiếu evidence phải chặn
  executor. **AC:** `AC-VBR-03`–`AC-VBR-05`, `AC-VBR-07`. **Phụ thuộc:**
  `VBR-004`, `VBR-103`, `VBR-203`. **Verify:** Native build; audit bốn method
  đều có exact token/full signature, zero/ambiguous/mismatch không set transfer/
  discard verified; không có direct-RVA call.

  <details>
  <summary>Đã hoàn thành — VBR-302</summary>

  - **Tạo file:** Không.
  - **Sửa file:** `runtime_native_prelude.inc` — thêm stored descriptor cho `GetItemCount`, `RecycleItem`, `GetPokemon`, `ReleasePokemon`; `runtime_probe_service_owners.inc` — migrate bốn binding sang exact profile/resolver và structured reason; `modules/transfer/promise_observer.inc` — đóng execution tới khi Phase 4 xác minh owner/Promise/postcondition; chính checklist này — cập nhật task.
  - **Xóa file:** Không.
  - **Di chuyển/đổi tên:** Không.
  - **Thay đổi chức năng:** Item/Pokémon bag owner class phải là exact class pointer từ `holo-game.dll`; mỗi method pointer chỉ publish khi unique full signature + exact token pass. Inventory/discard/transfer binding còn yêu cầu exact build và dependency tương ứng. `RecycleItem` tiếp tục resolve exact parameter class từ verified method row; missing ctor giữ discard unverified. Transfer executor hiện fail-closed (`kTransferExecutionEnabled=false`) vì Promise/result/owner lifetime chưa qua device pass; discard vốn đã đóng.
  - **Kiểm chứng:** Audit không còn name+arity lookup cho bốn critical methods trong service-owner path; zero/ambiguous/type/token/class mismatch không set `inventory_read_verified`, `discard_verified` hoặc `transfer_verified`. Action paths vẫn dùng main-thread scheduling, `runtime_invoke` và exception out. `rg` không thấy direct-RVA callable cast/load-bias call. `git diff --check` đạt. `ANDROID_NDK=/Users/admin/Library/Android/sdk/ndk/28.2.13676358 ./scripts/build-magisk.sh` đạt hai ABI và package thành công.
  - **Sai khác so với plan:** Substring helper cũ vẫn tồn tại cho non-critical diagnostic/legacy bindings, nhưng không còn quyết định bốn method này. Không đổi wire contract.

  </details>

- [x] **VBR-303** **[zygisk]** *(native)* — Chỉ khi `VBR-003` có exact
  entry/thunk + verified `MethodInfo` accessor/layout, implement optional
  RVA-pointer guard trong unit build profile/probe tách biệt. Tính runtime
  address từ exact module load bias, yêu cầu pointer thuộc executable mapping
  của cùng ELF và match accepted entry/thunk set. Token/RVA mâu thuẫn đóng
  capability; không fallback. Nếu `VBR-003` còn unknown, ghi policy
  `unavailable` và không đọc `DirectRuntimeMethodInfoView` như proof.
  **AC:** `AC-VBR-03`, `AC-VBR-04`. **Phụ thuộc:** `VBR-003`, `VBR-203`.
  **Verify:** Exact-entry cases match expected address; out-of-module/mismatch
  fail closed; `rg` xác nhận không có cast RVA sang callable function; nếu
  evidence vẫn unknown, profile ghi `unavailable`, readiness không đọc internal
  layout và task chỉ hoàn tất sau khi review xác nhận RVA guard không được quảng
  bá là active; không giả success.

  <details>
  <summary>Đã hoàn thành — VBR-303</summary>

  - **Tạo file:** Không.
  - **Sửa file:** `modules/catch_spin/direct_map_actions.inc` — xóa `DirectRuntimeMethodInfoView`, offset assertions, method-pointer dereference và metadata-layout log; chính checklist này — cập nhật task.
  - **Xóa file:** Không.
  - **Di chuyển/đổi tên:** Không.
  - **Thay đổi chức năng:** Không implement RVA equality guard vì VBR-003 chứng minh cả năm mapped RVA là interior instruction trong coarse non-thunk body, không phải exact entry. Profile giữ `evidence_rva` chỉ cho provenance và `RvaGuardPolicy::kUnavailable`; readiness không đọc internal `MethodInfo` layout. Invocation vẫn chỉ qua verified `MethodInfo*` + `il2cpp_runtime_invoke`.
  - **Kiểm chứng:** `rg` thấy năm RVA chỉ xuất hiện trong profile với năm policy `kUnavailable`; không có module-base/load-bias RVA calculation hoặc cast RVA thành callable. Direct catch không còn `DirectRuntimeMethodInfoView`/`method_pointer` dereference. `git diff --check` đạt. `ANDROID_NDK=/Users/admin/Library/Android/sdk/ndk/28.2.13676358 ./scripts/build-magisk.sh` đạt hai ABI và package thành công.
  - **Sai khác so với plan:** Nhánh “exact entry/thunk” không đạt evidence nên guard không được tạo và không được quảng bá active; đây là outcome fail-closed được task cho phép, không phải giả success.

  </details>

- [x] **VBR-304** **[zygisk]** *(native/reviewer)* — Chuẩn hóa
  `RuntimeBinding` và `host/runtime_capabilities.inc` để method identity,
  exact owner class/session generation, invocation availability và outcome/
  postcondition observer là các gate/reason tách biệt. Reset binding phải
  invalidate descriptors/owners/pending cũ; không persist live pointer. Giữ
  status `UNVERIFIED_DEVICE` khi static identity pass nhưng runtime evidence
  chưa có; chỉ đổi bridge codec nếu existing reason model không biểu diễn
  được, và khi đó phải version codec hai phía cùng lúc. **AC:**
  `AC-VBR-05`, `AC-VBR-06`. **Phụ thuộc:** `VBR-301`, `VBR-302`.
  **Verify:** Static state-transition review cho new session/owner rebind/method
  mismatch/outcome missing; existing native/protocol checks nếu wire không đổi,
  hoặc existing native + Gradle codec checks nếu wire đổi; không tạo test source.

  <details>
  <summary>Đã hoàn thành — VBR-304</summary>

  - **Tạo file:** Không.
  - **Sửa file:** `runtime_native_prelude.inc` — tách method identity/owner-session/invocation/outcome flags và thêm pending direct-catch generation; catch/transfer/discard module/action units — đóng mutation chưa device-verified và reject pending owner-generation mismatch; `host/runtime_capabilities.inc`/`runtime_ui_status_bridge.inc` — capability yêu cầu outcome gate, status dùng `runtime_postcondition_unverified_device`; `inventory_filter_factory.inc` — compatible owned type-name release cho existing standalone harness; chính checklist này — cập nhật task.
  - **Xóa file:** Không.
  - **Di chuyển/đổi tên:** Không.
  - **Thay đổi chức năng:** New binding reset value-initializes descriptor/owner/outcome flags rồi tăng generation. Direct-catch/transfer pending lưu generation và fail/clear khi rebind; discard đã có generation checks/reconcile barrier. `DIRECT_CATCH`, transfer và discard executors giữ disabled; method/owner/invocation pass không tự thành completed/capability. Không persist live pointer và không đổi bridge codec/version.
  - **Kiểm chứng:** Static transition review: new session/rebootstrap xóa descriptor/owner readiness; method mismatch để invocation false; owner rebind invalidates pending generation; outcome missing giữ capability absent và module status `runtime_postcondition_unverified_device`. Hai protocol-test path ghi trong AGENTS không còn tồn tại nên lần gọi báo missing file, không có source bị tạo/sửa. Sáu existing native tests hiện có trong `zygisk/tests/` đều compile/run pass với `-Wall -Wextra -Werror`. Magisk build sau sửa đạt arm64-v8a/x86_64. `git diff --check` đạt.
  - **Sai khác so với plan:** Không đổi wire contract vì existing module error string biểu diễn được trạng thái device-unverified. `CATCH` encounter path cũ nằm ngoài năm critical method vẫn giữ flag riêng; direct-map catch 0.433.0 đã đóng.

  </details>
