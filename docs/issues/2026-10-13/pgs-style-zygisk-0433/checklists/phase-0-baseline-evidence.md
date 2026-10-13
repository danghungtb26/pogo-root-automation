# Phase 0 — Baseline và evidence contract

Plan: [Tổng quan](00-overview.md)

## Điều kiện bắt đầu

- Checkout hiện tại đọc được và chưa có thao tác reset/checkout làm mất thay đổi.
- Đã đọc `AGENTS.md` tại root và trong các thư mục sẽ chạm tới.
- `00-overview.md` đã pin identity của Pokémon GO 0.433.0.
- Artifact read-only, manifest hash, báo cáo lịch sử và bundle REA đã được đặt
  dưới `reverse/`; script reverse cũ không còn tồn tại.

## Task

- [x] **T-001** **[reverse]** *(native/reverse)* — Lập evidence ledger tại `docs/issues/2026-10-13/pgs-style-zygisk-0433/evidence/pogo-0433-binding-evidence.md`. Bắt đầu bằng `reverse/artifacts/SHA256SUMS`, import/query bundle `reverse/rea/rea-evidence-2026-10-13.json`, rồi ghi artifact path/hash, Evidence ID, authority, coverage và limitation cho package/version/versionCode, base APK, metadata, build ID `libil2cpp.so` và bảng method (`GetPokemon`, `ReleasePokemon`, `GetItemCount`, `RecycleItem`). Báo cáo trong `reverse/reports/` chỉ là historical reference. Phân biệt metadata identity với RVA/runtime address và đánh dấu field/signature chưa có REA evidence là `UNRESOLVED`. **AC:** Ledger cho phép reviewer truy ngược từng binding về artifact + REA Evidence và không biến suy đoán/report cũ thành fact. **Phụ thuộc:** Không. **Verify:** `shasum -a 256 -c reverse/artifacts/SHA256SUMS`; REA bundle summary có 17 record/2 unknown; đối chiếu BuildID `da59ce3024c4ccfe16fa6616281a4d111de67aa2`; review ledger không có RVA giả định.

  <details>
  <summary>Đã hoàn thành — T-001</summary>

  Đã tạo `evidence/pogo-0433-binding-evidence.md`, ghi artifact/hash, retained
  Evidence IDs, authority, coverage và limitation. Bundle import/query trả 17
  record/2 unknown; toàn bộ SHA256SUMS đạt; ELF build ID khớp. Các method storage
  chỉ có token từ historical material nên giữ `UNRESOLVED`, không có RVA giả định.

  </details>

- [x] **T-002** **[docs]** *(reviewer)* — Chụp baseline kiến trúc và thay đổi chưa commit vào `docs/issues/2026-10-13/pgs-style-zygisk-0433/evidence/repository-baseline.md`: call path Kotlin → bridge → native, module packaging hiện tại, runtime fingerprint guard, capability gates, file đang dirty và file gần ngưỡng 500 dòng. Ghi rõ không có version/update network trong phạm vi mới. **AC:** Mỗi phase sau biết điểm nối hiện có và vùng phải bảo toàn; danh sách dirty files khớp worktree tại thời điểm bắt đầu. **Phụ thuộc:** Không. **Verify:** `git status --short`, `git diff --stat`, `rg` các entry point/config/runtime status và kiểm tra line count; kết quả thực tế được ghi vào ledger.

  <details>
  <summary>Đã hoàn thành — T-002</summary>

  Đã tạo `evidence/repository-baseline.md` tại HEAD
  `47800bada0dc8cd97910b4d7a1bf7f034a2bd719`. Baseline worktree sạch; đã ghi
  call path, packaging, fingerprint/capability guards, lifecycle gaps, HTTP
  surface cũ và các source gần 500 dòng.

  </details>

- [x] **T-003** **[build]** *(reviewer)* — Chạy baseline không sửa source: existing host-native checks theo `.github/workflows/ci.yml`, `ANDROID_NDK_HOME=<ndk> ./scripts/build-magisk.sh`, và `./gradlew test assembleDebug`; ghi command, toolchain, pass/fail và lỗi sẵn có vào `repository-baseline.md`. Không tạo hoặc sửa test source. **AC:** Có mốc so sánh thực tế trước implementation; lỗi baseline không bị gán nhầm cho phase sau. **Phụ thuộc:** `T-002`. **Verify:** Exit code và artifact path của từng lệnh được lưu; `git status --short` sau chạy không xuất hiện source edit ngoài generated/ignored output dự kiến.

  <details>
  <summary>Đã hoàn thành — T-003</summary>

  Baseline PASS: sáu host C++ checks; native build/package bằng NDK
  28.2.13676358; `./gradlew test assembleDebug` (51 task, exit 0). ZIP SHA-256
  `942667...973bbd`; APK SHA-256 `3b3e6a...d94c4`. Worktree vẫn sạch sau build.

  </details>

## Điều kiện hoàn tất

- Evidence ledger và repository baseline tồn tại, có nguồn và trạng thái unknown rõ ràng.
- Baseline build/check đã chạy hoặc task vẫn mở với blocker có log cụ thể.
- Dirty worktree ban đầu được ghi lại và không bị ghi đè.

Sau mỗi task, agent phải cập nhật checkbox và khối `<details>` của chính task đó
trước khi chuyển sang task kế tiếp.
