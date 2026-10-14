# Brainstorm: Resolve IL2CPP method bằng metadata name hay RVA

**Loại:** design-api
**Ngày phân tích đầu tiên:** 2026-10-14
**Phạm vi game/build/ABI:** Pokémon GO `0.433.0`, arm64-v8a, IL2CPP metadata v39

## Bước 1 — Yêu cầu và phạm vi

### Contract đang đánh giá

Bên gọi là injected native runtime. Bên cung cấp method identity là IL2CPP
runtime inspection API (`class_get_methods`, `method_get_name`,
`method_get_param*`, `type_get_name`). Code hiện resolve instance method bằng
`klass + method name + parameter count + exact parameter type names`, sau đó
gọi qua `il2cpp_runtime_invoke`.

Yêu cầu là đánh giá contract này so với binding trực tiếp theo RVA đã derive từ
đúng cặp `global-metadata.dat` + `libil2cpp.so` 0.433.0.

### Câu hỏi cần trả lời

- Name/type resolution hiện tại bảo vệ được những lỗi nào và còn blind spot gì?
- RVA resolution mạnh/yếu hơn ở đâu, đặc biệt với ASLR, thunks, generic methods,
  value-type ABI và hidden `MethodInfo*`?
- Nên dùng một trong hai hay kết hợp chúng thành cross-check?
- Điều kiện nào đủ để set capability `*_verified` và gọi method an toàn?

### Ràng buộc

- Giữ build fingerprint, session/owner generation, main-thread bridge và
  fail-closed behavior.
- Không gọi trực tiếp một RVA chưa được REA/metadata/native ABI corroborate.
- Không dùng name/arity match một mình để mở mutation.
- Chi tiết build nằm trong native binding profile; module policy không phụ thuộc
  RVA hoặc obfuscated field name.

## Bước 2 — Bằng chứng từ reverse

### Artifact và method identity

Hash manifest đã pass lại 10/10 entry. Sau focused Evidence merge/export,
bundle canonical có 33 records, 2 unknown identities và 4 unknown revisions.

| Method 0.433.0 arm64 | Token | Slot | Arity | RVA | Native Evidence |
|---|---:|---:|---:|---:|---|
| `MapPokemon.TryCapture` | `0x06013ab5` | 44 | 2 | `0x9e1caa4` | `ev_51dac350...`: RVA nằm trong analyzed code; Ghidra boundary còn gộp |
| `PokemonBagImpl.GetPokemon` | `0x06002a39` | 12 | 1 | `0xa04bd58` | `ev_987116...`: interior instruction, không phải entry |
| `PokemonBagImpl.ReleasePokemon` | `0x06002a43` | 19 | 1 | `0xa04cf68` | `ev_7f6095cf...`: code containment, boundary còn gộp |
| `ItemBagImpl.GetItemCount` | `0x0600295b` | 9 | 1 | `0x9ffe438` | `ev_2dfe76...`: interior instruction, không phải entry |
| `ItemBagImpl.RecycleItem` | `0x06002971` | 23 | 3 | `0xa006bec` | `ev_d768040...`: code containment, boundary còn gộp |

Mapping áp dụng cho ELF SHA-256
`6b1e11619362ddd53f9dbcd7be1d4296bdec5dda27b9f742c16b8403359c25a0`
và metadata SHA-256
`f5961a8548256da287b4f79466a169c302b90ec59be7f6461e8e58b482c0fb0d`.
RVA là static build identity; runtime address phải là `module_load_base + RVA`
theo load image tương ứng, không phải cast RVA thành pointer tuyệt đối.

### Quan sát source framework hiện tại

- `find_runtime_method_with_two_parameters` enumerate tối đa 512 method trên
  đúng `Il2CppClass`, so exact method name, arity và hai chuỗi type name.
- Các helper được dùng để lấy `MethodInfo*`/method descriptor rồi gọi qua
  `il2cpp_runtime_invoke`; code hiện không cast method pointer sang native
  function signature cho các action này.
- `runtime_invoke` nhận managed argument array, tự xử lý boxing/unboxing,
  exception output và IL2CPP invocation semantics. Đây là lợi thế quan trọng
  so với direct native call, nhất là value type như `PokeballThrow`.
- Một số binding khác chỉ dùng `class_get_method_from_name(name, arity)` hoặc
  fallback hierarchy/name matching; mức guard không đồng nhất.
- API hiện có `method_get_name`, `method_get_param_count`, `method_get_param`,
  `type_get_name` và return-type inspection ở các contract khác, nhưng helper
  hai param chưa kiểm return type, instance/static, declaring class identity,
  token, flags hoặc expected method pointer/RVA.

