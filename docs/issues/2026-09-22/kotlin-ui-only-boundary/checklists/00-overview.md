# Plan refactor: Kotlin UI + fake location, native PoGo runtime

**Ngày:** 2026-09-22. **Trạng thái:** mới lập plan, chưa triển khai.
**Quy mô:** 6 phase (0–5), 23 task (T-001–T-023).
**Nguồn:** [brainstorm](../brainstorm.md) — Tiếp nối 1–2;
[danh mục giữ/chuyển/xóa](../refactor-inventory.md);
[AGENTS.md](../../../../../AGENTS.md);
[kiến trúc](../../../../ARCHITECTURE.md).

## Mục tiêu và quyết định đã chốt

Kotlin giữ UI, settings/persistence, client IPC và Android location controller:
fake location, joystick, teleport, walk-to-coordinate/favorite, speed/arrival,
navigation lease và provider cleanup. Native giữ game bindings, gameplay,
game-state interpretation và trình tự runtime/readiness/config application.

Đích production là:

```text
UI / HTTP facade → persist desired config → client IPC
    → companion auth/routing → native desired-state owner
    → native readiness / apply / module lifecycle / gameplay
    → native status + UI event / navigation
    → Kotlin render hoặc Kotlin location controller → Android location
```

Không đưa các loop gameplay/readiness cũ về Kotlin dưới tên class mới.
Reader/reconnect transport, validation message, UI refresh và movement tick
vẫn được phép. Native game bindings không thay; không cần tra reverse mới
trừ khi task phát sinh thay đổi method/field/RVA (khi đó tách discovery).

Phạm vi source hiện có được xác nhận ngày 2026-09-22. Reverse mặc định của repo
là PoGo 0.427.0 / 2026082702 / arm64-v8a; plan này chưa xác minh game đang chạy.
APK và Zygisk phải build riêng; target device là BlueStacks Air 1 qua scripts.

## Những gì sẽ xóa / giữ

- **Xóa sau cutover:** `HeadlessAutomationEngine`, `AutomationCycle`,
  `HeadlessAutomationStatusReporter`; DTO trong file phải chuyển trước.
- **Bỏ khỏi app:** POGO adapter construction, raw nearby/encounter/inventory
  decoding/cache/lifecycle inference, `RuntimeObservationTick`/`RuntimeStatusRepository`
  nếu hết caller, `toCorePolicy`, `recordEncounter` chưa dùng.
- **Gỡ dependency APK:** `:game-adapter:pogo`, `:game-adapter:api`, PoGo
  protobuf runtime và vendor wildcard kéo thừa; xác minh consumer trước khi gỡ.
- **Dọn core theo symbol:** runner/planner chết; giữ model/enums/constants/location
  còn consumer. Không xóa nguyên module/folder.
- **Giữ UI/location/persistence:** đã chốt trong inventory, không port movement
  sang native và không tạo native persisted config thứ hai.
- **Giữ compatibility có khai báo:** coordinator/3 dispatchers/interface/mapping
  cũ đang được `RuntimeLifecycleCoordinatorTest` gọi. Không có production caller
  sau cutover; không viết/sửa/xóa/skip test để xóa những file này cho bằng được.
  Xóa vật lý nhóm compatibility chưa nằm trong DoD của plan này.
- **Giữ vendor files và modules adapter trong repo:** CI/checksum/compileOnly
  còn dùng; gỡ khỏi APK không đồng nghĩa xóa artifacts hay notices.

Danh sách path và điều kiện xóa từng nhóm: [inventory](../refactor-inventory.md).
Alias `A/`, `B/`, `C/`, `P/`, `N/` trong các phase có mapping ở đó.

## AC → task

Các AC kế thừa yêu cầu/brainstorm. AC-03 cũ bị thay bằng nhóm AC-LOC.
Nhóm AC-RF là tiêu chí triển khai đề xuất từ yêu cầu refactor.

