# Brainstorm: Chuyển auto catch-spin sang native và đồng bộ config/filter

**Type:** architecture + refactor
**Date:** 2026-09-11

---

## Phạm vi và kết luận sơ bộ

Yêu cầu được hiểu là đánh giá hai việc:

1. Đưa vòng đời auto catch-spin (scan, chọn target, quyết định catch/spin, queue,
   cooldown và xử lý outcome) từ Kotlin xuống native.
2. Khi filter/config thay đổi ở UI/API Kotlin, dispatch một snapshot config sang
   native để native áp dụng và có thể giữ lại bản config đó.

Kết luận sơ bộ: **có thể làm về mặt kỹ thuật, nhưng không nên chuyển một bước
toàn bộ policy sang native**. Native hiện đã có execution layer khá rõ: đọc map,
đọc forts/inventory, gọi `TryCapture`/spin trên main thread và trả result. Phần
Kotlin hiện vẫn là policy/state-machine layer: phát `SCAN_MAP`, map snapshot,
filter/chọn target, serialize mutation, timeout, settle và recovery.

Phương án nên chọn là **hybrid theo hai giai đoạn**:

- Giai đoạn 1: tạo `CatchSpinConfig` + protocol `CONFIG_SET`, Kotlin vẫn là
  nguồn persistence chính; native giữ một runtime mirror được validate, version
  hóa và ACK.
- Giai đoạn 2: chỉ khi outcome direct-catch, snapshot correlation và native
  arbiter đã được verify trên thiết bị thì mới chuyển cadence/queue/execution
  orchestration sang native. Filter có thể chuyển phần đơn giản, nhưng các rule
  cần IV/shiny/encounter metadata vẫn nên để Kotlin trừ khi native đã có nguồn
  dữ liệu tương đương.

Việc “save trong native” nên được hiểu là native **áp dụng và giữ bản runtime
config trong session**. Nếu bắt buộc native phải giữ config qua restart của
Pokémon GO, persistence nên đặt ở root companion sidecar có atomic write; không
nên dùng bộ nhớ/file của process Zygisk làm nguồn sự thật thứ hai.

## Analysis

### 1. Vì sao cần xem xét refactor này bây giờ?

Mục tiêu hợp lý là giảm việc Kotlin phải poll/dispatch từng vòng và làm cho
catch-spin tự chủ hơn sau khi module được enable. Tuy nhiên source hiện tại cho
thấy request này không chỉ là tách file:

- [`AutomationCycle.kt`](../../../../app/src/main/java/dev/pogoroot/automation/engine/AutomationCycle.kt)
  đang phát `SCAN_MAP` theo cycle.
- [`StructuredAutomationController.kt`](../../../../app/src/main/java/dev/pogoroot/automation/runtime/structured/StructuredAutomationController.kt)
  nhận observation, dựng snapshot, gọi `AutomationRunner` và xử lý result.
- [`AutomationRunner.kt`](../../../../core/src/main/kotlin/dev/pogoroot/automation/core/automation/AutomationRunner.kt)
  đang là owner của one-mutation-at-a-time, identity, freshness, timeout,
  `INDETERMINATE`, map-sync và settle gate.
- Native [`module.inc`](../../../../zygisk/jni/modules/catch_spin/module.inc)
  mới sở hữu binding/execution và `SCAN_MAP`; module chưa tự chọn target hay tự
  chạy một planner native.

Vì vậy động lực refactor phải là một trong các mục tiêu đo được sau, không chỉ là
“logic nằm cùng một chỗ”:

- native có thể tiếp tục catch-spin khi Kotlin không phải dispatch từng action;
- giảm latency giữa observation và execution;
- một owner duy nhất cho queue/cooldown/outcome ở phía runtime;
- thay đổi filter không cần rebuild native;
- giữ được fail-closed khi session, binding, lifecycle hoặc outcome không chắc chắn.

Nếu mục tiêu duy nhất là tách code Kotlin cho dễ đọc thì không cần chuyển policy
sang native; chỉ cần tạo package/file `catchspin` trong app/core là rủi ro thấp hơn.

### 2. Cái gì thay đổi và cái gì phải giữ nguyên?

#### Có thể thay đổi

- Native `CatchSpinModule` có thêm config store và, ở giai đoạn sau, coordinator
  tự chạy cadence/action state machine.
