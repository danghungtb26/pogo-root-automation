# Brainstorm: move auto transfer sang native và transfer ngay sau catch

- Loại: feature + refactor
- Ngày: 2026-09-12
- Phạm vi: native Zygisk runtime, bridge config, Kotlin lifecycle/UI và cleanup
  transfer pipeline cũ

## 1. Mục tiêu và kết quả mong muốn

Sau khi native đọc được kết quả cuối của `TryCapture`, nếu kết quả là bắt thành
công thì native lấy đúng Pokémon vừa được lưu bằng `CapturedPokemonId`. Native
tự đọc metadata, áp dụng toàn bộ điều kiện keep, rồi:

- giữ Pokémon nếu thỏa một điều kiện keep;
- gọi release/transfer ngay nếu không thỏa điều kiện nào;
- chờ kết quả cuối của `ReleasePokemon` Promise trước khi báo transfer thành công;
- giữ catch-spin ở trạng thái bận trong suốt quá trình resolve transfer.

Kotlin không còn là nơi lập danh sách Pokémon cần transfer, không còn chờ
`AutomationSnapshot.storage` để quyết định và không còn phát lệnh transfer sau
một vòng quan sát muộn. Kotlin vẫn có thể là nơi người dùng chỉnh và lưu cấu
hình bền vững; native nhận một bản mirror có revision để thực thi trong session.

## 2. Hiện trạng đã kiểm tra

### Luồng catch hiện tại

Native đã có luồng authoritative:

```text
TryCapture
  -> giữ Promise bằng IL2CPP GC handle
  -> poll trên Unity main thread
  -> đọc CatchPokemonOutProto.Status + CapturedPokemonId
  -> map sync
  -> gửi RuntimeActionResult
```

Điểm cần gắn auto transfer là ngay sau khi `Status == CATCH_SUCCESS`, có
`CapturedPokemonId != 0`. Không nên gắn tại thời điểm gọi `TryCapture`, vì lúc
đó chưa biết catch thành công, và cũng không nên đợi Kotlin nhận late result.

### Native transfer hiện tại

Native đã có binding chính xác theo dump Pokémon GO 0.427.0:

- `IPokemonBag`/`PokemonBagImpl.GetPokemon(ulong id)`;
- `PokemonBagImpl.ReleasePokemon(PokemonProto pokemon)`;
- `ReleasePokemon` trả `IPromise<ReleasePokemonOutProto>`;
- command tag `6` đã có parser/executor cho lệnh transfer từ bên ngoài.

Executor hiện tại mới coi Promise khác null là `transfer_invoked` và trả phase
`COMPLETED`. Đây chỉ xác nhận lời gọi đã được tạo, chưa xác nhận server/game đã
release thành công. Phần này phải được thay bằng state machine Promise native,
dùng lại cho auto transfer và command transfer thủ công.

### Kotlin transfer hiện tại

Kotlin hiện còn nhiều trách nhiệm không phù hợp với mục tiêu mới:

- `TransferActionPlanner` quét `snapshot.storage` và tạo `TransferPokemon`;
- `StructuredAutomationWildState` dựng Pokémon tạm từ template catch;
- `AutomationCoordinator` lập kế hoạch transfer sau observation tiếp theo;
- `AutomationRunner` dispatch tag `6`;
- `TransferModuleDescriptor` chỉ bật module theo `autoTransfer`.

Luồng này có hai vấn đề chính:

1. native catch trực tiếp chỉ trả ID, còn Kotlin template không có đầy đủ IV,
   favorite, legendary, mythical và background;
2. quyết định transfer xảy ra muộn, có thể không có storage snapshot hoặc đọc
   metadata thiếu rồi áp dụng policy sai.

`docs/auto-transfer.md` là tài liệu provenance của pipeline cũ; một số mô tả
trong đó nói executor còn disabled nhưng source hiện tại đã đặt
`kTransferExecutionEnabled = true`, nên implementation mới phải dựa trên source
và dump hiện tại, không dựa vào trạng thái cũ trong tài liệu.

## 3. Policy keep cần giữ nguyên

Thứ tự hiện tại trong `TransferPlanner.shouldTransfer` là hợp lý và cần giữ
nguyên để tránh thay đổi hành vi ngoài ý muốn:

1. favorite;
2. shiny;
3. hundo;
4. special background;
5. legendary;
6. mythical;
7. IV thấp hơn ngưỡng;
8. IV không đọc được thì dùng `keepUnknownIv`.

Config người dùng đang có:

