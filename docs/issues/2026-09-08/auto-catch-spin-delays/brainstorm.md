# Brainstorm: Thêm delay ổn định trước action tiếp theo cho auto-catch và auto-spin

**Type:** feature
**Date:** 2026-09-08

---

## Analysis

### 1. Tính năng này giải quyết vấn đề gì?

Auto-catch và auto-spin cần một khoảng nghỉ ngắn sau khi action hiện tại kết
thúc trước khi runner gửi mutation kế tiếp. Nếu gửi ngay khi vừa nhận
`COMPLETED`, state trong Pokémon GO có thể vẫn đang chuyển màn hình, cập nhật
result hoặc phát observation trung gian; action sau đó dễ bị reject, chạy trên
state cũ hoặc bị gửi dồn.

Yêu cầu có hai ý cần tách:

- **Delay giữa hai action:** sau outcome chắc chắn, chờ game settle rồi mới
  cho phép mutation tiếp theo.
- **Timeout của command:** thời gian tối đa chờ runtime accept/complete một
  command. Nếu người dùng nói “catch timeout dài hơn spin”, đây là timeout
  thứ hai, không phải delay giữa action.

Trong code hiện tại, `AutomationRunner` đã serialize một mutation, chờ result
và yêu cầu observation mới; tuy nhiên chưa có gate thời gian sau result. Mọi
action vẫn dùng `commandTimeoutNs = 15_000_000_000L` trong runner, còn
`HeadlessAutomationEngine` chỉ ngủ theo `loopIntervalMs`. `loopIntervalMs`
không phải là settle delay đáng tin cậy vì action chỉ được replan khi có
observation mới.

### 2. Ai được hưởng lợi?

Người vận hành structured auto-catch/auto-spin trên thiết bị có runtime binding
đã được verify và allowlist. Delay nên áp dụng cho automation do
`AutomationRunner` điều phối, không phụ thuộc UI overlay và không quay lại
screenshot hoặc `input tap/swipe`.

Runtime binding cũng được hưởng lợi vì command kế tiếp không đến quá sớm khi
client-owned action trước đó mới chỉ vừa phát outcome. Các action khác như
discard/transfer không tự động bị kéo vào scope nếu chưa có yêu cầu tương tự.

### 3. Các use case cốt lõi

**Must-have:**

1. Khi `Spin` có outcome definitive, runner chặn mutation tiếp theo trong
   khoảng spin settle delay; hết khoảng này, observation hợp lệ tiếp theo mới
   được plan action mới.
2. Khi `Catch` có outcome definitive, runner chặn mutation tiếp theo trong
   khoảng catch settle delay; khoảng này dài hơn spin delay.
3. Catch timeout có thể dài hơn spin timeout nếu runtime cần nhiều thời gian
   cho throw/result. Timeout action và settle delay được cấu hình/tính riêng.
4. Trong thời gian settle, runner không queue thêm action từ snapshot cũ và
   không gửi lại cùng `Catch`/`Spin` chỉ vì observation lặp lại.
5. Khi action `INDETERMINATE`, runner tiếp tục behavior fail-closed hiện tại:
   suspend, yêu cầu resync và không tự chạy tiếp chỉ vì hết delay.
6. Khi stop, disconnect hoặc runtime session thay đổi, timer cũ không được
   mang sang session mới.

**Nice-to-have, chưa cần đưa vào vòng đầu:**

- Một status field hiển thị còn bao nhiêu ms settle.
- Tự động đo latency runtime và điều chỉnh delay.
- Delay ngẫu nhiên hoặc anti-detection behavior. Đây không thuộc mục tiêu
  architecture hiện tại và không nên thêm.

### 4. Các edge cases

- **Result đến theo nhiều phase:** `ACCEPTED`/`STARTED` chưa nên bắt đầu settle
  timer. Timer nên bắt đầu từ phase definitive, ưu tiên timestamp lúc
  controller nhận result hoặc lúc runtime xác nhận action hoàn tất.
