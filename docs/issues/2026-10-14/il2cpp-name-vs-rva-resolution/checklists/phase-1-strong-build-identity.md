# Phase 1 — Strong runtime build identity

Plan: [Tổng quan](00-overview.md)

Điều kiện bắt đầu: `VBR-001` hoàn tất; expected package/version/
build ID/metadata hash đã pin.

Điều kiện hoàn tất: injected runtime tự xác minh bốn strong identity
field, có reason fail-closed và không tin controller fingerprint là proof.

Sau mỗi task, cập nhật ngay khối details dưới checkbox với file tạo/
sửa/xóa/di chuyển, thay đổi method/param/hành vi và kiểm chứng thực
tế trước khi chuyển task. Chỉ tick `[x]` khi Verify đạt; chưa đạt
thì giữ `[ ]` và ghi phần còn thiếu.

## Task

- [x] **VBR-101** **[zygisk]** *(native)* — Chốt design source tin cậy để
  injected runtime đọc installed `versionCode` và hash exact
  `global-metadata.dat` bytes. Đánh giá Android/JNI package info so với
  installed APK/split path + archive member, quyền đọc, lifecycle và chi phí;
  chọn một owner duy nhất trong `shared/runtime/probe/`, không shell/ADB/
  filesystem command queue. Ghi quyết định vào evidence ledger trước code.
  **AC:** `AC-VBR-01`. **Phụ thuộc:** `VBR-001`. **Verify:** Design nêu exact
  input bytes/API, trust boundary, cache/invalidation và failure reason; reviewer
  xác nhận không dùng controller claim là observation.

  <details>
  <summary>Đã hoàn thành — VBR-101</summary>

  - **Tạo file:** Không.
  - **Sửa file:** `docs/issues/2026-10-14/il2cpp-name-vs-rva-resolution/evidence-ledger.md` — thêm trust boundary, exact inputs, cache/invalidation và failure policy; chính checklist này — cập nhật task.
  - **Xóa file:** Không.
  - **Di chuyển/đổi tên:** Không.
  - **Thay đổi chức năng:** Chưa đổi runtime; design chọn target `PackageManager` cho `versionCode` và target `AssetManager` member `bin/Data/Managed/Metadata/global-metadata.dat` cho SHA-256.
  - **Kiểm chứng:** Static design review xác nhận cả hai observation xuất phát từ `ActivityThread.currentApplication()` trong process Pokémon GO, cache một lần/process, metadata giới hạn 64 MiB, không dùng controller fingerprint/IPC làm proof, mọi parse/read/mismatch đều fail closed với reason riêng.
  - **Sai khác so với plan:** Không dùng filesystem split/archive parser vì `AssetManager` đã định nghĩa chính xác installed asset bytes, xử lý compressed/uncompressed member và tránh tạo transport/file queue thứ hai.

  </details>