| Trường | Mặc định | Native cần nhận |
|---|---:|---:|
| `autoTransfer` | `false` | Có |
| `transferKeepHundo` | `true` | Có |
| `transferKeepShiny` | `true` | Có |
| `transferKeepSpecialBackground` | `true` | Có |
| `transferKeepFavorite` | `true` | Có |
| `transferMinimumIvPercent` | `80.0` | Có |
| `keepUnknownIv` | `true` trong core | Có, dù hiện chưa có UI |
| `keepLegendary` | `true` trong core | Có, dù hiện chưa có UI |
| `keepMythical` | `true` trong core | Có, dù hiện chưa có UI |

### Cách đọc metadata native

Sau khi catch thành công, native gọi `GetPokemon(captured_id)` trên main thread
và dùng field/method reflection đã verify thay vì tự đoán offset:

- IV: `IndividualAttack`, `IndividualDefense`, `IndividualStamina`;
- hundo: cả ba IV bằng `15`;
- favorite: `Favorite`;
- shiny/background: đọc `PokemonDisplayProto` và các property đã xác nhận.

`PokemonProto` không có flag legendary/mythical trực tiếp. Cần resolve một
classifier từ Game Master/Pokedex data bằng binding version-specific. Cho tới
khi classifier này được verify trên device, metadata legendary/mythical không
đọc được phải được coi là **unknown và giữ lại**, không được suy ra là false.

Tương tự, ý nghĩa `specialBackground` phải được chốt bằng binding của
`PokemonDisplayProto` (khả năng cao liên quan `HasLocationCard`/location card).
Nếu không chắc hoặc property không đọc được thì fail-safe là keep.

## 4. Thiết kế native đề xuất

### Module và file ownership

Tạo các file riêng dưới `zygisk/jni/modules/transfer/`, giữ trách nhiệm tập trung
trong transfer module:

- `config.inc`: snapshot policy, revision, validate và reset theo session;
- `pokemon_metadata.inc`: `GetPokemon` + đọc metadata + kết quả keep;
- `promise_observer.inc`: retain/poll `ReleasePokemon` Promise, timeout và terminal;
- `execute.inc`: common release executor cho auto/manual, guard và kết quả;
- `coordinator.inc`: nhận catch-success, chống duplicate, serialize transfer;
- `module.inc`: ownership tag/control action và `observe()`.

Không nhồi policy transfer vào `catch_spin/config.inc`. Catch-spin chỉ phát ra
sự kiện `catch succeeded`; transfer module sở hữu policy và release lifecycle.

### Trình tự runtime đề xuất

```text
TryCapture Promise terminal
  -> status == CATCH_SUCCESS && captured_id != 0
  -> transfer module nhận native catch-success
  -> GetPokemon(captured_id)
  -> đọc metadata đầy đủ
  -> KEEP ? log reason + kết thúc :
       ReleasePokemon(PokemonProto)
       -> giữ Release Promise bằng GC handle
       -> poll completeCalled/errorCalled trên main thread
       -> đọc ReleasePokemonOutProto.Status
       -> SUCCESS/FAILURE/INDETERMINATE
```

`direct_catch_promise_observer.inc` chỉ nên gọi API cấp module, ví dụ
`on_runtime_catch_succeeded(context, captured_id)`, không tự đọc policy hoặc
gọi `ReleasePokemon`. Như vậy dependency vẫn một chiều: catch phát event,
transfer quyết định và thực thi.

### Serialize với catch-spin

Cần có trạng thái native chung để biểu diễn transfer đang pending:

- không nhận cùng một Pokémon hai lần;
- không release đồng thời hai Pokémon;
- không scan/catch target kế tiếp khi đang resolve release;
- không dựa vào thứ tự static registration của module để đảm bảo an toàn.

Observer tick nên poll transfer Promise trước khi catch-spin được phép scan hành
động mới. Có thể dùng `runtime_transfer_is_busy()`/`runtime_transfer_terminal()`
để catch-spin giữ `suspended = true` đến khi transfer terminal. Nếu transfer
timeout hoặc indeterminate, giữ trạng thái suspended và không tự retry, vì retry
có thể release trùng khi request trước đã chạy nhưng response bị mất.

### Đọc Promise và kết quả transfer

`ReleasePokemon` Promise phải được xử lý giống nguyên tắc đã dùng cho
`TryCapture`:

