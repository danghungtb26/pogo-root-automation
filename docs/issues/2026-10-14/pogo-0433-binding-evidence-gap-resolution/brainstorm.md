# Brainstorm: Khép kín evidence gap cho binding Pokémon GO 0.433.0

**Loại:** architecture
**Ngày phân tích đầu tiên:** 2026-10-14
**Phạm vi game/build/ABI:** Pokémon GO `0.433.0`, versionCode `2026100702`,
Android arm64-v8a, Unity `6000.3.22f1`, IL2CPP metadata v39

## Bước 1 — Yêu cầu và phạm vi

### Vấn đề cần giải quyết

Focused metadata map hiện đã nối managed method token tới RVA trong
`libil2cpp.so`, nhưng chưa đủ evidence để gọi an toàn các binding phục vụ Phase
3–5. Sáu nhóm claim còn thiếu là: exact native boundary, ABI, parameter/return
types, runtime owner/lifetime, Promise/result và runtime postcondition.

### Kết quả mong muốn

- Chia mỗi unknown thành một probe nhỏ có đầu vào, output và tiêu chí đạt rõ.
- Xác định phần nào giải được offline bằng metadata/Ghidra/REA và phần nào bắt
  buộc xác minh read-only trên process thật.
- Giữ `DIRECT_CATCH`, transfer/discard và Virtual GO Plus fail-closed cho tới
  khi toàn bộ evidence bắt buộc của capability tương ứng đạt.
- Tạo đường đi tăng dần từ mapping hiện có tới binding profile có guards, không
  dùng layout 0.427.0 hoặc gọi mutation để “thử xem”.

### Ràng buộc

- Artifact canonical là cặp `libil2cpp.so` + `global-metadata.dat` 0.433.0 đã
  pin hash; REA Evidence là authority cho claim reverse.
- Project Ghidra gốc chỉ được đọc qua snapshot/copy; mọi retype/split function
  phải diễn ra trên clone có provenance.
- Runtime survey chỉ đọc cho tới khi exact binding, identity, thread và lifetime
  guards đã đạt; không dispatch catch/transfer/discard trong bước calibration.
- Không thêm test file, không dùng raw ADB, không khôi phục Il2CppDumper hoặc
  script reverse cũ.

### Thuộc tính chất lượng ưu tiên

1. Fail-closed và không làm thay đổi game state trong calibration.
2. Evidence tái lập theo exact build/ABI.
3. Tách static fact, runtime observation và inference.
4. Binding profile nhỏ, version-scoped; domain/runtime scheduling không phụ
   thuộc tên/layout riêng của build.

### Giả định và câu hỏi còn thiếu

- CodeRegistration `0xe235508` và MetadataRegistration `0xe6378c8` đã được
  xác định cho đúng artifact, nhưng cần giữ provenance trong mỗi output mới.
- Chưa xác định exact native calling convention thực tế ở ba mutation path,
  owner acquisition path, main-thread requirement và terminal state source.
- Chưa biết generated managed assembly có đủ signature/generic metadata cho
  mọi method hay cần parser metadata v39 mở rộng.

## Bước 2 — Bằng chứng từ reverse

### Identity và nguồn đã kiểm

