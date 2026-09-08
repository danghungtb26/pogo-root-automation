# Brainstorm: Bỏ qua catch preview và tự đóng sau khi bắt thành công

**Type:** feature
**Date:** 2026-09-08

---

## Analysis

### 1. Tính năng này giải quyết vấn đề gì?

Sau khi ném ball, Pokémon GO có một đoạn animation/result preview khá lâu trước khi màn hình encounter đóng hoặc quay lại overworld. Người vận hành muốn rút ngắn flow auto-catch: chỉ khi server/client xác nhận Pokémon đã bắt thành công thì tự đóng encounter ngay; nếu ball trượt, Pokémon breakout hoặc outcome chưa chắc chắn thì không được đóng nhầm.

Có hai vấn đề khác nhau cần tách:

- **Giảm thời gian polling/command timeout:** chỉ làm controller phản ứng nhanh hơn, không bỏ được animation đang chạy bên trong game.
- **Bỏ qua preview/tự đóng encounter:** cần can thiệp vào state hoặc method của client Pokémon GO. Đây mới là phần đáp ứng yêu cầu.

### 2. Ai được hưởng lợi?

Người vận hành auto-catch trên Pokémon GO chạy trong process đã được root/instrument. Phạm vi hợp lý ban đầu là structured auto-catch của repo; hiện repo không còn screen automation hoặc manual `/v1/actions/catch` trực tiếp.

Nên coi đây là tùy chọn có thể bật/tắt theo build fingerprint. Không nên mặc định bật cho mọi bản game vì binding client sẽ phụ thuộc version.

### 3. Các use case cốt lõi

Must-have:

1. Khi runtime gửi action `Catch` và client xác nhận outcome `CAUGHT`, runtime đóng encounter/result preview bằng client-owned invocation.
2. Khi outcome là `MISSED`, `BREAKOUT`, `FLED`, `NO_BALL` hoặc bị reject, không gọi close-success path; flow được resync để quyết định có ném lại hay kết thúc.
3. Khi command/outcome là `INDETERMINATE`, runner giữ fail-closed, không tự đóng và không tự retry catch.
4. Sau khi close thành công, observation tiếp theo phải phản ánh overworld hoặc lifecycle hợp lệ; không được re-catch encounter cũ do snapshot stale.
5. Có capability/build gate và status/error rõ ràng nếu binding close preview chưa được cài cho client hiện tại.

Nice-to-have, chưa nên đưa vào scope đầu tiên:

- Bỏ riêng từng animation phase trước khi có outcome `CAUGHT` chắc chắn.
- Tối ưu animation thời gian ném ball/hit/shake.
- Hỗ trợ mọi version Pokémon GO mà không cần adapter/version profile.

### 4. Các edge cases

- **Ball trượt / breakout:** không đóng encounter; có thể tiếp tục catch nếu policy còn cho phép.
- **Pokémon chạy mất / encounter kết thúc tự nhiên:** không coi là `CAUGHT`; resync sang overworld và ghi outcome tương ứng.
- **Catch thành công nhưng close invocation lỗi:** không retry mù. Phát error, yêu cầu observation mới; nếu state đã về overworld thì xem close là đã hoàn tất về mặt UX.
- **Outcome đến trễ hoặc out-of-order:** dùng `commandId`, `runtimeSessionId` và `messageSeq`; bỏ qua event stale.
- **Kết quả thành công nhưng storage observation chưa cập nhật:** không chờ storage như điều kiện duy nhất để đóng, vì storage có thể đến trễ; ưu tiên event/result semantic từ client và xác nhận lifecycle.
- **Timeout sau khi ball đã được ném:** không biết command có chạy hay chưa thì phải chuyển `INDETERMINATE`, suspend/resync, không đóng cũng không ném lại tự động.
- **Berry trước Catch:** `UseBerry` vẫn là action riêng; chỉ bắt đầu catch-preview flow sau khi `UseBerry` đã có outcome phù hợp.
- **Nhiều encounter liên tiếp:** không dùng một cờ global `caught=true`; state phải gắn với `encounterId`/`commandId`.
- **Version chưa được allowlist hoặc binding không khớp:** capability close không xuất hiện; app giữ behavior an toàn hiện tại và báo read-only/rejected.
- **Người dùng đóng thủ công trong lúc automation đang chờ:** observation lifecycle mới phải thắng snapshot cũ; runner không được phát command cho encounter đã hết hiệu lực.

