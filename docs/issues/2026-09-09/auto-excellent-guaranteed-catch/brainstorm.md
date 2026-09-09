# Brainstorm: Auto Excellent và guaranteed catch theo hành động ném của người dùng

**Type:** feature
**Date:** 2026-09-09

---

## Analysis

### 1. Tính năng này giải quyết vấn đề gì?

Người dùng muốn hai behavior trong encounter Pokémon GO:

1. hỗ trợ ném bóng với mục tiêu `EXCELLENT`;
2. khi người dùng thực hiện một lần ném, encounter được xử lý theo chế độ
   “100% catch”; một lần ném hụt vẫn được tính là một lần ném hợp lệ cho chế
   độ này.

Repo hiện đã có `ThrowProfile` và `ThrowQualityTarget.EXCELLENT` ở domain và
bridge; native parser hiện giữ được profile qua wire nhưng đó vẫn mới là
intent. Catch
execution đang bị khóa bằng `kCatchExecutionEnabled = false`, và chưa có hook
để bắt `PokeballThrow` do thao tác người dùng tạo ra.

### 2. Ai được hưởng lợi?

Người vận hành framework trên đúng Pokémon GO build/ABI đã được xác minh.
Behavior này không nên bật mặc định cho runtime chưa có strong identity,
allowlist và calibration trên thiết bị. Người dùng hiện phải tự ném và tự
đánh giá kết quả; mục tiêu là có structured state/outcome thay vì suy luận từ
ảnh màn hình.

### 3. Các use case chính

Must have:

- Chọn `EXCELLENT` làm mục tiêu quality cho encounter catch.
- Chọn chế độ guaranteed-catch theo user throw, không tự tạo một throw thay
  cho người dùng.
- Ghi nhận cả `Missed = true` là một throw attempt; không bỏ qua miss vì
  không va vào Pokémon.
- Giữ các guard hiện có: session, PID, package, build fingerprint, lifecycle,
  observation freshness, command id và fail-closed khi thiếu binding.

Nice to have:

- Báo cáo `actualQuality`, `actualCurve`, `hit/miss` và catch outcome qua
  bridge/UI.
- Cho phép bật riêng auto-excellent và guaranteed-catch.

### 4. Edge cases

- `PokeballThrow` có `Missed = true`: vẫn phát hiện attempt; quality có thể là
  `NONE` và không được tự báo `EXCELLENT` nếu game không xác nhận.
- Throw vào Pokémon nhưng bị `BREAKOUT`, `FLED`, hết bóng hoặc mất kết nối:
  không được biến trạng thái không chắc chắn thành `CAUGHT`.
- Người dùng ném hai lần liên tiếp trong khi attempt trước chưa có outcome:
  không được xử lý trùng hoặc retry mù.
- Encounter đổi, thoát encounter, AR+ hoặc Master Ball cutscene đang chạy:
  hủy binding tạm thời và fail closed.
- Game update làm đổi RVA/signature/layout của `PokeballThrow` hoặc
  `AttemptCapture`: capability phải biến mất, không reuse binding cũ.
- Runtime bridge mất giữa lúc throw đang chạy: báo `INDETERMINATE` và yêu cầu
  resync; không gửi lại mutation.
- Không có ball hoặc item inventory không hợp lệ: giữ outcome `NO_BALL`/reject
  theo state của game.

### 5. Ràng buộc

Technical:

- Binding game-specific chỉ nằm ở `zygisk/` và `game-adapter/pogo`; core không
  chứa RVA, pointer, Unity type hoặc layout IL2CPP.
- `PokeballThrow` của build `0.427.0` là struct gồm `BallType`, `ReticleSize`,
  `HitBullseye`, `Spinning`, `Missed`; `Grade.Excellent` là một giá trị do
  game tính từ struct và encounter settings.
- `EncounterInteractionState.AttemptCapture(PokeballThrow)` và
  `PokeballService.Throw(GameObject, Vector3)` là hai seam khác nhau. Gọi
  `PokeballService.Throw` không chứng minh user throw và không cung cấp
  guaranteed catch.
