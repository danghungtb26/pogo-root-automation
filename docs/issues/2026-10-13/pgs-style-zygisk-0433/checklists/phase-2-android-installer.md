# Phase 2 — Installer APK và lifecycle

Plan: [Tổng quan](00-overview.md)

## Điều kiện bắt đầu

- `T-104` đạt và APK đã chứa ZIP + manifest được xác minh.
- Contract module id, hash và output path không còn thay đổi tùy máy.

## Task

- [ ] **T-201** **[app]** *(android)* — Cứng hóa `app/src/main/java/dev/pogoroot/automation/root/RootShell.kt`: API chỉ chạy command/script cố định, truyền dữ liệu qua stdin hoặc argv đã kiểm soát, drain stdout/stderr đồng thời, timeout và trả exit code/output có giới hạn. Không ghép path/config người dùng vào shell string. **AC:** Installer có primitive root an toàn, không deadlock output và phân biệt timeout/non-zero/no-root. **Phụ thuộc:** `T-104`. **Verify:** Chạy existing Gradle tests/checks; review mọi call site `RootShell` và `rg` bảo đảm không có interpolation không kiểm soát.

  <details>
  <summary>Chưa thực hiện — T-201</summary>

  Chưa có thay đổi hoặc kết quả kiểm chứng. Agent cập nhật khối này ngay sau khi xử lý task, trước khi sang task tiếp theo.

  </details>

- [ ] **T-202** **[app]** *(android)* — Thêm `BundledModuleInstaller.kt` và `ModuleInstallStateRepository.kt` dưới package `root`: copy asset vào app-private temp, kiểm SHA-256 và ZIP entries/module id trước `su`, kiểm tra Magisk/Zygisk, gọi Magisk install command tương thích, dọn temp và persist state `NOT_CHECKED`, `ROOT_REQUIRED`, `INSTALLING`, `INSTALLED`, `REBOOT_REQUIRED`, `ACTIVE`, `FAILED`. Cài lại cùng hash phải idempotent; module khác hash phải đi qua reinstall có chủ đích. **AC:** Không có ZIP chưa validate được đưa vào root shell; state sống qua process restart và lỗi có mã/nguyên nhân rõ. **Phụ thuộc:** `T-201`. **Verify:** Existing app checks/build; static review install command/path; dùng fixture chính là bundled ZIP đã build để kiểm hash/entries trong debug run, không thêm test source.

  <details>
  <summary>Chưa thực hiện — T-202</summary>

  Chưa có thay đổi hoặc kết quả kiểm chứng. Agent cập nhật khối này ngay sau khi xử lý task, trước khi sang task tiếp theo.

  </details>

- [ ] **T-203** **[app]** *(android)* — Thêm `ModuleInstallCoordinator.kt` để serialize preflight/install, chỉ xin `su` khi Activity đang foreground, tối đa một attempt tự động cho mỗi foreground entry và chỉ retry sau thao tác rõ của người dùng. Expose immutable state cho UI; cancel không để lại `INSTALLING` giả. **AC:** Không có root prompt từ service/background, không có hai install đồng thời và denial không tạo prompt loop. **Phụ thuộc:** `T-202`. **Verify:** Review lifecycle/coroutine ownership, rotate/recreate Activity trong debug build và xác nhận chỉ một coordinator operation; existing Gradle checks đạt.

  <details>
  <summary>Chưa thực hiện — T-203</summary>

  Chưa có thay đổi hoặc kết quả kiểm chứng. Agent cập nhật khối này ngay sau khi xử lý task, trước khi sang task tiếp theo.

  </details>

- [ ] **T-204** **[app]** *(android)* — Wire coordinator vào `AutomationApplication.kt`, `MainActivity.kt` và UI screen hiện có: tự chạy module preflight/install khi người dùng mở app lần đầu, hiển thị module version/hash, trạng thái, lỗi và nút retry. Preflight/install không được arm Auto Catch. Không tự reboot và không mô tả Package Installer là có thể tự launch app. **AC:** First foreground launch được code để dẫn tới tối đa một root request khi cần nhưng Auto Catch vẫn OFF; từ chối root vẫn có UI/retry rõ. **Phụ thuộc:** `T-203`. **Verify:** `./gradlew :app:assembleDebug`; review lifecycle/call graph chứng minh chỉ foreground Activity gọi install coordinator và không có call site arm/reboot tự động.

  <details>
  <summary>Chưa thực hiện — T-204</summary>

  Chưa có thay đổi hoặc kết quả kiểm chứng. Agent cập nhật khối này ngay sau khi xử lý task, trước khi sang task tiếp theo.

  </details>

