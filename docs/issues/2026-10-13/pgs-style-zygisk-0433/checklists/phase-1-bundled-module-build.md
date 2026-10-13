# Phase 1 — Build và nhúng Magisk module

Plan: [Tổng quan](00-overview.md)

## Điều kiện bắt đầu

- `T-001`–`T-003` đã có kết quả baseline.
- Luồng package hiện tại trong `scripts/build-magisk.sh`, `scripts/package-magisk.sh`
  và `app/build.gradle.kts` đã được xác định.

## Task

- [x] **T-101** **[build]** *(native/build)* — Chuẩn hóa output của `scripts/build-magisk.sh` và `scripts/package-magisk.sh`: sinh một Magisk ZIP canonical cùng manifest máy đọc được chứa module id, version, ABI, SHA-256 ZIP, fingerprint target và source revision. Không commit thêm bản ZIP trùng lặp vào source tree. **AC:** Cùng checkout/toolchain cho ra module có cấu trúc hợp lệ và manifest trỏ đúng ZIP/hash; package fail khi thiếu `arm64-v8a` library hoặc module metadata. **Phụ thuộc:** `T-003`. **Verify:** Chạy `bash -n` cho script, chạy `ANDROID_NDK_HOME=<ndk> ./scripts/build-magisk.sh`, liệt kê ZIP và tính lại SHA-256 để so manifest.

  <details>
  <summary>Đã hoàn thành — T-101</summary>

  `package-magisk.sh` nay validate `module.prop`, hai ABI và sinh manifest JSON
  cạnh ZIP với module/version/ABI/hash/source revision/target 0.433.0. `bash -n`
  và native build bằng NDK 28.2 PASS; ZIP SHA-256 `d0486c...9be43` khớp manifest.

  </details>

- [x] **T-102** **[build]** *(android)* — Sửa `app/build.gradle.kts` để task assemble phụ thuộc task chuẩn bị bundled module, copy ZIP + manifest từ output `T-101` vào generated assets (đề xuất `app/build/generated/pogoModule/assets/pogo-module/`) và fail build nếu artifact thiếu/stale. Xuất version/hash cần thiết qua generated resource hoặc `BuildConfig`, không hard-code hai nguồn. **AC:** Debug APK self-contained, mỗi build lấy đúng module vừa sinh và không cần network. **Phụ thuộc:** `T-101`. **Verify:** `./gradlew :app:assembleDebug`; mở APK như ZIP và xác nhận đúng một module ZIP + manifest, hash asset khớp manifest bên trong APK.

  <details>
  <summary>Đã hoàn thành — T-102</summary>

  Thêm Gradle task `prepareBundledPogoModule`: fail khi thiếu/stale/revision hoặc
  SHA lệch, copy ZIP+manifest vào generated assets và sinh resource id/version/hash.
  `:app:assembleDebug` PASS. APK có đúng hai entry dưới `assets/pogo-module/`;
  SHA của ZIP nhúng là `d0486c...9be43`, khớp manifest.

  </details>

- [x] **T-103** **[build]** *(build/reviewer)* — Cập nhật `.github/workflows/ci.yml` và tài liệu build liên quan để CI build native/package trước APK, kiểm tra bundled asset và không dựa vào file sinh từ máy developer. Giữ lệnh local tương đương CI và không thêm download/update runtime. **AC:** Fresh checkout có thứ tự dependency xác định; CI phát hiện ZIP/hash thiếu hoặc lệch trước khi publish APK. **Phụ thuộc:** `T-101`, `T-102`. **Verify:** Review workflow dependency, chạy các lệnh tương đương local, kiểm tra không có asset generated bị commit nhầm bằng `git status --short`.

  <details>
  <summary>Đã hoàn thành — T-103</summary>

  CI nay gọi canonical `build-magisk.sh` trước host checks/Gradle, rồi kiểm ZIP,
  manifest, APK entry count và hash trước upload; các block CMake/package trùng đã
  bỏ. README ghi cùng thứ tự local. Generated assets vẫn nằm dưới ignored build/.

  </details>

- [x] **T-104** **[build]** *(reviewer)* — Verify phase: chạy native package, `./gradlew test assembleDebug`, kiểm tra APK entries/module ZIP/module manifest, `git diff --check`, và kiểm tra mọi source không phải Markdown còn tối đa 500 dòng. Không sửa/tạo test source để “làm xanh” build. **AC:** `AC-01` đạt với artifact tự chứa, reproducible theo manifest và không có whitespace/line-limit regression. **Phụ thuộc:** `T-101`–`T-103`. **Verify:** Lưu command/exit code/hash/artifact path vào details; nếu bất kỳ check nào fail thì giữ task `[ ]`.

  <details>
  <summary>Đã hoàn thành — T-104</summary>

  Native package PASS; `./gradlew test assembleDebug` PASS (52 tasks). Standalone
  và bundled ZIP đều có SHA-256 `d0486c...9be43`; APK SHA-256
  `7c29bd...91b2a`; đúng một ZIP asset và manifest khớp. `git diff --check` và
  source line-limit audit PASS; không tạo/sửa test source.

  </details>

## Điều kiện hoàn tất

- Debug APK chứa module ZIP và manifest/hash đúng từ cùng build.
- Native ZIP vẫn dùng được như artifact độc lập; CI/local build có cùng thứ tự.
- Không có download/version-update path mới.

Sau mỗi task, agent phải cập nhật checkbox và khối `<details>` của chính task đó
trước khi chuyển sang task kế tiếp.