- Native hiện không có inline-hook/trampoline framework và catch action chưa
  có outcome hook. Việc bật cờ compile-time đơn thuần là không đủ để đáp ứng
  yêu cầu.

Compatibility:

- Default behavior hiện tại phải giữ nguyên khi các option mới tắt.
- Protocol phải versioned; payload cũ không được bị diễn giải nhầm thành
  guaranteed-catch.

Operational:

- Đây là gameplay automation có thể vi phạm điều khoản của game; chỉ dùng
  trên account/device mà người vận hành được phép kiểm thử.
- Không dùng screenshot inference, `input tap`, `input swipe`, root hiding,
  anti-cheat bypass hoặc sửa result/server payload.

### 6. Các phương án đã cân nhắc

**A — Exact-build user-throw observation và client-owned outcome (khuyến nghị):**
bind đúng `AttemptCapture`/throw pipeline của build, quan sát attempt kể cả
miss, rồi chỉ công bố outcome sau khi game trả state/result. Đây là phương án
phù hợp boundary nhưng cần reverse/calibration và live verification.

**B — Tự động gọi `PokeballService.Throw`:** dễ nối vào action hiện có nhưng
không phải user throw, không bảo đảm Excellent và không giải quyết semantics
“miss vẫn tính”. Không chọn làm implementation của yêu cầu này.

**C — Sửa `PokeballThrow` hoặc `CatchPokemonOutProto` để ép quality/caught:**
đây là result tampering, không có evidence rằng server sẽ chấp nhận, và có thể
làm client state lệch server. Không coi là capability hợp lệ.

**D — Dùng gesture/screenshot automation:** phụ thuộc pixel/timing và bị cấm
bởi architecture của repo. Không chọn.

### 7. Tương tác với tính năng hiện có

- `CatchPolicy.throwProfile` đã truyền được `EXCELLENT` qua
  `AutomationAction.Catch`; action tag `10` và payload profile được native
  parser đọc/validate, nhưng execution vẫn bị khóa cho tới khi có binding live.
- `GameCapability.THROW_CONTROL` và `OBSERVE_THROW_OUTCOME` đã tồn tại nhưng
  chưa được native publish.
- `AutomationConfig` đã lưu `catchThrowQuality`, nhưng settings UI chưa có
  control cho quality/guaranteed-catch.
- `AutomationRunner` phân biệt `INDETERMINATE` và không retry mutation đã có
  thể chạy; semantics này phải được giữ cho user throw.
- Direct map catch là flow riêng, không được nhận throw profile hoặc
  guaranteed-catch encounter semantics.

### 8. Dependencies

- Exact reverse output cho `0.427.0`, đặc biệt method pointer/ABI của
  `EncounterInteractionState.AttemptCapture` và state transition sau Promise.
- Device calibration trên BlueStacks Air 1 với ADB serial được xác nhận.
- Native hook/observation implementation có guard build, object lifetime và
  main-thread safety; không nên tự viết một hook generic trước khi có proof.
- Bridge payload mở rộng nếu cần truyền `ThrowOutcome`/attempt marker; các
  model Kotlin đã có phần lớn kiểu dữ liệu cần thiết.
- Test fake/contract cho miss, hit, breakout, flee, timeout và duplicate.

### 9. Rủi ro

- **Rủi ro lớn nhất:** nhầm “user đã ném” với “server đã bắt”. Miss có thể
  được quan sát ở client nhưng không tạo capture request; không được báo
  `CAUGHT` nếu chưa có authoritative transition.
- **Crash sau update:** hook theo RVA/layout cũ có thể làm Pokémon GO crash.
  Biện pháp là exact fingerprint, signature/layout checks, capability off khi
  bất kỳ check nào thất bại.
- **Duplicate mutation:** timeout sau khi game đã nhận throw có thể dẫn tới
  ném lại. Biện pháp là idempotency per attempt và `INDETERMINATE` không retry.
- **False UI promise:** hiển thị “100%” trước khi native proof sẽ gây hiểu
  nhầm. UI nên hiển thị `requested`/`ready` và trạng thái capability thật.

### 10. Acceptance approach

