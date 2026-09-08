# Brainstorm: Inject để điều khiển chất lượng ném bóng, curve, AR+ và auto snapshot trong Pokémon GO

**Type:** architecture
**Date:** 2026-09-08

---

## Phạm vi và cách hiểu

Yêu cầu được hiểu là đánh giá khả năng dùng runtime Zygisk/injected binding
trong process Pokémon GO để:

1. cho người dùng chọn mục tiêu `Nice`, `Great` hoặc `Excellent`;
2. chọn ném `Curve` hoặc ném thường;
3. hỗ trợ flow `AR+`;
4. tự động thực hiện hoặc kích hoạt `GO Snapshot`/snapshot trong game.

`AR+` và `auto snapshot` còn mơ hồ về UX chính xác. Phân tích này tách chúng
khỏi throw thường; nếu `snapshot` chỉ có nghĩa là lưu ảnh màn hình thì đó là
một bài toán khác với việc kích hoạt flow camera của Pokémon GO.

Phân tích chỉ đánh giá kiến trúc và tính khả thi trong phạm vi repo. Không
đề xuất bypass anti-cheat, giả mạo kết quả server, né phát hiện, hoặc dùng
screenshot/`input tap`/`input swipe` làm fallback.

---

## Analysis

### 1. Bài toán cần giải quyết

Repo hiện đã có các seam cần thiết cho một action có cấu trúc:

```text
UI/config
  -> core policy/planner
  -> AutomationAction
  -> bridge command có session/build/observation context
  -> version-scoped binding trong process game
  -> client-owned flow
  -> command result + structured observation
```

Tuy nhiên, runtime hiện tại chỉ là probe/bridge. `RuntimeReady` đang không
quảng cáo mutation capability; `AutomationAction.Catch` mới có
`encounterId`, `reason` và ý định đóng preview sau khi bắt thành công, chưa có
throw profile; binding exact build để gọi flow ném bóng chưa tồn tại.

Mục tiêu kiến trúc nên được phát biểu chính xác như sau:

- `target quality`: yêu cầu runtime nhắm tới vùng/kịch bản tạo ra kết quả
  mong muốn, nhưng kết quả cuối cùng phải do game xác nhận;
- `curve`: yêu cầu kiểu throw, không phải một cờ để tự động sửa kết quả;
- `AR+`: một encounter mode có state/camera/permission riêng;
- `snapshot`: một action riêng có lifecycle và outcome riêng.

Nếu mục tiêu là “luôn ép server trả Excellent/Curve mà không cần mô phỏng
flow ném”, đó là result forgery, không phải action binding thông thường; repo
nên từ chối phạm vi đó vì trái với client-owned flow và fail-closed boundary.

### 2. Các ràng buộc

#### Hard constraints

- Pokémon GO là Unity/IL2CPP; layout, method signature, object lifetime và
  state machine thay đổi theo package/version/ABI.
- `zygisk/` hiện chỉ làm target-process detection, IL2CPP probe, companion và
  bridge. `zygisk/README.md` nói rõ chưa invoke gameplay methods.
- `docs/ARCHITECTURE.md`, `docs/RUNTIME_BRIDGE.md` và các contract hiện tại
  yêu cầu binding version-scoped, strong identity, capability check, freshness
  và fail-closed.
- `game-adapter:pogo` chỉ decode structured payload đã được runtime gửi; nó
  không thể tự suy ra throw result từ pixel hoặc gesture.
- Không được đưa logic game-build-specific vào `core` hoặc bridge chung.
- Repo đã loại trừ root hiding, Play Integrity bypass và anti-cheat bypass.

#### Soft constraints

- Có thể mở rộng `AutomationAction`, `GameCapability`, bridge payload và
  adapter POGO nếu contract vẫn backward-compatible.
- Có thể tách throw thường, AR+ và snapshot thành các phase độc lập để giảm
  blast radius.
- Có thể hỗ trợ “profile” ở UI trước, nhưng profile không được làm cho người
  dùng tưởng rằng `Excellent` là guarantee khi runtime chưa chứng minh được.

### 3. Chất lượng kiến trúc cần ưu tiên