| Nguồn | Identity / Evidence | Authority | Phát hiện | Coverage / limitation |
|---|---|---|---|---|
| `native/libil2cpp.so` | SHA-256 `6b1e11619362ddd53f9dbcd7be1d4296bdec5dda27b9f742c16b8403359c25a0` | shipped artifact | ELF arm64 đúng build; 10/10 entry trong `SHA256SUMS` đã pass lại | Static bytes, không quan sát runtime |
| `native/global-metadata.dat` | SHA-256 `f5961a8548256da287b4f79466a169c302b90ec59be7f6461e8e58b482c0fb0d` | shipped artifact | metadata v39, ghép đúng ELF trên | Không tự cho biết native ABI/lifetime |
| `pogo-0.433.0-il2cpp-mapping.json` | SHA-256 `8db8b2e...`; Cpp2IL layout reference commit `b5ad444...` | deterministic derived artifact | 219 images/modules, 46.636 type, 294.327 method, 583 row lọc Phase 3–5 | Token→RVA và arity/slot; chưa có parameter type/native ABI/generic instantiation |
| Existing Ghidra project | `ev_51dac350...` | REA/Ghidra 12.1.3, observed | `MapPokemon.TryCapture` mapped RVA `0x9e1caa4` nằm trong analyzed code | Ghidra đang gộp vào `FUN_09e1c6cc`, range `0x9e1c6cc..0x9e1cf73`; chưa phải exact boundary |
| Existing Ghidra project | `ev_7f6095cf...` | REA/Ghidra 12.1.3, observed | `PokemonBagImpl.ReleasePokemon` mapped RVA `0xa04cf68` nằm trong analyzed code | Gộp vào `FUN_0a04ce80`, range `0xa04ce80..0xa04cfff` |
| Existing Ghidra project | `ev_d768040...` | REA/Ghidra 12.1.3, observed | `ItemBagImpl.RecycleItem` mapped RVA `0xa006bec` nằm trong analyzed code | Gộp vào `FUN_0a006ba8`, range `0xa006ba8..0xa006c63` |
| Generated `holo-game.dll` | `ev_66a9d9de...` | derived auxiliary; REA managed parser | Assembly metadata hợp lệ, inspection complete | Không phải shipped target; không execute/decompile/resolve CLR dependency; không được tự dùng để bật capability |

Bundle canonical import lại thành công với 26 records và 4 unknown revisions.
Native residual unknown `unk_27690a...` đang ở revision 3/`investigating`.

### Managed identities đã tìm thấy trong derivation

Các signature dưới đây xuất hiện nhất quán trong Cpp2IL Diffable C# sinh từ
đúng cặp artifact. Chúng là hypothesis mạnh để áp type vào Ghidra clone, chưa
phải runtime/call-path evidence:

| Managed method | Token / slot / arity | RVA | Signature candidate |
|---|---|---:|---|
| `Niantic.Holoholo.Map.MapPokemon.TryCapture` | `0x06013ab5` / 44 / 2 | `0x9e1caa4` | `IPromise<CatchPokemonOutProto> TryCapture(PokeballThrow throwData, ARPlusEncounterValuesProto arEncounterValues)` |
| `Niantic.Holoholo.Internal.PokemonBagImpl.GetPokemon` | `0x06002a39` / 12 / 1 | `0xa04bd58` | `PokemonProto GetPokemon(ulong id)` |
| `Niantic.Holoholo.Internal.PokemonBagImpl.ReleasePokemon` | `0x06002a43` / 19 / 1 | `0xa04cf68` | `IPromise<ReleasePokemonOutProto> ReleasePokemon(PokemonProto pokemon)` |
| `Niantic.Holoholo.Internal.ItemBagImpl.GetItemCount` | `0x0600295b` / 9 / 1 | `0x9ffe438` | `int GetItemCount(Item itemType)` |
| `Niantic.Holoholo.Internal.ItemBagImpl.RecycleItem` | `0x06002971` / 23 / 3 | `0xa006bec` | `IPromise<RecycleItemOutProto> RecycleItem(ItemData itemData, int numToRecycle, ISet<Item> expiringItemsCopy)` |

### Evidence gap được phân loại lại

- **Đã có:** exact build identity, managed owner/name/token/slot/arity, candidate
  parameter/return types, method-pointer RVA và code containment.
- **Có thể giải offline tiếp:** exact entry/boundary, AArch64 register assignment,
  MethodInfo tail parameter, caller/callee, Promise construction và callback/
  continuation path.
- **Bắt buộc runtime read-only:** instance owner acquisition, owner/scene/session
  lifetime, main-thread condition, concrete Promise state transitions và object
  identity correlation.
- **Bắt buộc một calibration mutation có kiểm soát sau khi mọi guard đạt:** server
  outcome và map/storage postcondition. Static hoặc passive runtime không thể
  chứng minh action thật đã hoàn tất.

