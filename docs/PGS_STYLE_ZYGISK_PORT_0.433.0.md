# Ghi chú triển khai: Zygisk tự cài và automation kiểu PGS cho Pokémon GO 0.433.0

## 1. Mục tiêu và phạm vi

Tài liệu này mô tả những phần cần sửa trong `pogo-root-automation` để tạo một
bộ chạy độc lập có các tính năng:

- controller APK lưu cấu hình và điều khiển runtime;
- APK cài module Magisk/Zygisk đã đóng gói sẵn bằng quyền `su`;
- Zygisk tự nạp native runtime vào `com.nianticlabs.pokemongo`;
- native runtime đọc state game và tự điều phối Auto Catch, Auto Transfer,
  Auto Discard;
- chỉ hỗ trợ cố định Pokémon GO `0.433.0` arm64, không có API kiểm tra hoặc tải
  bản core mới.

Không chép binary hoặc source của PGSharp. Chỉ tái hiện độc lập các hành vi đã
quan sát được từ artifact: inject qua Zygisk, resolve service/object IL2CPP,
đọc state trực tiếp trong process game và gọi API game trên main thread.

## 2. Kết luận kiến trúc

Kiến trúc hiện tại của repo cơ bản là đúng và không cần viết lại:

```text
Controller APK
  ├─ UI + SharedPreferences
  ├─ cài module bằng su
  └─ gửi desired config qua Unix socket
            │
            ▼
Zygisk companion/broker
            │
            ▼
Injected native runtime trong Pokémon GO
  ├─ resolve Unity/IL2CPP + Zenject services
  ├─ giữ snapshot game state trong RAM
  ├─ Auto Catch
  ├─ Auto Transfer
  └─ Auto Discard
```

Kotlin không được đọc object Pokémon GO hoặc tự lập kế hoạch gameplay. Native
runtime phải sở hữu state sống, target selection, pending action, Promise
outcome và cooldown. Kotlin chỉ sở hữu cấu hình bền vững, UI và location.

## 3. APK có thể tự gọi `su` để inject Zygisk không?

### Câu trả lời

Có thể tự động cài module sau khi người dùng mở APK lần đầu. Tuy nhiên Android
không chạy code ứng dụng ngay tại thời điểm Package Installer vừa cài APK, nên
không có điểm vào đáng tin cậy gọi `su` đúng nghĩa “ngay khi cài xong”. Luồng
thực tế nên là:

```text
Cài APK
  -> mở APK lần đầu
  -> APK yêu cầu quyền root qua su
  -> copy ZIP module nhúng ra app-private cache
  -> xác minh hash + nội dung ZIP
  -> magisk --install-module <zip>
  -> xác minh module đã xuất hiện trong /data/adb/modules
  -> yêu cầu người dùng reboot
  -> sau reboot, Zygisk inject khi Pokémon GO được khởi động
```

Không nên thử “hot inject” bằng cách restart zygote hoặc ghi trực tiếp module rồi
kill process hệ thống. Cách đó dễ bootloop và khó khôi phục. Reboot sau cài module
là boundary an toàn, giống vòng đời chuẩn của Magisk/Zygisk.

### UX đề xuất

Mặc định dùng flow có trạng thái rõ ràng:

1. Khi app mở lần đầu, tự chạy preflight và hiển thị root prompt.
2. Nếu đã có Magisk + Zygisk, tự cài module một lần.
3. Hiển thị `Installed — reboot required`.
4. Nút reboot phải do người dùng bấm xác nhận; không tự reboot thiết bị.
5. Sau reboot, app kiểm tra module/runtime và mới cho bật automation.

Nếu root prompt bị từ chối hoặc Magisk/Zygisk không tồn tại, app phải dừng ở
trạng thái có lỗi cụ thể, không lặp prompt vô hạn trong background.

## 4. Các file APK/controller cần thêm hoặc sửa