- **`COMPLETED` nhưng catch outcome là `MISSED`/`BREAKOUT`:** vẫn cần một
  khoảng settle để encounter state cập nhật; không coi `COMPLETED` đồng nghĩa
  với đã bắt được.
- **`REJECTED`/`FAILED`/`SAFE_TIMEOUT`:** action chắc chắn không thành công,
  nên dùng backoff ngắn hoặc action-specific failure delay; không áp dụng mù
  catch success delay nếu action chưa chạy.
- **`INDETERMINATE`:** không đặt timer rồi tiếp tục tự động. Đây là nhánh cần
  resync vì không biết mutation đã chạy ở runtime hay chưa.
- **Catch có berry trước đó:** `UseBerry` là mutation riêng. Nếu không có delay
  sau berry, `Catch` có thể đến ngay khi animation berry chưa kết thúc. Cần
  quyết định thêm một berry settle delay nhỏ hoặc coi đây là dependency của
  catch flow.
- **Nhiều fort/encounter:** mỗi observation có thể chứa nhiều candidate nhưng
  runner chỉ được gửi một mutation. Delay không được biến planner thành queue
  bulk từ snapshot cũ.
- **Observation đến trong lúc delay:** vẫn cập nhật `lastObservationSeq` để
  không bị stale sequence, nhưng không dispatch mutation. Sau khi delay hết,
  cần observation mới hơn để replan.
- **Loop interval lớn:** với `loopIntervalMs = 900 ms`, action có thể chạy trễ
  hơn delay danh nghĩa tối đa gần một chu kỳ poll. Đây là latency polling, không
  nên dùng nó để thay thế settle delay.
- **Clock:** timer phải dùng monotonic `nowElapsedNs`, không dùng wall-clock
  `nowEpochMs`, để clock chỉnh giờ không làm action chạy sớm hoặc trễ bất thường.
- **Runtime restart/binding loss:** reset hoặc bỏ timer cùng với runner session;
  command cũ vẫn phải đi qua logic indeterminate hiện tại.

### 5. Các ràng buộc

**Technical:**

- `AutomationCoordinator` chỉ tạo intent; không nên chứa `Thread.sleep`,
  version-specific timing hay biết runtime đã settle đến đâu.
- `AutomationRunner` là nơi đang giữ state `active`, `terminalActionsForSnapshot`,
  resync và monotonic clock injection, nên phù hợp để đặt mutation gate.
- `AutomationCommand` đã có `expiresAtElapsedNs`; nếu chỉ thay timeout tính ở
  controller thì không nhất thiết phải đổi bridge protocol. Runtime vẫn phải
  tôn trọng deadline và trả phase rõ ràng.
- App config hiện đã xoá các field cũ của screen driver. Nếu cho phép tuning,
  cần thêm field có nghĩa structured, không khôi phục
  `catchThrowDurationMs`, `spinOpenDelayMs` hoặc `spinSwipeDurationMs`.
- Native probe hiện vẫn read-only/không quảng bá mutation capability. Feature
  này chỉ có tác dụng sau khi có runtime binding client-owned phù hợp.

**Backward compatibility:**

- Giá trị mặc định phải giữ behavior an toàn: delay không được làm runner tự
  retry command cũ hoặc bypass capability/identity/freshness checks.
- Nếu config cũ còn các preference delay, migration chỉ nên bỏ qua/xoá chúng
  như hiện tại; không map screen swipe/open delay sang structured action một cách
  tự động.

**Baseline lịch sử:**

- Screen driver trước đây dùng `spinResultDelayMs = 1_000L`.
- Screen driver trước đây dùng `catchResultDelayMs = 3_500L`.
- Đây là baseline tham khảo để bắt đầu đo, không phải acceptance đã được user
  chốt. `catch > spin` là điều kiện quan trọng hơn con số cụ thể.

