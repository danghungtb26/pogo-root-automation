# Brainstorm: Pokémon GO native crash khi runtime START ở cold start

**Type:** bug
**Date:** 2026-09-10

---

## Analysis

### 1. Hành vi hiện tại

Pokémon GO bị native crash trên BlueStacks Air 1 trong lúc service tự khởi động
runtime. Log có ba lần process chết trong cùng vòng restart:

- `19:10:32.135`: `SIGTRAP` trên `Thread-8`; ngay trước đó Unity ghi
  `Graphics device is null` và stack managed kết thúc ở
  `Zenject.ProjectContext:get_Instance()`.
- `19:10:38.611`: process mới chết bằng `SIGSEGV`, `Cause: null pointer
  dereference`, ngay sau `runtime host start requested`.
- `19:11:07.077`: process mới tiếp tục chết bằng `SIGSEGV`; tombstone phụ có
  `UnityMain`/`il2cpp_init`, phù hợp với việc probe managed đụng vào quá trình
  IL2CPP đang khởi tạo.

### 2. Hành vi kỳ vọng

Native process probe chỉ được phát hiện `libil2cpp.so`/ELF export trong giai đoạn
đầu. Không được gọi managed IL2CPP, Zenject hoặc Unity API cho đến khi game đã
vào màn hình ổn định. Nếu runtime chưa đủ ready, bridge phải giữ trạng thái
probe-only hoặc trả lỗi fail-closed; Pokémon GO vẫn phải sống.

### 3. Thời điểm và điều kiện xảy ra

Xảy ra khi cấu hình automation đang enabled và controller reconnect vào process
mới trong lúc game còn cold-start. Chuỗi bằng chứng ngày 10/09:

```text
19:10:30.738 probe: il2cpp=1 ... assemblies=0 classes=0
19:10:31.624 runtime ready seq=1 strong=false capabilities=[]
19:10:31.634 runtime host start requested
19:10:32.132 Graphics device is null
```

Sau khi process chết, `HeadlessAutomationEngine` tự reconnect/retry nên cùng
điều kiện lặp lại ở `19:10:38` và `19:11:07`. Đây là tái hiện ổn định trong
capture hiện tại, không phải lỗi intermittent của gameplay.

### 4. Có thể tái hiện không?

Có, bằng cách bật service khi Pokémon GO vừa được launch/relaunch trên Air 1.
Native probe báo `RuntimeReady` trước khi managed assembly tồn tại đầy đủ; vòng
automation kế tiếp gửi `START` gần như ngay lập tức. Không cần gửi gameplay
command hay kích hoạt catch/transfer để crash xảy ra.

### 5. Nguyên nhân gốc

Root cause là gọi managed diagnostic sai lifecycle/thread:

1. Probe coi việc resolve ELF export là đủ để phát `RuntimeReady`, dù log probe
   vẫn có `assemblies=0`, `classes=0`; broker cũng phát `strong=false` và không có
   capability.
2. `RuntimeLifecycleCoordinator.ensureRunning()` gọi `bridge.startRuntime()`
   ngay khi nhận readiness; không chờ stable game screen, `strongIdentity` hoặc
   một managed-ready signal.
3. Native `activate_runtime()` coi `START` là bước chạy
   `run_managed_runtime_diagnostic()`.
4. Diagnostic attach thread rồi gọi `runtime_invoke()` vào
   `ProjectContext.get_Instance()`. Code chỉ bắt managed exception; native
   `SIGTRAP`/`SIGSEGV` không thể được bắt bằng `exception` pointer.
5. Ở cold start, call này đi vào Zenject/Unity `Resources.Load` khi graphics
   device chưa tồn tại hoặc cạnh tranh với `il2cpp_init`, dẫn đến crash trong
   process game.

Stack `Graphics device is null -> ProjectContext.get_Instance()` là bằng chứng
trực tiếp cho lần đầu. Các lần sau không cần coi là nguyên nhân mới: chúng là
những biến thể của cùng race trong quá trình restart, với một lần còn chạm
`il2cpp_init` trên `UnityMain`.

### 6. Dependency map

- **Upstream:** `HeadlessAutomationEngine.runLoop()` →
  `RuntimeLifecycleCoordinator.ensureRunning()` → `RuntimeBridgeClient.connect()`
  → native `START`; native binding probe chỉ kiểm tra loaded ELF/export.
- **Failing call path:** `activate_runtime()` →
  `run_managed_runtime_diagnostic()` → `discover_runtime_owners()` →
  `invoke_runtime_method()` → `ProjectContext.get_Instance`.
- **Downstream:** capability publication, module synchronization và structured
  controller. Trong capture, crash xảy ra trước gameplay action, vì vậy chưa có
  bằng chứng catch/spin/discard/transfer executor đã chạy.
- **Shared concern:** mọi đường gọi `run_managed_runtime_diagnostic()` hoặc
  `refresh_runtime_encounter_owners()` đều phải tuân thủ cùng readiness/thread
  boundary; không được sửa riêng catch để che race chung.