### 4.1 Nhúng Magisk ZIP vào APK

Sửa:

```text
app/build.gradle.kts
scripts/build-magisk.sh
scripts/package-magisk.sh
settings.gradle.kts (chỉ khi cần task wiring mới)
```

Thêm task build theo thứ tự:

```text
build native Zygisk
  -> package pogo-root-automation-magisk.zip
  -> copy ZIP vào generated res/raw hoặc generated assets
  -> assemble controller APK
```

Không commit cùng lúc một ZIP sinh tự động vào source và một bản khác trong
`build/`; chỉ có một artifact canonical được đưa vào APK. Build phải sinh kèm:

- SHA-256 của ZIP;
- module ID;
- module version;
- danh sách ABI bắt buộc.

Với mục tiêu chỉ chạy điện thoại arm64 có thể chỉ nhúng
`zygisk/arm64-v8a.so`. Nếu vẫn muốn giữ BlueStacks thì tiếp tục đóng gói cả
`arm64-v8a.so` và `x86_64.so`.

### 4.2 Thêm installer trong controller APK

Nên thêm:

```text
app/src/main/java/dev/pogoroot/automation/root/BundledModuleInstaller.kt
app/src/main/java/dev/pogoroot/automation/root/ModuleInstallCoordinator.kt
app/src/main/java/dev/pogoroot/automation/data/ModuleInstallStateRepository.kt
```

Trách nhiệm:

`BundledModuleInstaller`:

- copy ZIP nhúng sang `cacheDir` bằng tên cố định;
- tính SHA-256 và so với manifest build-time;
- kiểm tra ZIP có `module.prop` và đúng thư viện Zygisk;
- phát hiện Magisk binary bằng danh sách path hữu hạn;
- gọi `magisk --install-module` qua `su`;
- kiểm tra exit code, stdout, stderr;
- kiểm tra `/data/adb/modules/pogo_root_automation/module.prop` sau cài;
- xóa file tạm khi hoàn tất.

`ModuleInstallCoordinator`:

- state machine `NOT_CHECKED`, `ROOT_REQUIRED`, `INSTALLING`, `INSTALLED`,
  `REBOOT_REQUIRED`, `ACTIVE`, `FAILED`;
- chỉ cho một install chạy tại một thời điểm;
- không tự retry liên tục khi user từ chối root;
- phân biệt “module đã cài” với “module đã load trong process game”.

`ModuleInstallStateRepository`:

- lưu version/hash module đã cài trong SharedPreferences của controller;
- đây chỉ là cache UI, không thay thế kiểm tra module/runtime thật;
- không lưu pointer, PID hoặc readiness của phiên game.

### 4.3 Làm an toàn lớp root shell

Sửa:

```text
app/src/main/java/dev/pogoroot/automation/root/RootShell.kt
```

`ProcessRootShell.execute(String)` hiện ghép toàn bộ lệnh vào `su -c`. Với installer,
nên bổ sung một API nhận argv hoặc script qua stdin, đồng thời chỉ cho phép các
đường dẫn do app tạo. Không nội suy trực tiếp filename hoặc dữ liệu UI vào shell.

Các command cần hỗ trợ có phạm vi hữu hạn:

```text
id -u
magisk -v
magisk --path
magisk --install-module <validated-app-private-path>
test/read module.prop tại module ID cố định
reboot (chỉ sau thao tác xác nhận riêng của người dùng)
```

Timeout cài module nên lớn hơn timeout mặc định 3 giây. Cần đọc stdout/stderr
đồng thời để tránh process bị chặn vì đầy pipe.

### 4.4 Điểm gọi installer và UI

Sửa:

```text
app/src/main/java/dev/pogoroot/automation/MainActivity.kt
app/src/main/java/dev/pogoroot/automation/AutomationApplication.kt
app/src/main/java/dev/pogoroot/automation/service/HeadlessAutomationService.kt
app/src/main/java/dev/pogoroot/automation/service/AutomationBootReceiver.kt
app/src/main/AndroidManifest.xml
```