- GC handle dùng kiểu ABI-width `Il2CppGcHandle`, không dùng `uint32_t`;
- chỉ poll trên Unity main thread;
- log `completeCalled`, `errorCalled`, value pointer, poll count và layout;
- verify object là `ReleasePokemonOutProto`;
- đọc `Status` bằng reflection;
- chỉ phát `AUTO_TRANSFER_SUCCESS` sau terminal status thành công.

Nếu `GetPokemon` chưa thấy object ngay sau catch, không được quyết định transfer
dựa trên object null. Nên retry bounded trong một cửa sổ ngắn trên main thread;
nếu hết hạn thì giữ Pokémon và log `metadata_unavailable`. Đây là điểm bắt buộc
để tránh transfer nhầm do inventory cache cập nhật chậm.

## 5. Config: Kotlin dispatch, native runtime mirror

Khuyến nghị tạo protocol riêng cho transfer thay vì mở rộng
`RuntimeCatchSpinConfig`:

```text
TRANSFER_CONFIG_SET
  marker riêng, schema riêng
  runtimeSessionId, requestId
  configRevision
  autoTransfer
  keepUnknownIv
  minimumIvPercentToKeep
  keepShiny
  keepHundo
  keepSpecialBackground
  keepFavorite
  keepLegendary
  keepMythical
  identity/build/expiry guards
```

Lý do tách protocol:

- transfer module tự sở hữu policy của nó;
- bật/tắt catch-spin không làm mất policy transfer;
- native có thể reject config cũ hoặc sai build độc lập;
- sau này module transfer có thể nhận nguồn trigger khác ngoài catch.

Kotlin vẫn persist setting trong `AutomationConfig`/SharedPreferences vì đó là
nguồn dữ liệu UI bền vững. `TransferConfigDispatcher` chỉ giữ cache
`sessionId + configRevision + snapshot`, gửi một lần khi thay đổi hoặc khi đổi
session. Native không cần tự ghi disk của app; native chỉ giữ bản runtime trong
session và reset khi STOP/restart.

Các điểm Kotlin cần thay đổi khi implement:

- thêm codec/model `RuntimeTransferConfig`;
- thêm `TRANSFER_CONFIG_SET` vào `ModuleControlAction`, owner là TRANSFER;
- thêm dispatcher và gọi song song với catch-spin config trong
  `AutomationCycle`/`RuntimeLifecycleCoordinator`;
- để `TransferModuleDescriptor` tiếp tục điều khiển enable theo `autoTransfer`;
- sau khi native flow ổn định, xóa `TransferActionPlanner`, pending transfer trong
  `StructuredAutomationWildState` và nhánh dispatch transfer của
  `AutomationCoordinator`;
- không xóa config UI/persistence hoặc event telemetry nếu vẫn cần hiển thị.

Trong giai đoạn chuyển tiếp có thể giữ command tag `6` thủ công, nhưng phải dùng
chung `start_runtime_transfer()` và Promise observer. Không được để manual path
và auto path có hai implementation release khác nhau.

## 6. Các lựa chọn và trade-off

### A. Kotlin tiếp tục quyết định sau storage observation

Ưu điểm: ít thay đổi native hơn. Nhược điểm: timing chậm, thiếu metadata, phụ
thuộc storage reader và có thể transfer sai direct-map Pokémon. Không đáp ứng mục
tiêu move toàn bộ logic sang native.

### B. Catch native gọi thẳng hàm release của transfer

Ưu điểm: latency thấp. Nhược điểm: catch-spin phải biết policy, metadata và
Promise lifecycle; dễ tạo coupling và file catch-spin phình to. Không khuyến nghị.

### C. Catch phát native event, transfer module tự quyết định và poll Promise

Ưu điểm: ownership rõ, trigger authoritative, dùng chung manual/auto executor,
fail-closed và dễ log/test. Nhược điểm: cần thêm protocol config, state machine
và binding classifier legendary/mythical. Đây là phương án khuyến nghị.

## 7. Rủi ro và cách giảm thiểu

| Rủi ro | Biện pháp |
|---|---|
| Catch success nhưng ID bằng 0 | Không release; log và giữ Pokémon |
| Inventory chưa cập nhật | Retry `GetPokemon` bounded trên main thread; hết hạn thì keep |
| Metadata thiếu | Unknown => keep theo default policy |
| Release Promise khác null nhưng request thất bại | Chỉ báo thành công sau đọc `ReleasePokemonOutProto.Status` |
| Promise mất target | Indeterminate, suspend, không retry tự động |
| Catch-spin chạy tiếp trước khi release xong | shared busy gate + giữ `suspended` |
| Config cũ đến sau config mới | monotonic `configRevision` + session guard |
| Kotlin và native cùng transfer | xóa planner/pending queue sau migration; có metric duplicate |
| Legendary/mythical classifier sai | chưa verify thì coi unknown và keep |
| Background semantics chưa chắc | property unavailable/unknown thì keep |
| module disable giữa Promise | `on_disable` giải phóng handle có kiểm soát và reset state; không giả thành công |