- [x] **VBR-102** **[zygisk]** *(native)* — Implement design `VBR-101`
  trong các unit mới đề xuất
  `zygisk/jni/shared/runtime/probe/runtime_package_identity.inc` và
  `runtime_metadata_identity.inc`, wire hẹp qua `main.cpp`/
  `runtime_probe_discovery.inc`. Hash đúng installed metadata bytes một lần
  mỗi process/session, bounded I/O, không persist pointer/readiness; parse/version
  failure trả false + reason. Giữ mỗi file ≤500 dòng. **AC:** `AC-VBR-01`.
  **Phụ thuộc:** `VBR-101`. **Verify:** Native compile/build; offline
  positive/negative checks chỉ dùng fixture/artifact đã có nếu existing test
  harness hỗ trợ, không tạo/sửa test source; static review success/missing/
  unreadable/hash-mismatch branches.

  <details>
  <summary>Đã hoàn thành — VBR-102</summary>

  - **Tạo file:** `zygisk/jni/shared/runtime/probe/runtime_package_identity.inc` — đọc package/versionCode qua JNI từ application của chính process; `zygisk/jni/shared/runtime/probe/runtime_metadata_identity.inc` — đọc exact metadata asset, SHA-256 streaming 64 KiB, giới hạn 64 MiB và cache một lần/process.
  - **Sửa file:** `zygisk/jni/shared/runtime/probe/runtime_build_profile.h` — thêm expected metadata byte count và observation DTO; `zygisk/jni/shared/core/runtime_native_prelude.inc` — thêm process-local identity cache/mutex; `zygisk/jni/main.cpp` — include hai identity unit; `runtime_probe_diagnostic.inc`/`runtime_probe_discovery.inc` — wire observation hẹp vào owner discovery; chính checklist này — cập nhật task.
  - **Xóa file:** Không.
  - **Di chuyển/đổi tên:** Không.
  - **Thay đổi chức năng:** Runtime lấy package/version độc lập bằng `ActivityThread.currentApplication()` + `PackageManager`, hash `bin/Data/Managed/Metadata/global-metadata.dat` từ `AssetManager`, ghi reason riêng cho unavailable/missing/read/copy/size/hash/version mismatch. Cache không persist pointer/readiness. JNI thread chỉ detach nếu helper tự attach.
  - **Kiểm chứng:** Static review đủ nhánh success, application/JVM/API unavailable, asset missing/unreadable, zero read, size limit, size/hash mismatch và version/package mismatch. Line count: metadata 275, package 115, profile 66, đều ≤500. `git diff --check` đạt. `ANDROID_NDK=/Users/admin/Library/Android/sdk/ndk/28.2.13676358 ./scripts/build-magisk.sh` đạt cho arm64-v8a và x86_64, sinh ZIP multi-ABI cùng manifest.
  - **Sai khác so với plan:** Override NDK 27.1 ghi trong AGENTS không tồn tại trên máy nên lần chạy đầu fail trước compile; dùng NDK 28.2.13676358 đã cài và là default của script. Không có existing offline identity harness phù hợp, vì vậy không tạo test source mới.

  </details>

- [x] **VBR-103** **[zygisk]** *(native/reviewer)* — Thay hai assignment
  hard-code `false` trong `discover_runtime_owners` bằng observed result từ
  `VBR-102`; giữ `RuntimeBuildProfile::Verification::mutation_allowed()` yêu cầu
  package + versionCode + build ID + metadata hash. Chuẩn hóa diagnostic/status
  reason cho mismatch và bảo đảm build khác chỉ read-only. Không mở
  feature capability chỉ vì build identity pass. **AC:** `AC-VBR-01`,
  `AC-VBR-05`. **Phụ thuộc:** `VBR-102`. **Verify:** Static call-path review
  không còn hard-coded false/true bypass; native build; mismatch reason hiển thị
  qua existing runtime status path mà không thay wire contract nếu không cần.

  <details>
  <summary>Đã hoàn thành — VBR-103</summary>

  - **Tạo file:** Không.
  - **Sửa file:** `zygisk/jni/shared/runtime/probe/runtime_probe_discovery.inc` — dùng observed version/metadata verification thay hard-code `false`, log reason và observed values; chính checklist này — cập nhật task.
  - **Xóa file:** Không.
  - **Di chuyển/đổi tên:** Không.
  - **Thay đổi chức năng:** `exact_build_verified` chỉ true khi package process, installed versionCode, loaded IL2CPP build ID và installed metadata SHA-256 đều khớp. Mismatch vẫn đi qua discovery read-only nhưng `Verification::mutation_allowed()` đóng mutation; build identity pass không tự bật feature capability vì các gate owner/method/outcome vẫn độc lập.
  - **Kiểm chứng:** `rg` xác nhận không còn assignment `profile.version_code = false/true` hoặc `profile.metadata_sha256 = false/true`; diagnostic hiện xuất `reason`, `version_reason`, `metadata_reason`, observed version và metadata bytes qua log/status diagnostic hiện có, không đổi wire contract. Lần compile đầu bắt lỗi format macro, đã đổi sang `%llu` + cast đúng như convention hiện hữu. Build lại bằng `ANDROID_NDK=/Users/admin/Library/Android/sdk/ndk/28.2.13676358 ./scripts/build-magisk.sh` đạt cả arm64-v8a/x86_64 và đóng gói multi-ABI.
  - **Sai khác so với plan:** Chưa claim device-verified; nhánh runtime thật sẽ được thu ở Phase 4 bằng script repo. Không mở thêm field wire vì reason chi tiết đã có trong diagnostic path.

  </details>
