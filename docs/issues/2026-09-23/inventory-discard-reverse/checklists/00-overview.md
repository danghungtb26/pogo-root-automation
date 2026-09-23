# Plan — Inventory và discard theo reverse 0.427.0

Ngày lập: **2026-09-23**. Scope: `native-binding` và vòng đời auto-discard native. Nguồn: [brainstorm mới](../brainstorm.md), [AGENTS.md](../../../../../AGENTS.md), [kiến trúc hiện hành](../../../../ARCHITECTURE.md). Plan nằm trong issue mới theo yêu cầu người dùng; không ghi vào issue `2026-09-12/auto-discard-native`.

Kế hoạch này được lập trước khi triển khai. Trạng thái cập nhật ngày **2026-09-23**: các task reverse/contract, binding/context, main-thread scheduling, lifecycle barrier, outcome handling, build, kiểm thử tĩnh và tài liệu kiến trúc đã được thực hiện theo details; T-018 chưa chạy được vì BlueStacks Air 1 không online, còn T-019 là controlled mutation cần item/amount được ủy quyền. T-020/T-021 vì thế vẫn mở; production discard mutation vẫn bị khóa fail-closed. Nhãn owner là vai trò; không yêu cầu tạo subagent.

## Đính chính bắt buộc trước khi triển khai

**Kết luận “ListSortedPlayerInventory có 6 tham số” trong brainstorm là sai.** Khi viết plan đã đối chiếu lại curated extract trước, rồi đọc `reverse/pogo-0.427.0/dump.cs.gz:486503` và `script.json.gz:417755–417756` theo số dòng nội dung giải nén. Cả hai xác nhận **5 tham số nghiệp vụ**:

```csharp
List<ItemInventoryItemWidget.ItemData> ListSortedPlayerInventory(
    Func<ValueTuple<Item, HoloItemType>, bool> usableFilter,
    bool cannotDeleteItems,
    Func<ValueTuple<Item, HoloItemType>, bool> showUnusableFilter,
    Func<Item, bool> showUnavailableFilter,
    Action<Item> itemExpiredCallback)
```

Chữ ký native thêm `__this` phía trước và `const MethodInfo* method` phía sau; hai thành phần này không tính vào arity của `class_get_method_from_name` hoặc mảng tham số `runtime_invoke`. RVA vẫn là `0x7E7F4A8`. Generic có dấu phẩy bên trong, không được đếm chúng thành tham số method.

Vì vậy resolver arity `5` và mảng `arguments[5]` hiện có **không phải lỗi lệch số tham số**. AC-03 trong plan thay thế AC-03 của brainstorm: xác minh **đúng 5 kiểu, đúng thứ tự, owner/return và nullability**, giữ arity 5. Không thêm phần tử thứ sáu. Những phát biểu “6/5 mismatch” và yêu cầu đổi thành 6 ở các bước 4–6 của brainstorm không áp dụng. `Delegate.CreateDelegate` là method riêng; không thay số arity hàng loạt.

Các phát hiện khác vẫn cần xử lý: expiration context bắt buộc; `yts` có rollback; amount có thể cũ lúc invoke; reset làm mất pending; Promise/reconcile và lifetime chưa đủ bằng chứng runtime.

## Build và kết quả mong muốn

- Mục tiêu theo dự án: package `com.nianticlabs.pokemongo`, version `0.427.0`, version code `2026082702`.
- Binary đã phân tích: AArch64 / arm64-v8a, BuildID `84ed21009899c84abca01e0b8c6f4b0b054f264e`. Version code/APK và binary thực chạy còn phải đối chiếu ở T-018; không suy ra từ tên thư mục.
- Native đọc inventory có baseline đáng tin, tính phần vượt limit ngay trước invoke, dùng model/context thật của game và chỉ chạy một mutation tương quan tại một thời điểm.
- Phân biệt kết quả server với readiness của cache. Game sở hữu prediction/rollback; native giữ barrier tới khi có đủ bằng chứng cho action tiếp theo.
- Config, toggle, disconnect, đổi owner và timeout không được biến request chưa rõ kết quả thành request đã hủy.
- Kotlin tiếp tục persist cấu hình, gửi desired state, render trạng thái; không thêm vòng gameplay hay đọc raw game state.

## Phạm vi và giới hạn

Sửa module `zygisk/jni/modules/discard/`, binding/runtime dùng chung liên quan trực tiếp, main-thread dispatch, module status/diagnostic và tài liệu inventory. Giữ `main.cpp` làm include wiring, một translation unit. Mọi API/file mới trong các phase đều được ghi là đề xuất, chưa tồn tại.