- `SCAN_MAP` có thể trở thành native-internal read thay vì control request do
  Kotlin phát.
- Thêm control protocol `CONFIG_SET`, config revision, ACK/status và event/result
  cho action do native tự tạo.
- Kotlin `AutomationCycle`/`StructuredAutomationController` có thể bỏ phần
  catch-spin planning sau khi native orchestration đã được verify.
- Tổ chức lại source thành các file riêng dưới `zygisk/jni/modules/catch_spin/`
  để không dồn config, planner và execution vào `module.inc`.

#### Phải giữ nguyên

- Exact build/package/ABI guard, strong identity, allowlist fingerprint và
  lifecycle guard.
- Không dùng screenshot, `input tap`, `input swipe` hoặc fallback visual.
- Mọi managed read và mutation vẫn phải đi qua đúng Unity/main-thread bridge.
- Một mutation tại một thời điểm trên game surface; catch không chạy song song
  với spin.
- Command/action phải có correlation, expiry, session identity và xử lý
  `INDETERMINATE` fail-closed.
- `TryCapture` chỉ được coi là thành công sau authoritative outcome; Promise
  non-null hoặc invoke thành công không đủ.
- Hành vi filter hiện có không được thay đổi âm thầm trong một refactor. Direct
  map chỉ được quyết định từ metadata nearby thực sự có.

### 3. Dependency map hiện tại

| Tầng | Owner hiện tại | Vai trò | Nếu chuyển sang native |
|---|---|---|---|
| UI/API | `AutomationCategoryFragment`, `JoystickOverlayService`, `AutomationControlServer` | Ghi settings | Vẫn cần ghi settings; thêm dispatch snapshot |
| Persistence | `AutomationConfigRepository` + `SharedPreferences` | Nguồn config bền vững | Nên vẫn là canonical store; native chỉ mirror |
| Activation | `CatchSpinModuleDescriptor`, `RuntimeLifecycleCoordinator`, `CatchSpinArmState` | Enable/disable module | Giữ module lifecycle; config runtime điều khiển behavior |
| Cadence | `AutomationCycle` | Phát `SCAN_MAP` | Có thể chuyển vào native coordinator |
| Decode/cache | `BridgePogoRuntime`, `RuntimeCatchSpinPayloadCodec` | Decode nearby/forts/inventory | Native có thể giữ raw/live state; cần protocol status nếu Kotlin còn hiển thị |
| Policy | `AutomationCoordinator`, `CatchSpinActionPlanner`, `CatchPlanner` | Chọn catch/spin | Có thể chuyển rule đơn giản; rule giàu metadata cần nguồn dữ liệu mới |
| Queue/safety | `AutomationRunner` | Serialize, timeout, settle, recovery | Phải tái tạo trong native hoặc giữ Kotlin; không được có hai owner |
| Execution | `catch.inc`, `spin.inc`, `direct_map_actions.inc` | Resolve object + invoke | Đã ở native; tiếp tục ở đây |
| Thread boundary | `runtime_main_thread_bridge.inc` | Marshal read/action lên main thread | Phải dùng lại; không đọc managed collection từ bridge thread |
| Transport | `runtime_bridge_broker.inc`, bridge codecs | Forward observation/result | Thêm config/event message hoặc control action |

Các call chain quan trọng hiện tại:

```text
UI/API
  -> AutomationConfigRepository.update()
  -> HeadlessAutomationEngine đọc config mỗi vòng
  -> AutomationCycle.requestCatchSpinScan(cycle)
  -> native SCAN_MAP
  -> REQUEST_CATCH_SPIN observation
  -> StructuredAutomationController
  -> PogoGameAdapter + AutomationCoordinator + AutomationRunner
  -> BridgeBackedPogoActionExecutor
  -> native TRY_CATCH / TRY_SPIN
  -> AutomationCommandResult
  -> Kotlin Runner xử lý result/recovery
```

Đây là một pipeline đầy đủ; chuyển native sẽ thay cả nửa sau, không chỉ đổi
`CatchSpinModule.kt`.

### 4. Filter/config hiện tại thực sự gồm những gì?

Không nên giả định mọi filter đã có trong config catch-spin:

- [`HeadlessAutomationConfig`](../../../../app/src/main/java/dev/pogoroot/automation/config/AutomationConfig.kt)
  có `autoCatch`, `autoSpin`, throw profile, berry, delay và các policy khác,
  nhưng không có field persisted riêng cho `catchAll`, `alwaysCatchShiny`,
  `alwaysCatchHundo` hoặc `minimumIvPercent` của `CatchPolicy`.
- [`AutomationPolicyBridge.kt`](../../../../app/src/main/java/dev/pogoroot/automation/config/AutomationPolicyBridge.kt)
  dùng `CatchPolicy` mặc định; `catchAll` hiện đi theo default `true`.
- [`CatchSpinActionPlanner.kt`](../../../../core/src/main/kotlin/dev/pogoroot/automation/core/automation/modules/CatchSpinActionPlanner.kt)
  ở overworld chỉ dùng lifecycle, arm, `autoCatch`, `catchAll`, nearby target,
  ball state và fort availability. Target là nearby spawn sắp hết hạn.
- [`CatchPlanner.kt`](../../../../core/src/main/kotlin/dev/pogoroot/automation/core/automation/CatchPlanner.kt)
  có rule shiny/hundo/IV nhưng dùng cho active encounter, nơi có metadata giàu
  hơn; không thể áp dụng y nguyên cho direct-map nearby nếu payload không có
  IV/shiny.

Do đó native có thể nhận và áp dụng các field scalar hiện có (`autoCatch`,
`autoSpin`, `catchAll`, ball strategy, delay, revision), nhưng muốn native filter
theo species/IV/shiny thì phải bổ sung dữ liệu native tương ứng và xác định rõ
semantics khi metadata thiếu. Không được coi `nearby=[]` là “không có Pokémon”
khi reader thực tế đang unavailable.

### 5. Có chuyển toàn bộ logic sang native được không?

| Thành phần | Khả thi | Nhận định |
|---|---:|---|
| Đọc nearby/forts/inventory | Có | Native đã có readers và `SCAN_MAP`; cần snapshot atomic/correlation |
| Chọn target gần hết hạn | Có | Rule chỉ cần nearby + monotonic time; dễ chuyển |
| Catch-all direct map | Có điều kiện | Native đã invoke `TryCapture`, nhưng outcome observer vẫn là gate |
| Spin target | Có | Native đã resolve PokéStop và invoke spinner |
| Queue/cooldown/one mutation | Có | Cần native arbiter riêng; hiện `ProbeContext` chỉ có một đường pending main-thread |
| Timeout/resync/late result | Có điều kiện | Phải port semantics của `AutomationRunner`, không được tạo behavior đơn giản hơn |
| Filter IV/shiny/species | Chưa đủ dữ liệu | Nearby payload hiện không bảo đảm metadata; encounter flow là flow khác |
| Toast/status UI | Không nên native tự điều khiển | Native nên phát event/status; Kotlin vẫn render UI |
| Persist config trong target process | Không phù hợp làm canonical | Process GO có thể chết/restart; cần companion/app-owned persistence |
| Persist config qua root companion sidecar | Có | Có thể dùng atomic temp + rename, revision và session-independent schema |

Native coordinator về lý thuyết làm được, nhưng phải giải quyết thêm các vấn đề
mà Kotlin hiện đang che chắn:

1. Native action tự phát cần `commandId`/`cycleId` do native tạo; Kotlin hiện chỉ
   nhận result nếu `AutomationRunner` đang có active request tương ứng. Nếu chỉ
   reuse `AutomationCommandResult`, result native tự phát sẽ bị bỏ qua ở
   `consumeResult()` vì không có active request.
2. Native phải có event contract riêng cho `FILTER_DECISION`, `ACTION_STARTED`,
   `ACTION_RESULT`, `CONFIG_APPLIED` và `RECOVERY_REQUIRED`, hoặc một payload
   status đủ giàu. Không nên để Kotlin đoán trạng thái từ logcat.
3. Native phải serialize mọi action trong catch-spin và phối hợp với ENCOUNTER,
   DISCARD/TRANSFER nếu các module kia còn chạy. Hai planner độc lập không được
   cùng chạm game surface.
4. Direct-catch hiện vẫn có nhánh `direct_catch_outcome_unavailable` và trả
   `INDETERMINATE`; chuyển scheduler xuống native không làm mất rủi ro này.

### 6. Các ràng buộc kiến trúc và chất lượng cần ưu tiên