## Bước 3 — Function/method, param và hành vi

Các register ABI dưới đây là **candidate** theo IL2CPP arm64/AAPCS64 cho instance
method (`x0=this`, managed args tiếp theo, hidden `MethodInfo*` cuối). Chỉ được
nâng thành binding sau khi caller/value-flow trên clone xác nhận register use và
hidden argument tại đúng RVA.

### `Niantic.Holoholo.Map.MapPokemon.TryCapture` — `PARTIAL_STATIC`

- Nguồn: mapping từ artifact pair; RVA `0x9e1caa4`; dossier
  `ev_51dac350...`; build/ABI: 0.433.0/arm64.
- Signature candidate:
  `IPromise<CatchPokemonOutProto> TryCapture(PokeballThrow throwData, ARPlusEncounterValuesProto arEncounterValues)`.
- Boundary candidate: entry chính xác từ codegen method pointer là
  `0x9e1caa4`; method pointer kế tiếp của type là `PlaySpawnFx` tại
  `0x9e1d440`. Đây chỉ là upper bound, không phải body range.

| Param | Kiểu / modifier | Nguồn và ý nghĩa | ABI candidate | Trạng thái |
|---|---|---|---|---|
| receiver | `MapPokemon*` | live source object đã qua eligibility | `x0` | runtime owner/lifetime chưa xác minh |
| `throwData` | `PokeballThrow` value type | ball type, reticle, bullseye/spin/miss; layout candidate 12 byte | `x1`/`x2` hoặc aggregate ABI khác | phải xác nhận bằng native caller; không giả là object pointer |
| `arEncounterValues` | `ARPlusEncounterValuesProto*` | AR+ proximity/awareness/fear; nullable chưa xác minh | register kế sau aggregate | phải xác nhận callsite/null path |
| hidden method | `MethodInfo*` candidate | IL2CPP metadata context | register cuối | chưa xác nhận |

- Return candidate: managed pointer tới `IPromise<CatchPokemonOutProto>`.
- Terminal payload candidate: `CatchPokemonOutProto.Status` tại field offset
  `0x10`; `CatchSuccess=1`, `CatchEscape=2`, `CatchFlee=3`, `CatchMissed=4`,
  `CatchItemReplacement=5`; `CapturedPokemonId` tại offset `0x20`.
- Chưa xác minh: exact value-type ABI của `PokeballThrow`, nullability, Promise
  concrete class/vtable, callback thread, map-removal ordering và server status
  correlation.

### `Niantic.Holoholo.Internal.PokemonBagImpl.GetPokemon` — `PARTIAL_STATIC`

- Nguồn: mapping, token `0x06002a39`, slot 12, RVA `0xa04bd58`.
- Signature candidate: `PokemonProto GetPokemon(ulong id)`.

| Param | Kiểu / modifier | Nguồn và ý nghĩa | ABI candidate | Trạng thái |
|---|---|---|---|---|
| receiver | `PokemonBagImpl*` | bag instance | `x0` | owner acquisition/lifetime chưa xác minh |
| `id` | `ulong` | storage Pokémon ID | `x1` | cần caller corroboration |
| hidden method | `MethodInfo*` candidate | IL2CPP metadata context | `x2` | chưa xác nhận |

- Return candidate: `PokemonProto*` trong `x0`, có thể null khi không tìm thấy;
  null behavior chưa xác minh.
- Boundary upper bound: method kế tiếp `TryGetPokemon` ở `0xa04bdd4`.

### `Niantic.Holoholo.Internal.PokemonBagImpl.ReleasePokemon` — `PARTIAL_STATIC`

- Nguồn: mapping; RVA `0xa04cf68`; dossier `ev_7f6095cf...`.
- Signature candidate:
  `IPromise<ReleasePokemonOutProto> ReleasePokemon(PokemonProto pokemon)`.