| ID | Tiêu chí | Task chính |
|---|---|---|
| AC-01 | Native sở hữu runtime orchestration; Kotlin chỉ gửi intent | T-003, T-007, T-008, T-011, T-013, T-016, T-020, T-022 |
| AC-02 | App không diễn giải raw game state; hiển thị state từ native | T-003, T-004, T-009, T-012, T-017, T-019, T-020 |
| AC-04 | Giữ auth/identity/version/seq/freshness/capability và fail-closed | T-002, T-005–T-012, T-021, T-022 |
| AC-05 | Config persist một nguồn; desired/received/applied/ready rõ | T-003, T-004, T-006, T-009–T-015, T-020, T-022 |
| AC-06 | Một owner gameplay, không Kotlin mutation executor live | T-008, T-016, T-018, T-020, T-022 |
| AC-07 | Không viết/sửa/xóa/disable test; chỉ dùng kiểm chứng hiện có | T-001, T-015–T-018, T-020, T-021, T-023 |
| AC-LOC-01 | Kotlin giữ fake location/joystick/teleport/walk | T-014, T-022 |
| AC-LOC-02 | Native chọn target từ game; Kotlin nhận lệnh có guards | T-012, T-014, T-021, T-022 |
| AC-LOC-03 | Local arrival không tự cho phép game mutation | T-014, T-022 |
| AC-LOC-04 | Giữ lease, cleanup, stop và một writer location | T-002, T-014, T-021, T-022 |
| AC-RF-01 | Cleanup có caller evidence; gỡ app dependency, ghi compatibility | T-001, T-016–T-020, T-023 |
| AC-RF-02 | Giữ UI/service/HTTP operational contracts và lifecycle đã chốt | T-002, T-009, T-010, T-013, T-022, T-023 |
| AC-RF-03 | Full desired config revision; bootstrap/apply/ACK không lẫn | T-003–T-009, T-015, T-021, T-022 |

## Phase và dependency

1. [Phase 0 — Inventory, invariants, contract](phase-0-inventory-contract.md): T-001–T-003.
2. [Phase 1 — Wire contract Kotlin/C++](phase-1-wire-contract.md): T-004–T-006.
3. [Phase 2 — Native owner](phase-2-native-owner.md): T-007–T-010.
4. [Phase 3 — Kotlin client/UI cutover](phase-3-kotlin-cutover.md): T-011–T-015.
5. [Phase 4 — Cleanup](phase-4-cleanup.md): T-016–T-019.
6. [Phase 5 — Verification/docs](phase-5-verification.md): T-020–T-023.

T-004/T-005 có thể độc lập sau contract. Phase 3 thực thi T-011 → T-012/T-015
→ T-013 → T-014: mapper phải có trước service cutover, location nối lại sau đó.
Đây là dependency logic, không phải chỉ thị spawn subagent. Mặc định một agent
thực hiện, owner `coder/native/android/reviewer` là vai trò theo task.

Không xóa old entry point trước khi native/client replacement compile và đã
qua Verify của task phụ thuộc. Khi cutover chỉ một owner được phép chạy.
Nếu rollout lỗi, dùng artifacts/source baseline đã ghi, khôi phục một đường
owner; không dual-run hoặc fallback tự động để che protocol mismatch.
Không dùng destructive Git/reset hay ghi đè thay đổi người dùng để rollback.

## Những việc chưa chốt và task giải quyết

| Nội dung | Task/output | Phần phụ thuộc |
|---|---|---|
| Disconnect/app-kill/backpressure semantics | T-002 ghi ma trận từ source/runtime evidence hợp lệ | T-003 contract và T-010 broker lifecycle; không chặn inventory độc lập |
| Wire version/markers, field schema, handshake compatibility | T-003 tạo `../bridge-contract.md` | T-004/T-005 và mọi command/status mới |
| Setting nào native hiện hỗ trợ, key nào chỉ persist/deprecated | T-001 inventory + T-003 schema | T-008/T-015; không mở feature mới |
| Candidate cleanup có shared model/caller | T-001 map + T-018 verify | Chỉ chặn candidate cần tách, không xóa folder |
| Thiếu công cụ gửi malformed packet/kill controller | T-002/T-022 ghi hạn chế và phương án trong scope | Không giả pass; không tạo test/harness/script thay thế |
| Test cũ gắn semantics Kotlin cũ | Đã chọn giữ compatibility không production caller | Không chặn cutover; xóa vật lý nhóm đó là phạm vi riêng |

Nếu một quyết định sản phẩm thật sự không suy ra được, agent ghi câu hỏi cùng
phần độc lập vẫn làm được; không biến mọi thiếu chi tiết kỹ thuật thành gate
xin duyệt. Plan không tự cấp quyền deploy/reboot chỉ từ yêu cầu viết tài liệu.

