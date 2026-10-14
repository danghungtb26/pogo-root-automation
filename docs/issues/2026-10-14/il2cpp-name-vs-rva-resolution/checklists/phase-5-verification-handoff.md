# Phase 5 — Tích hợp, build và bàn giao

Plan: [Tổng quan](00-overview.md)

Điều kiện bắt đầu: Phase 1–4 implementation task đạt Verify hoặc
phần blocked được giữ fail-closed với residual unknown rõ; không có task
code phụ thuộc blocker nhưng bị báo hoàn tất.

Điều kiện hoàn tất: existing checks/build/package pass, source policy
đạt, evidence/docs và plan PGS-style cũ phản ánh đúng mức sẵn sàng.

Sau mỗi task, cập nhật ngay khối details dưới checkbox với file tạo/
sửa/xóa/di chuyển, thay đổi method/param/hành vi và kiểm chứng thực
tế trước khi chuyển task. Chỉ tick `[x]` khi Verify đạt; chưa đạt
thì giữ `[ ]` và ghi phần còn thiếu.

## Task

- [ ] **VBR-501** **[build]** *(native/reviewer)* — Chạy full existing
  verification theo source/CI, không tạo/sửa test: hai protocol checks trong
  `zygisk/jni`, sáu host-native test trong `.github/workflows/ci.yml`,
  `ANDROID_NDK="$HOME/Library/Android/sdk/ndk/27.1.12297006" ./scripts/build-magisk.sh`,
  và `./gradlew test assembleDebug --rerun-tasks`. Inspect ZIP có
  `zygisk/arm64-v8a.so` + `zygisk/x86_64.so`, APK chứa đúng một bundled ZIP/
  manifest, hash bundle khớp manifest. Chạy `git diff --check` và audit mọi
  source không phải Markdown đã đổi ≤500 dòng. **AC:** `AC-VBR-09`.
  **Phụ thuộc:** `VBR-103`, `VBR-304`, `VBR-401`, `VBR-402`.
  **Verify:** Ghi exact command/exit code, test binary pass/fail, artifact path/
  size/SHA-256 và audit result vào details; thiếu SDK/NDK hoặc failure giữ
  task `[ ]`, không báo code-complete.

  <details>
  <summary>Đã chạy nhưng còn blocker — VBR-501</summary>

  - **Tạo file:** Không.
  - **Sửa file:** Chính checklist này — ghi kết quả thực tế; không sửa test source.
  - **Xóa file:** Không.
  - **Di chuyển/đổi tên:** Không.
  - **Thay đổi chức năng:** Không.
  - **Kiểm chứng đạt:** Sáu host-native tests hiện có trong `.github/workflows/ci.yml` compile/run exit 0 với `-Wall -Wextra -Werror`. `ANDROID_NDK=/Users/admin/Library/Android/sdk/ndk/28.2.13676358 ./scripts/build-magisk.sh` exit 0 cho arm64-v8a/x86_64. `./gradlew test assembleDebug --rerun-tasks` exit 0, 52 task executed. `git diff --check`, `bash -n` REA helper và hai `node --check` mapper exit 0. Mọi source changed/untracked không phải Markdown đã audit ≤500 dòng; lớn nhất `runtime_probe_service_owners.inc` 495 dòng.
  - **Artifacts:** `app-debug.apk` 6,987,714 bytes, SHA-256 `eb8b769d70cbecde9f6c88fd1afb2ce07a9445e7f3e0d531193653a98fae5aff`; ZIP 3,264,456 bytes, SHA-256 `bedccaff3878106adb5dfff95ce68df032fe8adc725e06e12950bc0c7975a4c7`; manifest 787 bytes, SHA-256 `1632ba875224fff2ef94d37a6a34cdd4d728fb773128467d5ce57ee38f3d09ec`. ZIP chứa đúng hai ABI entries; APK chứa đúng một bundled ZIP và một manifest; bundled hashes khớp standalone/manifest.
  - **Blocker:** Hai path bắt buộc `zygisk/jni/runtime_command_protocol_test.cpp` và `runtime_observation_protocol_test.cpp` không tồn tại; compile trả exit 1 `no such file or directory`. Rule cấm tạo/sửa test nên không tạo replacement. NDK 27.1 path ghi trong task cũng không tồn tại; lần build đầu exit 1 trước compile, sau đó dùng installed/default NDK 28.2.13676358. Vì Verify yêu cầu mọi gate pass, giữ task `[ ]`.
  - **Sai khác so với plan:** Existing protocol coverage hiện có là `runtime_automation_event_protocol_test.cpp` và đã pass, nhưng không được coi là thay thế hai file thiếu.

  </details>
