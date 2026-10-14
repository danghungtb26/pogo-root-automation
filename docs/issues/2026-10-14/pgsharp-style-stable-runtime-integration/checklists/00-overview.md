# Plan: Tích hợp runtime ổn định theo lifecycle

**Ngày:** 2026-10-14. **Trạng thái:** đang triển khai; Phase 0–2 có implementation/host checkpoint, Phase 3 đã có T-012/T-014 implementation, T-013/T-015 còn mở; live verification còn ở T-020.
**Nguồn:** [brainstorm](../brainstorm.md), [phân tích startup trước đó](../../startup-connection-vs-pgsharp/brainstorm.md), [kiến trúc](../../../../ARCHITECTURE.md), [AGENTS.md](../../../../../AGENTS.md).
**Đích:** Pokémon GO 0.433.0, `arm64-v8a` và `x86_64`/translation; kiểm chứng thủ công trên `BlueStacks Air 1`.

## Kết quả mong muốn

Zygisk attach tạo session và broker sớm; loader nạp core riêng có manifest/digest theo ABI. Core phát phase đơn điệu từ loader event và event **sau khi** original `il2cpp_init` return. Chỉ worker sau gate này mới attach IL2CPP, xác minh exact build, chuyển việc cần Unity sang main thread và mở capability có bằng chứng. Controller kết nối trước/sau game đều nhận snapshot session hiện tại và gửi lại full desired config; gameplay không được tự arm ở session mới. Early bootstrap status là handshake session; không dựng `buildFingerprint` giả để thỏa `RUNTIME_READY` hiện tại.

Đây là migration có checkpoint buildable. Không repack game, late-inject PID, thêm transport file, chuyển gameplay state machine sang Kotlin hay suy ra hook bytes/RVA từ PGS. `runtime.status` chỉ là diagnostics. Không tạo/sửa/xóa test theo rule repo; chạy test hiện có. Không đổi `READ_MAP_TARGET` khi chưa đạt calibration riêng của capability đó.

## Evidence và giới hạn hiện tại

- `shasum -a 256 -c reverse/artifacts/SHA256SUMS` đã trả `OK` cho cả 10 artifact khi lập plan. PGS manager APK: `2e0f328a15094ebc0f2f91cfac72fb94024abdce95ac48b229b4be7b3dce7297`; core ARM64 tham chiếu: `893b84c077942b988e3419eeac54d973a35f9dd90a65e79b33bd894f081a6ef2`; PoGo `libil2cpp.so` ARM64: `6b1e11619362ddd53f9dbcd7be1d4296bdec5dda27b9f742c16b8403359c25a0`.
- Retained REA ở `reverse/rea/rea-evidence-2026-10-14-startup-connection.json` chứng minh **static manager**: `MainActivity.onCreate(Bundle)` `ev_d1e8e756d9510c239c7e324d198ae71ca20fc301f6fc9cd97005e622c40cac3a`, `t()` `ev_cf6e34e02f695c61c1c1567cccc7f3e0643c1cbcf059240e4fa8010177d192b`, `F()` `ev_3fef6d9b5fe09752c68fbc45404dbb35bb9615fef2a1f6a461635f7359eadbf7`. Coverage chỉ gồm các method manager; không chứng minh exact loader hook/core ABI hoặc x86_64. Historical PGS report chỉ là hypothesis.
- Source hiện tại: `PogoAutomationModule::preAppSpecialize` phát `kTargetAttached`; `companion_handler` chỉ mở broker sau `kProbeComplete`; `binding_probe_thread` poll mappings; `RuntimeBridgeClient.connect()` đợi `RUNTIME_READY`; `HeadlessAutomationService` có nhánh `stopSelf()` khi runtime inactive. Source này là baseline project, không phải bằng chứng binding game.
- `il2cpp_init` là export lifecycle được **đề xuất để xác minh**; ABI/hook backend, khả năng arm gate trước khi loader trả control, và load core qua read-only FD trên target vẫn chưa được chứng minh. T-002/T-003 chặn đúng phần phụ thuộc. Không biến mapping metadata 0.433.0, report cũ hoặc tên symbol thành entry đã verify.

## AC → task