### 6. Các phương án đã cân nhắc

1. **Gate trong `AutomationRunner` sau definitive result — khuyến nghị.**
   Giữ serialization, resync và safety ở một state machine duy nhất; dễ test
   bằng fake monotonic clock; không block bridge thread.
2. **Chỉ tăng `loopIntervalMs`.** Không đủ: polling interval không đảm bảo
   action trước đã settle và làm toàn bộ observation/action phản hồi chậm hơn.
3. **`Thread.sleep` trong `HeadlessAutomationEngine`.** Không khuyến nghị:
   trộn scheduling với domain state, khó phân biệt action nào cần chờ, và có thể
   làm mất cơ hội xử lý BindingLost/result trong lúc sleep.
4. **Delay trong runtime binding trước khi trả `COMPLETED`.** Có ích nếu
   runtime biết chính xác client state đã ổn định, nhưng không thay thế được
   guard controller và khó dùng làm config chung giữa các build.
5. **Một `actionCooldownMs` dùng chung.** Đơn giản nhưng không đáp ứng catch
   cần dài hơn spin và không phản ánh khác biệt giữa timeout thực thi và settle.
6. **Timer cố định để tự gửi action dù chưa có observation mới.** Không chọn vì
   có thể chạy trên snapshot stale, trái với contract hiện tại “mỗi definitive
   result cần observation mới trước khi replan”.

### 7. Tương tác với các feature hiện có

Flow hiện tại:

```text
ObservationEvent
  -> readSnapshot()
  -> AutomationCoordinator.plan()
  -> AutomationRunner.onObservation()
  -> ActionRequest(expiresAtElapsedNs)
  -> bridge command
  -> AutomationCommandResult
  -> AutomationRunner.onResult()
  -> observation mới
```

Điểm cần bổ sung là một state tương đương `nextMutationAllowedAtElapsedNs`
trong runner:

```text
COMPLETED Catch/Spin
  -> set nextMutationAllowedAtElapsedNs
  -> nhận observation nhưng không dispatch
  -> delay hết + observation mới
  -> replan mutation
```

`StructuredAutomationController` vẫn chỉ chuyển result bridge vào runner. Nó có
thể publish status/error hiện có, nhưng không nên tự giữ thêm một timer thứ hai.
`AutomationConfig`/`AutomationPolicyBridge` chỉ tham gia nếu delay và timeout
được expose cho người dùng; nếu không, có thể giữ timing policy ở core với
default rõ ràng.

Berry cần được xem là một phần của flow catch: nếu `UseBerry` vẫn là action
riêng như hiện tại, nên có settle delay riêng nhỏ hơn catch delay hoặc một
mapping rõ ràng để tránh ném ball ngay sau berry.

### 8. Các dependencies

- `AutomationPolicy` hoặc một `AutomationTimingPolicy` mới trong `core` để
  mô tả `spinSettleDelay`, `catchSettleDelay` và nếu cần
  `spinCommandTimeout`, `catchCommandTimeout`.
- `AutomationRunner` cần timing provider/action classifier và fake clock test.
- `AutomationConfig` + `AutomationPolicyBridge` nếu muốn cấu hình qua
  `POST /v1/config`; cần range clamp và persistence trong
  `AutomationConfigRepository`.
- `BridgeBackedPogoActionExecutor` cần dùng timeout theo action khi nó tự tạo
  `ActionRequest` qua `execute(action)`, nếu entry point này vẫn được giữ.
- Không bắt buộc đổi `BridgeProtocol` nếu chỉ đổi deadline đã truyền trong
  `AutomationCommand`. Chỉ cần đổi protocol nếu muốn runtime gửi semantic
  `readyAt`/settle metadata.
- Test ở `core/src/test/.../AutomationRunnerTest.kt`, config/API tests nếu
  thêm preference, và bridge/adaptor test để đảm bảo `expiresAtElapsedNs` được
  truyền đúng.