1. **Tính đúng và fail-closed:** không gửi action khi sai build, sai encounter,
   stale observation, mất binding hoặc thiếu capability.
2. **Khả năng bảo trì:** toàn bộ binding phụ thuộc game build nằm sau adapter/
   native boundary; không rải offset hay tên class vào `core`.
3. **Khả năng kiểm thử:** throw profile, command validation và outcome mapping
   phải test được bằng fake runtime; device test chỉ dành cho exact binding.
4. **An toàn vận hành:** không coi command `accepted` là đã ném, không retry
   mù khi outcome `INDETERMINATE`, không tạo duplicate throw/snapshot.
5. **Hiệu năng:** đủ nhanh để giữ context của encounter và input window,
   nhưng không đánh đổi correctness bằng hook generic hoặc polling quá dày.

Không nên tối ưu “tỷ lệ Excellent tuyệt đối” bằng cách bỏ qua các ưu tiên trên;
đó sẽ biến một binding dễ bảo trì thành một hook dễ vỡ và khó xác minh.

### 4. Đánh giá khả thi theo từng tính năng

| Tính năng | Khả thi về nguyên tắc | Điều kiện tối thiểu | Kết luận cho repo hiện tại |
|---|---|---|---|
| Chọn `Nice`/`Great`/`Excellent` | Có, dưới dạng target/profile cho throw pipeline | Exact build binding, encounter state, dữ liệu aim/throw hợp lệ và outcome từ game | Chưa làm được; chưa có throw binding hay profile trong contract |
| Curve | Có thể hỗ trợ như một throw mode | Binding phải đi qua cùng flow ném của client và xác nhận curve trong outcome | Khả thi sau throw thường; không nên chỉ set một boolean không được game xác nhận |
| `AR+` | Có thể nhưng rủi ro và chi phí cao hơn nhiều | AR session/camera/pose/anchor/permission và state machine AR+ đã được bind | Nên để phase sau; không suy ra từ encounter thường |
| Auto snapshot | Có thể nếu nghĩa là kích hoạt flow snapshot của game | Action riêng, camera lifecycle, permission, idempotency và kết quả rõ ràng | Có thể tách thành capability độc lập; chưa có contract |
| Ép trực tiếp kết quả `Excellent`/`Curve` | Không nên coi là capability hợp lệ | Đòi hỏi can thiệp vào result/server path, không còn là client-owned input | Loại khỏi phạm vi |

#### Nice/Great/Excellent

Các nhãn này nên được coi là **outcome**. Runtime có thể nhận một
`ThrowProfile`/`ThrowIntent` như “nhắm mục tiêu Excellent”, nhưng không được
cam kết kết quả trước khi game trả event. Các ngưỡng, vòng mục tiêu, timing,
animation và điều kiện encounter là build/game-state dependent; không nên
hard-code một công thức hình học trong `core` rồi giả định mọi phiên bản dùng
cùng luật.

Có hai mức hỗ trợ khác nhau:

- **Hỗ trợ an toàn hơn:** chọn profile và để client-owned throw pipeline xử lý,
  sau đó trả `actualQuality = NICE|GREAT|EXCELLENT|NONE|UNKNOWN`.
- **Ép kết quả:** thay đổi result path hoặc payload sau khi game đã tính. Mức
  này không phù hợp với boundary hiện tại, khó chứng minh an toàn và có nguy
  cơ bị server từ chối hoặc khóa tài khoản.

#### Curve

Curve cần được mô hình hóa như thuộc tính của attempt, không phải outcome
duy nhất. Một attempt có thể có `requestedCurve`, `actualCurve` và
`quality`. Nếu runtime mất state hoặc event không đủ dữ liệu, phải trả
`UNKNOWN/INDETERMINATE`, không suy ra curve từ việc command đã được accept.

#### AR+

AR+ không chỉ là thêm một flag vào `Catch`. Nó thường liên quan đến camera,
AR session, tracking/pose, mặt phẳng hoặc anchor, vị trí Pokémon trong không
gian, permission và việc chuyển mode encounter. Những dependency này làm cho
AR+ nhạy với thiết bị, phiên bản Unity/AR stack và lifecycle foreground.

