# Brainstorm: Thay thế đường lấy Promise result sau IL2CPP invoke

**Loại:** design-api
**Ngày phân tích đầu tiên:** 2026-10-14
**Phạm vi game/build/ABI:** Pokémon GO `0.433.0`, versionCode `2026100702`,
IL2CPP metadata v39; arm64-v8a là artifact reverse chính, runtime còn phải xét
x86_64 trên BlueStacks Air 1

## Bước 1 — Yêu cầu và phạm vi

### Yêu cầu đã biết

- Bên gọi là injected native runtime trong `zygisk/`; bên xử lý bất đồng bộ là
  Promise implementation của Pokémon GO/IL2CPP.
- `il2cpp_runtime_invoke` hiện trả được object mà managed method trả về, nhưng
  đường quan sát hiện tại không lấy được Promise completion value/result data
  một cách đủ tin cậy.
- Cần phân tích một đường thay thế để nhận exact result cho direct catch,
  transfer và discard, đồng thời giữ main-thread, session/generation,
  correlation, timeout và fail-closed semantics.
- Không được coi return object, dispatch thành công hay Promise state local là
  gameplay success. Authoritative map/storage/inventory postcondition vẫn là
  gate riêng.

### Phạm vi contract

- Cần cả command và observation: command dispatch mutation qua verified
  `MethodInfo*`; observation nhận completion/value/rejection của Promise.
- Không thay đổi gameplay policy hoặc Kotlin ownership. Game-build-specific
  Promise binding nằm sau native adapter/binding boundary.
- Không gọi trực tiếp RVA, không dựa vào private `MethodInfo` layout và không
  dùng binding 0.427.0 làm proof.

### Điều kiện hoàn tất ban đầu

1. Xác định exact Promise interface/concrete type và API completion/value/error
   có thể bind trong 0.433.0.
2. Giải thích vì sao observer hiện tại không nhận result data.
3. So sánh các phương án: subscribe callback/continuation, poll state/value qua
   verified managed API, hoặc observe authoritative downstream event/state.
4. Chốt contract thay thế, ownership/lifetime và tiêu chí runtime evidence trước
   khi mở mutation capability.

### Giả định và câu hỏi còn thiếu

- Chưa xác minh concrete Promise class được trả ở runtime cho từng method.
- Chưa xác minh Promise API có `Then`/`Catch`/`Done`, state/value accessor hay
  event/callback nào ổn định trong exact build.
- Chưa xác minh failure hiện tại nằm ở return object, GC lifetime, wrong generic
  class, callback registration, polling layout hay result decoding.
- Không cần hỏi thêm người dùng ở bước này; các câu trên sẽ được tra bằng
  retained REA Evidence trước, sau đó mới đối chiếu framework source.

## Bước 2 — Bằng chứng từ reverse

### Identity và nguồn

| Nguồn | Identity / Evidence | Authority và coverage | Limitation |
|---|---|---|---|
| `reverse/artifacts/pogo-0.433.0/native/libil2cpp.so` | SHA-256 `6b1e1161...c25a0`; canonical bundle có 13 record khớp exact ELF | shipped artifact; Ghidra 12.1.3 | Static native evidence không quan sát Promise instance runtime. |
| `reverse/artifacts/pogo-0.433.0/native/global-metadata.dat` | SHA-256 `f5961a85...fb0d`; metadata v39 | shipped artifact + deterministic derivation | Signature/type identity, không chứng minh completion behavior. |
| `reverse/rea/rea-evidence-2026-10-13.json` | Import thành công, 33 retained records/4 unknown revisions trước lượt Promise mới | canonical REA Evidence | Chưa có Promise member record trước lượt này. |
| `build/rea/cpp2il-pogo-0.433.0/Niantic.Promises.dll` | SHA-256 `4016974d...7fde`; `ev_0c4a9079...`, `ev_3bdab983...` | REA đọc metadata complete: 29 types, 112 methods, 63 fields | Đây là auxiliary Cpp2IL reconstruction, không phải shipped managed DLL. Dù provider trả authority `shipped-artifact`, repo provenance buộc coi nó là **derived auxiliary**; token/order không được dùng thay global metadata. |
| `build/rea/pogo-0.433.0-promise-mapping.json` | SHA-256 `0cab5f8e...4131`; 515 method rows | deterministic derivation từ exact ELF + metadata | Generic method definitions có RVA `0`; chưa resolve inflated generic method pointer/RGCTX. |