### 9. Các rủi ro

- **Nhầm timeout với settle delay:** tăng timeout không tự ngăn command kế tiếp
  chạy quá sớm; thêm delay sau result không giúp command đang chạy nếu timeout
  quá ngắn. Hai giá trị phải có tên, owner và test riêng.
- **Delay bắt đầu sai thời điểm:** bắt đầu từ lúc submit sẽ ăn vào thời gian
  action thực thi; bắt đầu từ `ACCEPTED` có thể vẫn sớm khi game chưa result.
  Với mục tiêu hiện tại, bắt đầu sau definitive result là dễ giải thích và an
  toàn nhất.
- **Delay quá dài:** throughput auto-catch/spin giảm và encounter/nearby có
  thể hết hạn. Cần đo p50/p95 latency và expiry margin trên device trước khi
  chốt giá trị production.
- **Delay quá ngắn:** lỗi race vẫn còn, đặc biệt với catch outcome/breakout và
  berry. Cần device test chứ không chỉ unit test.
- **Replan khi state stale:** nếu gate chỉ nằm ở engine mà không nằm trong
  runner, một caller khác có thể bypass. Runner phải là enforcement point.
- **Timeout catch dài nhưng indeterminate vẫn không retry:** đây là behavior
  đúng để tránh ném trùng; không được biến việc tăng timeout thành lý do bỏ
  fail-closed.

### 10. Các tiêu chí nghiệm thu sơ bộ

Feature chỉ nên được coi là đạt khi có test state machine và đo được trên runtime
binding đã pin; command submit thành công một lần chưa đủ chứng minh delay hoạt
động đúng.

## Acceptance Criteria (from spec)

> Source: không tìm thấy `docs/newspec/**` hoặc `docs/specs/**` cho yêu cầu này;
> các tiêu chí dưới đây là **inferred — needs BA confirm**, dựa trên ticket
> người dùng và contract structured hiện tại.

| ID | Rule / Requirement | Formula / Expected | Acceptance note |
|----|--------------------|--------------------|-----------------|
| AC-1 | Tách hai loại thời gian | `settleDelay(action) != commandTimeout(action)` | Tăng timeout không được thay thế settle delay; tên/config/test phải phân biệt. |
| AC-2 | Spin có delay trước action tiếp theo | Sau definitive `Spin` tại `t0`, không dispatch mutation khi `nowElapsedNs < t0 + spinSettleDelay` | Baseline lịch sử: `spinSettleDelay = 1_000 ms`, cần BA confirm. |
| AC-3 | Catch chờ lâu hơn spin | `catchSettleDelay > spinSettleDelay` | Baseline lịch sử: `catchSettleDelay = 3_500 ms`, cần BA confirm giá trị chính thức. |
| AC-4 | Catch timeout dài hơn nếu yêu cầu timeout là command timeout | `catchCommandTimeout > spinCommandTimeout` | Cần xác nhận user đang nói đến command timeout hay settle delay; timeout hết hạn vẫn chuyển `INDETERMINATE`/resync theo contract. |
| AC-5 | Không queue action trong settle window | Observation lặp lại trong window → `request == null`, không submit command mới | Vẫn cập nhật sequence/freshness; không chạy action từ snapshot stale. |
| AC-6 | Cần observation mới sau delay | Delay hết nhưng không có observation mới → không tự dispatch; observation hợp lệ tiếp theo mới replan | Giữ contract hiện tại của `AutomationRunner`. |
| AC-7 | Fail-closed khi outcome không chắc chắn | `INDETERMINATE`/binding loss/timeout → `suspended == true`, `needsResync == true`, không tự retry Catch/Spin | Không dùng timer để bypass an toàn. |
| AC-8 | Reset theo runtime session | Stop/disconnect/session replacement → timer cũ không ảnh hưởng session mới | Không mang `nextMutationAllowedAtElapsedNs` qua identity khác. |
| AC-9 | Berry không làm catch chạy quá sớm | `UseBerry` → catch chỉ được submit sau outcome/settle phù hợp | Cần chốt berry delay hoặc explicit exclusion trước implementation. |
| AC-10 | Timing dùng monotonic clock | Kết quả không thay đổi khi wall-clock bị chỉnh; test dùng fake `nowElapsedNs` | Không tính settle bằng `nowEpochMs`. |
| AC-11 | Cấu hình có giới hạn nếu expose | Persist/read/write round-trip; giá trị ngoài range bị clamp/reject | Không khôi phục các preference screen-only như swipe duration/open delay. |
| AC-12 | Test được các mốc biên | `delay - 1 ms` không dispatch; `delay` hoặc lớn hơn cho phép replan nếu có observation mới | Bao gồm catch/spin và các phase `COMPLETED`, `REJECTED`, `FAILED`, `SAFE_TIMEOUT`, `INDETERMINATE`. |

