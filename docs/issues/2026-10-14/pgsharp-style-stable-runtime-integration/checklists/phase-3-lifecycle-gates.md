# Phase 3 — Lifecycle gates và readiness

Plan: [Tổng quan](00-overview.md)

**Điều kiện bắt đầu:** core T-011 build được; T-002 phải có evidence backend tương ứng trước khi viết hook ABI đó.
**Điều kiện hoàn tất:** loader event và post-`il2cpp_init` gate có postcondition, build/main-thread/owner readiness phân tầng, ABI không hỗ trợ fail closed, polling không cấp readiness.

Theo yêu cầu ngày 2026-10-14, tick phần **implementation đã làm** và chuyển live Verify sang T-020. Task còn thiếu source/backend vẫn giữ `[ ]`; notes ghi rõ bằng chứng và giới hạn.

- [x] **T-012** **[zygisk]** *(native)* — Tạo `LibraryLifecycleObserver` interface và ARM64 backend trong `zygisk/runtime-core/hooks/` theo dossier T-002, không chép trampoline PGS. Observer được cài synchronously khi core start, bắt successful return của loader cho `libil2cpp.so`, arm `il2cpp_init` gate trước khi trả control; callback chỉ phát event one-shot/đánh thức worker, không gọi managed API, scan class hoặc IPC dài. Wrapper gọi original đúng ABI, bảo toàn return/errno theo contract và chỉ phát `IL2CPP_INITIALIZED` sau return. Duplicate `dlopen`/init không tạo worker thứ hai; event miss/hook install fail → `FAILED_LIFECYCLE_GATE`. **AC:** AC-01, AC-02, AC-07. **Phụ thuộc:** T-002 (ARM64 evidence đủ), T-011. **Verify:** native build ARM64; device observation theo script cho thứ tự `LOADER_OBSERVER_ACTIVE → IL2CPP_LIBRARY_SEEN → IL2CPP_INITIALIZED`, duplicate event không làm tăng bootstrap count, failed hook không mở capability. Live Verify chuyển T-020 theo yêu cầu người dùng; tick này chỉ đánh dấu source/host build.

  <details>
  <summary>Implementation ARM64 hoàn thành — T-012; live Verify hoãn</summary>

  - **Thay đổi:** `zygisk/runtime-core/hooks/library_lifecycle_observer.inc` thêm observer và ARM64 backend: cài hook `android_dlopen_ext` synchronously ở core start, chấp nhận đúng `libil2cpp.so` sau successful return, kiểm export/prologue PoGo 0.433.0 rồi cài hook `il2cpp_init` trước khi trả control; wrapper gọi original, giữ return/errno, chỉ ghi one-shot success sau `return == 1`. Worker trong `runtime_control.inc` phát `IL2CPP_LIBRARY_SEEN`/`IL2CPP_INITIALIZED` theo thứ tự, có watchdog 300/60 giây và terminal failure khi thiếu event. `bootstrap_coordinator.inc` phát `LOADER_OBSERVER_ACTIVE` khi cài thành công. Helper inline được chuyển từ module encounter vào `shared/runtime/hooks/runtime_inline_hook.inc`, thêm whitelist lệnh relocation, từ chối BTI/unknown prologue và rollback patch nếu restore protection lỗi. Không dùng code trampoline từ PGS.
  - **Bằng chứng:** PoGo export RVA `0x4eec28c`, body 44 byte (`ev_0b61a01c4fd758dcdd3823abde95ad5fd6f623405ccecdd08c6cb29aac7665a9`, `ev_699ebfc87035955321e2e2dbc923420e7b030408492ad2cf83054f7f56a09c89`); NDK `llvm-objdump` trên đúng ELF cho 4 word prologue `a9bf4ffe aa0003f3 d0fe6f61 913c4021`. User xác nhận target đi qua `android_dlopen_ext`; đây là tiền đề được user cung cấp, chưa phải Evidence REA hoặc live capture của project. Bundle libhela hook-path chỉ là tham chiếu độc lập.
  - **Host check:** `./scripts/build-magisk.sh` pass ARM64/x86_64, tạo ZIP; `git diff --check` pass. Không tạo/sửa test.
  - **Live Verify hoãn:** Chưa xác nhận linker prologue/BTI, hook install/namespace và thứ tự callback trên BlueStacks Air 1. Runtime từ chối prologue/owner không hỗ trợ; trước `LIVE_READY` capability vẫn rỗng. T-020 phải kiểm mọi postcondition trước khi coi feature chạy được.

  </details>