Nếu `AR+` trong yêu cầu thực ra là một nghĩa khác, cần chốt lại trước khi
thiết kế contract; không nên dùng tên capability chung chung.

#### Auto snapshot

Nếu là kích hoạt `GO Snapshot`/Buddy Snapshot, nên coi là action riêng:

```text
SnapshotRequest
  -> client-owned snapshot flow
  -> SnapshotStarted
  -> SnapshotCompleted | SnapshotFailed | SnapshotIndeterminate
```

Nếu là chụp màn hình Android, đó là capability của controller/OS và không nên
đặt trong POGO runtime. Nếu là lấy ảnh render/camera từ Unity, cần thêm vấn đề
buffer lifetime, quyền camera, định dạng, kích thước payload và nơi lưu ảnh;
không nên gộp vào observation throw.

### 5. Các phương án kiến trúc

#### Phương án A — Binding exact build cho client-owned flow (khuyến nghị)

Mở rộng contract bằng profile và action typed; runtime chỉ gọi flow nội bộ đã
được xác minh cho đúng package/version/ABI, rồi phát outcome có cấu trúc.

Ưu điểm:

- phù hợp với ranh giới hiện có của `GameAdapter` và bridge;
- giữ result authority ở game/server;
- test được policy/serialization độc lập;
- có thể bật từng capability bằng allowlist.

Khó khăn:

- chi phí reverse-engineering/calibration và device verification cao;
- phải revalidate sau mỗi game update;
- cần xử lý thread/GC/object lifetime/Unity main-thread đúng cách;
- không bảo đảm mọi target quality đều đạt được trong mọi encounter.

Phương án này phù hợp cho throw thường trước, rồi mới xem xét curve,
snapshot và cuối cùng AR+.

#### Phương án B — Điều khiển gesture/UI bên ngoài runtime

Dùng accessibility, overlay, input injection hoặc screen coordinate để kéo
ball và chụp snapshot.

Ưu điểm là dễ prototype trên một màn hình cụ thể. Nhưng nó phụ thuộc độ phân
giải, orientation, animation timing, touch dispatch và UI update; không có
structured state để biết lệnh đã thực sự thành công. Nó cũng đi ngược quyết
định trong repo: không dùng screenshot-based inference hoặc `input tap/swipe`.

Không chọn làm kiến trúc production; chỉ có thể là prototype tách biệt nếu
phạm vi dự án sau này thay đổi rõ ràng.

#### Phương án C — Can thiệp result/server path

Sửa event/result sau khi client tính hoặc giả mạo dữ liệu để “luôn
Excellent/Curve”.

Đây không còn là điều khiển flow client mà là can thiệp tính toàn vẹn của
gameplay. Nó không tương thích với fail-closed, khó xác minh, có rủi ro account
sanction và nên bị loại khỏi thiết kế.

#### Phương án D — Tách snapshot thành một app/OS capability độc lập

Chỉ tự động chụp ảnh Android hoặc lưu ảnh từ camera/MediaStore, không cố gắng
điều khiển encounter throw.

Ưu điểm là giảm coupling với IL2CPP. Nhược điểm là không tự động hóa được
`GO Snapshot` nếu yêu cầu thực sự là game-owned snapshot. Phương án này hợp lý
cho screenshot ngoài game, không thay thế được snapshot binding.

### 6. Trade-off và lựa chọn

| Phương án | Dễ làm | Đúng structured flow | Bền sau update | Rủi ro gameplay/account | Quyết định |
|---|---:|---:|---:|---:|---|
| A. Exact-build client-owned binding | Thấp | Cao | Trung bình nếu pin build | Thấp hơn các phương án can thiệp result | Chọn |
| B. UI/gesture automation | Trung bình | Thấp | Thấp | Trung bình/cao | Không chọn trong repo |
| C. Result/server tampering | Thấp/trung bình | Không | Rất thấp | Cao | Loại bỏ |
| D. OS snapshot độc lập | Trung bình | Không áp dụng cho throw | Cao ở OS layer | Thấp | Chỉ dùng nếu snapshot nghĩa là ảnh hệ thống |