- [ ] **VBR-502** **[docs]** *(reviewer)* — Export updated deterministic REA
  bundle chỉ khi có Evidence mới; ghi bundle/snapshot SHA-256 và update
  `reverse/README.md`, evidence ledger, brainstorm kết luận và tài liệu readiness
  nếu support level đổi. Cập nhật các details/dependency trong plan
  2026-10-13: `T-303`, `T-304`, `T-305` và downstream `T-403`/`T-501`/
  `T-601`/`T-701` chỉ theo kết quả thực; task cũ chỉ tick khi Verify riêng
  của nó đạt. Nêu rõ static identity, code-complete,
  `UNVERIFIED_DEVICE` và `LIVE_READY`; không gộp chúng. **AC:** `AC-VBR-08`,
  `AC-VBR-09`. **Phụ thuộc:** `VBR-003`, `VBR-403`, `VBR-501`.
  **Verify:** Link/path/Evidence ID hợp lệ; `git diff --check` sạch; AC-to-task
  review không có dependency vòng hoặc blocked task bị báo complete; không
  claim device result chưa chạy.

  <details>
  <summary>Đã cập nhật nhưng còn dependency blocker — VBR-502</summary>

  - **Tạo file:** Không trong task này.
  - **Sửa file:** `reverse/rea/rea-evidence-2026-10-13.json` (REA deterministic merge/export); `reverse/README.md`; evidence ledger; brainstorm kết luận; details của `T-303`, `T-304`, `T-305`, `T-403`, `T-501`, `T-601`, `T-701`; checklist này.
  - **Xóa file:** Không.
  - **Di chuyển/đổi tên:** Không.
  - **Evidence export:** Import canonical bundle 26 records rồi focused bundle; 7/11 focused records là mới. Export deterministic trả 33 records, 4 unknown revision entries; canonical SHA-256 `5eb3c187417c0b05956a46f1fe4819b2bb8ed89247f7838de8eb8cef944aecc0`. Snapshot SHA-256 `81c4c7a1...`; focused export `b2201daf...`; mapping schema v2 `3323117e...` với 672 methods/6 type contracts.
  - **Readiness ghi nhận:** Static method identity và build-identity implementation là code-complete; RVA vẫn evidence-only/`unavailable`; owner/Promise/postcondition là `UNVERIFIED_DEVICE`; không capability nào được claim `LIVE_READY`.
  - **Plan cũ:** Không tick thêm task. `T-303`/`T-304` được nâng mô tả từ arity/RVA sang exact managed signature/type contracts nhưng vẫn thiếu runtime AC. `T-305` phản ánh runtime observation thật. Downstream `T-403`/`T-501`/`T-601`/`T-701` vẫn mở với blocker thu hẹp đúng evidence.
  - **Kiểm chứng:** Link/path/Evidence ID được đối chiếu với bundle/mapping; không có dependency vòng mới. `git diff --check` sẽ được chạy ở bước handoff cuối.
  - **Blocker:** Dependency `VBR-501` chưa đạt vì hai protocol test path bắt buộc không tồn tại và NDK 27.1 không có. Vì vậy giữ `VBR-502` `[ ]` dù phần docs/evidence đã xử lý; không claim full `AC-VBR-09`.

  </details>
