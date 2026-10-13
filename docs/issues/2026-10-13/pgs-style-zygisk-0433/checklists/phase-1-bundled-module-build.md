# Phase 1 — Build và nhúng Magisk module

Plan: [Tổng quan](00-overview.md)

## Điều kiện bắt đầu

- `T-001`–`T-003` đã có kết quả baseline.
- Luồng package hiện tại trong `scripts/build-magisk.sh`, `scripts/package-magisk.sh`
  và `app/build.gradle.kts` đã được xác định.

## Task

- [ ] **T-101** **[build]** *(native/build)* — Chuẩn hóa output của `scripts/build-magisk.sh` và `scripts/package-magisk.sh`: sinh một Magisk ZIP canonical cùng manifest máy đọc được chứa module id, version, ABI, SHA-256 ZIP, fingerprint target và source revision. Không commit thêm bản ZIP trùng lặp vào source tree. **AC:** Cùng checkout/toolchain cho ra module có cấu trúc hợp lệ và manifest trỏ đúng ZIP/hash; package fail khi thiếu `arm64-v8a` library hoặc module metadata. **Phụ thuộc:** `T-003`. **Verify:** Chạy `bash -n` cho script, chạy `ANDROID_NDK_HOME=<ndk> ./scripts/build-magisk.sh`, liệt kê ZIP và tính lại SHA-256 để so manifest.

  <details>
  <summary>Chưa thực hiện — T-101</summary>

  Chưa có thay đổi hoặc kết quả kiểm chứng. Agent cập nhật khối này ngay sau khi xử lý task, trước khi sang task tiếp theo.

  </details>

- [ ] **T-102** **[build]** *(android)* — Sửa `app/build.gradle.kts` để task assemble phụ thuộc task chuẩn bị bundled module, copy ZIP + manifest từ output `T-101` vào generated assets (đề xuất `app/build/generated/pogoModule/assets/pogo-module/`) và fail build nếu artifact thiếu/stale. Xuất version/hash cần thiết qua generated resource hoặc `BuildConfig`, không hard-code hai nguồn. **AC:** Debug APK self-contained, mỗi build lấy đúng module vừa sinh và không cần network. **Phụ thuộc:** `T-101`. **Verify:** `./gradlew :app:assembleDebug`; mở APK như ZIP và xác nhận đúng một module ZIP + manifest, hash asset khớp manifest bên trong APK.

  <details>
  <summary>Chưa thực hiện — T-102</summary>

  Chưa có thay đổi hoặc kết quả kiểm chứng. Agent cập nhật khối này ngay sau khi xử lý task, trước khi sang task tiếp theo.

  </details>

- [ ] **T-103** **[build]** *(build/reviewer)* — Cập nhật `.github/workflows/ci.yml` và tài liệu build liên quan để CI build native/package trước APK, kiểm tra bundled asset và không dựa vào file sinh từ máy developer. Giữ lệnh local tương đương CI và không thêm download/update runtime. **AC:** Fresh checkout có thứ tự dependency xác định; CI phát hiện ZIP/hash thiếu hoặc lệch trước khi publish APK. **Phụ thuộc:** `T-101`, `T-102`. **Verify:** Review workflow dependency, chạy các lệnh tương đương local, kiểm tra không có asset generated bị commit nhầm bằng `git status --short`.

  <details>
  <summary>Chưa thực hiện — T-103</summary>

  Chưa có thay đổi hoặc kết quả kiểm chứng. Agent cập nhật khối này ngay sau khi xử lý task, trước khi sang task tiếp theo.

  </details>

- [ ] **T-104** **[build]** *(reviewer)* — Verify phase: chạy native package, `./gradlew test assembleDebug`, kiểm tra APK entries/module ZIP/module manifest, `git diff --check`, và kiểm tra mọi source không phải Markdown còn tối đa 500 dòng. Không sửa/tạo test source để “làm xanh” build. **AC:** `AC-01` đạt với artifact tự chứa, reproducible theo manifest và không có whitespace/line-limit regression. **Phụ thuộc:** `T-101`–`T-103`. **Verify:** Lưu command/exit code/hash/artifact path vào details; nếu bất kỳ check nào fail thì giữ task `[ ]`.

  <details>
  <summary>Chưa thực hiện — T-104</summary>

  Chưa có thay đổi hoặc kết quả kiểm chứng. Agent cập nhật khối này ngay sau khi xử lý task, trước khi sang task tiếp theo.

  </details>

## Điều kiện hoàn tất

- Debug APK chứa module ZIP và manifest/hash đúng từ cùng build.
- Native ZIP vẫn dùng được như artifact độc lập; CI/local build có cùng thứ tự.
- Không có download/version-update path mới.

Sau mỗi task, agent phải cập nhật checkbox và khối `<details>` của chính task đó
trước khi chuyển sang task kế tiếp.