| Param | Kiểu / modifier | Nguồn và ý nghĩa | ABI candidate | Trạng thái |
|---|---|---|---|---|
| receiver | `PokemonBagImpl*` | bag service instance | `x0` | runtime owner/lifetime chưa xác minh |
| `pokemon` | `PokemonProto*` | object trả từ bag lookup, không phải ID raw | `x1` | identity/freshness cần guard |
| hidden method | `MethodInfo*` candidate | IL2CPP metadata context | `x2` | chưa xác nhận |

- Return candidate: `IPromise<ReleasePokemonOutProto>*`.
- Terminal payload candidate: `ReleasePokemonOutProto.Status` offset `0x10`;
  `Success=1`, các lỗi deployed/egg/buddy/fusion có mã riêng. Postcondition cần
  chứng minh Pokémon ID biến mất khỏi bag revision mới, không chỉ Promise resolve.
- Boundary upper bound: `EvolvePokemon` ở `0xa04d60c`.

### `Niantic.Holoholo.Internal.ItemBagImpl.GetItemCount` — `PARTIAL_STATIC`

- Nguồn: mapping, token `0x0600295b`, slot 9, RVA `0x9ffe438`.
- Signature candidate: `int GetItemCount(Holoholo.Rpc.Item itemType)`.
- ABI candidate: `x0=this`, `w1=itemType`, `x2=MethodInfo*`, return `w0`.
- Boundary upper bound: `GetItem` ở `0x9ffe52c`.
- Chưa xác minh: enum domain validation, stale inventory behavior và main-thread
  requirement.

### `Niantic.Holoholo.Internal.ItemBagImpl.RecycleItem` — `PARTIAL_STATIC`

- Nguồn: mapping; RVA `0xa006bec`; dossier `ev_d768040...`.
- Signature candidate:
  `IPromise<RecycleItemOutProto> RecycleItem(ItemData itemData, int numToRecycle, ISet<Item> expiringItemsCopy)`.

| Param | Kiểu / modifier | Nguồn và ý nghĩa | ABI candidate | Trạng thái |
|---|---|---|---|---|
| receiver | `ItemBagImpl*` | item bag service instance | `x0` | owner/lifetime chưa xác minh |
| `itemData` | `ItemData*` | candidate là nested inventory UI/service DTO, không phải protobuf `ItemProto` | `x1` | exact declaring type/token và constructor path phải xác nhận |
| `numToRecycle` | `int` | số lượng cần xóa | `w2` | phải guard `>0` và `<= fresh count` |
| `expiringItemsCopy` | `ISet<Item>*` | snapshot item sắp hết hạn từ inventory service | `x3` | exact owner/source/freshness chưa xác minh |
| hidden method | `MethodInfo*` candidate | IL2CPP metadata context | `x4` | chưa xác nhận |

- Return candidate: `IPromise<RecycleItemOutProto>*`.
- Terminal payload candidate: `RecycleItemOutProto.Result` offset `0x10`;
  `Success=1`, `ErrorNotEnoughCopies=2`, `ErrorCannotRecycleIncubators=3`;
  `NewCount` offset `0x14`.
- Postcondition phải đối chiếu bag revision mới và `GetItemCount`; Promise success
  một mình chưa đủ.
- Boundary upper bound: `PredictUseItem` ở `0xa007574`.

### Promise contract — `PARTIAL_DERIVED`

Generated metadata có `IPromise<T>.Then(Action<T>)`, `Error(Action<string>)`,
`Finally(Action)` và `PromiseStatus` (`InProgress=0`, `CompletedFailed=1`,
`CompletedSucceeded=2`). Chưa có REA native dossier cho concrete Promise
implementation/vtable hoặc callback dispatch. Probe cần resolve chính xác
`Then/Error/Finally`, callback delegate construction, GC handle lifetime và
thread thực thi trước khi runtime theo dõi terminal result.

## Bước 4 — Luồng hành vi và điểm tích hợp

### Luồng nâng evidence theo ba gate