## Rủi ro và cách xử lý

- START hiện reset feature configs: desired snapshot mới phải riêng và không
  bị xóa trước apply; STOP thắng callback/revision cũ.
- Nửa bộ config: validate đầy đủ, pause dispatch khi cần và publish qua một
  revision gate; kiểm ba revision bằng nhau chưa đủ nếu active observer vẫn
  đọc trong lúc update từng field.
- Mất guard khi bỏ adapter: chuyển guards sang thin event boundary trước
  cleanup, không chuyển game-state inference theo.
- UI/API thiếu native status: status contract có unknown/unsupported; không
  suy strong readiness hoặc action success từ receipt.
- Dead code cleanup làm mất DTO dùng chung hoặc làm vỡ tests: xóa theo symbol,
  compatibility được liệt kê, existing tests giữ nguyên.
- Gỡ JAR theo import trực tiếp chưa đủ: rà transitive/local-library runtime;
  giữ vendor artifacts/notice/CI, chỉ giảm dependency app đã chứng minh thừa.
- Không có test mới cho desired protocol: công khai khoảng trống coverage;
  dùng review codec/guards, build hai phía, existing tests và device scenarios.
- Không thêm gameplay scanner, game binding, storage migration hoặc location
  behavior mới để “tiện thể” hoàn thành refactor.

## Quy tắc thực thi và ghi chú bắt buộc

> Sau mỗi task, kiểm tra diff và kết quả Verify, cập nhật ngay checkbox cùng
> khối `<details>` nằm dưới nó, rồi mới chuyển task. Chỉ đánh dấu `[x]` khi
> đạt Verify. Trong `<details><summary>Đã hoàn thành — T-ID</summary>...</details>`,
> ghi file đã tạo/sửa/xóa/di chuyển, function/method/param hoặc hành vi đã
> thêm/đổi/bỏ, và lệnh kiểm chứng cùng kết quả thực tế. Mục không phát sinh
> ghi “Không”. Nếu còn lỗi, chưa kiểm chứng hoặc bị chặn, giữ `[ ]`, ghi
> trạng thái và phần đã làm/điểm còn thiếu. Không dồn ghi chú tới cuối phase,
> không đánh dấu hoàn tất dựa trên dự kiến.

Trong details phải có: Tạo file; Sửa file; Xóa file; Di chuyển/đổi tên;
Thay đổi chức năng; Kiểm chứng; Sai khác so với plan. Nếu kết quả cũ không
còn đúng, mở lại task và bổ sung lịch sử, không xóa bằng chứng cũ.

Không viết test mới hoặc sửa test code theo rule người dùng; rule này ưu tiên
hướng dẫn bổ sung test trong template skill. Plan chọn giữ tests hiện có,
không xóa/skip chúng hoặc tạo harness tạm; vì vậy còn nhóm source compatibility
đã khai báo, thay vì giữ các file đó như một owner production.
Mọi source không phải Markdown ≤500 dòng. Giữ build/version/binding,
freshness và capability guards. Device chỉ qua scripts hiện có; khi thành công
không đọc log, khi thất bại chỉ đọc log liên quan. Không script/ADB thay thế.

## Definition of Done

- 23 task trong phạm vi có ghi chú thực tế; T-022 là device gate có thể giữ
  unchecked khi target Air 1 không tồn tại trên máy thực thi. Khi đó chỉ báo
  refactor/code verification hoàn tất có giới hạn, không báo device/live
  verification hoàn tất.
- Production app không chạy old engine/coordinator/raw POGO decoder và không
  có caller gửi mutation gameplay; module native là owner duy nhất.
- Kotlin location và UI/HTTP/settings còn đúng các AC; storage không bị mất
  hoặc nhân đôi nguồn authoritative.
- Contract native/Kotlin được version hóa, config/state/outcome đúng semantics,
  auth/session/freshness/identity/capability guards được giữ.
- File/dependency cleanup đúng inventory; nhóm compatibility còn giữ cho
  test được công khai. Không khẳng định đã xóa toàn bộ legacy Kotlin source.
- Focused checks, full Gradle, native host checks/multi-ABI build đạt; device
  verification và coverage/negative cases chưa thực hiện phải ghi rõ bằng
  chứng và không được suy diễn thành pass.
- Source limit, docs/references và `git diff --check` đạt; test files không đổi.