## 8. Phạm vi implement đề xuất

### Bắt buộc trong migration

1. Transfer config wire + dispatcher + native config snapshot.
2. Native metadata reader và keep evaluator.
3. Native catch-success entry point vào transfer module.
4. Native Release Promise observer, terminal status và timeout.
5. Busy gate giữa transfer và catch-spin.
6. Structured logs cho `KEEP`, `TRANSFER_START`, `TRANSFER_SUCCESS`,
   `TRANSFER_FAILURE`, `TRANSFER_INDETERMINATE`.
7. Xóa Kotlin planner/pending transfer decision sau khi native path đã verified.

### Không làm trong migration này

- không dùng screenshot hoặc input tap/swipe để suy ra metadata;
- không tự đoán offset mới khi reflection binding chưa xác minh;
- không tự retry release sau outcome indeterminate;
- không đổi semantics keep mặc định;
- không tạo storage scanner riêng nếu trigger sau catch đã đáp ứng yêu cầu hiện tại.

## 9. Verification plan

### Host/unit/protocol

- codec encode/decode transfer config, schema/version/size/identity;
- revision cũ bị reject, session khác bị reject;
- keep evaluator table test cho từng flag, thứ tự ưu tiên và unknown metadata;
- release state machine: waiting, success, error, malformed result, timeout,
  duplicate/in-flight;
- native protocol compile với `-Wall -Wextra -Werror`.

### Device log acceptance

Một ca catch đủ phải có chuỗi log tương đương:

```text
TryCapture Promise result ... status=CATCH_SUCCESS captured_id=N
auto transfer metadata id=N ...
auto transfer decision id=N action=KEEP|TRANSFER reason=...
auto transfer release invoked id=N promise=...
auto transfer promise state id=N complete=...
auto transfer result id=N status=... outcome=SUCCESS
```

Ca keep không được có `ReleasePokemon`. Ca transfer không được có Kotlin
`TransferPokemon` dispatch. Khi Promise chưa terminal, không được xuất hiện scan
catch target tiếp theo.

## 10. Acceptance Criteria

> Nguồn: `docs/auto-transfer.md` và yêu cầu người dùng — no spec found → inferred.

| ID | Tiêu chí nghiệm thu |
|---|---|
| AC-AT-1 | Chỉ trigger auto transfer sau authoritative `TryCapture` success và `CapturedPokemonId` khác 0. |
| AC-AT-2 | Native gọi `GetPokemon(id)` và áp dụng đầy đủ keep policy theo đúng thứ tự hiện tại. |
| AC-AT-3 | Metadata thiếu/không chắc được coi là unknown và fail-safe KEEP. |
| AC-AT-4 | Pokémon không thỏa keep mới được native gọi `ReleasePokemon(PokemonProto)` trên main thread. |
| AC-AT-5 | Native poll `ReleasePokemon` Promise và chỉ báo success sau kết quả terminal thành công. |
| AC-AT-6 | Chỉ có một transfer in-flight; catch-spin không scan/catch target mới trước khi transfer terminal. |
| AC-AT-7 | Tất cả transfer policy được gửi bằng config native riêng, có schema, revision, session, expiry và build guard. |
| AC-AT-8 | Kotlin không còn sở hữu transfer decision, pending queue hoặc trigger sau storage observation. |
| AC-AT-9 | Manual transfer nếu còn giữ sẽ dùng chung native executor/Promise observer và các guard hiện có. |
| AC-AT-10 | Timeout/indeterminate không tự retry release; runtime giữ trạng thái an toàn và báo rõ lý do. |
| AC-AT-11 | Log phân biệt được catch success, metadata, KEEP/TRANSFER decision, release invoked và release terminal result. |

## 11. Kết luận

### Key Insight

Native đã có đúng điểm dữ liệu và đúng binding để làm toàn bộ flow: Promise
`TryCapture` cho biết success + ID, còn `PokemonBagImpl` cho phép đọc object và
release. Kotlin hiện tại chỉ là workaround dựa trên storage snapshot, nên vừa
chậm vừa không đủ metadata.