1. **Offline clone gate**
   - Input: exact metadata map + snapshot clone của project Ghidra.
   - Tạo function entry tại từng codegen method pointer, nhưng chỉ sau khi xóa/
     split function body bị gộp trên clone; không ghi project gốc.
   - Áp prototype candidate và datatype tối thiểu; decompile lại, kiểm register
     use, mọi return edge, caller/callee và Promise construction.
   - Output: dossier mới cho exact entry, body ranges, ABI register map và call
     path. Nếu control flow chồng lấn hoặc thunk/shared body không tách được,
     giữ `PARTIAL_STATIC` thay vì ép function.

2. **Passive runtime calibration gate**
   - Runtime chỉ resolve metadata và quan sát: `class_from_name`, exact method
     token/name/param types, method pointer bằng `module_base + RVA`, concrete
     owner class, scene/session generation và thread ID.
   - Dùng hành động **do người dùng tự thực hiện trong game** để hook/log entry,
     args, returned Promise class và callback/result; diagnostic không tự gọi
     `TryCapture`, `ReleasePokemon` hay `RecycleItem`.
   - Pointer game chỉ giữ trong main-thread task hoặc GC handle có generation;
     scene/session đổi thì invalidate toàn bộ.

3. **Controlled mutation/postcondition gate**
   - Chỉ mở sau khi hai gate trên pass và người dùng chủ động arm một lần.
   - Dispatch trên Unity main thread qua bridge hiện có, retain Promise bằng GC
     handle, parse terminal proto, rồi đợi world-state revision mới.
   - `SUCCESS` chỉ khi payload status thành công **và** postcondition tương ứng
     xuất hiện; timeout/disconnect/scene reset là `INDETERMINATE`, không retry.

### Luồng riêng theo action

#### Catch

`fresh MapSnapshot target` → resolve lại live source object cùng
`session/ownerGeneration/stable id` → xác minh exact class/subclass và
`TryCapture` contract → tạo `PokeballThrow` đúng value-type ABI → invoke main
thread → retain `IPromise<CatchPokemonOutProto>` → đọc `Status` và
`CapturedPokemonId` → đợi target biến mất khi status success/flee và inventory
revision đổi khi success → phát terminal event.

Các cạnh từ source type tới live `MapPokemon` và điều kiện SendEncounterRequest
vẫn chưa được REA call-path chứng minh; vì vậy chưa thể bỏ qua encounter setup
hoặc cast mọi source về `MapPokemon`.

#### Transfer

`captured storage ID có provenance` → `GetPokemon(id)` trên main thread → xác
minh returned class/id và keep policy từ snapshot fresh → `ReleasePokemon`
→ retain Promise → đọc `ReleasePokemonOutProto.Status` → đợi ID biến mất khỏi
bag revision mới. Status `Success` mà bag vẫn chứa ID là `INDETERMINATE`, không
báo transferred.

#### Discard

`fresh inventory row` → lấy exact `ItemData` type/constructor hoặc reuse DTO từ
`ItemInventoryService` → lấy `ExpiringItemsCopy` cùng owner generation → kiểm
item/count/recyclable/expiration fields → `RecycleItem` → retain Promise → đọc
`Result/NewCount` → đợi `GetItemCount`/inventory snapshot bằng `NewCount`.

### Điểm tích hợp source hiện có

| Trách nhiệm | Source hiện có | Cần thay đổi sau khi evidence đạt |
|---|---|---|
| Main-thread marshal | `shared/runtime/mainthread/runtime_main_thread_bridge.inc` | Giữ nguyên ownership; thêm task calibration chỉ đọc và generation checks |
| Owner resolution | `shared/runtime/probe/runtime_probe_service_owners.inc`, `runtime_probe_owners.inc` | Thay field/name legacy bằng exact class/type/token evidence; log method pointer đối chiếu RVA |
| Catch binding/result | `modules/catch_spin/direct_map_bindings.inc`, `direct_catch_promise_observer.inc` | Thay 0.427 assumptions bằng profile 0.433; không set verified chỉ vì name/arity match |
| Transfer | `modules/transfer/promise_observer.inc`, `release_promise_observer.inc` | Bổ sung owner/session/revision và bag-removal postcondition trước terminal success |
| Discard | `modules/discard/recycle_item_action.inc`, `promise_observer.inc` | Xác nhận exact nested `ItemData`, expiring-set source và NewCount reconciliation |
| Shared state | `shared/runtime/state/runtime_world_state.*` | Dùng immutable revision/freshness làm postcondition, không giữ managed pointer |