### 7. Điểm tích hợp với repo

Các boundary hiện có phù hợp để mở rộng theo hướng sau:

- `core`: thêm model intent/profile và outcome ổn định; không chứa tên class,
  offset, pointer hoặc Unity API.
- `GameCapability`: tách capability tối thiểu, có thể gồm
  `THROW_CONTROL`, `OBSERVE_THROW_OUTCOME`, `AR_ENCOUNTER` và `SNAPSHOT`.
  `CATCH_AND_CLOSE_PREVIEW` vẫn là capability riêng, không suy ra từ catch.
- `AutomationAction.Catch`: chứa profile tùy chọn hoặc tham chiếu tới
  `ThrowIntent`; default phải giữ behavior hiện tại và không tự bật mutation.
- `bridge:protocol`: command phải mang runtime session, PID, package, build
  fingerprint, observation sequence và expiry như contract hiện tại. Outcome
  cần phân biệt `accepted`, `started`, `completed`, `failed`,
  `indeterminate` và actual throw quality/curve nếu binding chứng minh được.
- `game-adapter:pogo`: decode/map payload; không tự phán đoán quality từ
  screen.
- `zygisk`: chỉ cài binding cho exact build/ABI đã calibrated; runtime
  không được tự chọn candidate class/method chỉ từ keyword survey.
- `app`: UI chỉ hiển thị “target/profile” và capability readiness; không hiển
  thị guarantee khi runtime chưa trả outcome.

### 8. Luồng dữ liệu đề xuất

```text
Encounter observation
  -> controller validates identity/freshness/lifecycle
  -> user/policy selects ThrowIntent
  -> AutomationRunner creates one expiring Catch command
  -> bridge verifies session/build/capability
  -> exact-build binding verifies same encounter and game-owned state
  -> client-owned throw flow runs
  -> runtime emits outcome (quality, curve, ball, catch state)
  -> controller waits for terminal outcome and resyncs encounter
```

Snapshot đi theo flow riêng:

```text
Snapshot action
  -> capability/permission/foreground validation
  -> runtime starts game-owned snapshot flow
  -> completed event + optional media reference
  -> controller records idempotency key and result
```

AR+ phải có state machine riêng thay vì đi qua `expectedLifecycle = ENCOUNTER`
đơn thuần. Tối thiểu cần phân biệt `AR_SESSION_STARTING`, `AR_READY`,
`AR_THROWING`, `AR_FAILED` và `AR_EXITED`, nếu exact binding chứng minh được
các state này.

### 9. Cách kiểm thử

#### Unit/contract test trên host

- profile validation: target quality, curve preference, ball và AR mode không
  nhận giá trị mâu thuẫn;
- bridge encode/decode và backward compatibility;
- command bị từ chối khi sai session, build fingerprint, PID, lifecycle,
  observation sequence, expiry hoặc capability;
- `accepted` không bị map nhầm thành `completed`;
- outcome `UNKNOWN/INDETERMINATE` không kích hoạt retry throw tự động;
- idempotency cho snapshot và catch command;
- fake adapter mô phỏng từng terminal outcome.

#### Exact-build binding test trên device

- kiểm tra `strongIdentityVerified`, package/version/ABI và allowlist;
- encounter thay đổi giữa lúc lập command và lúc runtime nhận command;
- app background, camera permission bị thu hồi, AR session mất tracking;
- game restart/hook loss/GC pressure/Unity main-thread contention;
- timeout và process death không tạo duplicate action;
- đối chiếu event outcome của game với observation do bridge phát, không đoán
  bằng screenshot.

#### Acceptance/E2E

Chỉ test mutation trên build đã được calibrate và tài khoản/device được phép
kiểm thử. Không dùng test để tìm cách né anti-cheat hay xác nhận một result
forge.

### 10. Rủi ro và failure mode

- **Binding sai build:** gọi nhầm method/signature có thể crash game hoặc tạo
  hành vi không xác định. Biện pháp: strong identity, allowlist, capability
  riêng, fail-closed.
- **Nhầm intent với guarantee:** target `Excellent` không có nghĩa actual
  result là `Excellent`. Biện pháp: trả actual outcome và cho phép
  `INDETERMINATE`.
