# Phase 0 — Bằng chứng và contract

Plan: [Tổng quan](00-overview.md)

**Điều kiện bắt đầu:** brainstorm, `AGENTS.md`, `docs/ARCHITECTURE.md` và artifact index có sẵn.
**Điều kiện hoàn tất:** baseline, hook/FD feasibility và `bootstrap-contract.md` ghi rõ gate, owner, wire, failure, rollback; unknown nào chưa giải được phải giữ task phụ thuộc đóng.

Sau mỗi task, cập nhật ngay khối details dưới checkbox với file tạo/sửa/xóa/di chuyển, thay đổi method/param/hành vi và kiểm chứng thực tế trước khi chuyển task. Chỉ tick `[x]` khi Verify đạt; chưa đạt thì giữ `[ ]` và ghi phần còn thiếu.

- [x] **T-001** **[zygisk/app/build]** *(reviewer)* — Chốt baseline migration: inventory caller/read-owner của `PogoAutomationModule::preAppSpecialize/postAppSpecialize`, `binding_probe_thread`, `companion_handler`, `start_runtime_bridge_broker`, `RuntimeBridgeClient.connect`, `RuntimeUiAutomationFacade`, service `stopSelf()` và các điểm package trong `zygisk/jni/main.cpp`, `zygisk/jni/CMakeLists.txt`, `scripts/build-magisk.sh`, `scripts/package-magisk.sh`; ghi build artifact/ABI, wire version, include dependency, recovery boundary vào `../bootstrap-contract.md` (dự kiến tạo). **AC:** AC-03, AC-09; hỗ trợ AC-06. **Phụ thuộc:** Không. **Verify:** `rg` caller/callee và đối chiếu CI/source hiện tại; tài liệu nêu đúng một reader hiện có và checkpoint trước khi xóa code cũ.

  <details>
  <summary>Đã hoàn thành — T-001</summary>

  - **Tạo file:** `docs/issues/2026-10-14/pgsharp-style-stable-runtime-integration/bootstrap-contract.md` — baseline owner/wire/build và rollback boundary.
  - **Sửa file:** `checklists/phase-0-evidence-contract.md` — cập nhật trạng thái/ghi chú T-001.
  - **Xóa file:** Không.
  - **Di chuyển/đổi tên:** Không.
  - **Thay đổi chức năng:** Không; inventory xác nhận `companion_handler` đọc đến `kProbeComplete`, sau đó broker mới nhận `dup(fd)`, và broker chỉ drain runtime stream khi có controller.
  - **Kiểm chứng:** `rg` caller/callee trong `zygisk/jni` và `app/src/main`, đối chiếu `zygisk/jni/CMakeLists.txt`, `scripts/build-magisk.sh`, `scripts/package-magisk.sh`, `.github/workflows/ci.yml` → path, version, ABI và checkpoint khớp source; chưa chạy build vì task chỉ inventory.
  - **Sai khác so với plan:** Không.

  </details>