### 7. Phạm vi ảnh hưởng

Ảnh hưởng cold-start/reconnect khi automation enabled, đặc biệt process mới
trên thiết bị/emulator này. Có thể làm Pokémon GO restart loop và khiến bridge
phía app nhận `runtime bridge reader failed`/`Connection refused`. Không thấy
crash Kotlin/Java và chưa có bằng chứng crash phát sinh từ gameplay command.

### 8. Vì sao lỗi lọt qua

Contract và implementation đang lệch nhau. `docs/RUNTIME_BRIDGE.md` yêu cầu
diagnostic chỉ chạy sau stable game screen và ghi rõ managed call lúc
`il2cpp_init` từng crash, nhưng lifecycle refactor vẫn dùng `START` để chạy
diagnostic tự động. Ngoài ra `RuntimeReady` probe-only (`strong=false`) chưa
được dùng làm hard gate trước `START`. Test hiện tại không mô phỏng cold-start
với `assemblies=0` và concurrent `il2cpp_init`.

### 9. Rủi ro khi sửa

- Bỏ diagnostic khỏi `START` có thể làm capability/module chưa xuất hiện ngay;
  controller cần retry explicit diagnostic sau khi game ổn định.
- Chỉ thêm `sleep` là race-prone vì thời gian load thay đổi theo device/account;
  cần readiness condition hoặc thao tác explicit sau stable screen.
- Chuyển toàn bộ diagnostic sang Unity main thread an toàn hơn về thread nhưng
  vẫn phải gate lifecycle và giới hạn work vì diagnostic hiện khá nặng.
- Không được “nuốt” signal hoặc tiếp tục giữ binding half-initialized sau một
  lần probe thất bại; phải giữ fail-closed.

### 10. Minimal correct fix (chưa triển khai)

Tách native host `START` khỏi managed owner discovery: `START` chỉ xác nhận
module registry/identity và giữ host idle; chỉ chạy
`run_managed_runtime_diagnostic()` từ explicit post-init flow khi game đã stable
hoặc sau một managed-ready signal đáng tin cậy. Sau diagnostic thành công,
publish capability và sync module lại. Nếu vẫn cần runtime calls, chúng phải
được marshal qua đúng Unity main thread; không dùng delay cố định như safety
boundary.

### 11. Cách xác minh

- Cold-launch/relaunch với automation enabled: không còn `Graphics device is
  null`, `Fatal signal`, `SIGSEGV`/`SIGTRAP` trong process Pokémon GO.
- Khi probe còn `assemblies=0` hoặc `strong=false`, không có managed diagnostic
  call và không có module gameplay nào được enable.
- Sau khi game vào stable overworld, explicit diagnostic mới resolve Zenject và
  publish capability; module sync tiếp tục hoạt động bình thường.
- Chạy focused native protocol tests và Gradle test; nếu thay native build thì
  chạy full build, rồi kiểm tra `git diff --check`.

---

## Acceptance Criteria (from spec)

> Source: không tìm thấy spec riêng trong `docs/newspec/**` hoặc
> `docs/specs/**`; các tiêu chí dưới đây là **inferred — needs BA confirm**,
> dựa trên `docs/RUNTIME_BRIDGE.md`, `docs/LIVE_AUTOMATION_READINESS.md` và
> crash capture ngày 10/09.

| ID | Rule / Requirement | Formula / Expected | Acceptance note |
|----|--------------------|--------------------|-----------------|
| AC-1 | Probe-only không gọi managed | `assemblies=0 OR strong=false` → `managedDiagnosticCalls=0` | Trực tiếp ngăn crash cold-start; gắn với reported crash. |
| AC-2 | START không chạm Unity/Zenject sớm | `START` chỉ native host setup; không gọi `ProjectContext.get_Instance` trước stable gate | Không để `Graphics device is null` xuất hiện từ startup. |
| AC-3 | Diagnostic đúng lifecycle | Chỉ chạy sau stable game screen/managed-ready signal | Phù hợp contract hiện có trong `docs/RUNTIME_BRIDGE.md`. |
| AC-4 | Fail-closed | Diagnostic chưa chạy hoặc chưa đạt → capability/module mutation không được advertise/enable | Pokémon GO vẫn sống, controller nhận trạng thái unavailable. |
| AC-5 | Không restart loop do controller | Một native crash không được gây retry `START` liên tục trong cold-start | Retry chỉ sau process session mới và readiness hợp lệ. |
| AC-6 | Stable diagnostic vẫn hoạt động | Stable overworld → Zenject owner discovery thành công hoặc trả lỗi an toàn; không native crash | Bảo toàn binding/use case hiện có. |
| AC-7 | Không quy lỗi nhầm executor | Trước gameplay command không được có crash attribution cho catch/discard/transfer | Capture hiện tại crash trước action; các flag executor không phải bằng chứng root cause. |