Khuyến nghị:

- `AutomationApplication` chỉ tạo coordinator, không mở root prompt khi app chạy
  ngầm;
- `MainActivity.onCreate()` gọi preflight/install khi có foreground Activity;
- service không submit desired state trước khi module ở trạng thái `ACTIVE`;
- sau reboot, `AutomationBootReceiver` chỉ khởi động controller service nếu user
  đã bật auto-start; module Zygisk tự load khi process Pokémon GO specialize;
- MainActivity hiển thị module version, trạng thái cài, reboot required, runtime
  attached và lỗi gần nhất.

Không thêm logic sửa DenyList, spoof system property hoặc Play Integrity bypass.

## 5. Bỏ auto-update nhưng vẫn giữ build guard

Repo không cần endpoint kiểm tra version, tải core hoặc chọn module theo server.
Toàn bộ native runtime nằm trong ZIP đã nhúng trong APK.

Tuy nhiên không được xóa kiểm tra compatibility cục bộ. Binding IL2CPP có field
obfuscate và layout theo build; chạy trên bản game khác có thể crash process.

Profile cố định cần dùng cho 0.433.0 arm64:

```text
package:             com.nianticlabs.pokemongo
versionName:         0.433.0
versionCode:         2026100702
libil2cpp build ID:  da59ce3024c4ccfe16fa6616281a4d111de67aa2
global metadata:     f5961a8548256da287b4f79466a169c302b90ec59be7f6461e8e58b482c0fb0d
base APK:            1c8f198cd212e7303e5a5e38e34857f4ced42eb432ac62b07807bfd286503690
```

Sửa:

```text
zygisk/jni/shared/core/runtime_native_prelude.inc
zygisk/jni/shared/runtime/probe/runtime_probe_discovery.inc
app/src/main/java/dev/pogoroot/automation/config/AutomationConfig.kt
bridge/protocol/src/main/kotlin/dev/pogoroot/automation/bridge/RuntimeSessionManager.kt
docs/LIVE_AUTOMATION_READINESS.md
```

Nên gom các giá trị trên thành một `RuntimeBuildProfile` thay vì rải chuỗi ở
nhiều feature. Build khác phải attach ở chế độ diagnostic/read-only hoặc từ chối
khởi động module gameplay với lỗi `unsupported_build`; không tải update.

## 6. Đọc state game “giống PGS” nên hiểu thế nào?

### Nguồn reverse canonical trong repo

Không dùng `scripts/reverse-pogo-apk.sh` hoặc tạo script thay thế. Bộ input đã
được đặt tại `reverse/artifacts/`, hash ở `reverse/artifacts/SHA256SUMS`; bundle
REA canonical là `reverse/rea/rea-evidence-2026-10-13.json`. `base.apk`, arm64
split, `libil2cpp.so` và `global-metadata.dat` đều có extraction Evidence nối về
APKM 0.433.0.

Các báo cáo PGS/libhela/PGTools đã được sao chép vào `reverse/reports/` để tái
dùng câu hỏi, thuật ngữ và hypothesis. Chúng không tự chứng minh class/method,
RVA, callsite hay runtime behavior. Mỗi binding được đưa vào code phải trỏ tới
REA Evidence ID + artifact SHA-256 + coverage/limitation; thiếu provider hoặc
evidence thì giữ `UNRESOLVED`/capability đóng.

### Hành vi đã quan sát ở PGS

PGS không đọc pixel và không điều khiển UI để biết Pokémon/fort. Core được nạp
vào process game rồi:

- lấy object/service IL2CPP của game;
- nhận `MapPokemon`, `MapPokestop`, `PoiItemSpinner` và state encounter;
- đọc encounter ID, species, tọa độ, proto, player location và khoảng cách;
- giữ/cập nhật object hoặc state nội bộ khi game tạo/cập nhật chúng;
- gọi method game trực tiếp cho catch/spin/transfer/discard;
- dùng callback/Promise/state transition để biết kết quả.

