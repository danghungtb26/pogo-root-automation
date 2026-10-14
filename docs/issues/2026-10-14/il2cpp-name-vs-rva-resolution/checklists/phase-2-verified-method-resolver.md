# Phase 2 — Verified method resolver

Plan: [Tổng quan](00-overview.md)

Điều kiện bắt đầu: `VBR-002` đã chốt token/type-name ABI; exact
method rows từ `VBR-001` có đủ static identity hoặc explicit unknown.

Điều kiện hoàn tất: có resolver dùng chung trả structured result,
yêu cầu unique full-signature + token match và build-scoped expectation cho
năm critical method; call path vẫn dùng `runtime_invoke`.

Sau mỗi task, cập nhật ngay khối details dưới checkbox với file tạo/
sửa/xóa/di chuyển, thay đổi method/param/hành vi và kiểm chứng thực
tế trước khi chuyển task. Chỉ tick `[x]` khi Verify đạt; chưa đạt
thì giữ `[ ]` và ghi phần còn thiếu.

## Task

- [x] **VBR-201** **[zygisk]** *(native)* — Mở rộng
  `runtime_il2cpp_api.inc` và `runtime_probe_elf.inc` bằng typedef/member/symbol
  resolution cho `il2cpp_method_get_token`; chỉ thêm `il2cpp_free` và owned
  type-name wrapper nếu `VBR-002` xác nhận runtime caller phải free. Cập nhật
  `has_runtime_inspection_api` hoặc tách `has_verified_method_api` sao cho survey
  read-only cũ không bị tắt ngoài ý muốn, còn mutation resolver fail closed
  khi token API thiếu. **AC:** `AC-VBR-02`. **Phụ thuộc:** `VBR-002`.
  **Verify:** Native compile với `-Wall -Wextra -Werror`; static review typedef
  width/const/ownership; diagnostic phân biệt optional survey API và required
  verified-method API.

  <details>
  <summary>Đã hoàn thành — VBR-201</summary>

  - **Tạo file:** Không.
  - **Sửa file:** `zygisk/jni/shared/core/runtime_il2cpp_api.inc` — thêm exact typedef/member `Il2CppMethodGetToken`, `Il2CppFree`, đổi `Il2CppTypeGetName` thành caller-owned `char *` và thêm RAII `OwnedIl2CppTypeName`; `runtime_probe_elf.inc` — resolve hai export, thêm `has_verified_method_api` tách khỏi survey; các caller trong `runtime_probe_inspection.inc`, `runtime_probe_owners.inc`, `direct_map_actions.inc`, `spin_item_bubble_bindings.inc`, `hash_set_binding.inc`, `inventory_binding_contract.inc`, `inventory_filter_factory.inc` — release mọi type name qua RAII; chính checklist này — cập nhật task.
  - **Xóa file:** Không.
  - **Di chuyển/đổi tên:** Không.
  - **Thay đổi chức năng:** Read-only survey cũ vẫn dùng core/inspection predicates; verified resolver về sau chỉ khả dụng khi method enumeration, token, owned type name và free export đều có. Mọi call hiện hữu tới `il2cpp_type_get_name` đã được bọc và free đúng ownership.
  - **Kiểm chứng:** Mọi caller production release type-name allocation: phần chính dùng owned wrapper; `inventory_filter_factory.inc` dùng local SFINAE release helper để giữ compatibility với existing standalone fake-API test không có `il2cpp_free`. Typedef token là `uint32_t(const void *)`, free là `void(void *)`, type name là `char *(const Il2CppType *)`, khớp evidence VBR-002. Compile lần đầu phát hiện helper chưa dùng; đã dùng nó trong optional-API diagnostic. `ANDROID_NDK=/Users/admin/Library/Android/sdk/ndk/28.2.13676358 ./scripts/build-magisk.sh` sau sửa đạt `-Wall -Wextra -Werror` cho arm64-v8a/x86_64 và package thành công. `runtime_il2cpp_api.inc` 148 dòng, `runtime_probe_elf.inc` 328 dòng, đều ≤500.
  - **Sai khác so với plan:** Không.

  </details>