### 5. Các ràng buộc

Technical:

- `AutomationAction.Catch` và `AutomationRunner` đã có, nhưng `Catch` hiện mới là intent/command lifecycle tổng quát; chưa có semantic catch outcome đủ chi tiết để quyết định close.
- Native probe hiện công bố không có mutation capability và reject command bằng `binding_not_implemented`; live client-owned binding chưa tồn tại.
- `AutomationCommand` hiện chuyển action qua bridge, nhưng payload action phía native còn opaque. Cần mở rộng contract có version và outcome rõ ràng.
- `PogoEncounterMapper`/`PogoProtoDecoder` mới phục vụ encounter/map observations; chưa có observation/result schema cho `CAUGHT`, `BREAKOUT`, `FLED`.
- Architecture hiện là structured-only/fail-closed. Không được quay lại screenshot, nhận diện pixel, `input tap/back` hoặc fallback shell để đóng UI.
- Binding phải pin theo package, game build fingerprint, ABI và method/class layout. Không được coi mọi version Pokémon GO là tương thích.

Performance/UX:

- Mục tiêu nên đo từ thời điểm runtime xác nhận `CAUGHT` đến khi encounter chuyển về overworld, thay vì đặt một sleep cố định.
- Không được bỏ qua các bước server/client cần thiết trước khi outcome chắc chắn; “đóng sớm” trước confirmation có thể làm mất catch hoặc tạo command state không nhất quán.

### 6. Các phương án đã cân nhắc

1. **Client-owned binding: bắt outcome rồi gọi close method — khuyến nghị.** Runtime version-specific tìm đúng state/method trong client, phát `CAUGHT`, sau đó gọi thao tác đóng preview/encounter. Đây là hướng duy nhất giải quyết đúng yêu cầu và vẫn phù hợp structured architecture.
2. **Observation-driven close:** đợi observation cho thấy encounter đã hoàn tất hoặc lifecycle về overworld rồi mới coi flow đóng xong. Cách này hữu ích để xác nhận hậu điều kiện, nhưng không tự làm preview ngắn hơn nếu chưa có invocation close.
3. **Timer cố định sau khi ném:** đơn giản nhưng không phân biệt caught/missed/breakout và phụ thuộc network/device latency. Không an toàn để đóng hoặc retry dựa trên timer.
4. **UI automation/back tap/screenshot:** có thể làm trên một số màn hình nhưng trái với structured-only boundary, dễ vỡ theo resolution/UI version và không chứng minh được catch thành công. Không chọn.
5. **Chỉ giảm `loopIntervalMs` hoặc `commandTimeoutNs`:** có thể giảm thời gian controller phản ứng sau khi runtime đã phát event, nhưng không bỏ animation preview bên trong game. Không đủ để đáp ứng yêu cầu.

### 7. Tương tác với feature hiện có

Flow hiện tại:

```text
ObservationEvent(ENCOUNTER)
  -> BridgePogoRuntimeSource / PogoProtoDecoder
  -> EncounterSnapshot
  -> AutomationCoordinator / CatchPlanner
  -> AutomationRunner
  -> AutomationCommand(Catch)
  -> native runtime binding
  -> AutomationCommandResult + observation mới
```

Điểm cần thêm nằm sau `Catch` ở runtime side và trong outcome contract:

- `Catch` vẫn là intent do `AutomationCoordinator` tạo.
- Runtime executor cần báo outcome semantic, không chỉ `COMPLETED` chung chung.
- Chỉ với `CAUGHT` mới phát lệnh/transition đóng preview.
- Sau close, `BridgePogoRuntimeSource` phải nhận lifecycle/encounter observation mới và `AutomationRunner` phải resync trước mutation tiếp theo.

Không nên nhét close preview vào `CatchPlanner`; planner quyết định **có bắt hay không**, không biết client UI state và không nên chứa version-specific invocation.

### 8. Các dependencies

- Runtime binding theo đúng Pokémon GO build: class/method/state dùng để nhận biết catch outcome và đóng encounter preview.
- Bridge protocol version mới cho catch outcome/close result, hoặc một schema mở rộng nhưng backward-compatible.
- `GameCapability` mới hoặc capability tách riêng cho close preview, ví dụ `CLOSE_CATCH_PREVIEW`; capability phải do runtime verified công bố.
- Adapter/runtime test fixture cho ít nhất một build thực tế, gồm success, miss/breakout và binding failure.
- Có thể cần mở rộng model `ActionExecution`/`AutomationCommandResult` để mang `CatchOutcome`; không nên dùng `message` tự do làm protocol.
- Không cần API HTTP mới cho auto-catch. Nếu sau này có manual structured catch, nó vẫn phải đi qua `AutomationRunner` với `encounterId` và identity đầy đủ.

### 9. Các rủi ro

- **Sai điểm hook:** nhận event “ball hit” hoặc “animation started” thay vì server-confirmed caught có thể làm đóng nhầm. Giảm thiểu bằng outcome state machine và test miss/breakout.
- **Version drift:** tên method/layout IL2CPP thay đổi theo game update. Giảm thiểu bằng fingerprint allowlist, adapter riêng theo version và fail-closed khi không match.
- **State race:** close preview và observation overworld có thể đến khác thứ tự. Giảm thiểu bằng command/encounter identity, sequence validation và không retry khi outcome indeterminate.
- **Thay đổi hành vi client:** invocation nội bộ có thể có side effect ngoài đóng UI hoặc bị server/client guard từ chối. Chỉ gọi khi đã xác định method contract trên test build và giới hạn capability.
- **Hiểu sai mục tiêu:** nếu người dùng muốn bỏ cả animation ném/shake chứ không chỉ đóng result preview, scope sẽ lớn hơn đáng kể. Cần xác nhận tên phase chính xác trước implementation.

### 10. Các tiêu chí nghiệm thu sơ bộ

Feature chỉ nên được coi là đạt khi có cả unit/protocol test và device test trên build được pin. Không thể nghiệm thu chỉ bằng việc command `Catch` được submit thành công.

## Acceptance Criteria (from spec)

> Source: không tìm thấy `docs/newspec/**` hoặc `docs/specs/**` cho feature này; các tiêu chí dưới đây là **inferred — needs BA confirm**, dựa trên yêu cầu user và contract hiện tại.