Không triển khai RPC thay thế route 137, không tự sửa cache/prediction, không fabricate ItemData/expiration/set, không thêm Kotlin scheduler. Không mở rộng chính sách item hoặc thiết kế lại inventory capacity. Không đổi wire schema mặc định; nếu contract hiện tại không biểu diễn được kết quả/unknown, phải ghi quyết định và thêm task đồng bộ codec C++/Kotlin trước khi đổi schema.

Theo AGENTS.md: **không tạo/sửa test code**. Chạy test có sẵn; case mới dùng review đường chạy và bằng chứng thiết bị thích hợp. Test mock không chứng minh runtime binding. Không commit APK đầu vào, reverse generated hoặc artifact build.

## Quy ước đường dẫn và bằng chứng

- `DISCARD/` = `zygisk/jni/modules/discard/`.
- `RT/` = `zygisk/jni/shared/runtime/`.
- `APP/` = `app/src/main/java/dev/pogoroot/automation/`.
- `D:L`, `S:L`, `BIN@RVA`: theo quy ước nguồn ở bước 2–3 của [brainstorm](../brainstorm.md). Mọi RVA chỉ áp dụng binary ở trên.
- Kết quả discovery và contract mới ghi ngay trong details của task; bằng chứng dài nằm ở `../implementation-evidence.md` và được link từ các details. Không cần sửa lại brainstorm để thực thi plan; đính chính phía trên là quyết định ưu tiên của plan này.

## Phase và thứ tự

1. [Phase 0 — Bằng chứng và contract](phase-0-evidence-contract.md): T-001–T-003.
2. [Phase 1 — Binding, context và cache](phase-1-binding-context.md): T-004–T-007.
3. [Phase 2 — Scheduling và lifecycle](phase-2-scheduling-lifecycle.md): T-008–T-011.
4. [Phase 3 — Outcome, readiness và trạng thái](phase-3-outcome-status.md): T-012–T-015.
5. [Phase 4 — Kiểm chứng host và thiết bị](phase-4-verification.md): T-016–T-020.
6. [Phase 5 — Bật hỗ trợ và bàn giao](phase-5-rollout-docs.md): T-021–T-022.

Dependency chi tiết ghi tại từng task, không có vòng. Có thể làm task độc lập khi một discovery bị chặn, nhưng không mở gate của phần thiếu bằng chứng. T-004 chỉ tách cấu trúc nên không phụ thuộc kết quả discovery. Read-only discovery thiết bị chỉ sau reverse; gọi list tạo model có side effect local và gọi UpdateInventory có RPC, không gắn nhãn chúng là khảo sát chỉ đọc.

## Mapping tiêu chí chấp nhận

Các AC lấy từ bước 6 brainstorm, riêng AC-03 được đính chính như trên. Cột task gồm implementation và bằng chứng quyết định; số test pass không thay thế postcondition.

| AC | Kết quả cần đạt | Task |
|---|---|---|
| AC-01 | Plan/evidence ở issue mới, giữ nguyên issue cũ và thay đổi người dùng | T-022 |
| AC-02 | Build/ABI/dependency sai thì unavailable, không mutation | T-001, T-005, T-014, T-018, T-021 |
| AC-03 | ListSortedPlayerInventory đúng **5** tham số có type/owner/return guards | T-001, T-005, T-018 |
| AC-04 | Unknown/baseline stale khác count zero hợp lệ | T-002, T-007, T-008, T-018 |
| AC-05 | Snapshot 100, limit 50, current 70 → auto amount 20 | T-003, T-009, T-019 |
| AC-06 | Amount không hợp lệ, missing/protected item bị chặn trước invoke | T-006, T-009, T-016, T-019 |
| AC-07 | Expiration non-null, model/set/owner cùng generation và có lifetime hợp lệ | T-001, T-006, T-018 |
| AC-08 | Một action tương quan; prediction/ACK không là success | T-009, T-011, T-012, T-019 |
| AC-09 | Game rollback ở ytr/yts, framework không rollback lần hai | T-002, T-012, T-020 |
| AC-10 | Lỗi không replay mù; item bị suppress không chặn item khác khi cache đã an toàn | T-003, T-013, T-020 |
| AC-11 | Server success tách reconcile; không write NewCount; no-delta có điều kiện rõ | T-002, T-007, T-012, T-019, T-020 |
| AC-12 | Pending sống qua revision/disable; callback cũ không hoàn tất action mới | T-003, T-010, T-011, T-020 |
| AC-13 | Timeout/GC/layout/dispatch unknown giữ barrier, không auto replay | T-002, T-009, T-010, T-012, T-020 |
| AC-14 | Native gameplay, Kotlin config/UI; IPC hiện có | T-003, T-014, T-015, T-016 |
| AC-15 | Không diễn giải curated sum thành capacity/free slots thật | T-008, T-015 |
| AC-16 | Cold launch/UI bag đóng/relaunch hoạt động có guard hoặc fail closed rõ lý do | T-001, T-006, T-014, T-018, T-019, T-020 |