- [ ] **T-205** **[app]** *(android)* — Gate `HeadlessAutomationService.kt`, `AutomationBootReceiver.kt` và runtime connection theo state `ACTIVE`; phân biệt “ZIP đã cài”, “cần reboot”, “Zygisk module đã nạp vào target”. Thêm reboot action có dialog xác nhận và command root cố định; không reboot tự động. Runtime status phải hiện `module_inactive`/`reboot_required` thay vì khởi động gameplay mù. **AC:** Service code không phát lệnh gameplay trước `ACTIVE`; reboot chỉ qua callback xác nhận user; boot path idempotent. **Phụ thuộc:** `T-202`, `T-204`. **Verify:** Existing Gradle checks; review state transition/call sites và tìm kiếm command reboot để xác nhận không có automatic path. Trạng thái thực tế trước/sau reboot nằm trong handoff user.

  <details>
  <summary>Chưa thực hiện — T-205</summary>

  Chưa có thay đổi hoặc kết quả kiểm chứng. Agent cập nhật khối này ngay sau khi xử lý task, trước khi sang task tiếp theo.

  </details>

- [ ] **T-207** **[app]** *(android)* — Tách policy bền vững khỏi quyền arm phiên trong `AutomationConfig.kt`, `CatchSpinArmState.kt` và `RuntimeDesiredStateMapper.kt`: giữ `autoCatch`/filter/strategy trong preferences nhưng bỏ `catch_spin_armed` khỏi persisted authority; arm mặc định `false`, gắn với exact `runtimeSessionId`, không restore sau controller process restart. Thêm migration xóa key cũ để bản nâng cấp không tự bật. API đề xuất — chưa tồn tại: `CatchSpinArmState.arm(sessionId)`, `disarm(reason)`, `snapshot(sessionId)`. **AC:** `AC-15`; config cũ có `catch_spin_armed=true` sau upgrade vẫn bắt đầu OFF và desired state chỉ gửi armed khi session khớp. **Phụ thuộc:** `T-204`. **Verify:** Existing app checks + review preferences/migration/mapper; cold app start với prefs cũ cho `catchSpinArmed=false` nhưng giữ nguyên filter/feature config.

  <details>
  <summary>Chưa thực hiện — T-207</summary>

  Chưa có thay đổi hoặc kết quả kiểm chứng. Agent cập nhật khối này ngay sau khi xử lý task, trước khi sang task tiếp theo.

  </details>

- [ ] **T-208** **[app]** *(android)* — Wire manual arm/disarm vào `MainActivity.kt`, `JoystickOverlayService.kt`, `HeadlessAutomationService.kt`, `RuntimeUiAutomationFacade.kt` và `RuntimeUiEventRouter.kt`: nút/toggle user là nguồn duy nhất gọi `arm(currentRuntimeSessionId)`; service start, boot receiver, app foreground và Pokémon GO attach chỉ kết nối/read state. Khi PID chết, bridge disconnect, runtime session đổi, STOP/error hoặc service shutdown thì gọi `disarm(reason)`, đẩy disabled desired state nếu còn kênh, hủy pending UI và yêu cầu user bật lại ở session mới. **AC:** `AC-15`; code path mở Pokémon GO không tự catch, process/session termination luôn chuyển OFF và không replay arm cũ. **Phụ thuộc:** `T-207`. **Verify:** Existing app checks; static/call-graph walkthrough cho attach-OFF → explicit user arm-ON → disconnect/session-change-OFF → new-session-OFF; `rg` xác nhận không có non-UI caller của `arm`. Live sequence chuyển sang handoff user.

  <details>
  <summary>Chưa thực hiện — T-208</summary>

  Chưa có thay đổi hoặc kết quả kiểm chứng. Agent cập nhật khối này ngay sau khi xử lý task, trước khi sang task tiếp theo.

  </details>

- [ ] **T-206** **[build]** *(reviewer)* — Verify code/build phase installer/lifecycle: `./gradlew test assembleDebug`, inspect APK asset/hash, audit install/preflight/denial/retry/reboot/manual-arm transition table, `git diff --check` và line-count. Không tạo/sửa test source và không chạy APK/BlueStacks. **AC:** `AC-02`, `AC-03`, `AC-12`, `AC-15` đạt ở mức code-complete/build-complete; runtime status được ghi `UNVERIFIED_DEVICE`. **Phụ thuộc:** `T-201`–`T-205`, `T-207`, `T-208`. **Verify:** Lưu command/exit code/artifact hash và call-graph/state-table review; tạo mục handoff expected states nhưng không thực thi device script.

  <details>
  <summary>Chưa thực hiện — T-206</summary>

  Chưa có thay đổi hoặc kết quả kiểm chứng. Agent cập nhật khối này ngay sau khi xử lý task, trước khi sang task tiếp theo.

  </details>

## Điều kiện hoàn tất

- First-open flow tự preflight/install khi foreground, không cần network.
- Root denial, Magisk/Zygisk thiếu, reboot required và active là các state khác nhau.
- Gameplay service chỉ chạy sau khi module được chứng minh active.
- Auto Catch chỉ arm bằng thao tác người dùng cho runtime session hiện tại; kill,
  disconnect hoặc session mới làm nó OFF mà không xóa policy/filter đã lưu.
- APK compile/build hoàn tất; install/root/reboot/manual-arm transitions được bàn
  giao để user tự chạy trên BlueStacks, không phải gate hoàn tất phase của agent.

Sau mỗi task, agent phải cập nhật checkbox và khối `<details>` của chính task đó
trước khi chuyển sang task kế tiếp.