- Báo cáo “Pokémon GO crash native lúc automation tự start” → `AC-1`, `AC-2`,
  `AC-4`, `AC-5`.
- Yêu cầu bảo toàn binding sau khi game ổn định → `AC-3`, `AC-6`.

---

## Synthesis

### Key Insight

Crash xảy ra vì `RuntimeReady` hiện chỉ chứng minh native ELF export đã load,
nhưng controller đối xử nó như managed runtime đã sẵn sàng. `START` ngay sau đó
gọi `ProjectContext.get_Instance()` trên worker thread trong lúc Unity graphics
và/hoặc `il2cpp_init` chưa hoàn tất; log đã ghi đúng stack này. Các executor được
enable ở commit gần nhất chưa chạy trước thời điểm crash.

### Recommended Approach

Khôi phục boundary probe-only: làm `START` không chạy managed diagnostic và chỉ
cho explicit post-init diagnostic chạy sau stable gate; sau đó mới publish
capability/sync module. Nếu diagnostic cần đọc Unity-owned state, marshal qua
Unity main thread và giữ fail-closed khi chưa có readiness signal.

### Risks to Watch

- Stable gate phải dựa trên state/signal, không chỉ sleep cố định.
- Không publish binding/owner pointer nửa khởi tạo hoặc giữ pointer qua runtime
  session mới.
- Tránh retry tự động dồn dập sau process death; cần backoff/session identity.

### Open Questions

- ~~Signal nào sẽ là managed-ready chính thức trên Air 1: stable overworld,
  `il2cpp_init` completion, hay callback main-thread đã xác minh?~~ → Với
  hotfix, chưa cần tự động suy ra mốc này: diagnostic được gọi tường minh sau
  khi game đã vào stable screen. Tín hiệu tự động cho phase sau vẫn cần verify.
- ~~Diagnostic có cần toàn bộ chạy trên Unity main thread hay chỉ nhóm
  `ProjectContext`/Unity-dependent calls?~~ → Phase 1 không mở rộng full
  main-thread diagnostic. Mọi call đụng Unity/scene về sau phải được phân loại
  và marshal qua main-thread bridge sau khi đã verify.
- Sau khi bỏ diagnostic khỏi `START`, controller sẽ trigger diagnostic và
  re-sync modules tự động ở mốc nào mà vẫn không phá fail-closed? → Hotfix dùng
  explicit `DIAGNOSTIC` sau stable screen; auto-trigger và cơ chế retry không
  được giả định bằng sleep, cần chốt trong implementation phase.

## Section 1 — Phương án fix chốt

### Mục tiêu

Chấm dứt crash loop khi controller vừa reconnect vừa gửi `START`, đồng thời giữ
nguyên fail-closed: probe-only không được coi là runtime đã sẵn sàng để bật
module.

### Phase 1 — hotfix bắt buộc

1. Sửa native `activate_runtime()` để `START` chỉ kiểm tra registry/runtime
   identity cần thiết và khởi tạo host ở trạng thái conservative; không gọi
   `run_managed_runtime_diagnostic()` trong đường `START`.
2. Giữ `RuntimeReady(strong=false, capabilities=[])` là trạng thái probe-only.
   Controller không được enable module hoặc thực thi automation từ trạng thái
   này.
3. Diagnostic managed chỉ chạy qua action `DIAGNOSTIC`, sau khi Pokémon GO đã
   vào stable screen. Không thay bằng `sleep`, screenshot tap, hay retry vô hạn.
4. Nếu diagnostic thất bại hoặc binding không exact, trả lỗi rõ ràng và giữ
   runtime/module disabled; tuyệt đối không để exception/native signal từ worker
   thread làm chết process.

### Phase 2 — hoàn thiện lifecycle

1. Sau `DIAGNOSTIC` thành công, native publish lại capabilities/strong identity;
   controller re-sync modules.
2. Sửa `syncModules()` để trạng thái `UNAVAILABLE` được retry khi
   capabilities/binding chuyển sang ready, thay vì bị skip chỉ vì desired state
   vẫn là `true`.
3. Bổ sung backoff và trạng thái reconnect để tránh `START` lặp liên tục khi
   target process chưa ready.
4. Các call phụ thuộc Unity/scene phải chạy trên Unity main-thread bridge đã
   verify; không đưa full discovery trở lại worker thread.

### Acceptance test cho bản fix

- Reconnect lúc game còn ở loading/process-start không tạo `Graphics device is
  null`, `SIGTRAP`, hoặc `SIGSEGV`.
- `START` thành công nhưng probe-only thì module vẫn disabled và không có action
  runtime.
- Sau khi game stable, `DIAGNOSTIC` thành công thì capabilities được publish và
  module sync chạy lại.
- Diagnostic fail vẫn fail-closed, không crash process và không retry nóng.
- Chạy focused tests, native protocol tests, `./gradlew test assembleDebug`, rồi
  cài/relaunch trên BlueStacks Air 1 và kiểm tra logcat.
