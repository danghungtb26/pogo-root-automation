# Phase 0 — Evidence và exact contracts

Plan: [Tổng quan](00-overview.md)

Điều kiện bắt đầu: artifact 0.433.0 read-only và canonical REA bundle
tồn tại; mutation capability vẫn đóng.

Điều kiện hoàn tất: exact static identity/contract và residual unknown cho
năm critical method, token/type API, RVA entry/thunk và managed argument/result
được ghi bằng Evidence có provenance; không có unknown nào bị thay bằng
suy đoán từ 0.427.0.

Sau mỗi task, cập nhật ngay khối details dưới checkbox với file tạo/
sửa/xóa/di chuyển, thay đổi method/param/hành vi và kiểm chứng thực
tế trước khi chuyển task. Chỉ tick `[x]` khi Verify đạt; chưa đạt
thì giữ `[ ]` và ghi phần còn thiếu.

## Task

- [x] **VBR-001** **[reverse]** *(native)* — Re-verify
  `reverse/artifacts/SHA256SUMS`, import/query
  `reverse/rea/rea-evidence-2026-10-13.json`, và tạo
  `docs/issues/2026-10-14/il2cpp-name-vs-rva-resolution/evidence-ledger.md`
  (file đề xuất — chưa tồn tại). Ledger ghi exact image/namespace/class,
  name, instance/static, arity, param/return types, token, slot, RVA/file offset,
  artifact SHA-256, Evidence ID, authority, coverage và limitation cho
  `TryCapture`, `GetPokemon`, `ReleasePokemon`, `GetItemCount`, `RecycleItem`.
  Row chưa đủ evidence phải mang `UNRESOLVED`, không được suy ra từ report
  hay dump 0.427.0. **AC:** `AC-VBR-04`, `AC-VBR-08`. **Phụ thuộc:** Không.
  **Verify:** `shasum -a 256 -c reverse/artifacts/SHA256SUMS` pass 10/10;
  REA summary trả 26 retained record/4 unknown revision và 6 record cho ELF
  SHA nêu trong overview; review ledger không có token/RVA bị đánh đồng.

  <details>
  <summary>Đã hoàn thành — VBR-001</summary>

  - **Tạo file:** `docs/issues/2026-10-14/il2cpp-name-vs-rva-resolution/evidence-ledger.md` — ledger exact artifact/method identity, Evidence và residual unknown.
  - **Sửa file:** chính checklist này — cập nhật trạng thái/kết quả task.
  - **Xóa file:** Không.
  - **Di chuyển/đổi tên:** Không.
  - **Thay đổi chức năng:** Không; task chỉ chuẩn hóa static evidence, không đổi runtime.
  - **Kiểm chứng:** `shasum -a 256 -c reverse/artifacts/SHA256SUMS` → 10/10 `OK`; `import_evidence_bundle` → bundle đã có đủ 26 record, không import trùng; `get_evidence_bundle` lọc ELF SHA-256 → 6 matching record và 4 unknown revision. `jq` đối chiếu năm mapping row xác nhận token/slot/arity/RVA/file offset; ledger phân biệt rõ token với RVA và đánh `UNRESOLVED` cho signature/ABI/runtime behavior.
  - **Sai khác so với plan:** Không.

  </details>
- [x] **VBR-002** **[reverse]** *(native)* — Xác minh trên exact
  `libil2cpp.so` export/signature/ownership contract của
  `il2cpp_method_get_token`, `il2cpp_type_get_name` và `il2cpp_free`; dùng REA
  shipped-artifact Evidence và header cùng Unity/IL2CPP build nếu có provenance.
  Ghi candidate C ABI, ownership, null/error semantics và limitation vào ledger.
  Không đề xuất free chuỗi cho tới khi ownership được chứng minh.
  **AC:** `AC-VBR-02`, `AC-VBR-08`. **Phụ thuộc:** `VBR-001`.
  **Verify:** Evidence record có exact ELF SHA/authority/coverage; ledger nêu rõ
  typedef được chấp nhận hoặc residual unknown chặn `VBR-201`.

  <details>
  <summary>Đã hoàn thành — VBR-002</summary>

  - **Tạo file:** Không.
  - **Sửa file:** `docs/issues/2026-10-14/il2cpp-name-vs-rva-resolution/evidence-ledger.md` — thêm exact export/ABI/ownership/null contract và Evidence cho ba IL2CPP inspection API; chính checklist này — cập nhật task.
  - **Xóa file:** Không.
  - **Di chuyển/đổi tên:** Không.
  - **Thay đổi chức năng:** Chưa đổi runtime. Typedef được chấp nhận cho bước triển khai là `uint32_t (*)(const void *)`, `char *(*)(const Il2CppType *)`, và `void (*)(void *)`; buffer từ `type_get_name` là caller-owned và phải giải phóng đúng một lần qua `il2cpp_free`.
  - **Kiểm chứng:** REA regex search trên exact ELF/read-only Ghidra project tạo `ev_adbcb3…`, xác nhận export/address. Ba complete function dossier tạo `ev_8314c7…`, `ev_a9cabf…`, `ev_11f38e…`: token đọc 32-bit từ method pointer; type name cấp phát bằng imported `malloc`, copy NUL và trả pointer; free là thunk tới imported `free`. Exact SHA-256/authority/coverage/limitation đã ghi trong ledger. Mỗi Ghidra process exit code 0 và được xác nhận không còn tồn tại trước target kế tiếp.
  - **Sai khác so với plan:** Không tìm thấy header cùng Unity build có provenance; contract được chấp nhận từ complete shipped-artifact body. REA báo `cleanup_incomplete` do macOS `sysctl_errno_22`, nhưng partial observation chứa phân tích hoàn tất; limitation đã ghi rõ.

  </details>