Các AC dưới đây kế thừa [brainstorm](../brainstorm.md#bước-6--tiêu-chí-chấp-nhận-và-xác-minh). Không thêm AC sản phẩm mới.

| AC | Kết quả phải đạt | Task |
|---|---|---|
| AC-01 | Managed API chỉ sau `IL2CPP_INITIALIZED` | T-002, T-012, T-013, T-020 |
| AC-02 | Phase/sequence đơn điệu, bootstrap exactly-once | T-004, T-007, T-011, T-012, T-020 |
| AC-03 | Broker báo attach ngay từ `TARGET_ATTACHED` | T-006, T-007, T-020 |
| AC-04 | Transport/ACK/library-seen không cho phép mutation | T-004, T-007, T-013, T-018, T-020 |
| AC-05 | `LIVE_READY` cần exact identity, main thread, owner và capability | T-013, T-015, T-020 |
| AC-06 | Process mới có session mới, không reuse volatile state | T-006, T-011, T-018, T-020 |
| AC-07 | Các nhánh lỗi fail closed và có mã | T-003, T-010, T-012, T-014, T-020 |
| AC-08 | Controller reconnect/resubmit snapshot, không replay gameplay | T-016, T-017, T-018, T-020 |
| AC-09 | Một reader/reactor sở hữu runtime stream | T-006, T-020 |
| AC-10 | Polling chỉ còn watchdog diagnostics | T-015, T-019, T-020 |

## Thứ tự phase

1. [Phase 0 — Bằng chứng và contract](phase-0-evidence-contract.md): T-001–T-004.
2. [Phase 1 — Status và broker sớm](phase-1-status-broker.md): T-005–T-008.
3. [Phase 2 — Loader/core riêng](phase-2-loader-core.md): T-009–T-011.
4. [Phase 3 — Lifecycle gates và runtime readiness](phase-3-lifecycle-gates.md): T-012–T-015.
5. [Phase 4 — Controller hội tụ theo session](phase-4-controller.md): T-016–T-018.
6. [Phase 5 — Cutover và kiểm chứng](phase-5-verification.md): T-019–T-022.

Dependency nằm trong từng task. Phần status/broker và controller có thể hoàn thành trước khi hook backend được verify; chúng không được tự mở gameplay capability. Mặc định một agent thực hiện tuần tự; nhãn `owner` là trách nhiệm, không yêu cầu tạo subagent.

## Unknown, checkpoint và rollback

| Unknown / rủi ro | Task quyết định | Phần bị chặn / cách xử lý |
|---|---|---|
| Read-only FD load, linker namespace và ownership FD trên minSdk/BlueStacks | T-003 | T-010; nếu không khả thi, ghi phương án core artifact cục bộ có digest và path/namespace guard trong `bootstrap-contract.md`, không tự tải mạng hoặc đoán thành công |
| Hook backend ARM64, ABI/return/errno của loader và `il2cpp_init`, event miss | T-002 | T-012 và T-015; thiếu postcondition thì `FAILED_LIFECYCLE_GATE`, capability đóng |
| x86_64/translation backend và game artifact đúng ABI | T-002/T-014 | Chỉ mở capability trên ABI đã có evidence **và** device verification; ABI khác báo `unsupported` |
| Hai reader do broker sớm trong khi command loop hiện tại giữ runtime FD | T-004/T-006 | Reactor duy nhất sở hữu read; không `dup(fd)` rồi đọc cạnh tranh |
| Tách single translation unit và package hai `.so`/ABI | T-009/T-011 | Giữ từng checkpoint buildable; không xóa monolith trước T-020 |

Checkpoint sau Phase 1: status/broker sớm nhưng mọi phase trước live vẫn read-only. Checkpoint sau Phase 2: loader/core build được; nếu lifecycle gate chưa chứng minh, core báo lỗi có mã và capability rỗng. Chỉ cắt đường poll-primary và monolith khi T-020 đạt toàn bộ matrix. Rollback dùng source/artifact baseline đã ghi ở T-001, luôn chạy một gameplay owner; không dual-run hoặc fallback tự động che mismatch, không dùng Git destructive.

## Quy tắc thực thi và ghi chú bắt buộc

Sau mỗi task, kiểm tra diff và kết quả Verify, cập nhật ngay checkbox cùng khối `<details>` nằm dưới nó, rồi mới chuyển task. Chỉ đánh dấu `[x]` khi đạt Verify. Trong `<details><summary>Đã hoàn thành — T-ID</summary>...</details>`, ghi file đã tạo/sửa/xóa/di chuyển, function/method/param hoặc hành vi đã thêm/đổi/bỏ, và lệnh kiểm chứng cùng kết quả thực tế. Mục không phát sinh ghi “Không”. Nếu còn lỗi, chưa kiểm chứng hoặc bị chặn, giữ `[ ]`, ghi trạng thái và phần đã làm/điểm còn thiếu. Không dồn ghi chú tới cuối phase, không đánh dấu hoàn tất dựa trên dự kiến.

**Điều chỉnh theo yêu cầu người dùng 2026-10-14:** với T-009–T-018, checkbox có thể đánh dấu khi phần implementation đã làm và host build/check liên quan đạt; live/device Verify chuyển sang T-020. Task còn thiếu source/backend vẫn để mở. Checkbox này không tuyên bố capability live, và Definition of done toàn plan vẫn cần T-020.

Không tạo/sửa/xóa test code. Không sửa file source không phải Markdown vượt 500 dòng; tách file có trách nhiệm rõ và giữ include order của native. Thao tác BlueStacks/thiết bị chỉ qua `scripts/*.sh`; không dùng ADB ad hoc, không đọc log của run thành công. Khi một script thiếu chức năng, ghi gap để xử lý theo scope, không tạo script tạm hoặc bypass. Yêu cầu viết plan không tự cấp quyền cài/reboot thiết bị.

## Definition of done

Các task trong scope đạt Verify và có ghi chú thực tế ngay dưới checkbox; `./gradlew test assembleDebug --rerun-tasks`, focused Kotlin/native checks liên quan, native multi-ABI build/package và `git diff --check` đạt. Startup matrix trên `BlueStacks Air 1` được ghi theo session/phase/sequence/ABI, cùng các nhánh fail closed. Nếu thiếu backend hoặc device verification, giữ task và capability tương ứng chưa hoàn tất; không báo cả plan đã xong.