- [x] **T-002** **[reverse/zygisk]** *(native)* — Từ `reverse/README.md` và bundle retained, xác minh artifact SHA-256 và query Evidence trước khi phân tích thêm. Ghi vào `../bootstrap-contract.md` authority/coverage/limitation cho PGS ARM64 (`libhela` SHA `893b84c077942b988e3419eeac54d973a35f9dd90a65e79b33bd894f081a6ef2`) và PoGo 0.433.0 ARM64 `libil2cpp.so` SHA `6b1e11619362ddd53f9dbcd7be1d4296bdec5dda27b9f742c16b8403359c25a0`; điều tra ABI/call path của loader callback, `il2cpp_init` export và khả năng arm gate trước return trên **project backend**. Tách x86_64/translation thành ma trận evidence riêng, không suy từ ARM64. Export deterministic REA bundle nếu có evidence mới. **AC:** AC-01, AC-07. **Phụ thuộc:** T-001. **Verify:** dossier có Evidence ID hoặc residual unknown rõ cho từng ABI, signature/return/errno và postcondition cần live check; ABI thiếu bằng chứng được đánh `unsupported`, không có task code nào dùng RVA/hook bytes đoán.

  <details>
  <summary>Đã hoàn thành — T-002</summary>

  - **Tạo file:** `reverse/rea/rea-evidence-2026-10-14-libhela-loader.json` — bundle deterministic gồm 5 record REA CLI của `libhela` ARM64; `reverse/rea/rea-evidence-2026-10-14-pogo-il2cpp-init.json` — 2 record về export PoGo ARM64.
  - **Sửa file:** `bootstrap-contract.md` — Evidence ID, SHA-256, authority/coverage, matrix ABI và residual unknown; `reverse/README.md` — index bundle mới; `checklists/phase-0-evidence-contract.md` — trạng thái T-002.
  - **Xóa file:** Không.
  - **Di chuyển/đổi tên:** Không.
  - **Thay đổi chức năng:** Không; ARM64 init/hook và x86_64 vẫn chưa đủ evidence để mở capability.
  - **Kiểm chứng:** `shasum -a 256 -c reverse/artifacts/SHA256SUMS` → 10/10 OK. Bundle retained ban đầu 48 record. REA CLI trên existing `libhela.gpr` read-only tìm exact `__cxa_guard_enter` entry `0x29b640` (`ev_e919c291a7d8d121eff0dcb79ed858cdc724f0282e7d79afe3ba7794bb8c739d`), RVA suy ra `0x19b640`. Lượt PoGo `pogo-0.433.0.gpr` tìm `il2cpp_init` Ghidra `0x4fec28c` (`ev_0b61a01c4fd758dcdd3823abde95ad5fd6f623405ccecdd08c6cb29aac7665a9`) và dossier 44-byte non-thunk (`ev_699ebfc87035955321e2e2dbc923420e7b030408492ad2cf83054f7f56a09c89`); ELF symbol value/RVA `0x4eec28c` khớp chênh image base `0x100000`. `rea evidence-export` tạo thêm bundle 2 record, SHA-256 `789184a5a1446e5520a1021971d72254894c460798bc4d602ac7348275628c5e`. ABI thực/errno, loader arm gate và live lifecycle postcondition vẫn unknown; T-012 bị chặn.
  - **Sai khác so với plan:** User yêu cầu không tự gọi MCP REA; từ sau correction chỉ dùng retained bundle/quy trình repo, không gọi MCP REA nữa.

  </details>

- [x] **T-003** **[zygisk/build]** *(native)* — Kiểm SDK/minSdk, Zygisk companion FD API, linker namespace và target ABI theo tài liệu/API platform và các script repo; chọn cơ chế gửi read-only core FD + manifest `{protocol, abi, size, sha256}` hoặc core artifact cục bộ có digest/path guard khi FD load không khả thi. Ghi ownership/close timing, atomicity và điều kiện kiểm live vào `../bootstrap-contract.md`; không tạo workflow/script tạm hoặc thực hiện cài/reboot chỉ từ plan này. **AC:** AC-07; hỗ trợ AC-03. **Phụ thuộc:** T-001. **Verify:** nguồn API/platform đủ để chọn một implementation khả thi; giới hạn `UNVERIFIED_DEVICE` và postcondition phải kiểm tại T-010/T-020 được ghi rõ. Nếu chưa chọn được cơ chế có cơ sở, giữ T-003/T-010 mở; không coi desktop build là chứng minh load thành công.

  <details>
  <summary>Đã hoàn thành — T-003</summary>

  - **Tạo file:** Không.
  - **Sửa file:** `bootstrap-contract.md` — chọn `SCM_RIGHTS` + `android_dlopen_ext` FD path và ghi lifetime/namespace/live gate; `checklists/phase-0-evidence-contract.md` — trạng thái T-003.
  - **Xóa file:** Không.
  - **Di chuyển/đổi tên:** Không.
  - **Thay đổi chức năng:** Không; đây là quyết định implementation có điều kiện, chưa chứng minh load trên device.
  - **Kiểm chứng:** `app/build.gradle.kts` minSdk 28; `scripts/build-magisk.sh` Android platform 28; Zygisk vendored API có `connectCompanion()`/`exemptFd()` và companion ABI-aware; NDK 28.2 `android/dlext.h` có `ANDROID_DLEXT_USE_LIBRARY_FD`/`android_dlopen_ext` và namespace flag internal. `BlueStacks Air 1` vẫn `UNVERIFIED_DEVICE`, postcondition chuyển T-010/T-020.
  - **Sai khác so với plan:** NDK 27.1 override ghi trong plan không được cài trên host; NDK 28.2 mặc định của script có sẵn.

  </details>