`shasum -a 256 -c reverse/artifacts/SHA256SUMS` pass lại 10/10 entry.

### Symbol Promise đã xác nhận statically

Global metadata mapping của exact build xác nhận:

| Symbol | Token / slot | Exact managed signature | Native location / giới hạn |
|---|---:|---|---|
| `Niantic.Promises.IPromise<T>.Then` value overload | `0x06000026` / 2 | `IPromise<T> Then(System.Action<T>)` | interface, RVA `0`; đúng API để nhận completion value về mặt managed contract |
| `Niantic.Promises.IPromise<T>.Then` no-value overload | `0x06000025` / 1 | `IPromise<T> Then(System.Action)` | interface, RVA `0`; không mang result data |
| `Niantic.Promises.IPromise<T>.Error` | `0x06000027` / 3 | `IPromise<T> Error(System.Action<System.String>)` | interface, RVA `0` |
| `Niantic.Promises.IPromise<T>.Finally` | `0x06000028` / 4 | `IPromise<T> Finally(System.Action)` | interface, RVA `0` |
| `Niantic.Promises.Promise<T>.get_Completed` | `0x0600003c` / 9 | `System.Boolean get_Completed()` | generic definition RVA `0`; không cung cấp value |
| `Niantic.Promises.Promise<T>.Then` value overload | `0x06000041` / 6 | `IPromise<T> Then(System.Action<T>)` | generic definition RVA `0`; concrete inflated address chưa có |
| `Niantic.Promises.Promise<T>.Error` | `0x0600003f` / 7 | `IPromise<T> Error(System.Action<System.String>)` | generic definition RVA `0` |

Ba game mutation method trả chính xác:

- `MapPokemon.TryCapture(...) -> IPromise<CatchPokemonOutProto>`;
- `PokemonBagImpl.ReleasePokemon(...) -> IPromise<ReleasePokemonOutProto>`;
- `ItemBagImpl.RecycleItem(...) -> IPromise<RecycleItemOutProto>`.

Auxiliary reconstruction cho thấy concrete `Promise<T>` có các field mang tên
`completeCalled`, `errorCalled`, `completedValue`, `errorValue`, và có callback
lists cho `Action`, `Action<T>` và error handlers. Đây là hypothesis mạnh cho
implementation model, nhưng field token/order/offset của auxiliary DLL mâu thuẫn
với global metadata token ordering, nên chưa đủ để đọc raw field bằng offset.

### Kết luận bước 2

- **Đã xác nhận:** managed API chuẩn để nhận value là overload
  `IPromise<T>.Then(Action<T>)`, không phải return value lần hai từ
  `il2cpp_runtime_invoke`.
- **Đã xác nhận:** `get_Completed()` chỉ là boolean; dùng nó riêng không thể lấy
  `T`.
- **Suy luận có cơ sở:** Promise giữ value/error và callback list nội bộ; callback
  đăng ký sau completion có thể cần replay ngay, nhưng hành vi này chưa có native
  call-path/runtime Evidence.
- **Chưa xác minh:** concrete runtime class, inflated generic `Then` method,
  delegate construction ABI, callback thread và lifetime. Đây là các blocker
  trước khi chọn callback subscription làm production binding.

## Bước 3 — Function/method, param và hành vi

### Contract managed và native đang dùng

