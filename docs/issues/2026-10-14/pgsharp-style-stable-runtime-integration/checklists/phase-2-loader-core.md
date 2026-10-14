# Phase 2 — Loader và runtime core riêng

Plan: [Tổng quan](00-overview.md)

**Điều kiện bắt đầu:** T-004 contract và T-008 checkpoint broker đạt; T-003 phải chứng minh cơ chế nạp core được chọn trước T-010.
**Điều kiện hoàn tất:** loader/core build riêng cho ABI hỗ trợ, core integrity/ABI mismatch báo lỗi; gameplay chưa mở nếu lifecycle gate chưa đạt.

Theo yêu cầu ngày 2026-10-14, checkbox Phase 2 đánh dấu **implementation đã làm và host build đạt**; phần Verify trên device được hoãn sang T-020. Notes dưới mỗi task vẫn ghi rõ bằng chứng đã có và điều chưa kiểm.

- [x] **T-009** **[build/zygisk]** *(native)* — Tách target build trong `zygisk/jni/CMakeLists.txt`, `scripts/build-magisk.sh`, `scripts/package-magisk.sh` để ZIP chứa Zygisk loader và runtime core `.so` cho từng ABI; tạo manifest `{protocol,abi,size,sha256}` cố định từ artifact build cục bộ, xác minh package/entry/export trước khi đóng ZIP. Thêm source dir `zygisk/loader/`, `zygisk/runtime-core/` khi thật sự tách, giữ `main.cpp` single translation unit/include dependency cho phần runtime trong giai đoạn migration; không copy toàn bộ `.inc` sang hai owner. **AC:** AC-07; hỗ trợ AC-01. **Phụ thuộc:** T-001, T-004, T-008. **Verify:** `ANDROID_NDK="$HOME/Library/Android/sdk/ndk/27.1.12297006" ./scripts/build-magisk.sh` pass; ZIP có đúng loader/core `arm64-v8a` và `x86_64`, digest manifest khớp byte package; `bash -n` với shell đã sửa.

  <details>
  <summary>Implementation hoàn thành — T-009; NDK 27 Verify hoãn</summary>

  - **Thay đổi:** `zygisk/jni/CMakeLists.txt` build thêm `pogo_runtime_core`; `zygisk/runtime-core/bootstrap/core_bootstrap_abi.h` khóa `protocol=1`, struct size và export `pogo_runtime_core_start_v1(const CoreBootstrapArgsV1*)`; `core_entry.cpp` chuyển vào `BootstrapCoordinator`, báo terminal failure khi chưa có lifecycle backend. `scripts/build-magisk.sh` chuyển cả hai core `.so` và NDK `llvm-readelf` cho `scripts/package-magisk.sh`. Script package kiểm ELF64/machine/export, đóng gói `runtime-core/<abi>.so`, manifest từng ABI `{protocol,abi,size,sha256}` và `coreArtifacts` trong manifest ngoài; kiểm SHA-256 byte trong ZIP. `app/build.gradle.kts` theo dõi source core để phát hiện ZIP cũ; CI kiểm entry/digest.
  - **Kiểm chứng host:** `bash -n scripts/build-magisk.sh scripts/package-magisk.sh` pass; `./scripts/build-magisk.sh` pass với NDK mặc định 28.2.13676358, tạo bốn `.so` và hai manifest trong ZIP. `jq .coreArtifacts` khớp hash core đóng gói; CI hiện có bước kiểm hash tương đương.
  - **Bổ sung thực tế:** `zygisk/loader/main.cpp` chứa Zygisk registration, broker và companion; `zygisk/jni/main.cpp` build thành core, chứa một owner của gameplay scheduler/binding. `core_entry.cpp` là export có xử lý thực, không còn stub. `app/build.gradle.kts` theo dõi source loader khi kiểm ZIP cũ. `scripts/package-magisk.sh` dùng `llvm-readelf --dyn-syms --wide` để kiểm export không bị truncate. `./scripts/build-magisk.sh` pass cả hai ABI với NDK mặc định 28.2.13676358; ZIP chứa hai loader, hai core và manifest digest khớp byte package theo script. `bash -n scripts/build-magisk.sh scripts/package-magisk.sh` pass; `git diff --check` pass.
  - **Verify hoãn:** NDK 27.1.12297006 trong lệnh Verify không có trên host; NDK 28.2.13676358 build/package thành công. Postcondition nạp core trên BlueStacks thuộc T-020, chưa quan sát.

  </details>