### Phân loại bằng chứng

- **Đã xác nhận:** exact build has one mapped managed identity/RVA cho các method
  trên; source runtime hiện resolve bằng metadata và invoke qua IL2CPP API.
- **Suy luận có cơ sở:** name/type lookup sẽ tiếp tục hoạt động khi RVA đổi giữa
  build nhưng metadata identity giữ nguyên; RVA sẽ phát hiện method identity
  lệch dù tên/type tình cờ match.
- **Chưa xác minh:** runtime `MethodInfo` layout/pointer extraction, exact method
  token API, pointer equality với `base+RVA`, generic sharing/thunk behavior và
  type-name normalization cho nested/generic/byref types.

## Bước 3 — Function/method, param và hành vi

### `find_runtime_method_with_two_parameters` — hiện có

- Nguồn framework:
  `zygisk/jni/shared/runtime/probe/runtime_probe_owners.inc`.
- Signature:
  `const void *find_runtime_method_with_two_parameters(const Il2CppApi &api, Il2CppClass *klass, const char *method_name, const char *first_parameter_type_name, const char *second_parameter_type_name)`.

| Param | Kiểu | Ý nghĩa | Ràng buộc hiện tại |
|---|---|---|---|
| `api` | `const Il2CppApi&` | exported IL2CPP inspection functions | yêu cầu inspection, method enumeration và type-name API |
| `klass` | `Il2CppClass*` | declaring/concrete class cần scan | non-null; helper không tự walk parent |
| `method_name` | `const char*` | exact managed name | `strcmp`, không chấp nhận explicit interface suffix |
| `first_parameter_type_name` | `const char*` | exact string từ `type_get_name` | phụ thuộc formatting của IL2CPP runtime |
| `second_parameter_type_name` | `const char*` | exact string từ `type_get_name` | cùng giới hạn |

- Kết quả: trả method descriptor đầu tiên match name + arity 2 + hai type
  strings; không trả diagnostic reason/ambiguity.
- Điểm mạnh: descriptor dùng được trực tiếp với `il2cpp_runtime_invoke`.
  `runtime_invoke` nhận reference argument dưới dạng object pointer và value
  argument dưới dạng address của value; exception được trả riêng. Caller không
  phải tự gọi native ABI của compiled method.
- Điểm yếu:
  - không kiểm `method_is_instance`, return type, image/declaring class, token;
  - trả match đầu tiên, không chứng minh uniqueness;
  - cap 512 có thể biến truncated enumeration thành “không thấy” mà không có
    reason;
  - exact type-name string dễ vỡ với nested/generic/byref formatting;
  - helper không kiểm method descriptor thuộc đúng build profile;
  - ownership của chuỗi `il2cpp_type_get_name` chưa được code biểu diễn; nếu
    runtime trả allocated buffer thì vòng scan có nguy cơ leak.

### Resolve/call bằng RVA — phương án trực tiếp

Pseudo-signature thường phải tự định nghĩa, ví dụ candidate arm64:

```cpp
using TryCaptureNative = void *(*)(
    void *self,
    DirectMapPokeballThrow throw_data,
    void *ar_plus,
    const void *method_info
);
```

Signature trên **chưa được xác minh** và minh họa đúng rủi ro: value type có thể
được chia register/stack khác dự kiến; hidden `MethodInfo*`, adjustor thunk hoặc
shared generic body có thể khác. Direct call bỏ qua `runtime_invoke` exception/
boxing semantics và crash process nếu prototype sai.

RVA còn phải được relocate bằng exact executable load bias. `0x9e1caa4` không
phải runtime pointer tuyệt đối. Pointer chỉ có nghĩa khi build-id/ELF hash, ABI,
module mapping và executable range đều khớp.

### Token accessor — candidate bổ sung

Pinned ELF dynsym có `il2cpp_method_get_token`, cùng các export
`il2cpp_method_get_return_type` và `il2cpp_free`. Đây là local shipped-artifact
observation từ `llvm-readelf`, chưa có retained REA Evidence: probe REA mới thất
bại trước khi Ghidra launch vì host Swift SDK/compiler mismatch. Vì vậy token
accessor là hướng implementation hợp lý nhưng chưa được dùng như canonical
Evidence trong lượt này.

Nếu resolve được export ở runtime, signature candidate là:

```cpp
using Il2CppMethodGetToken = uint32_t (*)(const void *method);
```