| ID | Rule / Requirement | Formula / Expected | Acceptance note |
|----|--------------------|--------------------|-----------------|
| CP-1 | Xác nhận đúng outcome | `Catch` phải phân biệt tối thiểu `CAUGHT`, `MISSED/BREAKOUT`, `FLED`, `NO_BALL`, `INDETERMINATE` | Không dùng một `COMPLETED` chung làm bằng chứng đã bắt thành công. |
| CP-2 | Đóng sau khi bắt thành công | `outcome == CAUGHT` → invoke close-preview/close-encounter đúng `encounterId` | Đáp ứng yêu cầu “quăng ball mà bắt được thì tự đóng luôn”. |
| CP-3 | Không đóng sớm | `outcome != CAUGHT` hoặc chưa có confirmation → không invoke close | Không làm gián đoạn throw/animation cần thiết hoặc đóng nhầm khi breakout. |
| CP-4 | Bỏ qua preview thực sự | Sau `CAUGHT`, thời gian từ confirmation đến lifecycle overworld/encounter closed giảm so với baseline; không chờ timer cố định | Cần đo trên device; chưa chốt ngưỡng ms vì user chưa cung cấp baseline/target. |
| CP-5 | Resync sau close | close thành công → observation mới hợp lệ, encounter cũ không được plan lại | Dùng `runtimeSessionId`, `commandId`, `encounterId`, `messageSeq`. |
| CP-6 | Fail closed khi không chắc chắn | timeout/binding loss/out-of-order/close failure → `INDETERMINATE` hoặc error, không tự retry Catch/close | Bảo vệ không bắt trùng và không thao tác trên state stale. |
| CP-7 | Version/capability gate | build fingerprint không match hoặc thiếu capability → không gửi close command; status nêu rõ lý do | Behavior an toàn trên build chưa hỗ trợ. |
| CP-8 | Không ảnh hưởng catch policy | `CatchPlanner` vẫn quyết định target/reason như trước; berry vẫn là action riêng | Tính năng chỉ thay đổi post-catch handling. |
| CP-9 | Protocol có version | encode/decode outcome và close result có schema/version test; client cũ bị reject rõ ràng | Không truyền outcome qua free-form message. |
| CP-10 | Verification | JVM tests, bridge/native protocol tests, compile và device smoke trên build allowlist pass | Bao phủ caught, miss/breakout, timeout, version mismatch và manual close race. |

- Requirement “bỏ qua đoạn lâu sau khi ném ball” → **CP-2, CP-4**; done khi close được gọi sau confirmation và latency được đo giảm trên device.
- Requirement “nếu bắt được thì tự đóng luôn” → **CP-1, CP-2, CP-5**; done khi chỉ `CAUGHT` trigger close và encounter cũ không bị xử lý lại.
- Safety/backward compatibility → **CP-3, CP-6, CP-7, CP-8, CP-9, CP-10**.

## Synthesis

### Key Insight

Có khả năng làm, nhưng phần khó không nằm ở `CatchPlanner` hay việc giảm delay trong app. Repo đã có đường biểu diễn `Catch`, nhưng chưa có live client binding, catch outcome semantic hoặc method đóng preview; native probe hiện reject toàn bộ command. Vì vậy “tự đóng khi bắt được” chỉ an toàn khi runtime theo đúng build xác nhận `CAUGHT`, sau đó gọi close trong client và chờ observation resync.

### Recommended Approach

Chốt scope đầu tiên là **auto-close post-catch**, không cố bỏ animation trước khi biết kết quả. Thiết kế một state machine/outcome contract cho `Catch`, thêm capability/version gate cho close preview, triển khai adapter client-owned theo một build Pokémon GO cụ thể, rồi test success/miss/breakout/indeterminate trên device. Giữ behavior fail-closed: build chưa hỗ trợ hoặc outcome không chắc chắn thì không close và không retry tự động.

### Risks to Watch

- Hook nhầm intermediate state có thể đóng encounter khi Pokémon chưa được bắt.
- Game update làm thay đổi IL2CPP binding/method layout; phải fail-closed theo fingerprint.
- Timeout hoặc event out-of-order có thể gây duplicate catch nếu không khóa theo `commandId`/`encounterId`.

### Open Questions

- “Catch preview” chính xác là result screen sau khi ball bắt thành công, hay muốn bỏ cả đoạn throw/shake/ball animation?
- Tính năng chỉ áp dụng cho auto-catch structured hiện tại, hay cần cả manual structured catch trong tương lai?
- Build Pokémon GO đầu tiên để làm adapter là version/fingerprint nào trên thiết bị test?
- Có cần một toggle cấu hình như `autoCloseCatchPreview`, mặc định `false`, hay bật cố định khi capability được hỗ trợ?
- Mục tiêu latency sau confirmation là bao nhiêu ms, và baseline hiện tại đo trên device nào?