- **Duplicate action:** bridge retry sau timeout có thể ném hai lần hoặc
  chụp hai ảnh. Biện pháp: command ID/idempotency và không retry khi
  `mayHaveRun`.
- **AR/camera lifecycle:** state không đồng bộ hoặc mất permission có thể làm
  flow treo. Biện pháp: state machine riêng, timeout an toàn và binding loss.
- **Game update:** metadata/class/method thay đổi hoặc outcome schema đổi.
  Biện pháp: build-scoped module, versioned payload, không fallback generic.
- **Privacy/media:** snapshot có thể chứa hình ảnh/camera data. Biện pháp:
  chỉ lưu khi có opt-in rõ ràng, giới hạn payload và xác định owner của file.
- **Account/ToS:** automation gameplay có thể vi phạm điều khoản game và có
  rủi ro sanction. Đây phải là một cảnh báo sản phẩm, không được che giấu
  bằng anti-detection.

Worst-case cần ngăn chặn là: command gửi vào một binding stale, runtime báo
thành công giả, planner tiếp tục spam action, hoặc game crash trong process.

### 11. Lộ trình và rollback

#### Phase 0 — Chốt semantics

- xác định `AR+` nghĩa là AR encounter nào;
- xác định `snapshot` là GO Snapshot, Buddy Snapshot hay Android screenshot;
- xác nhận “chọn target” hay “ép actual result” là yêu cầu thật;
- chốt telemetry tối thiểu cho quality/curve/ball.

#### Phase 1 — Contract không mutation

Thêm model/profile, outcome schema, capability names và fake tests; UI chỉ cho
chọn profile nhưng runtime vẫn read-only. Đây là phase có thể merge mà chưa
đụng binding game.

#### Phase 2 — Throw thường trên một exact build

Chỉ hỗ trợ encounter thường, một action tại một thời điểm, outcome rõ ràng và
không auto-retry khi indeterminate. Curve có thể là một biến thể sau khi throw
thường đã có evidence.

#### Phase 3 — Snapshot độc lập

Bind snapshot flow và test idempotency/media ownership. Không phụ thuộc vào
throw profile.

#### Phase 4 — AR+

Chỉ bắt đầu sau khi có evidence rằng AR state/camera/pose có thể được quan sát
và điều khiển ổn định trên đúng thiết bị/ABI. Nếu không, giữ capability tắt.

Rollback ở mọi phase là xóa capability khỏi allowlist hoặc không quảng cáo nó
trong `RuntimeReady`; controller tự trở về read-only. Không cần đưa fallback
UI/input vào production.

---

## Acceptance Criteria (from spec)

> Source: không tìm thấy `docs/newspec/**` hoặc `docs/specs/**` cho feature này; tiêu chí dưới đây là **inferred — needs BA/device confirmation**, dựa trên yêu cầu người dùng và các contract hiện có trong repo.

