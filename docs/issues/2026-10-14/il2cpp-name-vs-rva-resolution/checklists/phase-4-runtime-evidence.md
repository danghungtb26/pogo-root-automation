# Phase 4 — Runtime evidence và postcondition handoff

Plan: [Tổng quan](00-overview.md)

Điều kiện bắt đầu: Phase 3 build được; capability vẫn fail closed
khi owner/outcome evidence chưa đạt. Viết plan không tự cấp quyền cài,
reboot, mở game hay mutation trên device.

Điều kiện hoàn tất: runtime có bounded diagnostic để quan sát
descriptor/owner/session và Promise/result/postcondition; user handoff phân tách
read-only với controlled action; evidence nhận lại được reconcile vào
ledger/capability thay vì suy diễn.

Sau mỗi task, cập nhật ngay khối details dưới checkbox với file tạo/
sửa/xóa/di chuyển, thay đổi method/param/hành vi và kiểm chứng thực
tế trước khi chuyển task. Chỉ tick `[x]` khi Verify đạt; chưa đạt
thì giữ `[ ]` và ghi phần còn thiếu.

## Task

- [x] **VBR-401** **[zygisk]** *(native)* — Bổ sung bounded read-only
  diagnostics vào existing probe/status path để ghi exact build verification,
  method resolution reason/token, declaring/concrete owner class, session/
  `owner_generation`, main-thread state và descriptor invalidation sau rebind.
  Không log raw gameplay payload/sensitive identifier không cần thiết, không
  invoke mutation và không tạo filesystem queue. **AC:** `AC-VBR-05`,
  `AC-VBR-06`, `AC-VBR-07`. **Phụ thuộc:** `VBR-304`.
  **Verify:** Native build; static log-rate/size review; startup/rebind paths có
  bounded one-shot/reason-change output và stale descriptor không được reuse.

  <details>
  <summary>Đã hoàn thành — VBR-401</summary>

  - **Tạo file:** Không.
  - **Sửa file:** `zygisk/jni/shared/runtime/probe/runtime_probe_diagnostic.inc` — thêm one-shot binding summary cho generation/rebind, main-thread bridge và năm descriptor/gate; chính checklist này — cập nhật task.
  - **Xóa file:** Không.
  - **Di chuyển/đổi tên:** Không.
  - **Thay đổi chức năng:** Mỗi managed bootstrap/rebind log đúng một summary và năm bounded method records gồm reason/token/count, owner-session, invocation và outcome; concrete ItemBag/PokemonBag owner class dùng existing read-only object logger. Không log gameplay payload/encounter/Pokémon/item identifiers, không invoke mutation và không tạo queue/file transport.
  - **Kiểm chứng:** Static review xác nhận log chỉ chạy sau fresh `RuntimeBinding{}` discovery; previous/new generation và `stale_descriptors_invalidated` cho thấy rebind đã loại pointer cũ. Main-thread state đọc dưới mutex; owner logs bounded hai object; resolver enumeration đã bounded 2048. File 181 dòng, ≤500. `git diff --check` đạt. Magisk build đạt arm64-v8a/x86_64 và package.
  - **Sai khác so với plan:** Device startup/rebind output chưa được thu trong turn này; handoff VBR-403 giữ trạng thái runtime evidence chưa xác minh.

  </details>