Code hiện đã có main-thread execution, GC handle và Promise polling scaffolding.
Tuy nhiên các field như `completeCalled`, `errorCalled`, `completedValue` và
owner resolution hiện là assumption từ binding cũ cho tới khi 0.433.0 passive
observation xác nhận. Không được coi scaffold hiện hữu là evidence của game.

### Nhánh fail-closed

- RVA/pointer mismatch, param type mismatch hoặc MethodInfo mismatch: capability
  `UNAVAILABLE`, không fallback sang arity-only.
- Owner khác exact concrete class, scene/session generation đổi hoặc pointer
  ngoài managed heap: bỏ snapshot/pending/GC handle.
- Promise concrete layout không khớp: dùng callback API đã xác minh hoặc giữ
  `INDETERMINATE`; không đọc offset cũ.
- Payload success nhưng postcondition không tới trước deadline: blocked session,
  không blind retry.
- Runtime observation của manual action khác static call path: static inference
  bị hạ cấp và quay lại focused clone analysis.

## Bước 5 — Hướng xử lý

### So sánh phương án

| Phương án | Ưu điểm | Nhược điểm / điều bị loại trừ | Kết luận |
|---|---|---|---|
| A. Chỉ dùng runtime reflection theo name/arity | Nhanh, không cần sửa Ghidra | Không chứng minh native pointer/ABI/call path; dễ bind nhầm overload hoặc owner stale | Không đủ để mở mutation |
| B. Gọi trực tiếp `module_base + RVA` | Tránh metadata lookup lúc chạy | Nguy hiểm nhất: value-type ABI/MethodInfo/thunk/lifecycle sai có thể crash hoặc mutate sai; khó guard | Loại bỏ |
| C. Writable Ghidra clone + metadata/type overlay, sau đó passive runtime correlation | Tái lập, tạo exact dossier và vẫn đối chiếu được object/lifetime thật | Cần thêm tooling import/split và một vòng user calibration | **Khuyến nghị** |

### Lộ trình đề xuất

#### Work package 1 — Nâng mapper thành binding manifest typed

Mở rộng output hiện tại thay vì tạo pipeline khác. Với mỗi method cần ghi:

- image/namespace/type/method/token/slot/arity/RVA;
- exact parameter type token/name, value/reference type, size/alignment candidate;
- return type token/name;
- declaring type parent/interface và method pointer kế trước/kế sau;
- provenance hash, metadata version, registration addresses và limitation.

Output mới chỉ là static manifest, không được tự đặt `verified=true`. Đặc biệt
phải resolve exact nested identity của `ItemData` và layout/value-type của
`PokeballThrow`.

#### Work package 2 — Tạo disposable analyzed clone có symbol/type

Từ `.gpr/.rep` hiện có, tạo clone riêng cho 0.433.0. Import manifest theo
transaction:

1. kiểm Program SHA-256 và image base;
2. đặt label tại exact method pointer;
3. split/create function chỉ khi disassembly/control-flow hợp lệ và không chồng
   body ngoài upper bound;
4. áp prototype candidate kèm comment provenance, không áp type khi ABI còn mơ
   hồ;
5. chạy focused analysis/decompile lại chỉ vùng liên quan;
6. mở clone read-only qua REA và tạo dossier/call-path Evidence mới.

Project gốc không thay đổi. Clone là analysis artifact, không thay thế shipped
ELF trong Evidence subject.

#### Work package 3 — Khép ABI và Promise path offline

Cho từng entry:

- kiểm prologue/epilogue và data flow của `x0..x4`/`w0..w2`;
- tìm callers dùng cùng register layout và hidden `MethodInfo*`;
- xác nhận `PokeballThrow` được truyền split-register/stack hay qua address;
- resolve concrete Promise allocation/constructor/vtable và các write tới
  completion/result fields;
- dựng call path có frontier rõ từ caller → action method → RPC/Promise callback.

Chỉ khi function dossier cùng artifact/provider xác nhận mới ghi native ABI vào
binding profile.

#### Work package 4 — Thêm diagnostics-only runtime calibration

Thêm capability nội bộ mới, đề xuất — chưa tồn tại:

```cpp
struct RuntimeBindingCalibration {
    uint64_t session_generation;
    uint64_t owner_generation;
    uintptr_t module_base;
    uintptr_t resolved_method_pointer;
    uint64_t managed_token;
    bool main_thread;
    bool exact_owner_class;
    bool pointer_matches_profile_rva;
};
```

Diagnostic chỉ resolve/compare/log; không gọi mutation. Với manual actions của
người dùng, hook quan sát entry/return và Promise transition có bounded event
buffer, không giữ raw object qua frame. Kết quả cần ghi vào structured runtime
status/event để có session/freshness, không chỉ log text.

#### Work package 5 — Calibration mutation có user arm

Sau WP1–4 pass, mở lần lượt từng capability, không mở cả ba cùng lúc:

1. Catch với một target và Poké Ball 1, kiểm result + captured ID + map/inventory.
2. Transfer một Pokémon do bước catch vừa xác nhận, kiểm status + bag removal.
3. Discard một item recyclable với amount 1, kiểm result/NewCount + inventory.

Mỗi action dùng một pending arbiter, timeout thành `INDETERMINATE`, không retry
và reset khi session/owner generation đổi. Virtual GO Plus là investigation
riêng; không dùng direct-catch evidence để mở fallback.

### Thứ tự gỡ blocker Phase 3–5

- **Phase 3:** WP1–3 hoàn tất static contract; WP4 xác minh owner/lifetime.
- **Phase 4:** sau owner/type/event evidence, thay typed collectors và dùng
  world-state revision làm postcondition source.
- **Phase 5:** chỉ sau Phase 4 snapshot/revision hoạt động mới làm WP5 catch;
  Promise success không được đi trước world-state reconciliation.

Đây là đường ngắn nhất mà vẫn an toàn: tận dụng mapper, Ghidra project, REA
Evidence và main-thread bridge hiện có; không viết lại REA và không gọi RVA trực
tiếp từ binding chưa được xác minh.

## Bước 6 — Tiêu chí chấp nhận và xác minh

### Tiêu chí chấp nhận

| ID | Nguồn yêu cầu | Điều kiện đầu vào | Hành vi / postcondition | Method liên quan | Cách kiểm chứng |
|---|---|---|---|---|---|
| `AC-01` | Exact boundary | Pinned ELF/metadata và writable clone | Mỗi method pointer là exact entry; body ranges không chồng sai và có upper bound/call-flow rõ | Năm method ở Bước 3 | REA `analyze_function`, assembly/basic-block/reference facets trên clone |
| `AC-02` | ABI | Typed manifest + exact entry | Register/stack/hidden `MethodInfo*` và return register được caller/callee corroborate; `PokeballThrow` value ABI không còn unknown | `TryCapture`, storage methods | REA value-flow + ít nhất một compatible caller dossier cùng provider/profile |
| `AC-03` | Parameter types | Metadata v39 đúng hash | Exact type token/name/value/reference/nullability cần thiết được ghi, gồm nested `ItemData` và `ISet<Item>` | Tất cả | Deterministic manifest regeneration + managed metadata inspection; không chỉ Diffable C# text |
| `AC-04` | Owner lifetime | Diagnostic-only runtime trên exact build | Owner concrete class, acquisition source, main thread, session/scene/owner generation và invalidation được quan sát | Map/bag/inventory services | Structured read-only calibration events; pointer/profile RVA correlation |
| `AC-05` | Promise/result | User tự thực hiện manual action hoặc calibration đã arm | Concrete Promise type, retention, completion/error/value path và terminal proto field được quan sát | Catch/release/recycle | Passive hook first; exact class/field/token and GC-handle lifetime evidence |
| `AC-06` | Runtime postcondition | Terminal proto success | State revision mới xác nhận map removal/captured ID, bag removal hoặc item NewCount; không báo success chỉ từ dispatch | Catch/release/recycle | World-state before/after correlation cùng session/generation/freshness |
| `AC-07` | Fail-closed | Bất kỳ mismatch/timeout/reset | Capability giữ `UNAVAILABLE` hoặc action thành `INDETERMINATE`; không retry hoặc fallback sau dispatch | Runtime capability/arbiter | Existing host checks, static gate review và manual negative cases |
| `AC-08` | Canonical evidence | Mỗi probe hoàn tất | Bundle export deterministic chứa artifact SHA, provider/profile, Evidence ID, authority, coverage và limitation | Reverse workflow | REA import/export/compare; ledger và Phase checklist được cập nhật |