| ID | Rule / Requirement | Formula / Expected | Acceptance note |
|---|---|---|---|
| AC-1 | Exact runtime identity | Action mutation chỉ được phép khi package, PID, session, ABI/build fingerprint và strong identity khớp allowlist | Ràng buộc cho toàn bộ throw/AR+/snapshot; nếu sai thì reject |
| AC-2 | Encounter freshness | `basedOnObservationSeq` còn hợp lệ, lifecycle đúng và encounter identity không đổi trước khi binding chạy | Ngăn ném vào encounter cũ hoặc target đã biến mất |
| AC-3 | Target quality là intent, không phải forged result | `requestedQuality ∈ {NICE, GREAT, EXCELLENT, ANY}`; `actualQuality` chỉ lấy từ client-owned outcome, có `UNKNOWN` khi thiếu bằng chứng | Ties trực tiếp tới yêu cầu chọn Nice/Great/Excellent |
| AC-4 | Curve được xác nhận | Kết quả tách `requestedCurve` và `actualCurve`; không suy ra `actualCurve=true` từ `COMMAND ACCEPTED` | Ties tới yêu cầu curve và tránh false positive |
| AC-5 | Catch terminal state | Mỗi command kết thúc ở `CAUGHT`, `MISSED`, `BREAKOUT`, `FLED`, `NO_BALL` hoặc `INDETERMINATE`; không auto-retry khi có thể đã chạy | Tái sử dụng `CatchOutcome` hiện có và mở rộng khi cần |
| AC-6 | Snapshot là action/capability riêng | Có capability và outcome riêng: `STARTED`, `COMPLETED`, `FAILED`, `INDETERMINATE`; có idempotency key | Ties tới auto snapshot, không gộp vào catch result |
| AC-7 | AR+ fail-closed | Không bật `AR_ENCOUNTER` nếu thiếu AR session/camera/permission/build binding hoặc state calibration | Ties tới yêu cầu AR+; không dùng encounter thường làm fallback giả |
| AC-8 | Không có UI/screenshot fallback | Không thêm screenshot inference, `input tap`, `input swipe`, root hiding hoặc anti-cheat bypass | Phù hợp `AGENTS.md`, `docs/ROADMAP.md` và `zygisk/README.md` |
| AC-9 | Observability và rollback | Log/bridge phân biệt accepted/started/completed/failed/indeterminate; tắt capability trong allowlist đưa hệ thống về read-only | Cho phép rollback sau game update hoặc binding loss |
| AC-10 | Verification | Có unit/contract tests trên host và exact-build device tests cho stale state, timeout, process death, permission loss, duplicate prevention | “Done” không chỉ là app build thành công |

### Mapping yêu cầu → tiêu chí

- “Có thể chọn `Nice`, `Great`, `Excellent`” → AC-2, AC-3, AC-5.
- “Có thể chọn `Curve`” → AC-2, AC-4, AC-5.
- “Hỗ trợ `AR+`” → AC-1, AC-2, AC-7.
- “Auto snapshot” → AC-1, AC-6, AC-9.
- “Không làm hỏng flow khi binding/update không phù hợp” → AC-1, AC-8, AC-9,
  AC-10.

---

## Synthesis

### Key Insight

Về nguyên tắc có thể inject để điều khiển một **client-owned throw/snapshot
flow** trên một exact Pokémon GO build, nhưng không thể coi đây là một hook
chung chỉ cần thêm cờ `nice/great/excellent/curve`. Chất lượng throw là
outcome cần game xác nhận; AR+ và snapshot là các subsystem riêng, trong đó
AR+ có độ phức tạp cao nhất. Repo hiện mới có bridge/action seam và còn thiếu
binding exact build, nên chưa thể triển khai mutation thật một cách có trách
nhiệm.

### Recommended Approach

Chọn Phương án A: mở rộng contract typed và capability-aware, bắt đầu bằng
throw thường trên một exact build, trả actual quality/curve thay vì giả định
guarantee. Tách snapshot thành action độc lập; chỉ xem xét AR+ sau khi chứng
minh được AR lifecycle trên thiết bị cụ thể. Giữ mọi feature mới sau strong
identity, allowlist, freshness, idempotency và fail-closed; không dùng input/
screenshot fallback hay result forgery.

### Risks to Watch

- Dùng binding stale sau game update có thể crash hoặc tạo action sai.
- “Target quality” bị hiểu nhầm thành guarantee, dẫn tới retry/spam khi outcome
  thực tế là indeterminate.
- AR+/camera và snapshot có state/lifetime riêng; gộp vào `Catch` sẽ làm
  contract khó kiểm thử và khó rollback.

### Open Questions

- ~~`AR+` có nghĩa là AR encounter của Pokémon GO, hay một tính năng khác?~~ → Đã xác nhận trong Section 13: AR encounter của Pokémon GO.
- ~~`auto snapshot` là GO Snapshot/Buddy Snapshot hay Android screenshot?~~ → Đã xác nhận trong Section 13: GO Snapshot trong encounter, không phải Android screenshot; thời điểm trigger chính xác vẫn cần chốt.
- ~~Người dùng cần “nhắm tới” quality hay thực sự muốn “ép” actual result?~~ → Đã trả lời trong Section 12: yêu cầu là ép hit 100%, kể cả khi animation hiển thị ném ra ngoài.
- Có exact package/version/ABI/device nào được phép làm binding spike không?
- Có chấp nhận rủi ro account/ToS của gameplay automation và có môi trường
  test riêng không?