- [x] **T-010** **[zygisk]** *(native)* — Làm mỏng `PogoAutomationModule::preAppSpecialize/postAppSpecialize` ở `zygisk/jni/shared/core/runtime_module.inc` bằng loader source mới: exact process filter và companion handshake, nhận core FD/manifest từ broker theo T-003, verify protocol/ABI/size/hash, nạp core sau specialize bằng cơ chế đã chứng minh, resolve duy nhất export project-owned `pogo_runtime_core_start_v1(const CoreBootstrapArgsV1*)` (**đề xuất — chưa tồn tại**). Contract header phải khóa version/struct size và ownership FD/handle; sai digest, export, ABI hoặc loader return thì báo `FAILED_CORE_INTEGRITY`/`FAILED_CORE_ABI`, capability rỗng. Chỉ unload loader khi callback/hook không còn tham chiếu code loader. **AC:** AC-02, AC-07. **Phụ thuộc:** T-003 (feasibility đạt), T-009. **Verify:** native build và kiểm static ABI/manifest; trên target sau khi được phép cài, status phân biệt core loaded với các lỗi integrity/ABI, không có gameplay trước gate.

  <details>
  <summary>Implementation hoàn thành — T-010; live Verify hoãn</summary>

  - **Thay đổi:** `zygisk/loader/core_handoff.h`, `core_sha256.h` thêm contract `SCM_RIGHTS`, manifest canonical, giới hạn 64 MiB, ELF64/ABI và SHA-256; `runtime_companion.inc` mở core read-only theo ABI, kiểm rồi gửi FD trước khi giao socket cho broker. `loader_module.inc` giữ exact process filter, nhận/exempt FD có timeout, kiểm lại ELF/size/digest, gọi `android_dlopen_ext(..., ANDROID_DLEXT_USE_LIBRARY_FD)`, resolve duy nhất `pogo_runtime_core_start_v1`, chuyển quyền socket khi return 0; lỗi gửi `FAILED` với mã phân biệt missing/manifest/integrity/ABI/load/start. `core_bootstrap_abi.h` khóa layout/version/ownership bằng static assert; loader giữ handle sống vì core callback còn dùng code của core.
  - **Kiểm chứng host:** `./scripts/build-magisk.sh` pass ARM64/x86_64; package script kiểm dynamic export; `llvm-readelf -d` cho core ARM64 chỉ có `liblog/libdl/libm/libc` trong `DT_NEEDED`; `git diff --check` pass. Không tạo/sửa test.
  - **Verify hoãn:** Chưa có live BlueStacks Air 1 để xác nhận namespace/SELinux, FD transfer và status thực. T-003 đã chọn cơ chế có cơ sở từ NDK/Zygisk API; postcondition chuyển sang T-020.

  </details>

- [x] **T-011** **[zygisk]** *(native)* — Tạo `BootstrapCoordinator` trong `zygisk/runtime-core/bootstrap/` (**đề xuất — chưa tồn tại**) làm owner duy nhất của phase, sequence, exactly-once core start, worker cancellation và session-scoped volatile state. Chuyển include/registration trong `zygisk/jni/main.cpp` theo từng phần vào core, giữ một native gameplay scheduler ở `zygisk/jni/modules/`; bridge/companion ownership vẫn riêng process. Khi thiếu lifecycle gate, chỉ báo `FAILED_LIFECYCLE_GATE` hoặc trạng thái chờ, không chuyển probe-poll sang `LIVE_READY`. Process EOF/restart hủy pointer, pending action, config mirror, lease và arm; settings durable vẫn ở app. **AC:** AC-01, AC-02, AC-06, AC-07. **Phụ thuộc:** T-004, T-006, T-010. **Verify:** native build hai ABI và source review không có hai core starts, hai schedulers hoặc IL2CPP managed call trước gate; session mới không mang volatile state cũ.

  <details>
  <summary>Implementation hoàn thành — T-011; live Verify hoãn</summary>

  - **Thay đổi:** `zygisk/jni/main.cpp` chỉ build vào `pogo_runtime_core`; Zygisk registration/broker ở `zygisk/loader/`. `core_entry.cpp` chuyển vào `BootstrapCoordinator` (`bootstrap_coordinator.inc`): kiểm ABI/identity, atomic exactly-once, khởi tạo main-thread helper, phát `CORE_LOADED`, rồi `FAILED` với `lifecycle_gate_unavailable` (ARM64) hoặc `unsupported_abi` (x86_64). EOF dừng worker/module, xóa config mirror, desired snapshot, applied revision, world state và main-thread pending work (`runtime_control.inc`, `runtime_desired_state.inc`); process mới có globals mới. `runtime_native_prelude.inc` đưa hằng số phase chung về đúng nơi; `runtime_bridge_reactor.inc` giữ terminal failure snapshot để controller mở muộn còn thấy lỗi; broker từ chối lệnh khi FAILED.
  - **Kiểm chứng host:** native build cả hai ABI pass, `./gradlew test assembleDebug --rerun-tasks` pass (52 tasks), hai host native tests `runtime_automation_event_protocol_test` và `runtime_readiness_retry_test` pass, `git diff --check` pass. Source review: một core entry, một command worker và không có managed IL2CPP call trong probe diagnostic.
  - **Verify hoãn / dependency Phase 3:** Coordinator chưa có verified lifecycle observer sau `il2cpp_init` (T-012); hiện core chuyển terminal FAILED theo chủ ý nên gameplay đóng. Chưa kiểm session/FD EOF trên device; chuyển T-020.
  - **Cập nhật Phase 3:** T-012 đã thêm ARM64 observer sau checkpoint Phase 2; mô tả `lifecycle_gate_unavailable` ở trên là trạng thái tại lúc tick T-011, không còn là nhánh ARM64 mặc định. Postcondition vẫn chuyển T-020.

  </details>