Token cho phép cross-check `TryCapture == 0x06013ab5` sau name/type lookup mà
không đọc layout nội bộ của `MethodInfo`.

### So sánh semantics

| Tiêu chí | Name/type + `runtime_invoke` | Direct RVA call |
|---|---|---|
| ASLR | Tự xử lý bởi IL2CPP descriptor | Caller phải tính load bias đúng |
| Khác build | Có thể vẫn resolve nếu contract không đổi | RVA gần như chắc đổi |
| Overload | Tốt nếu kiểm full type/return/token | Không có metadata identity tại pointer |
| Value type | `runtime_invoke` marshal theo metadata | Caller phải đúng native ABI tuyệt đối |
| Exception | Có exception out parameter | Phụ thuộc direct ABI/implementation |
| Thunk/generic sharing | IL2CPP descriptor sở hữu routing | Caller phải biết pointer/thunk nào |
| Evidence build-specific | Yếu nếu chỉ name/type | Mạnh khi exact hash/RVA đã chứng minh |
| Dùng làm call path | Tốt | Rủi ro cao, không khuyến nghị |
| Dùng làm cross-check | Cần token/RVA bổ sung | Rất tốt làm expected identity |

Kết luận kỹ thuật ở bước này: RVA nên là **guard**, không phải invocation API.
Giữ `MethodInfo* + il2cpp_runtime_invoke` làm đường gọi.

## Bước 4 — Luồng hành vi và điểm tích hợp

### Luồng hiện tại

```text
exact build-id gate
  -> resolve assembly/class hoặc concrete owner object
  -> enumerate method theo name + parameter types
  -> lưu MethodInfo* vào RuntimeBinding
  -> set *_verified khi các pointer thành phần khác non-null
  -> main-thread action gọi il2cpp_runtime_invoke(MethodInfo*, owner, args, exception)
  -> promise observer / inventory refresh / map sync xử lý hậu điều kiện
```

`TryCapture` đang đi qua `discover_direct_map_bindings`, sau đó
`direct_map_actions.inc` gọi `runtime_invoke`. Transfer và discard cũng lưu
method descriptor trong `RuntimeBinding` và gọi qua cùng API. `owner_generation`
và main-thread scheduling là guard riêng; chúng không thể được thay thế
bằng token hay RVA.

Hiện `direct_catch_verified` chỉ yêu cầu `TryCapture` và
`get_SpawnPointId` non-null. Trong khi đó `discard_recycle_item_signature`
đã kiểm contract mạnh hơn: instance/static, return type, parameter types
và resolve parameter class. Mức `verified` vì vậy chưa đồng nhất giữa
các module.

### Luồng lai được khuyến nghị

```text
exact ELF + metadata/build profile
  -> resolve exact image / namespace / declaring class
  -> enumerate to exhaustion; collect every full-signature match
  -> require exactly one candidate
  -> verify instance/static + exact return/parameter metadata
  -> verify metadata token
  -> optionally verify normalized method pointer against module_base + RVA
  -> retain MethodInfo* (not the raw RVA) in RuntimeBinding
  -> invoke through il2cpp_runtime_invoke on the main thread
  -> verify Promise/result and authoritative runtime postcondition separately
```

Token/RVA mismatch phải là diagnostic + capability unavailable, không phải
lý do fallback sang direct native call. Cách này giữ RVA trong binding profile
theo build, không làm module policy biết offset.

### Vị trí RVA trong gate

- Tính expected code address từ load bias của exact `libil2cpp.so` và RVA;
  bắt buộc pointer nằm trong executable mapping của cùng module.
- Chỉ đọc `MethodInfo.methodPointer` khi layout prefix đã được chứng minh
  cho exact IL2CPP build. `DirectRuntimeMethodInfoView` hiện là diagnostic
  layout cục bộ; static assertions chỉ chứng minh compiler layout của struct
  tự khai báo, không chứng minh runtime layout.
- Không nên ép equality tuyệt đối nếu REA chưa phân biệt method body,
  adjustor thunk hoặc generic-sharing thunk. Profile phải nêu rõ expected
  pointer kind/accepted entry set; chưa có evidence thì bỏ RVA-pointer check
  ở trạng thái `unknown`, không tự suy diễn.
- Cache method descriptor chỉ có giá trị trong process/session/build hiện tại.
  Khi owner generation hoặc runtime session reset, binding và các verified flag
  phải bị xóa và resolve lại.

### Device có cần cho phần này không?