- Yêu cầu “spin cần delay trước hành động tiếp theo” → `AC-2`, `AC-5`, `AC-6`.
- Yêu cầu “catch cần delay dài hơn spin” → `AC-3`, cộng `AC-9` cho flow berry.
- Yêu cầu “catch timeout dài hơn spin” nếu đúng ý người dùng → `AC-4`.

---

## Synthesis

### Key Insight

Repo hiện đã serialize action và chờ runtime result, nhưng chưa có **settle
gate**; `loopIntervalMs` và command timeout không giải quyết cùng một vấn đề.
Các delay cũ từng bị xoá là delay của screen driver, nên không nên phục hồi
nguyên xi. Thiết kế đúng là thêm timing theo action vào `AutomationRunner`: spin
chờ ngắn hơn, catch chờ dài hơn, còn timeout thực thi cũng có thể tách theo
action nếu đó là ý người dùng.

### Recommended Approach

Thêm một timing policy structured, tối thiểu có `spinSettleDelay` và
`catchSettleDelay`, với baseline tham khảo `1_000 ms` và `3_500 ms`; để
`AutomationRunner` giữ `nextMutationAllowedAtElapsedNs` bằng monotonic clock.
Chỉ bắt đầu settle sau definitive result, vẫn yêu cầu observation mới sau khi
timer hết, và giữ nguyên nhánh `INDETERMINATE` suspend/resync. Nếu cần timeout
riêng, thêm `spinCommandTimeout`/`catchCommandTimeout` độc lập và truyền deadline
đã tính qua command hiện có; không dùng một `actionCooldownMs` chung.

### Risks to Watch

- Chưa rõ “timeout” trong yêu cầu là settle delay hay command execution timeout;
  cần chốt trước implementation.
- Giá trị `1 s`/`3.5 s` chỉ là baseline từ implementation screen cũ, chưa được
  đo lại trên structured runtime/build thực tế.
- Berry → catch, outcome breakout/missed và observation đến trễ có thể tạo race;
  phải có test fake clock và device test với binding version cụ thể.

### Open Questions

- “Spin 1 timeout” có nghĩa `spinSettleDelay = 1_000 ms`, hay
  `spinCommandTimeout = 1_000 ms`?
- Catch muốn dùng baseline `3_500 ms` của flow cũ hay chỉ cần lớn hơn spin một
  khoảng nhỏ? Giá trị production cụ thể là bao nhiêu?
- Delay chỉ áp dụng sau `COMPLETED`, hay cả `REJECTED`/`FAILED`/`SAFE_TIMEOUT`
  với backoff ngắn?
- `UseBerry` có cần settle delay riêng trước `Catch` không?
- Có cần expose timing qua `/v1/config`/overlay, hay giữ fixed default để tránh
  người dùng cấu hình quá thấp?
- Runtime binding sẽ phát `COMPLETED` sau khi client đã settle, hay controller
  vẫn phải giữ minimum settle delay bảo vệ?