Đây là mô hình object/state-driven, không phải screen automation.

### Repo hiện tại

Repo đã đáp ứng phần lớn boundary đúng:

- `runtime_observation_thread` và feature modules chạy trong injected native;
- object game được resolve bằng IL2CPP/Zenject;
- map và inventory read được marshal sang Unity main thread;
- Kotlin chỉ gửi desired config và render event;
- transfer được kích từ kết quả catch;
- mỗi mutation giữ pending state và Promise riêng.

Điểm khác lớn nhất là catch/spin hiện chủ yếu tạo snapshot định kỳ khoảng 2 giây.
Legacy observer còn cơ chế dirty flag nhưng trong explicit snapshot mode, module
`catch_spin` tự poll. Để gần mô hình PGS hơn, nên chuyển thành hybrid event-driven.

## 7. Thiết kế state reader đích

### 7.1 Một native world-state store duy nhất

Nên thêm một lớp/store native, ví dụ:

```text
zygisk/jni/shared/runtime/state/runtime_world_state.h
zygisk/jni/shared/runtime/state/runtime_world_state.inc
```

State đề xuất:

```text
RuntimeWorldState
  sessionId
  ownerGeneration
  worldRevision
  observedAtElapsedNs
  mapState: AVAILABLE | EMPTY | STALE | NOT_READY | ERROR
  inventoryState: AVAILABLE | STALE | NOT_READY | ERROR
  lifecycle
  playerLocation
  pokemon candidates
  fort candidates
  item counts
  active encounter
  pending catch/spin/transfer/discard
```

Mỗi collector tạo dữ liệu tạm, validate đầy đủ rồi commit nguyên snapshot dưới
lock/revision. Không sửa dần state đang được coordinator đọc.

Quan trọng: `EMPTY` phải khác `UNAVAILABLE/NOT_READY`. Nếu map reader lỗi, không
được hiểu là xung quanh không có Pokémon.

### 7.2 Hook chỉ đánh dấu dirty; đọc object trên main thread

Giữ hook map response/object lifecycle hiện có hoặc bổ sung hook build-specific
cho 0.433.0:

```text
S2CellManager.OnMapQueryResponse hoặc callback map tương đương
  -> increment dirty generation
  -> schedule bounded main-thread refresh
  -> read map cells/Pokémon/fort/player location
  -> validate
  -> commit RuntimeWorldState
  -> wake catch_spin coordinator
```

Hook không nên tự duyệt collection lớn hoặc gọi mutation ngay trong callback.
Nó chỉ phát tín hiệu. Việc đọc object và gọi method Unity/IL2CPP phải chạy qua
main-thread bridge hiện có.

Giữ fallback reconcile khoảng 30 giây để phục hồi nếu bỏ lỡ event. Không cần
quét map đều đặn mỗi 2 giây khi không có dirty signal; timer 2 giây chỉ nên dùng
cho cooldown/retry/pending action, không phải nguồn state chính.

Sửa chủ yếu:

```text
zygisk/jni/shared/runtime/observation/runtime_observation.inc
zygisk/jni/shared/runtime/observation/runtime_observation_policy.inc
zygisk/jni/shared/runtime/map/runtime_map_reader.inc
zygisk/jni/shared/runtime/map/runtime_map_dictionary.inc
zygisk/jni/modules/catch_spin/coordinator.inc
zygisk/jni/modules/catch_spin/module.inc
zygisk/jni/shared/core/runtime_native_prelude.inc
```

### 7.3 Resolve owner/service và quản lý lifetime

Không giữ pointer managed vô hạn. Mọi object/service cache cần đi cùng
`ownerGeneration` và invalidation khi scene/domain/session thay đổi.

Sửa/kiểm tra:

```text
zygisk/jni/shared/runtime/probe/runtime_probe_discovery.inc
zygisk/jni/shared/runtime/probe/runtime_probe_service_owners.inc
zygisk/jni/shared/runtime/probe/runtime_scene_owners.inc
zygisk/jni/shared/runtime/probe/runtime_encounter_owners.inc
```

Yêu cầu:

- resolve service bền vững như `IItemBag`, `IPokemonBag`, location provider;
- scene owner như map cells/encounter được refresh khi scene thay đổi;
- pointer dùng sau generation đổi phải bị từ chối;
- late owner resolution được retry có giới hạn khi map scene vừa khởi tạo;
- không gọi managed discovery đồng thời từ nhiều native thread;
- không gọi reflection/survey nặng ngay trong `il2cpp_init`.

### 7.4 Map Pokémon và fort

Current reader phải được port khỏi field obfuscate của 0.427.0. Không copy các
field như `eghw`, `egws`, `egwr`, `egwt` sang 0.433.0 nếu chưa đối chiếu lại.

PGS quan sát nhiều loại Pokémon map, không chỉ wild thường:

```text
WildMapPokemon
IncenseMapPokemon
TroyDiskMapPlacePokemon
DailyEncounterMapPokemon
PhotobombingMapPokemon
IncidentMapPokemon
PokestopSpawnPokemon
RouteMapPokemon
RaidMapPokemon
StationMapPokemon
```

Không nhất thiết hỗ trợ tất cả ngay ở phase đầu. Phase đầu nên hỗ trợ
`WildMapPokemon` với adapter typed; các nguồn khác thêm sau nhưng phải được phân
loại rõ thay vì cast tất cả về một layout.

Candidate chuẩn hóa cần tối thiểu:

```text
sourceType
encounterId
speciesId
latitude/longitude
distanceMeters
hasProto
shinyKnown/shiny
observedRevision
```

Fort candidate cần:

```text
fortId
latitude/longitude
distanceMeters
kind (PokéStop/Gym/...)
cooldownCompleteMs
spinner/state readiness
observedRevision
```

Sửa:

```text
zygisk/jni/shared/runtime/map/runtime_map_reader.inc
zygisk/jni/shared/runtime/map/runtime_map_forts.inc
zygisk/jni/modules/catch_spin/map_actions.inc
zygisk/jni/modules/catch_spin/direct_map_actions.inc
zygisk/jni/modules/catch_spin/direct_map_bindings.inc
```

### 7.5 Inventory state

PGS đọc count bằng `IItemBag.GetItemCount`. Cách repo hiện tại dùng method này là
phù hợp. Nên đổi trigger refresh:

- khi bật Auto Discard: đọc ngay;
- sau spin thành công: mark inventory dirty và refresh;
- sau discard thành công: refresh hoặc apply full response đã xác minh;
- fallback 30 giây;
- khi Auto Catch chọn ball: đọc count mới nếu snapshot stale.

Không cần polling mỗi item liên tục. Danh sách tối thiểu giữ giống UI PGS:

```text
1, 2, 3,
101, 102, 103, 104,
201, 202,
701, 703, 705, 706, 708,
1201, 1202
```

Sửa:

```text
zygisk/jni/modules/discard/inventory_reader.inc
zygisk/jni/shared/runtime/mainthread/runtime_inventory_request.inc
zygisk/jni/shared/runtime/inventory/runtime_inventory_full.inc
zygisk/jni/modules/catch_spin/coordinator.inc
zygisk/jni/modules/discard/module.inc
```

## 8. Port từng feature sang 0.433.0

### 8.1 Auto Catch

PGS Virtual GO Plus dùng đường accessory/game-state và Poké Ball thường; không
giả lập swipe throw. Repo hiện dùng direct map `TryCapture`/encounter path, có thể
giữ cách này nếu mục tiêu là hành vi tương đương chứ không bắt buộc mô phỏng
protocol GO Plus hoàn toàn.