Thứ tự ưu tiên đề xuất:

1. **Safety/fail-closed:** không catch/spin khi identity, lifecycle, target,
   binding hoặc outcome không xác định.
2. **Correctness/correlation:** không gửi lặp action do observation cũ, config
   cũ hoặc result đến muộn.
3. **Maintainability:** filter thay đổi thường xuyên không buộc sửa binding
   native hoặc rebuild module.
4. **Testability:** config parser, filter đơn giản và state machine có thể test
   host-side không cần Pokémon GO chạy.
5. **Latency:** giảm round-trip Kotlin cho cadence/action sau khi các gate trên
   đã ổn định.

Chuyển toàn bộ native tối ưu latency nhưng làm giảm maintainability/testability
và tăng phụ thuộc vào runtime/version-specific C++. Đây là trade-off không nên
đánh đổi chỉ để tránh một `SCAN_MAP` round-trip.

### 7. Các phương án đã cân nhắc

#### Phương án A — Giữ policy Kotlin, chỉ tách file Kotlin/native execution

Kotlin tiếp tục filter, queue và dispatch; native giữ readers/executors như hiện
tại. Có thể tách Kotlin thành `CatchSpinCoordinator`/`CatchSpinConfig` để giảm
độ lớn controller.

- Ưu: ít rủi ro nhất, đúng tài liệu
  [`independent-runtime-control.md`](../../../../docs/architecture/independent-runtime-control.md),
  giữ `AutomationRunner` làm safety owner, test dễ hơn.
- Nhược: vẫn có bridge round-trip theo cycle; native không tự chủ hoàn toàn.
- Tương lai: dễ chuyển cadence hoặc một rule đơn giản sau này.

#### Phương án B — Native runtime coordinator, Kotlin canonical config mirror

Kotlin persist config và gửi full snapshot qua `CATCH_SPIN_CONFIG_SET`; native
giữ config đã validate, tự scan/catch/spin và phát event/status. Kotlin chỉ
quản lý UI, persistence, module lifecycle và hiển thị kết quả.

- Ưu: đạt phần lớn mục tiêu tự chủ; thay filter scalar không cần rebuild native;
  native có live object/runtime context; config vẫn chỉnh từ UI/API.
- Nhược: phải port `AutomationRunner` safety semantics sang C++; cần arbiter
  cross-module, event protocol mới và device verification rộng.
- Tương lai: có thể thêm native selector nhưng phải version hóa schema/rule.
- Đánh giá: **khuyến nghị làm theo phase, không big-bang**.

#### Phương án C — Full native policy + native-owned durable config

Kotlin chỉ gửi lệnh enable/disable; native sở hữu filter, state machine và file
config bền vững.

- Ưu: Kotlin mỏng, native chạy độc lập sau enable.
- Nhược: hai hệ UI/API và native phải đồng bộ schema; config file trong target
  process không ổn định qua process restart; rule filter khó test/reverse; dễ
  tạo split-brain với `SharedPreferences`; khó rollback.
- Đánh giá: **không chọn cho v1**. Chỉ xem xét nếu bỏ hẳn Kotlin policy và có
  owner persistence rõ ràng ở companion, kèm migration/schema governance.

### 8. Thiết kế config dispatch nên như thế nào?

#### 8.1. Ownership đề xuất

- Kotlin `AutomationConfigRepository` là **canonical durable config** cho UI,
  API và process restart.
- Native `CatchSpinConfigStore` là **validated runtime mirror** của session hiện
  tại. Native không được tự đổi policy và coi thay đổi đó là persisted truth.
- Root companion có thể có `CatchSpinConfigSidecar` nếu yêu cầu bắt buộc là
  config phải được ghi bởi native và survive target-process restart. Sidecar này
  phải được xem là cache/backup hoặc canonical store theo quyết định explicit;
  không được để cả `SharedPreferences` và sidecar cùng âm thầm thắng nhau.

#### 8.2. Protocol mới

Nên tạo control action/module-owned riêng, không nhồi config vào `SCAN_MAP`:

```text
CATCH_SPIN_CONFIG_SET
  protocolVersion
  runtimeSessionId
  requestId
  module = CATCH_SPIN
  configRevision (monotonic, > 0)
  armed
  autoCatch
  autoSpin
  catchAll
  ball policy / out-of-balls behavior
  spinSettleDelayMs
  catchSettleDelayMs
  optional filter schema/version + normalized rules
  expiresAtElapsedNs
  pid/process/package/build fingerprint
```