- [x] **VBR-003** **[reverse]** *(native)* — Dùng
  `scripts/rea-ghidra-project.sh` và project read-only
  `reverse/artifacts/pogo-0.433.0/pogo-0.433.0.rep` để tạo focused
  snapshot/dossier cho RVA năm method. Xác định từng RVA là exact entry,
  adjustor thunk, generic-shared body hay chỉ code containment; export Evidence
  deterministic và snapshot vào `build/rea/`. Nếu provider/host toolchain lỗi,
  ghi blocker/residual unknown và giữ RVA guard `unavailable`, không chạy lại
  call giống hệt hoặc dùng script reverse thay thế. **AC:** `AC-VBR-04`,
  `AC-VBR-08`. **Phụ thuộc:** `VBR-001`. **Verify:** Mỗi RVA có exact
  entry/thunk verdict + Evidence ID, hoặc explicit unknown có provider error;
  canonical bundle/snapshot hash được ghi trong ledger.

  <details>
  <summary>Đã hoàn thành — VBR-003</summary>

  - **Tạo file:** `build/rea/pogo-0.433.0-ghidra-focused-evidence.json` — deterministic 11-record focused Evidence export; snapshot `build/rea/pogo-0.433.0-ghidra-analysis.snapshot.json` được cập nhật bởi REA.
  - **Sửa file:** `docs/issues/2026-10-14/il2cpp-name-vs-rva-resolution/evidence-ledger.md` — thêm verdict từng RVA và policy; chính checklist này — cập nhật task.
  - **Xóa file:** Xóa file trung gian `build/rea/pogo-0.433.0-ghidra-focused-evidence.raw.json` do task tạo sau khi REA export thành công.
  - **Di chuyển/đổi tên:** Không.
  - **Thay đổi chức năng:** Chưa đổi runtime; chốt policy direct-RVA `UNAVAILABLE` cho cả năm candidate.
  - **Kiểm chứng:** REA `function` trên năm RVA trong existing project read-only trả exact containing bodies: `0x9e1c6cc`, `0xa04bd08`, `0xa04ce80`, `0x9ffe404`, `0xa006ba8`; mọi body đều `thunk=false`, và mỗi candidate là instruction/basic-block nội bộ chứ không bằng entry. Evidence IDs từng row đã ghi trong ledger. `rea evidence-export` validate/export 11 records; snapshot SHA-256 `81c4c7…`, bundle SHA-256 `b2201d…`.
  - **Sai khác so với plan:** Kết quả mạnh hơn residual unknown: năm RVA đều bị chứng minh không phải entry theo existing Ghidra analysis, nên không tiếp tục xem chúng là direct-call candidate.

  </details>

- [x] **VBR-004** **[reverse]** *(native)* — Hoàn thiện static managed
  contracts liên quan trực tiếp tới invocation: exact 0.433.0 layout/type
  identity của `PokeballThrow`, `ItemData` và `ExpiringItemsCopy`; exact return/
  concrete generic Promise/result type của `TryCapture`, `ReleasePokemon`,
  `RecycleItem`; result/status/captured storage ID và candidate refresh/
  postcondition signal. Phân biệt static-confirmed, inferred và runtime-only.
  **AC:** `AC-VBR-07`, `AC-VBR-08`. **Phụ thuộc:** `VBR-001`.
  **Verify:** Ledger có một contract row cho mỗi argument/return/result với
  Evidence ID/limitation; runtime-only owner/lifetime/postcondition mang
  `UNVERIFIED_DEVICE` và chặn action capability.

  <details>
  <summary>Đã hoàn thành — VBR-004</summary>

  - **Tạo file:** `scripts/lib/il2cpp-metadata-v39.mjs` — parser fail-closed cho runtime type table, generic instance, method signature và selected field offsets metadata v39.
  - **Sửa file:** `scripts/il2cpp-metadata-map.mjs` — schema v2, thêm exact managed signatures/type contracts; `reverse/rea/pogo-0.433.0-il2cpp-mapping.json` — regenerate 672 matches/6 type contracts; `docs/issues/2026-10-14/il2cpp-name-vs-rva-resolution/evidence-ledger.md` — ghi contracts và residual runtime unknown; chính checklist này — cập nhật task.
  - **Xóa file:** Không.
  - **Di chuyển/đổi tên:** Không.
  - **Thay đổi chức năng:** Chưa đổi runtime. Static contract nay phân biệt đúng `ItemData` typeDef 2769 với type cùng tên, xác nhận `PokeballThrow` layout, exact generic Promise/result và `ExpiringItemsCopy` return type.
  - **Kiểm chứng:** `node --check` pass cho cả hai mapper module; regeneration pass trên exact ELF/metadata hashes, runtime type coverage 154,001, 672 method matches, 6 selected type contracts. `jq` xác nhận năm critical signature, `ItemData` 10-arg ctor token `0x06003b79`, `PokeballThrow` data offsets `0/4/8/9/10`, catch status/captured ID offsets `16/32`, release status offset `16`, recycle result/new-count offsets `16/20`. Mapping SHA-256 `3323117e…`; upstream exact-ELF REA Evidence IDs/limitations được liên kết trong ledger.
  - **Sai khác so với plan:** Static type/layout/result contracts đã đủ; owner/lifetime, Promise completion/error semantics, success enum meaning và authoritative postcondition cố ý giữ `UNVERIFIED_DEVICE`, nên mutation capability vẫn đóng.

  </details>