Không cần device để chốt managed identity, token, signature, build hash
và static RVA. Resolver chạy trong game process nên có thể tự cross-check
runtime metadata/pointer mà không thực hiện gameplay mutation. Device chỉ còn cần
cho nhóm evidence khác: owner instance/lifetime thực, Promise completion/value,
server result và postcondition sau action.

## Bước 5 — Hướng xử lý

### Phương án

1. **Giữ name/type resolver như hiện tại.** Ít thay đổi, nhưng chưa đủ
   cho mutation: overload ambiguity, return type và build identity chưa được
   chứng minh.
2. **Gọi trực tiếp `base + RVA`.** Không khuyến nghị: đòi exact native
   ABI, hidden arguments, thunk semantics và exception behavior. RVA đúng không
   tự nó làm prototype đúng.
3. **Hybrid descriptor resolver.** Khuyến nghị: managed metadata chọn
   `MethodInfo*`; token và RVA/profile chứng minh identity; `runtime_invoke`
   thực hiện call.

### Contract đề xuất

```cpp
struct RuntimeMethodExpectation {
    const char *image_name;
    const char *name_space;
    const char *class_name;
    const char *method_name;
    uint32_t metadata_token;
    bool is_instance;
    const char *return_type;
    uint32_t parameter_count;
    const char *parameter_types[3];
    uintptr_t code_rva;
};

enum class RuntimeMethodResolution {
    kVerified,
    kApiUnavailable,
    kClassMismatch,
    kNoMatch,
    kAmbiguous,
    kSignatureMismatch,
    kTokenMismatch,
    kCodePointerUnknown,
    kCodePointerMismatch,
};

struct VerifiedRuntimeMethod {
    const void *descriptor;
    uint32_t token;
    uintptr_t normalized_code_address;
};
```

Resolver nên nhận exact `Il2CppClass*` đã resolve từ expected image/
namespace/class và `RuntimeMethodExpectation`, trả structured reason thay vì
`nullptr`. Tìm toàn bộ iterator đến null, yêu cầu duy nhất một match;
không dùng hard cap như một kết quả bình thường. Nếu cần budget chống
corruption, vượt budget phải trả `kEnumerationLimit`, không trả `kNoMatch`.

Với mutation method, type và return type phải exact theo canonical runtime
name hoặc theo resolved `Il2CppClass*`; substring checks chỉ phù hợp survey/
diagnostic, không phù hợp strong identity. Token là required guard khi export
`il2cpp_method_get_token` có sẵn.

RVA guard có hai mức:

- `required`: exact method entry/thunk và `MethodInfo` pointer accessor/layout đã
  có evidence; mismatch đóng capability;
- `unavailable`: Ghidra boundary hoặc thunk relation chưa chốt; không giả vờ
  equality. Khi đó token + exact full signature + strong build identity là gate,
  còn RVA ở residual unknown.

Không nên có chế độ “token sai nhưng RVA đúng thì vẫn call” hoặc
ngược lại. Hai nguồn evidence mâu thuẫn nghĩa là profile/binding sai.

### Thứ tự triển khai hợp lý

1. Bổ sung `Il2CppMethodGetToken` và resolve
   `il2cpp_method_get_token`; xác nhận ownership ABI của
   `il2cpp_type_get_name`, bổ sung `il2cpp_free`/RAII nếu chuỗi là allocated.
2. Tạo resolver dùng chung trả structured reason và uniqueness; không nhân
   đôi helper 1/2/3 parameter.
3. Tạo build-scoped expectation cho `TryCapture`, `GetPokemon`,
   `ReleasePokemon`, `GetItemCount`, `RecycleItem` từ mapping 0.433.0.
4. Dùng descriptor đã verify trong `RuntimeBinding`; `runtime_invoke` và
   argument construction không đổi.
5. Chỉ bật RVA-pointer guard sau khi chốt exact method boundary/thunk và
   runtime `MethodInfo` accessor/layout. Xóa hoặc hạn chế
   `DirectRuntimeMethodInfoView` diagnostic khi public token API đã thay thế.
6. Chuẩn hóa điều kiện `*_verified`: method identity verified, owner exact
   class/session, và postcondition observer phải là các flag tách biệt.

### Blocker hiện tại quan trọng hơn RVA

Phần hard-code `profile.version_code = false` và
`profile.metadata_sha256 = false` đã được thay bằng runtime observation độc lập:
package/versionCode qua JNI, build-id từ loaded ELF và SHA-256 của installed
`global-metadata.dat`. Việc đó làm strong build identity code-complete nhưng
không tự mở execution. Blocker còn lại là live owner/session lifetime,
concrete Promise behavior và authoritative map/storage/inventory postcondition;
chúng vẫn cần handoff device có kiểm soát.