| Thành phần | Input / output | Hành vi có thể khẳng định | Residual unknown |
|---|---|---|---|
| `TryCapture`, `ReleasePokemon`, `RecycleItem` | các argument đã map; return `IPromise<TOutProto>` | `il2cpp_runtime_invoke` trả **Promise object ngay lúc dispatch**, không chờ `TOutProto` | Promise concrete class của từng call ở runtime |
| `IPromise<T>.Then(Action<T>)` | một managed delegate nhận `T`; return cùng contract Promise | API public duy nhất đã thấy mang completion value tới consumer | inflated generic method, callback thread và delegate target thực tế |
| `IPromise<T>.Error(Action<string>)` | managed delegate nhận error string | đường failure public | cùng blocker delegate/generic ở trên |
| `ICompletable.get_Completed()` | không param; return `bool` | chỉ cho biết complete, không trả value/error | concrete implementation có thể là wrapper/keeper |
| `read_runtime_promise_field<T>` | Promise object + field name | đi lên tối đa 8 parent class, gọi `class_get_field_from_name`, rồi `field_get_value` | field private không thuộc interface contract; type/owner/offset chưa được verify |

Observer catch/transfer/discard hiện giữ Promise bằng strong GC handle, poll trên
Unity main thread, rồi đọc bốn private field theo tên:
`completeCalled`, `errorCalled`, `completedValue`, `errorValue`. Khi
`completedValue` khác null, code mới kiểm tra exact result class và đọc
`Status`/`Result`/`NewCount`/`CapturedPokemonId`.

Vì vậy lỗi không nằm ở việc `runtime_invoke` "không return data" theo nghĩa
đồng bộ. Return đồng bộ đúng là `IPromise<T>`; `T` chỉ được publish về sau.
Failure hiện tại có xác suất cao nằm ở assumption rằng object trả về luôn là
`Niantic.Promises.Promise<T>` với đúng bốn private field đó. Interface wrapper,
`PromiseKeeper<T>`, `RpcPromise<T>`, parent khác, obfuscation/layout khác, hoặc
completion được forward qua continuation đều làm đường đọc field fail dù RPC
vẫn hoàn tất.

### Managed callback adapter không cần native thunk

`Then` nhận một **managed `System.Action<T>` object**, không nhận native function
pointer. Tuy nhiên không cần tự dựng raw delegate: BCL đã có
`System.Collections.Queue.Enqueue(object)`, là managed instance method có một
tham số reference type. `System.Delegate.CreateDelegate(Type, target,
"Enqueue", false, true)` có thể bind exact closed `Action<T>` mà actual `Then`
parameter yêu cầu vào method đó theo reference-parameter compatibility.

Exact delegate `System.Type` không được đoán từ generic layout. Adapter lấy
thẳng `Il2CppType*` của parameter trên actual `Then(Action<T>)`, đổi thành
`System.Type` bằng `type_get_object`, và tạo delegate bằng exact overload năm
tham số của `Delegate.CreateDelegate`. Một Queue khác nhận
`Error(Action<string>)`. Mỗi target được bọc bằng
`Queue.Synchronized(Queue)` để enqueue/poll không phụ thuộc callback thread.
Native giữ Queue và delegate bằng strong GC handles, rồi poll
`Queue.Count`/`Peek` trên Unity main thread. Queue giữ payload managed sống cho
tới khi pending action được terminalize và cleanup.

Cách này không cần raw delegate layout, native callback thunk, generic method
RVA, `MethodInfo` cast hay private Promise field. Điều còn phải calibrate trên
device là runtime Mono/BCL có chấp nhận binding contravariant này cho tất cả
closed `Action<TOutProto>` và callback-after-terminal có replay đúng contract.

### Bằng chứng Ghidra bổ sung