- [x] **VBR-202** **[zygisk]** *(native)* — Tạo resolver dùng chung
  (file đề xuất — chưa tồn tại:
  `zygisk/jni/shared/runtime/probe/runtime_method_contract.h` và
  `runtime_method_resolver.inc`) với `RuntimeMethodExpectation`,
  `VerifiedRuntimeMethod` và enum reason. Resolver nhận exact class đã resolve
  từ expected image/namespace/name; enumerate đến null, exact compare name,
  instance/static, param count/types, return type và token; collect tất cả match
  và chỉ success khi có một. Nếu cần corruption budget, vượt budget
  trả `kEnumerationLimit`, không trả `kNoMatch`. Không invoke và không đọc
  internal `MethodInfo` layout trong resolver này. **AC:** `AC-VBR-02`,
  `AC-VBR-03`. **Phụ thuộc:** `VBR-201`. **Verify:** Existing native
  harness/compile path cover được success/no-match/ambiguous/token mismatch nếu
  harness hiện có; nếu không, static deterministic walkthrough + native build,
  không tạo test source; audit mỗi file ≤500 dòng.

  <details>
  <summary>Đã hoàn thành — VBR-202</summary>

  - **Tạo file:** `zygisk/jni/shared/runtime/probe/runtime_method_contract.h` — expectation/result/reason contract; `runtime_method_resolver.inc` — deterministic unique full-signature + token resolver.
  - **Sửa file:** `zygisk/jni/main.cpp` — include contract trước prelude và resolver sau IL2CPP export resolution; chính checklist này — cập nhật task.
  - **Xóa file:** Không.
  - **Di chuyển/đổi tên:** Không.
  - **Thay đổi chức năng:** Resolver nhận exact class do caller resolve, enumerate tối đa 2048 method, exact-match name, instance/static, param count, từng full param type, full return type và token. Kết quả phân biệt API unavailable, invalid expectation, no match, unavailable type name, token mismatch, ambiguity và enumeration limit; chỉ một exact match mới `verified()`.
  - **Kiểm chứng:** Static deterministic walkthrough: zero signature match → `kNoMatch`; same signature/wrong token → `kTokenMismatch`; one signature+token → `kVerified`; hơn một exact match → pointer bị xóa + `kAmbiguous`; method thứ 2049 → `kEnumerationLimit`, còn đúng 2048 rồi null không bị coi là overflow. `rg` xác nhận resolver không có `runtime_invoke`, direct-RVA cast hay đọc `MethodInfo` layout. Hai file lần lượt 59/112 dòng, ≤500. `git diff --check` đạt. `ANDROID_NDK=/Users/admin/Library/Android/sdk/ndk/28.2.13676358 ./scripts/build-magisk.sh` đạt `-Wall -Wextra -Werror` cho hai ABI và đóng gói thành công.
  - **Sai khác so với plan:** Existing harness không có injectable fake `Il2CppApi`; tuân thủ rule không tạo test source nên dùng walkthrough tĩnh + full native build.

  </details>

- [x] **VBR-203** **[zygisk]** *(native)* — Thêm build-scoped expectation
  table 0.433.0 (file đề xuất — chưa tồn tại:
  `zygisk/jni/shared/runtime/probe/runtime_method_profile_0433.h`) cho
  `MapPokemon.TryCapture`, `PokemonBagImpl.GetPokemon`,
  `PokemonBagImpl.ReleasePokemon`, `ItemBagImpl.GetItemCount`,
  `ItemBagImpl.RecycleItem`. Copy chỉ value đã có trong ledger `VBR-001`/
  `VBR-004`; field chưa chốt không được placeholder có thể pass.
  RVA được lưu như expected evidence cùng enum policy
  `unavailable/optional/required`, không phải callable address. **AC:**
  `AC-VBR-02`, `AC-VBR-04`. **Phụ thuộc:** `VBR-001`, `VBR-004`,
  `VBR-202`. **Verify:** Đối chiếu table với
  `reverse/rea/pogo-0.433.0-il2cpp-mapping.json` và ledger; static review không
  có RVA-to-function cast, legacy 0.427.0 constant hay wildcard return/param cho
  mutation method.

  <details>
  <summary>Đã hoàn thành — VBR-203</summary>

  - **Tạo file:** `zygisk/jni/shared/runtime/probe/runtime_method_profile_0433.h` — build-scoped table cho năm critical method và explicit RVA guard policy.
  - **Sửa file:** `zygisk/jni/main.cpp` — include profile; chính checklist này — cập nhật task.
  - **Xóa file:** Không.
  - **Di chuyển/đổi tên:** Không.
  - **Thay đổi chức năng:** Table pin exact image/namespace/class/name, instance flag, full ordered parameter types, full return type, metadata token và evidence RVA cho `TryCapture`, `GetPokemon`, `ReleasePokemon`, `GetItemCount`, `RecycleItem`. Cả năm RVA policy là `kUnavailable`, nên value chỉ là provenance và không thể trở thành callable address/required equality guard.
  - **Kiểm chứng:** Đối chiếu từng token/signature/RVA với `reverse/rea/pogo-0.433.0-il2cpp-mapping.json` schema v2 và evidence ledger; `RecycleItem` dùng exact static type name `ItemData` của typeDef 2769 theo mapping, không dùng typeDef 11852 làm proof. `rg` xác nhận profile không có cast, constant 0.427, wildcard param/return hoặc policy optional/required. File 80 dòng, ≤500. `git diff --check` đạt. `ANDROID_NDK=/Users/admin/Library/Android/sdk/ndk/28.2.13676358 ./scripts/build-magisk.sh` đạt cả hai ABI và package thành công.
  - **Sai khác so với plan:** Không.

  </details>