## Bước 6 — Tiêu chí chấp nhận và xác minh

### Mức A — Strong static identity (host-only)

- Exact APK/base split, `libil2cpp.so` và `global-metadata.dat` hash khớp
  manifest.
- Mỗi mutation method có exact image/class/name, instance/static, return type,
  parameter types, token và mapping slot/RVA.
- Token accessor ABI được xác nhận từ pinned artifact/header evidence.
- Ghidra xác định RVA là exact entry, adjustor thunk hay shared body. Evidence
  chỉ chứng minh “contained in coarse function” không đủ bật equality guard.
- Resolver từ chối zero/multiple matches và token/signature mismatch.

Mức này không cần device và đủ để chốt phương pháp resolve/call.

### Mức B — Runtime read-only identity

- Runtime tự xác minh package/version/build-id/metadata hash; không tin
  fingerprint do controller gửi.
- Descriptor resolve ra duy nhất và token/full signature khớp expectation.
- Nếu RVA guard là required, normalized method pointer khớp accepted
  entry/thunk và nằm trong executable mapping của exact `libil2cpp.so`.
- Restart/reconnect làm owner generation tăng và descriptor/owner cũ không
  được tái sử dụng.

Mức này có thể chạy read-only trên device/process; chưa gửi gameplay
action.

### Mức C — Invocation contract

- Main-thread bridge thực thi descriptor đã verify qua `runtime_invoke`.
- Argument storage đúng theo metadata: reference dùng object pointer; value type
  dùng address của exact layout. Không cast RVA thành native function pointer.
- Owner là exact live class, cùng session/generation, còn lifetime trong suốt call.
- Managed exception khác null là failure; Promise null/wrong concrete generic type
  là failure.

### Mức D — Result và postcondition

- Promise observer chứng minh completion, success/failure và exact result type.
- Authoritative game/server result được decode; UI disappearance hoặc local
  inventory guess không được coi là action success.
- Inventory/map state refresh xác nhận expected postcondition và correlation
  với request/session.
- Timeout, disconnect, stale owner hoặc ambiguous result giữ kết quả
  indeterminate/fail-closed; không retry mutation mù.

Device chỉ bắt buộc ở C/D vì các mục này phụ thuộc instance
lifetime và hành vi backend thực, không thể suy ra chắc chắn từ
metadata/RVA.

### Kiểm chứng khi implementation

- Chạy existing native protocol checks và focused build; không thêm test file
  theo repository policy.
- Chạy `./gradlew test assembleDebug` khi môi trường cho phép.
- Chạy `git diff --check`.
- Export REA Evidence mới khi exact entry/thunk hoặc token accessor được
  chứng minh; ghi rõ artifact SHA-256, authority, coverage và limitation.

## Kết luận

Cách hiện tại đúng ở quyết định dùng managed descriptor và
`il2cpp_runtime_invoke`, nhưng helper name/two-types chưa đủ mạnh để tự
gán `verified`. Nên nâng nó thành hybrid resolver:

```text
exact build -> exact class/full signature -> unique match -> token -> RVA guard
            -> retain MethodInfo* -> runtime_invoke -> Promise/result/postcondition
```

RVA là evidence/guard build-specific, không phải API gọi thay thế.

### Kết quả triển khai 2026-10-14

Phương án hybrid đã được triển khai cho `TryCapture`, `GetPokemon`,
`ReleasePokemon`, `GetItemCount` và `RecycleItem`:

- resolver dùng exact image/class, static/instance, full parameter/return type,
  uniqueness và `il2cpp_method_get_token`;
- `il2cpp_type_get_name` được quản lý bằng `il2cpp_free`/RAII;
- retained binding là `MethodInfo*` và invocation vẫn qua
  `il2cpp_runtime_invoke` trên main-thread bridge;
- mọi RVA profile đang ở policy `unavailable`, vì Ghidra chỉ chứng minh RVA
  nằm trong coarse `FUN_*`, chưa chứng minh exact entry/thunk;
- build identity tự quan sát package/versionCode/build-id/metadata hash;
- method identity, owner session, invocation và outcome là các gate độc lập.

Do chưa có device evidence cho owner lifetime, Promise/result correlation và
runtime postcondition, direct catch/transfer/discard vẫn không `LIVE_READY`.
Promise success hiện trả indeterminate với reason postcondition-unverified;
không phát success/caught/transferred/discarded sớm. Host build/checks đạt trừ
hai protocol test path được yêu cầu nhưng không tồn tại trong repository.