Đã thử lại wrapper `scripts/rea-ghidra-project.sh` với project đã analyze và
profile `existing-project-read-only`. Project gốc không bị sửa. Lượt dossier
Promise mới chưa chạy được vì helper ownership của REA bị toolchain macOS
Swift/SDK mismatch (đồng thời sandbox chặn default Clang cache). Snapshot cũ vẫn
chứng minh exact artifact/profile, nhưng không chứa exact generic Promise
function boundary; do đó không nâng generic `Then`/delegate ABI thành fact.

### Kết luận bước 3

- Không có "invoke lần hai" nào tự trả `T`; cần continuation hoặc một observation
  channel khác.
- Đường field-poll hiện tại là implementation probe, không phải Promise
  contract, và chính là candidate lỗi chính.
- `Then(Action<T>)` là result channel đúng semantics và có thể dùng mà không
  fabricate delegate: managed Queue đóng vai callback target/lifetime boundary.
- Exact class check của payload và authoritative gameplay postcondition vẫn là
  hai gate riêng; callback result không tự chứng minh mutation đã có hiệu lực.

## Bước 4 — Luồng hành vi và điểm tích hợp

### Luồng thay thế đã chọn

```text
verified dispatch
  -> retain Promise GC handle
  -> resolve exact Then(Action<TOut>) + Error(Action<string>) on actual class
  -> create two synchronized managed Queues and exact managed delegates
  -> retain Queue/delegate GC handles
  -> Promise invokes Queue.Enqueue(result or error)
  -> poll Queue.Count/Peek on Unity main thread
  -> verify exact payload class and decode status/result
  -> require existing authoritative postcondition before final success
  -> free Promise + Queue + delegate handles on every terminal path
```

Điểm khác với code cũ là Promise chỉ được dùng qua public continuation contract.
Native không còn đọc `completeCalled`, `errorCalled`, `completedValue` hoặc
`errorValue`. Payload `TOut` vẫn được nhận đầy đủ, nhưng được giữ bởi managed
Queue thay vì dựa vào concrete `Promise<T>` layout.

### Correlation theo feature

| Feature | Callback payload | Postcondition authoritative | Verdict |
|---|---|---|---|
| Catch | exact `CatchPokemonOutProto`, gồm status và `CapturedPokemonId` | encounter/map/storage lifecycle hiện có vẫn quyết định postcondition | wrong type/status hoặc thiếu postcondition: indeterminate/failure, không retry mù |
| Transfer | exact `ReleasePokemonOutProto` | fresh storage/inventory authority phải chứng minh ID đã mất | proto success nhưng state chưa xác minh: chưa được coi completed |
| Discard | exact `RecycleItemOutProto`; reconcile nhận `GetHoloholoInventoryOutProto` | fresh full-inventory count/revision phải khớp expected delta | mismatch/stale/ambiguous: indeterminate |

### Ownership và race

- Pending record giữ correlation/owner fields hiện có, Promise GC handle, bốn
  Queue/delegate GC handles và deadline.
- Owner/session đổi làm action cũ thành `INDETERMINATE`, nhả handle/lease và cấm
  callback hoặc refresh muộn complete action mới. Queue chỉ sống cùng pending
  action; callback muộn không có native pointer để dereference.
- Mọi delegate creation, Promise subscription, Queue poll và managed snapshot
  read tiếp tục chạy qua main-thread bridge.
- Một action chỉ được terminalize bằng compare-and-close theo action ID + owner
  generation; refresh response muộn không được ghi đè verdict.
- Error callback hiện phân loại rejection; string payload chưa cần copy ra native
  để quyết định correctness. Result status vẫn được decode như đường cũ.

### Điểm tích hợp trong repo

- Tách `shared/runtime/promise/runtime_promise_result_sink.inc` dùng chung; không
  để helper feature đọc private Promise fields.
- Catch/transfer/discard giữ observer riêng để sở hữu baseline, correlation và
  postcondition domain-specific.
- `runtime_capabilities` chỉ quảng bá mutation-ready khi completion barrier và
  postcondition của feature đều verified; result-proto diagnostics không được
  nâng thành success gate.