Các việc bắt buộc:

- resolve lại map class/method/field cho 0.433.0;
- xác minh player position và catch range;
- chỉ dùng Poké Ball item ID `1` ở chế độ Virtual GO Plus-like;
- khi hết Poké Ball, ngừng chọn Pokémon và ưu tiên spin nếu được bật;
- giữ một pending catch;
- quan sát Promise/result authoritative;
- lấy `capturedPokemonId` từ kết quả thật;
- đồng bộ map sau catch rồi mới chọn target tiếp theo;
- phân biệt caught, fled, not-in-range, no-balls, stale target và indeterminate.

Sửa:

```text
zygisk/jni/modules/catch_spin/direct_map_bindings.inc
zygisk/jni/modules/catch_spin/direct_map_actions.inc
zygisk/jni/modules/catch_spin/direct_catch_promise_observer.inc
zygisk/jni/modules/catch_spin/direct_catch_map_sync.inc
zygisk/jni/modules/catch_spin/coordinator.inc
zygisk/jni/shared/runtime/map/runtime_catch_range.inc
```

Không quảng bá `DIRECT_CATCH` trước khi Promise outcome và map postcondition đều
được xác minh trên 0.433.0.

### 8.2 Auto Transfer

Class/method đã xác nhận trong 0.433.0:

```text
Niantic.Holoholo.IPokemonBag.ReleasePokemon(pokemon)
  slot 15, token 0x0600105f

Niantic.Holoholo.Internal.PokemonBagImpl.ReleasePokemon(pokemon)
  slot 19, token 0x06002a43
```

Đường thực thi:

```text
catch Promise terminal CAUGHT
  -> capturedPokemonId
  -> IPokemonBag.GetPokemon(id)
  -> đọc PokemonProto metadata
  -> apply keep policy
  -> IPokemonBag.ReleasePokemon(pokemon)
  -> giữ/poll ReleasePokemon Promise
  -> refresh storage hoặc xác minh response
```

Sửa/kiểm tra:

```text
zygisk/jni/shared/runtime/probe/runtime_probe_service_owners.inc
zygisk/jni/modules/transfer/metadata.inc
zygisk/jni/modules/transfer/promise_observer.inc
zygisk/jni/modules/transfer/release_promise_observer.inc
zygisk/jni/modules/transfer/coordinator.inc
```

Không dùng `PokemonInfoGuiController.TransferPokemon()` hoặc Pokémon HOME.
Không bật transfer nếu caught result không có Pokémon storage ID xác thực.

Policy nên ưu tiên keep/fail-closed cho:

- shiny;
- IV 100;
- legendary/mythical;
- favorite;
- special background;
- metadata chưa đọc đủ.

### 8.3 Auto Discard

Class/method đã xác nhận trong 0.433.0:

```text
Niantic.Holoholo.IItemBag.GetItemCount(itemType)
Niantic.Holoholo.Internal.ItemBagImpl.GetItemCount(itemType)

Niantic.Holoholo.IItemBag.RecycleItem(
    itemProto,
    numToRecycle,
    expiringItemsCopy
)

Niantic.Holoholo.Internal.ItemBagImpl.RecycleItem(
    itemData,
    numToRecycle,
    expiringItemsCopy
)
```

Config lưu ở controller dưới dạng map `itemId -> keepCount`. Native tính lại
ngay trước mutation:

```text
discardAmount = max(0, currentCount - keepCount)
```

Sửa/kiểm tra:

```text
zygisk/jni/modules/discard/execute.inc
zygisk/jni/modules/discard/item_data_factory.inc
zygisk/jni/modules/discard/item_inventory_lookup.inc
zygisk/jni/modules/discard/hash_set_binding.inc
zygisk/jni/modules/discard/recycle_item_action.inc
zygisk/jni/modules/discard/promise_observer.inc
zygisk/jni/shared/runtime/probe/runtime_probe_discovery.inc
```