Tên wire/action và schema cụ thể cần chốt khi implement; không reuse `SCAN_MAP`
vì semantics khác nhau. Payload nên là **whole snapshot replace**, không phải
chuỗi patch field-by-field.

Native áp dụng theo thứ tự:

1. xác thực frame, session, PID/process/package/build và expiry;
2. kiểm tra protocol/schema version, size cap và range (`delay`, item/species
   list, số lượng rule);
3. reject toàn bộ nếu bất kỳ field nào malformed;
4. nếu `configRevision` cũ hoặc bằng revision đã áp dụng thì ACK idempotent,
   không re-run action;
5. commit snapshot dưới mutex/atomic pointer và ghi `activeRevision`;
6. trả `CONFIG_APPLIED`/ACK có revision thực tế.

#### 8.3. Khi nào dispatch?

- sau mỗi `POST /v1/config` thành công;
- sau mỗi overlay setting update;
- ngay sau module `CATCH_SPIN` được enable;
- sau bridge reconnect/session mới;
- sau native báo revision chưa có hoặc config reset;
- trước khi native coordinator được phép tự chạy.

Nếu dispatch thất bại, Kotlin giữ `pendingConfigRevision`, retry ở lần
`ensureRunning`/reconnect kế tiếp và **không bật native automation với config
unknown**. Khi `autoCatch=false` hoặc `autoSpin=false`, vẫn gửi full snapshot để
native tắt đúng behavior cũ; không suy ra field còn lại từ patch.

#### 8.4. Có nên để native ghi file không?

Có ba mức:

| Mức | Nơi ghi | Đánh giá |
|---|---|---|
| Runtime mirror | `CatchSpinConfigStore` trong `ProbeContext` | Nên làm; mất khi process chết là đúng |
| Durable sidecar | root companion, temp file + `fsync` + `rename` | Có thể làm nếu yêu cầu “native save” là bắt buộc |
| File trong target Zygisk process | app sandbox/POSIX trực tiếp | Không chọn làm source-of-truth; lifecycle/race/restart khó đảm bảo |

Nếu dùng sidecar, file cần có `schemaVersion`, `configRevision`, checksum hoặc
độ dài hợp lệ, temp path theo session-independent owner, `fsync` trước rename,
giới hạn kích thước và behavior khi parse hỏng (bỏ qua + chờ Kotlin push). Không
ghi config trong callback main thread hoặc trên đường invoke gameplay.

### 9. File layout riêng đề xuất

Không sửa `module.inc` thành một file khổng lồ. Các file mới nên có trách nhiệm
hẹp và mỗi file vẫn dưới giới hạn 500 dòng:

```text
zygisk/jni/modules/catch_spin/
  module.inc                         # registration/ownership only
  catch_spin_config.inc               # struct, defaults, validation, revision store
  catch_spin_config_protocol.inc      # parse/apply request nếu đặt ở module
  catch_spin_coordinator.inc          # cadence, candidate selection, queue/state
  catch_spin_events.inc               # native automation events/status
  catch.inc                            # direct/encounter execution bridge
  spin.inc                             # spin execution bridge

zygisk/jni/shared/bridge_kotlin/
  runtime_catch_spin_config_protocol.h # wire constants + codec contract
  runtime_catch_spin_event_protocol.h   # event/status payload contract

app/src/main/java/dev/pogoroot/automation/
  catchspin/CatchSpinConfigDispatcher.kt # full snapshot dispatch + retry/ACK
```

Có thể đặt `parse/apply` trong shared protocol thay vì module nếu nhiều module
dùng chung, nhưng default ownership vẫn phải thuộc `catch_spin`.

Các file này là **đề xuất để implementation**, chưa được tạo trong lượt
brainstorm. Bước hiện tại chỉ tạo file phân tích này.

### 10. Data flow sau khi chọn phương án hybrid

#### Giai đoạn 1: config mirror, chưa chuyển planner