- Kotlin chỉ nhận structured terminal state/result provenance; không poll
  Promise hoặc diễn giải raw game state.

### Kết luận bước 4

Phương án thay thế là **public Promise callback -> managed Queue sink -> exact
payload verification -> authoritative reconciliation**. Nó giữ được raw result
mà vẫn bỏ dependency vào private Promise layout và generic/native ABI.

## Bước 5 — Hướng xử lý

### So sánh phương án

| Phương án | Ưu điểm | Nhược điểm / rủi ro | Verdict |
|---|---|---|---|
| A. Giữ poll bốn private field, thêm type/offset guard | Ít thay đổi; vẫn lấy raw proto khi layout đúng | Vẫn phụ thuộc concrete class/private layout; wrapper Promise tiếp tục làm fail | Chỉ giữ như diagnostic fallback, không làm correctness gate |
| B. `get_Completed()` + authoritative reconcile | Không cần result layout | Không có raw status/error; catch ID khó correlate; actual accessor/replay semantics vẫn phải calibrate | Fallback nếu Queue delegate không bind trên device |
| C. `Then(Action<T>)` qua managed Queue adapter | Nhận exact raw `T`; không cần native thunk/private field/generic RVA | Cần device calibration cho `CreateDelegate` compatibility và replay behavior | **Đã chọn và triển khai fail-closed** |
| D. Hook `Promise<T>.Complete`/RPC completion hoặc tự dựng delegate native | Có thể chặn mọi result | Hook global, generic sharing/RGCTX, raw layout và reentrancy rất rủi ro | Không chọn |

### Thiết kế đề xuất

1. Resolve duy nhất exact instance `Then(System.Action<TOut>)` và
   `Error(System.Action<System.String>)` từ actual Promise class/parent, gồm cả
   explicit-interface suffix; ambiguity làm subscription fail.
2. Lấy exact delegate type từ method parameter, tạo hai synchronized Queue và
   bind hai delegates bằng exact `Delegate.CreateDelegate` overload.
3. Root Queue/delegate/Promise bằng GC handles. Poll error queue trước result
   queue trên main thread và trả `WAITING`, `RESULT`, `ERROR`, `FAILED`.
4. Mỗi feature verify exact result class trước khi decode proto, rồi tiếp tục
   postcondition/state correlation hiện có. Timeout/ambiguity luôn fail closed.
5. Nhả toàn bộ handles khi abandon, reset, owner/session change, subscription
   failure hoặc terminal consumption. Không giữ private-field diagnostic path.

### Thứ tự migration

1. Adapter dùng chung đã được thêm trước feature modules trong single translation
   unit.
2. Encounter/direct catch/spin, transfer, discard và discard reconcile đã chuyển
   sang sink; bốn private-field name không còn trong `zygisk/jni`.
3. Exact result class/status decoding và authoritative postcondition hiện có được
   giữ nguyên; capability gate không được tự mở bởi host build.
4. Device calibration là phase kế tiếp; nếu delegate binding fail, action trả
   `INDETERMINATE` và capability vẫn fail closed.

### Vì sao không cần device để quyết định kiến trúc

Static metadata đủ để chứng minh `runtime_invoke` trả `IPromise<T>`, public API
không có `get_Result`, và exact public result channel là `Then(Action<T>)`.
Nó cũng đủ để xây adapter fail-closed mà không đoán offset/RVA.

Device chỉ cần ở calibration cuối để chứng minh actual Promise implementation
chấp nhận Queue delegate, callback chạy/replay đúng, object lifetime và các
postcondition thật. Nó không cần để quyết định kiến trúc hay build code.

### Kết luận bước 5

Triển khai C bằng managed Queue sink, giữ B là fallback kiến trúc nếu runtime
calibration bác delegate compatibility, bỏ A/D. Raw payload vẫn không được thay
thế authoritative postcondition.