## Section 12 — Bổ sung yêu cầu “100% trúng dù animation ném ra ngoài”

### Finding

Yêu cầu mới làm rõ rằng mục tiêu không chỉ là điều khiển quỹ đạo hoặc nhắm
tới `Nice/Great/Excellent`, mà là đảm bảo semantic result là **hit 100%** kể
cả khi animation/trajectory của ball hiển thị như đã bay ra ngoài Pokémon.

Điều này tạo ra hai trường hợp khác nhau:

1. **Hỗ trợ nhắm/ném:** runtime điều khiển client-owned throw flow; game tự
   tính hit/miss và trả outcome. Trường hợp này có thể nghiên cứu sau exact
   build binding nhưng không thể bảo đảm 100%.
2. **Ép hit bất chấp animation:** runtime phải thay đổi hit-resolution/result
   sau khi game đã tính, hoặc can thiệp vào dữ liệu gửi/nhận để biến miss
   thành hit. Đây là result manipulation, không còn là action binding.

### Tính khả thi và ranh giới

- Nếu server giữ quyền xác nhận catch/throw result, injected code trong client
  không thể bảo đảm hit 100% một cách đáng tin cậy mà không can thiệp vào
  protocol/result path.
- Nếu chỉ sửa local animation để trông như hit, server vẫn có thể trả miss;
  khi đó UI và semantic state lệch nhau, làm controller không thể biết outcome
  thật.
- Nếu ép semantic hit trong khi animation hiển thị miss, chính yêu cầu “visual
  miss nhưng result hit” là dấu hiệu của result forgery. Nó không phù hợp với
  `client-owned flow`, `strongIdentityVerified`, fail-closed và explicit
  capability boundary của repo; cũng làm tăng rủi ro account sanction.
- Encounter hết hạn, Pokémon đã flee, mất kết nối hoặc server từ chối request
  vẫn là các điều kiện mà local injector không thể hợp pháp biến thành hit.

### Quyết định kiến trúc

Không thêm capability kiểu `GUARANTEED_HIT`, `FORCE_CATCH` hoặc bất kỳ contract
nào cho phép coi animation miss là hit. `actualHit` chỉ được ghi nhận từ
outcome được game/client binding chứng minh; nếu visual state và semantic event
không khớp thì kết quả phải là `INDETERMINATE` và automation dừng để resync.

Phần khả thi còn lại là một capability ít mạnh hơn và trung thực hơn:

- `THROW_CONTROL`: chọn ball/curve/profile và để game tính kết quả;
- `OBSERVE_THROW_OUTCOME`: đọc `hit/miss`, quality và curve nếu binding chứng
  minh được event;
- adaptive aim/assist chỉ được tạo **intent** trước khi throw, không sửa result
  sau throw;
- không auto-retry khi command có thể đã chạy.

Nếu mục tiêu sản phẩm thực sự là guarantee hit, yêu cầu đó phải được đánh dấu
**out of scope** cho kiến trúc hiện tại thay vì cố thiết kế một đường vòng.

### Acceptance update

| ID | Rule / Requirement | Formula / Expected | Acceptance note |
|---|---|---|---|
| AC-11 | Không ép hit/result | `actualHit` chỉ nhận từ client-owned outcome; không có capability hoặc payload biến `MISS` thành `HIT` | Requirement “100% trúng dù animation ra ngoài” được phân loại là result manipulation và bị loại khỏi scope |
| AC-12 | Visual/semantic mismatch fail-closed | Nếu animation/trajectory báo miss hoặc không có evidence nhưng semantic outcome không xác minh được, kết quả là `INDETERMINATE`, dừng automation và resync encounter | Không coi local animation override là bằng chứng hit |
| AC-13 | Không guarantee vượt authority của server | Các điều kiện flee, stale encounter, network rejection hoặc server-side miss phải giữ nguyên outcome thật | Không quảng cáo “100% hit” trong UI/API |