### Phương án khuyến nghị

Chọn phương án C: catch phát event nội bộ, transfer module đọc metadata và tự
quyết định; policy đi qua `TRANSFER_CONFIG_SET`; release Promise được poll native
và khóa catch-spin cho tới khi terminal. Sau khi device verification đạt,
remove planner/pending transfer Kotlin nhưng giữ UI/persistence và dispatcher.

### Open questions cần verify khi implement

1. Giá trị enum `ReleasePokemonOutProto.Status` nào là success/failure trong
   build 0.427.0.
2. Property chính xác biểu diễn special background: `HasLocationCard`, location
   card hay field khác.
3. Đường classifier legendary/mythical bằng Game Master/Pokedex binding nào an
   toàn; trước khi có câu trả lời phải giữ unknown.
4. Cửa sổ retry `GetPokemon` phù hợp với thời gian inventory cache cập nhật trên
   BlueStacks Air 1.

Chưa implement code, build, push hoặc install trong lượt phân tích này.

## 12. Follow-up: chống catch trùng và trace transfer

### Hiện tượng và nguyên nhân bổ sung

Yêu cầu follow-up ghi nhận hai race/state gap: snapshot map có thể trả lại cùng
`spawn_id` ở vòng quét kế tiếp sau khi catch đã được invoke; ngoài ra observer
chưa chặn riêng pending `SendEncounterRequest`/`TryCapture`, nên một vòng quét
mới có thể tiếp tục đi vào nhánh catch trước khi Promise có result. Native đã có
`suspended` nhưng đây là state tổng quát, chưa đủ để log rõ nguyên nhân và không
bao phủ encounter Promise khi direct catch tạm thời bị abandon.

Transfer cũng có một lỗi semantics: `finish_runtime_transfer()` coi mọi terminal
phase completed không có error là success. Nhánh metadata quyết định `KEEP` cũng
đi qua handler này, nên có thể phát `POKEMON_TRANSFERRED` dù `ReleasePokemon`
chưa hề được gọi. Điều này làm log/toast gây hiểu nhầm khi người dùng kiểm tra
vì sao transfer không chạy.

### Thay đổi đã triển khai

- `RuntimeCatchSpinCoordinatorState` giữ `std::unordered_set<uint64_t>` theo
  `spawn_id`; chỉ insert sau khi direct catch thực sự trả
  `MainThreadActionOutcome::kInvoked`, và selector bỏ qua các ID đã ghi nhớ.
- Scan early-return khi có active encounter, pending `TryCapture`, hoặc pending
  `SendEncounterRequest`; các nhánh đều có log reason riêng.
- Transfer state giữ `release_attempted` và `release_invoked`; terminal được
  phân loại chính xác thành `KEEP`, `SUCCESS` hoặc `FAILED`.
- Native phát event telemetry `POKEMON_TRANSFER_TRIGGERED` sau khi
  `ReleasePokemon` Promise được retain; terminal success/failure phát event
  tương ứng. Kotlin map các event này thành custom toast.
- Release Promise observer được tách thành
  `zygisk/jni/modules/transfer/release_promise_observer.inc` để giữ giới hạn
  500 dòng/source file.

### Acceptance Criteria follow-up

| ID | Tiêu chí nghiệm thu | Expected |
|---|---|---|
| AC-AT-12 | Không catch lại spawn đã request | Cùng `spawn_id` không được invoke direct catch lần hai trong session/module run |
| AC-AT-13 | Chặn khi encounter/catch Promise pending | Không có map scan/action catch mới trước khi Promise terminal hoặc bị fail-safe block |
| AC-AT-14 | Phân biệt KEEP và transfer | KEEP không phát transfer success/trigger và không gọi `ReleasePokemon` |
| AC-AT-15 | Trace transfer end-to-end | Log/toast có `queued` → `trigger` → Promise `state/result` → success/failed |
| AC-AT-16 | Transfer toast đúng terminal | Chỉ Promise ReleasePokemon success mới hiện `Transfer success`; error/indeterminate hiện `Transfer failed` |

### Verification follow-up

`ANDROID_NDK="$HOME/Library/Android/sdk/ndk/27.1.12297006" ./scripts/build-magisk.sh`
đã pass cho arm64-v8a và x86_64. `./gradlew test assembleDebug --rerun-tasks`
đã pass; project hiện không có source unit test cho bridge protocol nên các
Gradle test task liên quan trả `NO-SOURCE`.
