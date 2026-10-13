# Phase 9 — Tích hợp, build và handoff

Plan: [Tổng quan](00-overview.md)

## Điều kiện bắt đầu

- Phase 1–8 đã hoàn tất ở mức code/build hoặc capability chưa đủ offline evidence
  vẫn fail closed với blocker rõ.
- Không yêu cầu BlueStacks/device/test account để hoàn tất phase agent.
- Cleanup đã bỏ deploy/E2E/headless scripts cũ; agent không cài/chạy artifact,
  chỉ chuẩn bị handoff UI thủ công và retained diagnostic commands cho user.

## Task

- [ ] **T-801** **[integration]** *(native/android/reviewer)* — Audit luồng tổng: policy config persist nhưng manual arm không persist trong `AutomationConfig.kt`/`CatchSpinArmState.kt`, mapper/dispatcher replay revision sau reconnect mà không replay arm, session reset không giữ owner/pointer/pending action, strategy không double-dispatch, arbiter serialize catch/spin/transfer/discard, và status/event action id khớp từ native tới UI. Sửa race/lifecycle ở file sở hữu logic, tách file nếu gần 500 dòng. **AC:** `AC-10`, `AC-11`, `AC-13`–`AC-16` đạt trên code path tích hợp; không có double dispatch, stale completion, auto-rearm qua session hoặc callsite tới code/script đã xóa; runtime-dependent claims mang `UNVERIFIED_DEVICE`. **Phụ thuộc:** `T-506`, `T-603`, `T-703`, `T-809`. **Verify:** Existing host-native checks, focused Gradle checks và static end-to-end call-path audit cho reconnect/kill/concurrent intent; `rg` không còn production consumer của HTTP API/script cũ; không chạy device sequence.

  <details>
  <summary>Bị chặn — T-801</summary>

  Manual-arm/config reconnect path và removal audit đạt, nhưng task không thể hoàn tất: action arbiter, typed world state, direct/fallback storage-ID pipeline và terminal transfer/discard paths ở Phase 4–7 chưa thể triển khai khi T-303/T-304/T-307 thiếu REA evidence. Profile 0.433.0 giữ mutation đóng, nên không có double-dispatch live nhưng cũng chưa đạt AC-10/11/13/14.

  </details>

- [x] **T-802** **[build]** *(build/reviewer)* — Chạy canonical release-candidate flow từ clean generated outputs nhưng không reset source: shell/static preflight cho script còn giữ, native/package bằng `ANDROID_NDK=<ndk> ./scripts/build-magisk.sh`, existing host C++ checks, `./gradlew test assembleDebug --rerun-tasks`, rồi static inspect APK/ZIP/manifest/hash. Không gọi script đã xóa và không chạy device. **AC:** Một debug APK tự chứa cùng standalone module ZIP được tạo từ cùng revision, pass mọi host check và khớp flow Phase 8/CI. **Phụ thuộc:** `T-801`. **Verify:** Ghi command order, artifact paths/sizes/SHA-256, APK/ZIP entry lists, exit codes; `git diff --check`, line-count audit và xác nhận không sửa test source.

  <details>
  <summary>Đã thực hiện — T-802</summary>

  Đã chạy canonical order: retained shell syntax; `ANDROID_NDK=.../28.2... ./scripts/build-magisk.sh`; sáu existing host C++ checks; `./gradlew test assembleDebug --rerun-tasks` (52 tasks executed); APK/ZIP/manifest entry+hash inspect. APK cuối `eb4bc5...d42f` (6,931,934 B), ZIP `aa7787...1790` (3,207,708 B), manifest `6beb1c...5ce6` (787 B). `git diff --check`, line audit và no-test-source diff sạch.

  </details>

### T-803 — Chuyển khỏi phạm vi agent

Theo yêu cầu người dùng ngày 2026-10-13, toàn bộ E2E BlueStacks được bỏ khỏi
task agent. Trạng thái trước khi đổi: chưa thực hiện. Kịch bản first-open install,
reboot, attach-OFF, manual arm, full-source scan, direct/fallback, transfer,
discard, kill-disarm và relaunch-OFF được giữ làm nội dung handoff user ở `T-804`.

- [x] **T-804** **[docs]** *(reviewer)* — Cập nhật `README.md`, `docs/ARCHITECTURE.md`, `docs/LIVE_AUTOMATION_READINESS.md` và tài liệu feature/config để phản ánh code/build status, `UNVERIFIED_DEVICE`, self-install, lifecycle/reboot, 0.433 fingerprint, full-source scan, manual arm, direct/fallback, transfer/discard và known limitations. Tạo `docs/issues/2026-10-13/pgs-style-zygisk-0433/bluestacks-user-handoff.md` (đề xuất — chưa tồn tại) chứa artifact path/hash, prerequisite, cài APK bằng UI BlueStacks, first-open `su`/self-install, reboot thủ công, thứ tự test, expected status/postcondition, stop conditions và retained diagnostic/log commands khi thất bại; agent không chạy các bước đó. Không tham chiếu deploy/E2E/headless scripts đã xóa. **AC:** `AC-12`, `AC-16`; tài liệu phân biệt code-complete với live-verified và đủ để user tự test. **Phụ thuộc:** `T-801`, `T-802`. **Verify:** Review path/command theo retained scripts, `rg` không còn hướng dẫn gọi script đã xóa hoặc claim live-ready sai, `git diff --check`; đối chiếu `AC-01`–`AC-16` với code evidence hoặc mục handoff tương ứng.

  <details>
  <summary>Đã thực hiện — T-804</summary>

  Đã cập nhật README/ARCHITECTURE/LIVE readiness và tài liệu control/build, đồng thời tạo `bluestacks-user-handoff.md` với artifact/hash, prerequisites, first-open install/reboot, attach-OFF/manual-arm/session-reset, expected unsupported status, stop conditions và retained diagnostics. Handoff nói rõ gameplay Phase 4–7 bị chặn/không LIVE_READY và không tham chiếu script đã xóa.

  </details>

## Điều kiện hoàn tất

- Build artifacts tự chứa, hash-consistent và pass existing checks.
- Code/build/host checks hoàn tất; APK/ZIP/hash và handoff BlueStacks được bàn giao.
- Capability chưa được user test phải ghi `UNVERIFIED_DEVICE`, không được mô tả
  là live-ready hoặc lấy E2E chưa chạy làm blocker code-complete.
- Manual arm/session và direct/fallback có guards đầy đủ trong code; quan sát live
  thuộc checklist user.
- Tài liệu và task details chứa đủ thay đổi, command và kết quả thực tế để bàn giao.

Sau mỗi task, agent phải cập nhật checkbox và khối `<details>` của chính task đó
trước khi chuyển sang task kế tiếp.