Trước mắt có thể hoàn thành phần contract/config/UI và test domain mà không
bật mutation. Runtime feature chỉ được coi là live-ready khi exact build phát
hiện được user throw, coi miss là attempt, và có outcome transition được xác
minh trên thiết bị. Nếu thiếu proof này, hệ thống phải giữ capability tắt và
trả lỗi rõ ràng thay vì giả lập 100%.

---

## Acceptance Criteria (from spec)

> Source: không tìm thấy `docs/newspec/**` hoặc `docs/specs/**` cho feature này;
> các tiêu chí dưới đây là **inferred — needs BA/device confirmation**, dựa
> trên yêu cầu người dùng và contract hiện có trong repo.

| ID | Rule / Requirement | Formula / Expected | Acceptance note |
|---|---|---|---|
| AC-EX-01 | Auto Excellent là intent có cấu hình | `ThrowQualityTarget.EXCELLENT` được lưu, encode/decode và truyền tới action encounter | Không thay đổi default `ANY`; direct map catch reject profile |
| AC-EX-02 | User throw là nguồn attempt | Mỗi `AttemptCapture(PokeballThrow)` hợp lệ tạo tối đa một attempt marker | Không tự phát sinh throw thay user |
| AC-EX-03 | Miss vẫn được tính | `Missed = true` vẫn tạo attempt; `quality = NONE` nếu game không có grade | Không drop vì không hit Pokémon |
| AC-EX-04 | Guaranteed catch không giả lập | Chỉ `CatchOutcome.CAUGHT` sau state/result transition do client/server cung cấp | `ACCEPTED`, `STARTED`, miss marker hoặc Promise non-null chưa đủ |
| AC-EX-05 | Không duplicate | Một throw attempt có idempotency key; timeout sau khả năng đã chạy chuyển `INDETERMINATE` | Không auto-retry mutation |
| AC-EX-06 | Exact binding gate | Chỉ publish capability khi package/version/ABI/fingerprint/layout/signature và live postcondition khớp | Mismatch => capability absent, fail closed |
| AC-EX-07 | Encounter guard | Attempt chỉ thuộc encounter hiện tại, lifecycle `ENCOUNTER`, observation còn fresh | Encounter đổi/thoát => reject hoặc indeterminate, không reuse |
| AC-EX-08 | UI trung thực | UI hiển thị option và readiness riêng; không báo guaranteed khi capability chưa ready | Ties tới false UI promise risk |
| AC-EX-09 | Regression | `git diff --check`, focused JVM tests, native protocol tests và `./gradlew test assembleDebug` pass khi môi trường cho phép | Không dùng live mutation khi chưa có device proof |

### Mapping yêu cầu → tiêu chí

- “Auto ball Excellent” → AC-EX-01, AC-EX-06, AC-EX-08.
- “100% catch theo hành động ném của người dùng” → AC-EX-02, AC-EX-04,
  AC-EX-05, AC-EX-07.
- “Ném hụt cũng tính 100%” → AC-EX-03; đây là attempt accounting, không phải
  bằng chứng `CAUGHT`.

---

## Synthesis

### Key Insight

Repo đã có phần lớn domain seam cho mục tiêu `EXCELLENT`, và command parser đã
giữ được intent này, nhưng chưa có đường
điều khiển/quan sát native đủ để implement “user throw + guaranteed catch”.
`PokeballService.Throw` là auto-throw pipeline, không phải hành động ném của
người dùng; còn `PokeballThrow` chỉ mô tả throw và không tự biến kết quả server
thành caught. Vì vậy cần tách rõ attempt marker (miss vẫn tính) khỏi
authoritative catch outcome.

### Recommended Approach

Triển khai trước option/config/UI và contract tests cho `EXCELLENT` cùng một
chế độ guaranteed-catch rõ ràng, nhưng để capability native ở trạng thái
disabled cho tới khi có exact-build hook và outcome postcondition trên Air 1.
Sau đó bind user throw ở encounter pipeline, ghi nhận `Missed` như attempt,
và chỉ kết thúc `CAUGHT` khi state/result transition được xác minh. Không bật
compile-time flag hoặc result rewrite để tạo cảm giác 100% khi chưa có proof.

### Risks to Watch