## Bước 6 — Tiêu chí chấp nhận và xác minh

### Acceptance cho implementation

- [x] Success path của catch/transfer/discard không đọc
  `completeCalled`, `errorCalled`, `completedValue` hoặc `errorValue`.
- [x] Adapter resolve exact `Then(Action<TOut>)` và `Error(Action<string>)` từ
  actual class/parents, lấy delegate type từ parameter và reject ambiguity.
- [x] Delegate được tạo qua managed `Delegate.CreateDelegate`, queue được bọc
  bằng `Queue.Synchronized`; không có native thunk, raw delegate layout, direct
  RVA hay private `MethodInfo` cast.
- [x] Promise, Queue và delegates được giữ bằng GC handle; mọi
  timeout, owner change, STOP và exception đều nhả handle đúng một lần.
- [x] Poll và managed invoke chạy trên existing main-thread paths; result phải
  pass exact class check trước khi decode.
- [x] Existing result status và authoritative postcondition/correlation gate
  được giữ; subscription/poll mismatch trả fail-closed.
- [ ] BlueStacks Air 1 chứng minh delegate binding/replay, callback thread,
  owner/session cleanup và postcondition ở runtime.

### Verify trên host trước device

1. Static review exact resolver signature, GC cleanup paths, state transitions
   và capability composition.
2. Chạy existing native protocol/build checks liên quan; không tạo test mới.
3. Chạy `./gradlew test assembleDebug --rerun-tasks` khi môi trường cho phép.
4. Chạy native/Magisk build qua `scripts/build-magisk.sh`, rồi
   `git diff --check` và line-count audit.
5. Giữ mutation capability ở `UNVERIFIED_DEVICE`; host pass không đổi thành
   `LIVE_READY`.

### Handoff device tối thiểu

Chỉ sau static pass, người dùng manual test trên **BlueStacks Air 1** bằng các
script repo. Diagnostic cần ghi actual Promise class, exact `Then/Error`
signature, delegate/queue subscription, owner generation, poll transition,
baseline/fresh revision, postcondition và final provenance. Nếu test thất bại
mới đọc log do
`collect-binding-diagnostics.sh`/`logcat-full.sh` tạo; không dùng raw ADB.

Calibration phải chứng minh riêng:

- `Delegate.CreateDelegate` bind được `Action<TOut>` vào `Queue.Enqueue(object)`;
- success queue nhận exact `TOut`, error queue nhận string và Promise terminal
  trước subscription vẫn replay callback đúng;
- owner/session đổi invalidates pending action;
- transfer/discard postcondition không báo success trên stale/unchanged state;
- catch không cấp storage ID khi diff ambiguous;
- timeout không retry mutation mù.

### Evidence state sau phân tích

- Canonical bundle đã export lại: 35 records, 4 unknown revisions, SHA-256
  `62249426b6119f4f74a4490e2ba2202704de362fda2f6e1dce837a37efaa55bf`.
- Promise auxiliary Evidence: `ev_0c4a9079...`, `ev_3bdab983...`; luôn giữ
  limitation đây là Cpp2IL-derived DLL, không phải shipped managed assembly.
- Exact global metadata mapping vẫn là authority cho signature/token; adapter
  không gọi generic RVA. Actual runtime class/delegate behavior còn
  `UNVERIFIED_DEVICE`.
- Lượt Ghidra Promise dossier mới bị chặn bởi macOS Swift compiler/SDK mismatch;
  project existing được dùng read-only qua copy-on-write, project gốc không đổi.

### Kết luận cuối

Thay đường private-field polling bằng public `Then/Error` và managed Queue sink
là hướng đủ static evidence và phù hợp kiến trúc hiện tại. Nó nhận được raw `T`
mà không phụ thuộc Promise layout hoặc generic/native ABI. Runtime calibration
và authoritative gameplay postcondition vẫn là điều kiện bắt buộc trước khi coi
capability là live-ready.