### Definition of done theo từng unknown

| Unknown | Được coi là giải quyết khi | Chưa đủ nếu chỉ có |
|---|---|---|
| Exact boundary | exact entry + complete body AddressSet/control-flow trên clone + dossier | RVA nằm trong một `FUN_*` lớn |
| ABI | parameter/register/stack/return/hidden arg được value-flow và caller corroborate | managed signature hoặc AAPCS64 convention chung |
| Parameter types | exact metadata type identity + native representation cần thiết | arity/token hoặc generated C# một mình |
| Owner lifetime | acquisition path + concrete instance + generation/reset observation | static class name hoặc một pointer snapshot |
| Promise/result | concrete Promise lifecycle + exact terminal payload/status | non-null return pointer hoặc dispatch ACK |
| Runtime postcondition | authoritative result cùng state revision before/after | Promise success hoặc log “invoked” |

### Kế hoạch xác minh

1. Offline: kiểm `SHA256SUMS`, regenerate typed manifest hai lần và so byte;
   import bundle canonical trước mọi REA session.
2. Clone analysis: áp labels/types/splits có transaction, mở lại bằng REA
   read-only, tạo dossier và call path; export snapshot/bundle mới.
3. Source/build: chạy native protocol checks liên quan, `scripts/build-magisk.sh`,
   `./gradlew test assembleDebug`, line-count audit và `git diff --check`.
4. Runtime: chỉ dùng target `BlueStacks Air 1` qua repository entrypoint. Chạy
   passive calibration trước; user thực hiện manual action, agent không tự
   dispatch mutation.
5. Controlled action: mỗi lần chỉ arm một capability và một action; ghi
   structured result + before/after revision. Khi manual test thất bại mới dùng
   `scripts/logcat-full.sh` để thu log.

`scripts/collect-binding-diagnostics.sh` hiện gọi raw `adb` và không dùng
`scripts/lib/emulator.sh`, nên chưa đáp ứng rule chọn đúng `BlueStacks Air 1`.
Không dùng script đó cho calibration cho tới khi người dùng yêu cầu sửa nó để
reuse selector/validation hiện có.

### Kết luận và câu hỏi còn mở

- Không cần đoán lại từ đầu: managed signatures, result enums/fields và method
  pointers đã có đủ để tạo typed static manifest.
- Blocker ưu tiên số một là exact `PokeballThrow` native ABI và function split
  của `TryCapture`; tiếp theo là concrete Promise layout và owner generation.
- Owner/lifetime và postcondition không thể khép kín chỉ bằng Ghidra. Chúng cần
  passive runtime observation, rồi một calibration action do người dùng arm.
- Chưa xác minh exact nested type token của `ItemData`, Promise concrete class,
  callback thread và source eligibility của từng map subtype.
- Capability tiếp tục đóng cho tới khi từng hàng `AC-01`–`AC-08` liên quan đạt;
  không có một Evidence đơn lẻ nào đủ mở Phase 5.