- Hook sai method/layout có thể crash game hoặc làm lệch state.
- Promise được trả về không đồng nghĩa server đã catch; retry mù có thể tạo
  duplicate throw.
- UI/cấu hình không được quảng bá guaranteed khi runtime chưa publish
  capability và outcome thật.

### Open Questions

- Exact user-throw callback/method nào trên Air 1 luôn nhận được cả miss và hit?
- Catch result transition nào là definitive cho từng encounter type, gồm
  standard, AR+, raid và special encounter?
- “100% catch” có được phép nghĩa là chỉ tính attempt trong thống kê, hay bắt
  buộc phải sửa server-authoritative result? Cần xác nhận trước khi mở mutation.

## Section 11 — Kiểm chứng flow hiện tại sau lần ném trên emulator

### Flow thực tế của các file `.inc`

1. `runtime_native_prelude.inc` chỉ định nghĩa `Il2CppApi`, `RuntimeBinding`
   và `ProbeContext`. Đây là bảng con trỏ/guard dùng chung; nó chưa gọi
   `AttemptCapture`.
2. `runtime_probe_diagnostic.inc` chỉ chạy khi controller gọi lệnh
   diagnostic. Nó dò class, method và thử resolve service qua Zenject để ghi
   log. Nó không phải callback của game và không chạy ngay tại thời điểm người
   dùng ném.
3. `runtime_probe_discovery.inc` lưu owner của
   `IEncounterPokemon`/`IEncounterState`/`IEncounterInteractionState` vào
   `RuntimeBinding`. Bản hiện tại resolve từ `ProjectContext`; encounter
   services lại có thể nằm trong scope của `SceneContext`.
4. `runtime_encounter_owners.inc` định kỳ refresh các owner. Bản hiện tại
   cũng chỉ dùng `ProjectContext`, nên khi encounter scope được tạo mà không
   nằm trong container này thì binding vẫn null.
5. `runtime_observation.inc` là worker poll mỗi khoảng 750 ms. Nó attach
   IL2CPP thread, đọc active encounter và chỉ gọi probe khi binding đã có
   owner. Đây là polling state, không phải hook vào call stack của game.
6. `runtime_throw_probe.inc` đọc
   `EncounterInteractionState.<Pokeball>k__BackingField`, kiểm tra object có
   đúng class `Pokeball`, rồi đọc các field `dpjx`, `dpki`, `dpks`, `dplt`,
   `dplu`, `dpkh`, `BallType` và `PokemonHitCollision`. Chỉ khi snapshot đọc
   thành công và có thay đổi nó mới tạo diagnostic. File này không override
   `ThrowMissed`, không gọi `AttemptCapture`, và không tạo `PokeballThrow`.
7. `runtime_observation_protocol.h` chỉ serialize diagnostic thành frame
   `POGT`; phía Kotlin decode frame, ghi `AutomationEvent`, rồi
   `ToastAutomationEventSink` ghi logcat/toast. Nếu bước 3–6 không phát event
   thì phía toast hoàn toàn không có gì để hiển thị.

### Bằng chứng lần ném vừa rồi

Log game có `Pokeball:bsus(Boolean, Boolean)`, nhưng không có
`runtime throw probe`, `runtime throw diagnostic`, `AttemptCapture` hoặc
`ThrowMissed` từ module. Lifecycle phía module vẫn là `OVERWORLD`, và không có
log owner encounter được refresh. Vì vậy kết quả này chứng minh pipeline
diagnostic hiện tại bị chặn trước probe; không thể dùng nó để kết luận game đã
đi qua `AttemptCapture`.

### Kết luận điều chỉnh

Giả định trước đây rằng chỉ cần poll các field của `Pokeball` là sẽ bắt được
user throw là sai. Muốn đáp ứng yêu cầu phải trước hết lấy đúng encounter
scope/owner, sau đó bind vào seam nhận `PokeballThrow` của game hoặc một state
transition đã chứng minh tương ứng; việc này mới cho phép ghi nhận cả
`Missed = true`. `AttemptCapture` hiện chưa được override/hook trong code, nên
chưa có logic nào biến miss thành capture request, càng chưa có logic
guaranteed catch.