`kDiscardExecutionEnabled` hiện đang `false`. Chỉ chuyển thành `true` sau khi
xác minh trên device:

- class thật của tham số đầu;
- field layout của `ItemData`;
- object `ExpiringItemsCopy` đúng owner/generation;
- Promise result thành công;
- inventory refresh/postcondition sau mutation.

## 9. Chọn target và điều phối action

PGS chắc chắn tính khoảng cách nhưng static evidence chưa chứng minh comparator
cuối cùng. Repo nên dùng policy rõ ràng, deterministic:

1. bỏ candidate stale/invalid/out-of-range;
2. nếu shiny policy yêu cầu pause thì không catch target đó;
3. ưu tiên Pokémon trong range theo `distanceMeters`, tie-break bằng encounter ID;
4. nếu không có Pokémon hợp lệ và Auto Spin bật, chọn PokéStop gần nhất chưa
   cooldown và trong range;
5. chỉ một mutation được pending tại một thời điểm;
6. sau terminal result và state refresh mới chọn target kế tiếp.

Coordinator không được coi timestamp mới hơn là bằng chứng game state đã đổi;
postcondition phải dựa vào map/inventory/storage content hoặc response xác thực.

## 10. Cấu hình và persistence

Tiếp tục dùng `AutomationConfigRepository`/SharedPreferences của controller làm
nguồn cấu hình bền vững. Native chỉ giữ mirror theo session/revision.

Không cần dùng các key ngụy trang của PGS như
`com.google.android.gms.safetynet`; project độc lập nên dùng namespace rõ ràng.

Cấu hình cần có:

```text
autoCatch
autoSpin
catchOnlyPokeBall
pauseOnShiny
catchSettleDelayMs
spinSettleDelayMs

autoTransfer
minimumIvPercentToKeep
keepUnknownIv
keepShiny
keepHundo
keepFavorite
keepLegendary
keepMythical
keepSpecialBackground

autoDiscard
discardKeepCountByItemId
showActionNotifications
```

Session mới phải nhận lại full desired snapshot. Không persist pending action,
game pointer, inventory snapshot hoặc runtime readiness.

## 11. Thứ tự triển khai đề xuất

### Phase A — Installer APK

- [ ] Build module ZIP trước APK.
- [ ] Nhúng ZIP + SHA-256 vào APK.
- [ ] Thêm `BundledModuleInstaller` và install state.
- [ ] Root prompt ở foreground lần đầu mở app.
- [ ] Xác minh Magisk/Zygisk/module ID.
- [ ] Hiển thị reboot-required; reboot chỉ khi user xác nhận.
- [ ] Sau reboot xác minh runtime attachment.

### Phase B — Profile 0.433.0

- [ ] Thay fingerprint 0.427.0 bằng profile 0.433.0.
- [ ] Resolve IL2CPP API và đúng `Assembly-CSharp`.
- [ ] Port service owners `IItemBag`/`IPokemonBag`/location.
- [ ] Port map/encounter/fort class và field obfuscate.
- [ ] Log riêng `NOT_READY`, `UNAVAILABLE`, `EMPTY`, `STALE`, `ERROR`.

### Phase C — Native state store kiểu PGS

- [ ] Thêm `RuntimeWorldState` có revision/generation.
- [ ] Hook map update chỉ mark dirty.
- [ ] Main-thread refresh và atomic commit.
- [ ] Event-driven wakeup; fallback reconcile 30 giây.
- [ ] Không còn scan full map mỗi 2 giây khi state không đổi.
- [ ] Inventory dirty sau spin/discard.
- [ ] Storage dirty sau catch/transfer.

### Phase D — Auto Catch

- [ ] Port direct catch binding.
- [ ] Xác minh catch range và Poké Ball count.
- [ ] Quan sát Promise outcome authoritative.
- [ ] Trích `capturedPokemonId`.
- [ ] Xác minh map sync sau catch.
- [ ] Bật capability chỉ sau live verification.