### Cập nhật mapping yêu cầu

- “Ném 100% trúng Pokémon” → **AC-11, AC-12, AC-13**; chỉ có thể hỗ trợ
  dưới dạng aim/throw intent, không đảm bảo actual hit.
- “Kể cả animation là bị ra ngoài” → **AC-11, AC-12**; đây là dấu hiệu yêu
  cầu ép result và không được triển khai trong boundary hiện tại.

## Section 13 — Chốt nghĩa AR+ và auto snapshot

### Finding

Hai thuật ngữ đã được xác nhận:

- `AR+` là **AR encounter của Pokémon GO**, tức encounter dùng camera/AR
  session và state riêng của game; không phải một capability chung của Android.
- `auto snapshot` là **GO Snapshot trong lúc đang encounter**, không phải
  Android screenshot, MediaProjection hay ảnh chụp màn hình của controller.

### Tác động kiến trúc

`AR+` và `GO Snapshot during encounter` phải được mô hình hóa như các flow
game-owned, có cùng encounter identity nhưng lifecycle riêng:

```text
EncounterReady
  -> AR+ session ready
  -> SnapshotRequest(encounterId)
  -> SnapshotStarted
  -> SnapshotCompleted(media/result metadata)
  -> return to encounter / resync
```

Không nên gửi raw image qua bridge như một observation bình thường. Contract
nên trả metadata tối thiểu và một media reference do client/controller quản lý,
ví dụ trạng thái hoàn tất, timestamp, encounter identity và quyền sở hữu file.
Payload ảnh chỉ nên được bổ sung nếu exact binding chứng minh được lifetime,
kích thước và đường lưu trữ ổn định.

Các điều kiện tối thiểu trước khi gọi snapshot:

- runtime có exact build identity và capability `SNAPSHOT_DURING_ENCOUNTER`;
- encounter identity và observation sequence còn fresh;
- AR session/camera permission/foreground state đã sẵn sàng nếu flow yêu cầu;
- không có throw hoặc action khác đang chạy trên cùng encounter;
- command có idempotency key để timeout không tạo hai snapshot.

### Phân biệt ba loại “ảnh”

| Loại | Có thuộc scope mới không? | Chủ sở hữu |
|---|---|---|
| GO Snapshot trong encounter | Có | Pokémon GO client-owned flow |
| Buddy/collection Snapshot ngoài encounter | Chưa | Flow khác, không tự suy ra từ yêu cầu này |
| Android screenshot/screen recording | Không | Android/controller layer |

Vì snapshot được yêu cầu trong encounter, controller không nên tự chụp màn
hình để thay thế event của game. Nếu game không phát được completion event hoặc
media reference hợp lệ, action phải kết thúc ở `INDETERMINATE` và resync, không
được báo thành công dựa trên việc camera đã mở.

### Acceptance update

| ID | Rule / Requirement | Formula / Expected | Acceptance note |
|---|---|---|---|
| AC-14 | Đúng loại AR+ | `AR+` chỉ được bật cho Pokémon GO AR encounter với AR session/camera state được binding xác minh | Không dùng Android AR/screenshot capability thay thế |
| AC-15 | GO Snapshot trong encounter | `SNAPSHOT_DURING_ENCOUNTER` chỉ chạy khi `encounterId` và observation context còn fresh; trả `STARTED` rồi `COMPLETED`/`FAILED`/`INDETERMINATE` | Không coi mở camera hoặc chụp màn hình là GO Snapshot thành công |
| AC-16 | Media ownership và idempotency | Completion trả metadata/reference hợp lệ; cùng `idempotencyKey` không tạo duplicate snapshot | Không truyền raw image qua bridge nếu chưa có contract lifetime/size |

### Open question còn lại

Đã chốt loại snapshot, nhưng vẫn cần xác định snapshot được trigger ở bước
nào trong encounter: ngay khi encounter render, sau khi AR+ placement hoàn tất,
trước khi throw, sau một throw thất bại, hay theo một nút/điều kiện cụ thể.
Điểm này quyết định `expectedLifecycle`, serialization với `Catch` và cách
resync sau khi snapshot hoàn tất.