- [x] **VBR-402** **[zygisk]** *(native)* — Theo contract `VBR-004`, chuẩn
  hóa Promise/result/postcondition gates trong
  `direct_catch_promise_observer.inc`, `release_promise_observer.inc`,
  `modules/discard/promise_observer.inc` và các reconcile owner. Verify concrete
  Promise/result class, completion/success/failure, request/session correlation,
  GC-handle lifetime và authoritative map/storage/inventory refresh. Timeout,
  disconnect, wrong result class hoặc ambiguous postcondition trả
  `INDETERMINATE`, nhả resource/arbiter và không blind retry. Phần không đủ
  static contract chỉ thêm diagnostic/gate, không bịa field offset.
  **AC:** `AC-VBR-05`, `AC-VBR-07`. **Phụ thuộc:** `VBR-004`, `VBR-304`.
  **Verify:** Native build + existing host state-machine checks nếu phủ; static
  review mọi terminal/timeout/reset path nhả GC handle và không emit completed
  trước authoritative postcondition; không tạo/sửa test source.

  <details>
  <summary>Đã hoàn thành — VBR-402</summary>

  - **Tạo file:** Không.
  - **Sửa file:** `direct_catch_promise_observer.inc`, `release_promise_observer.inc`, `modules/discard/promise_observer.inc` — one-shot concrete Promise diagnostic và fail-closed postcondition; `transfer/promise_observer.inc`/`discard/execute.inc` — reset trả indeterminate, free handles và dirty authoritative source; `transfer/coordinator.inc`/`transfer_candy_bubbles.inc` — giữ downstream side-effect helpers dormant tới khi gate mở; chính checklist này — cập nhật task.
  - **Xóa file:** Không.
  - **Di chuyển/đổi tên:** Không.
  - **Thay đổi chức năng:** Exact result classes/known fields vẫn được validate. Promise concrete class chưa có static contract nên chỉ log một lần ở poll đầu. `SUCCESS` từ TryCapture/ReleasePokemon/RecycleItem không còn emit completed/event/downstream transfer trước authoritative map/storage/inventory proof; trả `INDETERMINATE` với `*_postcondition_unverified`, giữ retry gate đóng. Wrong class/layout, timeout và owner-generation mismatch cũng indeterminate. Known server rejection vẫn có thể kết thúc như failure, không bị hiểu là mutation success.
  - **Kiểm chứng:** Static path review xác nhận direct-catch handle được free ở consume/abandon, transfer/discard finish/reset free GC handle, discard reconcile clear free reconcile handle; reset giữa pending gửi indeterminate cho correlated command và mark world-state dirty. Không thêm guessed Promise offset; runtime field names/class checks chỉ là diagnostic/gate. Sáu existing native tests compile/run pass; Magisk native build đạt `-Wall -Wextra -Werror` cho hai ABI; `git diff --check` đạt.
  - **Sai khác so với plan:** Không thể chứng minh authoritative postcondition chỉ bằng static artifact, nên không promote ba outcome flags và không bật executor. Đây là explicit residual `UNVERIFIED_DEVICE`, không phải implementation success của live mutation.

  </details>

- [x] **VBR-403** **[docs]** *(native/reviewer)* — Tạo
  `docs/issues/2026-10-14/il2cpp-name-vs-rva-resolution/runtime-evidence-handoff.md`
  (file đề xuất — chưa tồn tại) cho **BlueStacks Air 1**. Handoff tách:
  (A) install/reboot/start và read-only identity/owner/rebind observation;
  (B) controlled action từng feature chỉ sau khi A pass và user chấp nhận;
  (C) expected Promise/result/postcondition và stop conditions. Chỉ tham chiếu
  UI flow cùng retained `scripts/collect-binding-diagnostics.sh`/
  `scripts/logcat-full.sh` khi manual flow thất bại; không raw ADB, script mới
  hay đọc log khi flow pass. Sau khi user trả kết quả, update ledger/REA
  unknown/capability status; không promote `LIVE_READY` nếu owner lifetime hoặc
  postcondition còn unknown. **AC:** `AC-VBR-07`, `AC-VBR-08`.
  **Phụ thuộc:** `VBR-401`, `VBR-402`. **Verify:** Reviewer đối chiếu
  handoff với `AGENTS.md` và handoff 2026-10-13; mỗi observation có expected
  status, stop condition và evidence fields cần trả; device-dependent checkbox
  giữ `[ ]` cho tới khi có kết quả thực.

  <details>
  <summary>Đã hoàn thành — VBR-403</summary>

  - **Tạo file:** `docs/issues/2026-10-14/il2cpp-name-vs-rva-resolution/runtime-evidence-handoff.md`.
  - **Sửa file:** Chính checklist này — cập nhật task.
  - **Xóa file:** Không.
  - **Di chuyển/đổi tên:** Không.
  - **Thay đổi chức năng:** Không đổi runtime; handoff tách A read-only identity/owner/rebind, B controlled action cần user chấp nhận riêng từng feature, C Promise/result/authoritative postcondition. Mỗi row có expected, stop condition và evidence fields; device checkbox vẫn `[ ]`.
  - **Kiểm chứng:** Review với AGENTS và handoff 2026-10-13 xác nhận target duy nhất `BlueStacks Air 1`, install/reboot qua UI, không raw ADB, không đọc log khi pass, mutation artifact hiện tại vẫn disabled. Failure chỉ dùng target-guarded `logcat-full.sh`. Review source của `collect-binding-diagnostics.sh` phát hiện script vẫn gọi raw `adb` và không source `scripts/lib/emulator.sh`; handoff ghi rõ không chạy cho tới khi có task sửa riêng, không bypass policy.
  - **Sai khác so với plan:** Không chạy device/mutation trong turn này. Do retained collect script hiện không target-guarded như checklist giả định, nó được ghi thành blocker thay vì được quảng bá là lệnh hợp lệ.

  </details>