- [ ] **T-013** **[zygisk]** *(native)* — Nối worker sau T-012 với `zygisk/jni/shared/runtime/probe/`, `mainthread/`, `control/runtime_control.inc`, `host/` và modules: chỉ sau `IL2CPP_INITIALIZED` mới `il2cpp_thread_attach`, verify exact package/build/profile/descriptors theo PoGo 0.433.0, detach đúng lifetime; main-thread bridge xác nhận Unity/scene/owner sống trước `MAIN_THREAD_READY/BINDINGS_READY`. `LIVE_READY` yêu cầu strong identity và ít nhất capability thực sự verified; desired snapshot chỉ apply ở gate hợp lệ, ACK không phải action outcome. Giữ session/freshness/lease/arm checks của gameplay và capability đóng cho binding chưa verified. **AC:** AC-01, AC-04, AC-05. **Phụ thuộc:** T-004, T-011, T-012. **Verify:** native build; status/capability progression và runtime postcondition trên target, không có managed call/feature mutation trước init/main-thread/owner gate; source review xác nhận native là scheduler duy nhất.

  <details>
  <summary>Đã thêm guard trước managed API; T-013 còn mở</summary>

  `runtime_probe_diagnostic.inc` từ chối survey/`il2cpp_thread_attach` trước `IL2CPP_INITIALIZED` hoặc sau FAILED; `runtime_main_thread_bridge.inc` kiểm token và phase **trước** `domain_get`/`thread_attach`, chỉ cho read task sau init và action task ở `LIVE_READY`; `runtime_feature_modules.inc` chặn bật module trước `LIVE_READY`. Native ARM64/x86_64 build pass. T-012 nay có backend phát đến `IL2CPP_INITIALIZED`; chưa nối progression `BUILD_VERIFIED → MAIN_THREAD_READY → BINDINGS_READY → LIVE_READY`, giữ task mở.

  </details>

- [x] **T-014** **[zygisk/reverse]** *(native)* — Xác minh x86_64 và translation theo đúng artifact/build/ABI trước khi thêm backend trong `zygisk/runtime-core/hooks/`; ghi evidence và live postcondition riêng, không dùng ARM64 bytes/signature làm portable assumption. Nếu không có backend đủ bằng chứng, phát `FAILED_UNSUPPORTED_ABI`/`FAILED_UNSUPPORTED_TRANSLATION` với phase, ABI và capability rỗng; loader/broker vẫn không crash. Không sửa test hoặc dùng raw ADB để ép path. **AC:** AC-07. **Phụ thuộc:** T-002, T-011; backend code chỉ sau khi evidence tương ứng đạt. **Verify:** native package hai ABI; trên target hợp lệ, x86_64/translation hoặc đạt đầy đủ order/postcondition như T-012–T-013, hoặc trả unsupported ổn định không expose capability; ghi rõ nhánh nào thực tế đạt.

  <details>
  <summary>Implementation nhánh unsupported hoàn thành — T-014; live Verify hoãn</summary>

  `zygisk/runtime-core/bootstrap/bootstrap_coordinator.inc` gắn ABI vào bootstrap status; một lần đọc mapping phân loại `houdini`/`ndk_translation` khi đã hiện diện và báo `unsupported_translation`, còn x86_64 không có backend báo `unsupported_abi`. Cả hai là terminal FAILED/capability rỗng; không dùng ARM64 binding trên x86_64. `./scripts/build-magisk.sh` pass hai ABI. Chưa có đúng PoGo x86_64 artifact hoặc BlueStacks Air 1 postcondition; detection lúc core start có thể chưa thấy translation load muộn, nhưng nhánh x86_64 vẫn fail closed. Live Verify chuyển T-020.

  </details>

- [ ] **T-015** **[zygisk]** *(native)* — Đổi `binding_probe_thread`/`runtime_binding_probe.inc` và readiness caller sang event gates: one-shot mapping check chỉ phát hiện library đã present, không suy init; polling còn bounded watchdog `loader_event_missing`/diagnostics, tuyệt đối không chuyển phase sang ready. Event miss không có recovery proof → lỗi và yêu cầu process restart; giữ status/identity/owner freshness. Không xóa đường monolith trước T-020. **AC:** AC-05, AC-07, AC-10. **Phụ thuộc:** T-012, T-013, T-014 (nhánh unsupported của T-014 được chấp nhận). **Verify:** source review không có assignment readiness từ poll/timeout, native build; delayed `libil2cpp` và missed-event runtime observation thể hiện phase/error đúng, capability đóng khi thiếu event.

  <details>
  <summary>Đã giữ poll ở diagnostics; T-015 còn mở</summary>

  `runtime_binding_probe.inc` chỉ đọc mapping một lần cho diagnostic và không gọi managed API hay nâng bootstrap phase; observer T-012 dùng event one-shot và watchdog bounded để báo `loader_event_missing`/`init_event_missing`. Poll không nâng `IL2CPP_INITIALIZED`/`LIVE_READY`; native build pass. T-013 chưa nối identity/main-thread/owner readiness sau event nên T-015 giữ mở; delayed/missed event trên device chuyển T-020.

  </details>