### Phase E — Auto Transfer

- [ ] Resolve `GetPokemon` + `ReleasePokemon` trên 0.433.0.
- [ ] Port `PokemonProto` metadata fields.
- [ ] Giữ fail-closed khi metadata thiếu.
- [ ] Poll Promise và xác minh storage delta.

### Phase F — Auto Discard

- [ ] Port `ItemData` và `ExpiringItemsCopy` owner.
- [ ] Xác minh `GetItemCount` trên main thread.
- [ ] Xác minh `RecycleItem` Promise và inventory refresh.
- [ ] Sau đó mới bật `kDiscardExecutionEnabled`.

## 12. Tiêu chí nghiệm thu

### Installer

- APK không dùng network để lấy module.
- Mở lần đầu chỉ tạo một root request.
- Module ZIP sai hash hoặc thiếu entry bị từ chối.
- Cài lại cùng module version là idempotent.
- Không tự reboot.
- Sau reboot, runtime chứng minh đã attach đúng process.

### State reader

- Không đọc pixel hoặc dùng thao tác tap/swipe để suy ra game state.
- Mọi Unity/IL2CPP object read nhạy cảm chạy trên main thread.
- Map empty khác map unavailable.
- Snapshot có session, generation, revision và timestamp.
- Scene/session đổi làm invalid toàn bộ pointer cũ.
- Không ghép map mới với inventory/location cũ mà không đánh dấu partial/stale.

### Automation

- Chỉ một mutation pending.
- Auto Catch có terminal caught/fled/error đáng tin cậy.
- Auto Transfer chỉ chạy với captured Pokémon ID xác thực.
- Auto Discard tính lại count ngay trước recycle.
- Mọi Promise timeout trả `INDETERMINATE`, không retry mù.
- Game build khác 0.433.0 fail closed và không gọi mutation.

## 13. Điều không nên làm

- Không xóa build guard chỉ vì không cần auto-update.
- Không hardcode RVA từ `libhela.so` vào project; đó là địa chỉ của core PGS,
  không phải method address ổn định của game.
- Không dùng metadata token như địa chỉ runtime.
- Không gọi method Unity từ socket/observer worker thread.
- Không để Kotlin và native cùng chạy hai automation loop độc lập.
- Không tự sửa DenyList, verified-boot property hoặc attestation.
- Không bật compile-time mutation gate trước khi có outcome/postcondition thật.

## 14. Tài liệu đối chiếu

Trong repo:

- `docs/ARCHITECTURE.md`
- `docs/LIVE_AUTOMATION_READINESS.md`
- `docs/headless-autocatch-spin.md`
- `docs/auto-transfer.md`
- `docs/architecture/inventory-and-discard.md`

Research artifact bên workspace `dark-magic`:

- `apk/pgsroot-1.0.15-phan-tich-inject-pokemon-go.vi.md`
- `apk/libhela-1.273.1-virtual-go-plus-auto-catch.vi.md`
- `apk/libhela-1.273.1-auto-transfer-auto-discard.vi.md`
- `research/04-bang-chung-auto-catch-map-fort.md`
- `research/07-luong-thuc-thi-auto-transfer-auto-discard.md`
- `research/08-doi-chieu-pokemon-go-0.433.0.md`

## 15. Kết luận thực thi

Nên tiếp tục trên repo hiện tại thay vì dựng project mới. Phần APK tự cài Zygisk
là một feature độc lập và có thể làm trước. Phần đọc state hiện đã đúng boundary,
nhưng cần port build 0.433.0 và đổi catch/spin sang hybrid event-driven để gần
cách PGS bám object/state của game hơn. Auto Transfer có method contract rõ nhất;
Auto Discard là phần cuối cùng nên mở gate vì phụ thuộc layout `ItemData` và
postcondition inventory.