```text
UI/API change
  -> AutomationConfigRepository.update()
  -> CatchSpinConfigDispatcher gửi CONFIG_SET(full snapshot, revision)
  -> native validate + commit CatchSpinConfigStore
  -> CONFIG_APPLIED(revision)

Kotlin vẫn:
  SCAN_MAP -> decode -> filter -> AutomationRunner -> TRY_CATCH/TRY_SPIN

Native vẫn:
  validate target/binding/lifecycle -> main-thread invoke -> result
```

Đây là phase kiểm tra protocol/config mà không đồng thời thay state machine.

#### Giai đoạn 2: native coordinator

```text
CONFIG_SET + CATCH_SPIN ENABLE
  -> native coordinator start
  -> native main-thread snapshot {nearby, forts, inventory, position, lifecycle}
  -> validate snapshot + filter theo schema đã được native hỗ trợ
  -> native arbiter chọn tối đa một mutation
  -> main-thread TryCapture/Spin
  -> authoritative outcome hoặc recovery-required
  -> native event/status qua bridge
  -> Kotlin chỉ render/log/persist summary
```

Tại phase 2, phải tắt đường Kotlin `AutomationCycle` catch-spin trước khi bật
native coordinator. Không chạy song song hai planner vì cả hai sẽ thấy cùng
nearby/fort và có thể gửi duplicate mutation.

### 11. Test và verification hiện có

Repo hiện không có file `src/test` thực tế cho core/bridge/adapter; chỉ có native
protocol test scripts được AGENTS.md hướng dẫn. Vì vậy refactor này chưa có safety
net đủ mạnh.

Test tối thiểu cần bổ sung trước phase 2:

- host-side C++ test cho config codec: malformed, version, range, size cap,
  revision cũ/duplicate, atomic replace và default fail-closed;
- host-side C++ test cho native selector/arbiter: lifecycle, empty vs
  unavailable, expiry, out-of-balls, catch priority, spin fallback, one action;
- Kotlin protocol test cho encode/decode round-trip và config revision ACK;
- Kotlin regression test cho `AutomationRunner` hiện tại trước khi rút planner;
- integration test mô phỏng reconnect, module enable lại, config update trong
  lúc action pending, timeout và late outcome;
- device validation trên đúng **BlueStacks Air 1**, dùng các script repo quy định.

Device acceptance phải bao gồm log correlation `runtimeSessionId`,
`configRevision`, `snapshotId/cycleId`, `commandId`, target và outcome. Không
dựa vào việc game hiển thị Pokémon trên UI để kết luận native reader đã có
nearby payload.

### 12. Rủi ro và failure mode

1. **Hai source of truth cho config:** Kotlin và native sidecar khác revision;
   filter chạy theo config cũ. Giảm thiểu bằng full snapshot, revision monotonic,
   ACK và Kotlin canonical.
2. **Hai planner cùng chạy:** Kotlin runner chưa tắt nhưng native coordinator đã
   enable. Đây là failure có thể ném/catch/spin trùng. Chỉ cho một owner tại một
   thời điểm, có feature flag và session status rõ ràng.
3. **Native state machine port thiếu semantics:** bỏ sót settle, map-sync,
   indeterminate hoặc late outcome khiến game bị spam action. Port theo contract
   của `AutomationRunner`, không viết scheduler tối giản.
4. **Outcome direct-catch chưa authoritative:** `TryCapture` invoke/Promise
   non-null bị coi là `CAUGHT`. Phải giữ capability/guard cho tới khi observer
   correlate được outcome thật.
5. **Metadata thiếu:** native filter IV/shiny/species trên nearby payload không
   đủ dữ liệu sẽ catch sai hoặc skip sai. Mỗi rule phải khai báo input metadata
   bắt buộc và behavior khi unavailable.
6. **Lifecycle teardown:** module thread/coordinator còn chạm IL2CPP sau STOP/
   detach. Phải stop, signal, join trước khi đóng bridge/detach; GO-absent luôn
   override automation ON.
7. **Protocol drift/version Pokémon GO:** native policy coupling vào build làm
   mỗi game update có thể phá cả filter lẫn binding. Giữ schema và mechanics
   version-gated, không nhúng tên class/rule không cần thiết.
8. **Config update giữa action:** config mới không được mutate active command;
   chỉ áp dụng cho action kế tiếp sau khi state machine xử lý terminal/recovery.

### 13. Migration và rollback

Migration nên incremental:

1. Tạo domain/config schema và codec ở Kotlin/native; chưa đổi behavior.
2. Tạo dispatcher gửi full snapshot sau enable/reconnect/update; native chỉ
   ACK/cache, chưa tự dispatch.
3. Đo config revision, payload latency, dropped ACK và native apply errors.
4. Tách native `CatchSpinConfigStore`/events thành file riêng; thêm host tests.
5. Implement native snapshot coordinator ở trạng thái **disabled by default**.
6. Chỉ bật coordinator khi exact build + outcome observer + device calibration
   đạt gate; Kotlin catch-spin planner vẫn là fallback không được bật đồng thời.
7. Khi native coordinator lỗi, gửi `DISABLE` module hoặc rollback feature flag về
   Kotlin planner; không tự fallback sang visual/input.
8. Sau device verification ổn định mới xóa `AutomationCycle` catch-spin path và
   các decoder/cache chỉ phục vụ planner cũ.

Rollback an toàn là giữ `CATCH_SPIN_NATIVE_ORCHESTRATOR=false` và tiếp tục
`CATCH_SPIN` execution hiện tại. Không rollback bằng cách cho cả hai planner
chạy song song.

### 14. Expected improvement

Nếu làm đúng phase 1, cải thiện cụ thể là:

- mọi thay đổi config đều có revision/ACK rõ ràng ở native;
- native không chạy bằng config cũ hoặc config malformed;
- reconnect tự rehydrate config mà không phụ thuộc UI click lại;
- có thể quan sát được native đang áp dụng revision nào.

Nếu làm phase 2, cải thiện thêm là giảm round-trip scan/action qua Kotlin và native
có thể điều phối live state gần runtime hơn. Đổi lại, chi phí là một native
state machine mới, protocol event mới và test/device verification lớn. Nếu không
đạt được các metric này thì không nên tiếp tục full move.

## Acceptance Criteria (from spec)

> Source: không tìm thấy `docs/newspec/**` hoặc `docs/specs/**` cho yêu cầu này;
> các tiêu chí dưới đây là **inferred — needs BA confirm**, dựa trên requirement
> người dùng và behavior/contract hiện tại.

| ID | Rule / Requirement | Formula / Expected | Acceptance note |
|---|---|---|---|
| AC-CS-01 | Tách file theo ownership | Config/coordinator/event/protocol nằm trong file riêng, mỗi source file ≤ 500 dòng | `module.inc` chỉ giữ registration/ownership; không gom toàn bộ logic vào một file |
| AC-CS-02 | Native giữ execution boundary | `TryCapture`/spin chỉ chạy sau exact build, binding, session, lifecycle và main-thread guard | Không có screenshot hoặc input fallback |
| AC-CS-03 | Config change được dispatch | Mỗi config update → một full `CONFIG_SET(configRevision)` → native ACK `CONFIG_APPLIED` | Áp dụng cho UI, API, module enable, reconnect |
| AC-CS-04 | Config snapshot nguyên tử | Parse/validate toàn bộ snapshot trước commit; malformed snapshot không làm thay đổi config hiện hành | Không patch nửa chừng; reject version/range/size sai |
| AC-CS-05 | Revision/idempotency | `revision <= appliedRevision` không chạy lại action; revision mới hơn thay thế nguyên snapshot | Duplicate/lost ACK có thể retry an toàn |
| AC-CS-06 | Persistence không split-brain | `AutomationConfigRepository` là canonical; native mirror mất khi session chết hoặc sidecar có owner/revision rõ | Không để target process file và SharedPreferences cùng âm thầm là source-of-truth |
| AC-CS-07 | Master/lifecycle fail-closed | STOP, GO absent, binding loss hoặc config unknown → native coordinator không dispatch mutation | Automation OFF sau process restart; module phải enable lại có chủ đích |
| AC-CS-08 | Một mutation owner | Tại mọi thời điểm chỉ Kotlin runner hoặc native coordinator được phép dispatch catch-spin | Khi chuyển phase 2, đường Kotlin phải bị disable trước |
| AC-CS-09 | Snapshot atomic/fresh | Nearby, forts, inventory, position, lifecycle có cùng `snapshotId/cycleId` hoặc field unavailable rõ ràng | Không trộn snapshot cũ với mới; unavailable khác empty |
| AC-CS-10 | Filter đúng dữ liệu | Rule chỉ chạy khi input metadata cần thiết có mặt; direct-map không áp dụng IV/shiny nếu nearby không cung cấp | Target mặc định vẫn là spawn hợp lệ sắp hết hạn theo behavior hiện tại |
| AC-CS-11 | Catch outcome authoritative | Chỉ `CAUGHT`/`FLED`/outcome semantic sau observer correlate mới kết thúc thành công; invoke/Promise non-null chưa đủ | `INDETERMINATE` giữ recovery, không retry mù |
| AC-CS-12 | Timeout/settle giữ semantics | Timeout pending → indeterminate/recovery; terminal catch/spin → action-specific settle; không dùng timeout thay settle | Port từ `AutomationRunner`, test mốc biên bằng monotonic clock |
| AC-CS-13 | Config không làm đổi active action | Config mới chỉ ảnh hưởng quyết định sau action hiện tại đã terminal hoặc recovery-safe | Không mutate `TryCapture`/spin đang chạy |
| AC-CS-14 | Test và thiết bị | Codec/state-machine tests xanh; `git diff --check` sạch; device verify bằng script trên BlueStacks Air 1 | Không dùng raw ADB để thay script repo |