- [x] **T-004** **[bridge/protocol/zygisk]** *(coder)* — Hoàn thiện `../bootstrap-contract.md`: phase `CREATED → CORE_LOADED → LOADER_OBSERVER_ACTIVE → IL2CPP_LIBRARY_SEEN → IL2CPP_INITIALIZED → BUILD_VERIFIED → MAIN_THREAD_READY → BINDINGS_READY → LIVE_READY` hoặc `FAILED_<reason>`; broker sở hữu attach/session envelope rồi handoff transition cho core, không có hai writer phase/sequence cùng lúc. Chốt identity/session, capability mask, failure codes, wire version/size và `RUNTIME_READY` compatibility. `RUNTIME_BOOTSTRAP_STATUS` là **đề xuất — chưa tồn tại** với `sessionId,pid,processName,sequence,phase,gameBuild,abi,translationLayer,recoverable,errorCode,message,strongIdentityVerified,capabilities`; frame đầu tiên là handshake session để `RuntimeBridgeClient.connect()` không phải đợi build fingerprint. `gameBuild`/fingerprint chưa biết phải biểu diễn bằng nullable/absent versioned field; không dựng giá trị giả. `RUNTIME_READY` cũ chỉ dùng sau khi có identity hợp lệ, không cấp mutation. Nêu ownership read/write của reactor và correlation, config revision/lease/arm reset khi process đổi. Ghi unknown T-002/T-003 làm gate riêng, không chặn wire/broker độc lập. **AC:** AC-02, AC-04, AC-05, AC-06, AC-09, AC-10. **Phụ thuộc:** T-001. **Verify:** contract có bảng field/version/validation/phase guard và API connect/submit thay thế `RuntimeReady` khi build chưa biết; phân biệt ACK với outcome, review đối chiếu `RuntimeUiStatusPayloadCodec.kt`, `runtime_ui_status_protocol.h`, `runtime_bridge_broker.inc` và không có hai owner gameplay.

  <details>
  <summary>Đã hoàn thành — T-004</summary>

  - **Tạo file:** Không.
  - **Sửa file:** `bootstrap-contract.md` — bridge version/field limits/session handshake/phase ownership/guard; `checklists/phase-0-evidence-contract.md` — trạng thái T-004.
  - **Xóa file:** Không.
  - **Di chuyển/đổi tên:** Không.
  - **Thay đổi chức năng:** Không; schema/API là contract trước khi đổi Kotlin/C++.
  - **Kiểm chứng:** Đối chiếu `BridgeProtocol.kt` v3, `RuntimeReady.buildFingerprint` nonblank, `RuntimeUiStatusPayloadCodec.kt` schema1, native `runtime_bridge_protocol.inc` placeholder fingerprint và `runtime_bridge_broker.inc` read owner. Contract xác định frame/bootstrap v4, không nhầm transport/ACK với gameplay outcome.
  - **Sai khác so với plan:** Chọn frame bootstrap làm early session handshake thay cho gửi `RUNTIME_READY` với fingerprint giả.

  </details>