## Dependency chưa giải quyết và rủi ro

| Khoảng trống | Task đóng / phần bị chặn |
|---|---|
| Null contract ba delegate cuối, filter chvt và wrapper → chwm | T-001 → T-005/T-006; metadata arity đã xác nhận 5 |
| Owner service khi UI đóng, model/set trùng item và GC lifetime | T-001/T-006 + T-018; thiếu thì action unavailable |
| Concrete Promise sau Then/Catch, callback ordering | T-002 → T-012; runtime T-018–T-020 |
| Cache baseline/full/delta/prediction clear, no-delta và request transport còn bất định | T-002 → T-007/T-012; refresh thành công đơn lẻ không chứng minh request cũ không tới server muộn |
| Pending mất observer khi module disabled hoặc socket mất | T-003/T-010 đã thêm maintenance không phụ thuộc module enabled; live reconnect/owner evidence còn ở T-020 |
| Probe 778 dòng, main-thread bridge 582 dòng hiện tại | T-004 chia phần liên quan thành file <=500 dòng; không mở refactor toàn repo |
| SDK/NDK/BlueStacks chưa được kiểm trong lượt lập plan | T-017/T-018; thiếu thì ghi blocker, không workflow thay thế |
| Host test hiện có không phủ expiration/generation/reconcile mới | T-016 ghi coverage thật, T-018–T-020 bổ sung bằng chứng; không tự sửa test |

Không có yêu cầu sản phẩm mới cần hỏi để viết plan. Các khoảng trống là dependency kỹ thuật, không phải yêu cầu người dùng duyệt lại từng bước.

## Quy tắc bắt buộc khi thực thi

> Sau mỗi task, kiểm tra diff và kết quả Verify, cập nhật ngay checkbox cùng khối `<details>` nằm dưới nó, rồi mới chuyển task. Chỉ đánh dấu `[x]` khi đạt Verify. Trong `<details><summary>Đã hoàn thành — T-ID</summary>...</details>`, ghi file đã tạo/sửa/xóa/di chuyển, function/method/param hoặc hành vi đã thêm/đổi/bỏ, và lệnh kiểm chứng cùng kết quả thực tế. Mục không phát sinh ghi “Không”. Nếu còn lỗi, chưa kiểm chứng hoặc bị chặn, giữ `[ ]`, ghi trạng thái và phần đã làm/điểm còn thiếu. Không dồn ghi chú tới cuối phase, không đánh dấu hoàn tất dựa trên dự kiến.

Mỗi details còn ghi sai khác so với plan và lý do; thiếu ghi “Không”. Giữ lịch sử khi mở lại task. Quyền triển khai/kiểm thử thiết bị theo yêu cầu thực thi thực tế; yêu cầu viết plan hiện tại không cấp quyền cài module, restart game hoặc discard thử.

Thiết bị chỉ **BlueStacks Air 1**, xác minh target qua script có sẵn. Không raw ADB, không script tạm/thay thế, không bypass script lỗi. Thành công không đọc log file; thất bại mới thu/đọc log liên quan qua entry point repo. Không reboot nếu chưa nằm trong phạm vi được giao.

## Definition of Done

- Task trong phạm vi đạt Verify và có details thực tế; AC-03 sử dụng bản đính chính 5 tham số.
- Focused checks, full Gradle và build native riêng đạt; file source đã sửa/tạo <=500 dòng, `git diff --check` sạch.
- Bằng chứng build/ABI, owner/thread/lifetime, outcome và lifecycle có thể truy ngược tới artifact/runtime session. Case chưa chạy không ghi PASS.
- Gate production chỉ mở theo T-021; thiếu dependency quan trọng thì giữ unavailable và ghi phần chưa hoàn tất, không gọi toàn bộ triển khai là hoàn thành.
- Tài liệu mới nhất mô tả đúng native ownership, giới hạn inventory snapshot và mức hỗ trợ đã kiểm chứng; không sửa issue cũ hoặc nhận thay đổi người khác làm kết quả task.