- “Move logic auto catch-spin sang native” → AC-CS-01, AC-CS-07, AC-CS-08,
  AC-CS-09, AC-CS-10, AC-CS-11, AC-CS-12.
- “Filter/config thay đổi phải dispatch và save native” → AC-CS-03,
  AC-CS-04, AC-CS-05, AC-CS-06, AC-CS-13.
- “Tạo file riêng để xử lý” → AC-CS-01.

## Synthesis

### Key Insight

Native đã là nơi đúng cho **cơ chế live runtime**, nhưng chưa phải nơi có đủ
contract để làm **policy orchestrator**. Chuyển toàn bộ catch-spin xuống native
không chỉ là move Kotlin code; nó cần native scheduler/arbiter, event protocol,
outcome observer, snapshot atomic và migration khỏi `AutomationRunner`. Phần
config có thể dispatch xuống native ngay, nhưng native nên giữ một validated
runtime mirror với revision/ACK; persistence bền vững vẫn nên có một canonical
owner ở Kotlin hoặc root companion.

### Recommended Approach

Chọn hybrid phased migration. Trước tiên tạo các file riêng cho
`CatchSpinConfigStore`, `CONFIG_SET` protocol/dispatcher và ACK, giữ nguyên
Kotlin planner để xác minh propagation mà không đổi behavior. Sau đó, nếu đo đạc
cho thấy round-trip là bottleneck và direct-catch outcome đã authoritative, mới
thêm `CatchSpinCoordinator` native và chuyển từng trách nhiệm (cadence → arbiter →
selector), với feature flag để rollback. Filter chỉ chuyển sang native cho rule
mà payload native có đủ metadata; các rule IV/shiny/encounter vẫn giữ ở Kotlin.

### Risks to Watch

- Split-brain giữa config Kotlin, native mirror và native sidecar.
- Hai planner/queue cùng dispatch mutation hoặc port thiếu `INDETERMINATE`,
  settle và map-sync semantics.
- Native coi `TryCapture` invoke thành `CAUGHT` khi chưa có authoritative outcome;
  hoặc áp dụng filter trên metadata nearby không đầy đủ.

### Open Questions

- “Save trong native” có nghĩa là giữ trong session hay phải sống qua restart
  Pokémon GO? Nếu qua restart, canonical owner là Kotlin hay companion sidecar?
- Bộ filter thực tế cần native xử lý gồm những field nào: chỉ `autoCatch`,
  `autoSpin`, `catchAll`, ball/fort rule, hay cả species/IV/shiny?
- Có chấp nhận native coordinator chỉ áp dụng cho direct-map catch-spin, còn
  encounter/berry/throw vẫn do Kotlin/ENCOUNTER module quản lý không?
- Native outcome observer cho direct-catch sẽ được verify ở lifecycle/result nào,
  và cần bao nhiêu retry/recovery trước khi báo `INDETERMINATE`?
- Có cần giữ Kotlin UI/status chi tiết (selected target, filter reject reasons,
  last outcome) hay chỉ cần native event summary?
- BA cần chốt các giá trị default/range của filter và settle/timeout trước khi
  protocol schema được đóng băng.
