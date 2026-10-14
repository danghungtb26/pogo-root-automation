# Brainstorm: Kết nối lúc khởi động so với PGSharp

**Loại:** architecture
**Ngày phân tích đầu tiên:** 2026-10-14
**Phạm vi game/build/ABI:** Pokémon GO 0.433.0; Android/Zygisk arm64-v8a và x86_64 theo repository; artifact so sánh khả dụng là `pgsroot-1.0.15` (quan hệ chính xác với sản phẩm PGSharp cần xác minh)

## Bước 1 — Yêu cầu và phạm vi

### Vấn đề cần trả lời

Xác định liệu đường kết nối/khởi động hiện tại khi process Pokémon GO đang chạy có khác cách PGSharp làm hay không, khác ở lớp nào, và khác biệt đó có giải thích được tình trạng startup/connection hiện tại hay chỉ là khác biệt kiến trúc hợp lệ.

### Hành vi hiện tại đã biết từ tài liệu kiến trúc

- Controller Kotlin kết nối tới root companion qua abstract Unix socket `pogo_root_automation_runtime`.
- Root companion chuyển tiếp tới injected runtime bên trong process Pokémon GO qua kênh companion của Zygisk.
- Runtime readiness, binding và gameplay module là state theo process/session; socket kết nối hoặc ACK không tự chứng minh game runtime đã sẵn sàng.
- Khi có session mới, Kotlin phải gửi lại desired snapshot; native không khôi phục pointer, pending action hay readiness từ persistence.

Các điểm trên mới mô tả thiết kế của repository, chưa chứng minh hành vi thực tế trên thiết bị ở lần khởi động đang được hỏi.

### Hành vi mong muốn

- Khi Pokémon GO đã hoặc đang khởi động, module/root runtime phải attach/inject đúng vòng đời, công bố một session hợp lệ và cho phép controller đồng bộ config sau khi runtime thật sự ready.
- Nếu controller mở trước hoặc kết nối lại sau, hệ thống phải phân biệt rõ broker reachable, game process attached, binding ready và module active.
- Mất process/session phải fail closed, không dùng lại readiness hoặc object pointer cũ.

### Trigger và phạm vi

- Trigger chính: Pokémon GO được mở trong khi app controller/module đã cài và có thể đã chạy.
- Phạm vi: bootstrap Zygisk, companion broker, injected runtime, controller socket/reconnect, session/readiness/config replay; chỉ đối chiếu gameplay binding khi nó ảnh hưởng readiness.
- Ngoài phạm vi: triển khai sửa code, thay đổi protocol, hoặc kết luận hành vi độc quyền của PGSharp nếu artifact/bằng chứng tương ứng không đủ.

### Ràng buộc

- Giữ đúng kiến trúc `RuntimeBridgeClient` → abstract Unix socket → root companion → injected runtime.
- Không dùng file polling hay SharedPreferences làm transport gameplay thứ hai.
- Mọi state/pointer game phải theo session và fail closed khi process đổi.
- So sánh shipped artifact phải dựa trên REA Evidence; báo cáo lịch sử chỉ dùng làm giả thuyết.

### Tiêu chí hoàn tất ban đầu

1. Vẽ được chuỗi startup hiện tại bằng source và chỉ ra các gate kết nối/readiness.
2. Trích được bằng chứng phù hợp từ artifact so sánh về bootstrap/IPC/lifecycle, hoặc ghi rõ phần chưa xác minh.
3. Phân loại từng khác biệt thành: khác kiến trúc hợp lệ, nguy cơ race/lifecycle, hoặc chưa đủ bằng chứng.
4. Đề xuất hướng xử lý và cách xác minh bằng script hiện có, không tự triển khai.

### Giả định và câu hỏi còn thiếu

- Tạm hiểu “PGSharp” là cách hoạt động cần đối chiếu với artifact `pgsroot-1.0.15` đang có trong repository. Đây chưa phải bằng chứng rằng artifact này đại diện đầy đủ cho mọi phiên bản/sản phẩm PGSharp.
- Chưa có triệu chứng cụ thể (timeout, không thấy runtime, config không apply, hay chỉ khác trình tự UI), nên phân tích sẽ tập trung vào kiến trúc và các điểm race có thể quan sát.
- Chưa xác minh runtime log của phiên BlueStacks Air 1 hiện tại; static/source evidence không thay thế xác minh thiết bị.

## Bước 2 — Bằng chứng từ reverse

### Identity và nguồn bằng chứng

| Nguồn | Identity / Evidence | Authority và coverage | Giới hạn |
|---|---|---|---|
| PGS Root manager APK | `reverse/artifacts/pgsroot-1.0.15/pgsroot1.0.15_BIeEp(arm64).apk`; SHA-256 `2e0f328a15094ebc0f2f91cfac72fb94024abdce95ac48b229b4be7b3dce7297` | Hash đã qua `shasum -a 256 -c reverse/artifacts/SHA256SUMS`; REA inventory `ev_0248662389cf472ffa1f5c7a1d0a0cad9b3b8a48b16952e74fa6b23580854f08`, `ev_81ebaad17d536e240babf7da08ed128dc1be4975b430fbd49b5caf6cf8351a64`, và lần kiểm tra lại `ev_f2770ce4349aa61f541d796db66815d8f3415b5ca971f33013e75bca1dd3c7bb`; shipped-artifact, observed | Inventory archive không tự chứng minh runtime/call graph. APK có các path chỉ khác hoa/thường nên REA không thể materialize toàn bộ archive trên filesystem macOS hiện tại. |
| DEX/manifest PGS Root | Cùng APK/hash; REA JADX `ev_6b5217b536c9152d88d60ad0ef68875ed270484cc3a36437aea220a2c91a0c9c` | Xác nhận package `m.u.va.man`, version `1.0.15`/`1000015`, minSdk 21, targetSdk 33, 1202 class; manifest chỉ có launcher `m.u.va.man.MainActivity`, không có service injection riêng; shipped-artifact, observed | Static decompile; không quan sát process đang chạy, native library, chữ ký APK hay split. Exact DEX descriptor/instruction offset không có. |
| Class manager chính | REA JADX `ev_384b2575d0f48acaecf1816412cba882b63c522996d99608869debd1e56b6c72` | Xác nhận `m.u.va.man.MainActivity` có `onCreate`, `onResume`, native `gs(int)` và native `no(int, Object[])` cùng các method bị làm rối; shipped-artifact, observed | Danh sách method chưa tự chứng minh vai trò từng method. |
| Callback cài đặt bị làm rối | REA JADX `ev_04bd1cca653f7e0f3128fbb7eb2a24fed8d052de56f70acc8260e5d9c95e3737` | Xác nhận class `e3.i`, constructor `(MainActivity, int)` và callback `a(g2.b)`; shipped-artifact, observed | Vai trò cài module mới là giả thuyết từ report lịch sử cho tới khi inspect body/callers ở bước 3. |
| Report inject PGS Root | `reverse/reports/pgsroot-1.0.15-phan-tich-inject-pokemon-go.vi.md` | Historical analyst reference. Report ghi nhận module Zygisk lọc `com.nianticlabs.pokemongo`, rồi trong `postAppSpecialize` `dlopen(.../files/libhela.so)` và chuyển `android_dlopen_ext` cho core | Không thay thế retained REA Evidence. Phần ELF/companion opcode dựa trên toolchain lịch sử; chưa có runtime capture và chưa có exact opcode map. |
| PoGo/core liên quan | Pokémon GO 0.433.0 artifacts và `libhela-1.273.1-pogo-0.433.0.so`; các hash trong `reverse/artifacts/SHA256SUMS` đều hợp lệ | Dùng để đặt phạm vi build và hiểu core mà loader dự kiến nạp | Không cần dùng function gameplay để trả lời transport startup; core runtime behavior vẫn không được suy ra từ tên file. |

Bundle canonical `reverse/rea/rea-evidence-2026-10-13.json` đã được import trước khi probe mới: 35 record, 4 unknown revision. Hai record retained cũ khớp trực tiếp hash APK PGS Root là `ev_024866...` và `ev_81ebaa...`; các Evidence JADX ở trên là probe mới của lượt phân tích này.

Evidence mới đã được export thành `reverse/rea/rea-evidence-2026-10-14-startup-connection.json`: 48 record, 4 unknown revision; SHA-256 `fc3065d35500caaf914a77b70dbaec93026ea9af0986db88cc828a082ca956b2`.

### Symbol/entry liên quan đã tìm được

| Symbol / thành phần | Vai trò sơ bộ | Trạng thái |
|---|---|---|
| `m.u.va.man.MainActivity` | Manager UI, kiểm/cài module và chuẩn bị core theo report lịch sử | Class/method inventory đã xác nhận; hành vi từng method cần bước 3 |
| `MainActivity.<clinit>` | Nơi có khả năng load `libman.so` | Chưa inspect body trong lượt này |
| `MainActivity.gs(int)` | JNI trả chuỗi làm rối theo report lịch sử | Signature đã xác nhận; mapping `id` chưa xác minh |
| `MainActivity.no(int, Object[])` | JNI thao tác payload/config theo report lịch sử | Signature đã xác nhận; mapping opcode chưa xác minh |
| `e3.i.a(g2.b)` | Callback được report lịch sử gắn với cài module root | Signature đã xác nhận; body/caller chưa xác minh |
| `zygisk_module_entry` / `zygisk_companion_entry` trong module nhúng | Entry Zygisk loader và companion | Chỉ có trong report lịch sử ở thời điểm này; module nhúng chưa materialize lại bằng REA do collision tên file |
| `preAppSpecialize` / `postAppSpecialize` tương ứng | Lọc process và nạp core tại app specialization | Vai trò được report lịch sử mô tả; tên C++ exact có thể không còn symbol và chưa có retained function dossier |
| `dlopen`, `android_dlopen_ext`, core entry `__cxa_guard_enter` | Chuỗi nạp `libhela.so` và cài hook loader theo report lịch sử | Historical reference, chưa có function dossier/runtime capture trong lượt này |

### Kết luận ngắn của bước 2

- Đã xác nhận PGS Root manager không khai báo Android service riêng để “đợi rồi attach” vào game; manager và Zygisk module là hai lớp khác nhau.
- Bằng chứng lịch sử nhất quán với mô hình injection theo app specialization: Zygisk vào process lúc Pokémon GO được tạo, sau đó loader nạp core trong `postAppSpecialize`. Đây khác với một controller socket kết nối muộn, nhưng chưa đủ để nói startup hiện tại sai.
- Chưa có runtime evidence chứng minh hành vi reconnect của PGS Root khi Pokémon GO đã chạy trước lúc manager mở. Khả năng hợp lý nhất là manager không attach lại process đã tồn tại; muốn core mới/module mới có hiệu lực phải tạo lại process game (và khi cài module lần đầu còn cần reboot theo report).
- Residual unknown chính: exact companion opcode, thời điểm core báo ready, cơ chế core ↔ manager sau khi nạp, và hành vi khi controller/manager mở sau game.

## Bước 3 — Function/method, param và hành vi

### `m.u.va.man.MainActivity.<clinit>` — Đã xác nhận tĩnh

- Nguồn: REA `ev_b6077421d51f51e6194d16f1cffe947acf0773df4bded030b25b750025c795cf`; APK SHA-256 `2e0f...7297`; shipped-artifact/JADX 0.7.1.
- Signature: `<clinit>(): void`.
- Vai trò và hành vi: cấu hình main root shell rồi gọi `System.loadLibrary("man")`. Không có param. Đây là load JNI của manager, không phải load core vào Pokémon GO.
- Kết quả/side effect: `libman.so` được load trong process manager nếu thành công; lỗi load sẽ chặn class initialization.
- Thread/lifetime: chạy khi class `MainActivity` được initialize trong process `m.u.va.man`; static evidence không xác nhận thời điểm runtime chính xác ngoài quy tắc JVM/Android thông thường.

### `m.u.va.man.MainActivity.onCreate` — Đã xác nhận tĩnh

- Nguồn: REA `ev_d1e8e756d9510c239c7e324d198ae71ca20fc301f6fc9cd97005e622c40cac3a`; cùng APK/hash/build.
- Signature: `onCreate(android.os.Bundle): void`.

| Param | Kiểu / modifier | Ý nghĩa / nguồn giá trị | Ràng buộc | Bằng chứng |
|---|---|---|---|---|
| `bundle` | `android.os.Bundle` | Saved activity state do Android truyền | Có thể null theo Android lifecycle; decompiler không hiện annotation | `ev_d1e8e...` |

- Hành vi liên quan startup: gắn listener cho `btn_start`; listener lấy `PackageManager.getLaunchIntentForPackage(mainActivity.A)` và gọi `startActivity()` nếu intent tồn tại.
- `mainActivity.A` được điền bởi `t()` từ package Pokémon GO tìm thấy. Nút start chỉ launch game; không mở socket tới core, không attach PID và không chờ handshake runtime trong method này.
- Cùng method cũng tạo/xóa selector file trong `filesDir` khi enable/disable core và gọi callback cài module ở trạng thái tương ứng.
- Kết quả hoàn tất: activity/UI được thiết lập; `startActivity()` chỉ yêu cầu Android mở app đích, không chứng minh Zygisk/core đã nạp xong.

### `m.u.va.man.MainActivity.onResume` — Đã xác nhận tĩnh

- Nguồn: REA `ev_7f1f02e7ff87243c0a1bf04d56cf6ce85ea32342cb37a8da6db49f76377bee09`.
- Signature: `onResume(): void`; không có param.
- Hành vi: gọi `super.onResume()` rồi `t()` để refresh trạng thái manager.
- Không có IPC/runtime handshake quan sát được trong body.

### `m.u.va.man.MainActivity.t` — Đã xác nhận tĩnh

- Nguồn: REA `ev_cf6e34e02f695c61c1c1567cccc7f3e0643c1cbcf059240e4fa8010177d192b`.
- Signature: `t(): void`; không có param.
- Hành vi xác nhận:
  - đọc trạng thái/version module từ file hoặc root command làm rối qua `gs(...)`;
  - dùng `PackageManager.getPackageInfo(gs(0), 0)` rồi fallback `gs(22)` để nhận package/version của hai biến thể game;
  - kiểm core/version support qua `u(version)` và `v(version, channel)`;
  - kiểm selector file trong `filesDir`, xóa nếu core/game version không khớp preference;
  - đặt UI state rồi gọi `F()`.
- Kết quả: cập nhật state manager (`D`, `A`, `f4525z`, `f4524y`, `E`) và UI. Nó kiểm file/package/version, không kiểm một live Pokémon GO PID, injected-core readiness hay session handshake.
- Giới hạn: các chuỗi từ `gs(id)` chưa được giải mã trong Evidence này; ý nghĩa một số state number dựa trên cách `F()` render.

### `m.u.va.man.MainActivity.z` và `F` — Đã xác nhận tĩnh

- Nguồn: `z()` ở REA `ev_1bb7ae39f3818625d801e3a7dc38153a7b43163a9cef3943a5d78b8d1f063bcd`; `F()` ở `ev_3fef6d9b5fe09752c68fbc45404dbb35bb9615fef2a1f6a461635f7359eadbf7`.
- Signatures: `z(): void`, `F(): void`; không có param.
- `z()` bật refresh indicator, gọi `t()`, rồi chạy background `e3.k`.
- `F()` render game/core/manager version, trạng thái install/update/enable/reboot và chỉ hiện `btn_start` khi selector/core state `E` hợp lệ.
- Đây là readiness của **manager/file/version**, không phải readiness của core đang sống trong game process.

### `e3.i.a` — Đã xác nhận tĩnh, decompile có cảnh báo CFG

- Nguồn: REA `ev_5bf134c185993fc0371c2a83347473bcdd621d55ccd2ca32e12d50662fe4b134`.
- Signature: `a(g2.b): void`.

| Param | Kiểu / modifier | Ý nghĩa / nguồn giá trị | Ràng buộc | Bằng chứng |
|---|---|---|---|---|
| `bVar` | `g2.b` | Root shell/session object của manager | Phải báo usable qua `bVar.f()` để đi vào nhánh kiểm/cài | `ev_5bf134...` |

- Hành vi xác nhận: kiểm backend/root/Zygisk bằng command string từ `gs(id)`; gọi `no(0, filesDirPath)` để materialize ZIP module tạm; chọn command install theo backend; chạy lệnh, xóa ZIP tạm, rồi chuyển UI sang trạng thái reboot (`f4524y = 6`) khi phù hợp.
- Kết quả/side effect: thay đổi hệ thống qua root command và file tạm; không trực tiếp attach một game PID.
- Giới hạn: JADX báo duplicated/removed block; exact nhánh command phải được xem là partial dù cấu trúc cài module và cleanup đã hiện rõ.

### `MainActivity.gs` và `MainActivity.no` — Native boundary đã xác nhận, implementation chưa mở lại

- Nguồn signature: REA `ev_384b2575d0f48acaecf1816412cba882b63c522996d99608869debd1e56b6c72`.
- Signatures: `native String gs(int id)` và `native Object no(int operation, Object[] args)` (JADX hiển thị varargs thành mảng).
- `gs.id`: integer chọn chuỗi làm rối; mapping đầy đủ chưa xác minh trong retained Evidence mới.
- `no.operation`: integer chọn native operation; `operation=0` được caller `e3.i.a` dùng với `filesDirPath` để nhận đường dẫn ZIP module tạm. Các operation khác chưa cần cho câu hỏi startup.
- Return: `gs` trả `String`; `no` trả `Object`, caller cast thành `String` ở operation 0.
- Chưa xác minh exact native function address/call graph trong `libman.so` ở lượt này.

### Zygisk loader PGS — Historical reference, chưa có function dossier mới

- `zygisk_module_entry`, `zygisk_companion_entry`, các callback tương ứng `preAppSpecialize`/`postAppSpecialize`, `dlopen`, `android_dlopen_ext` và core entry `__cxa_guard_enter` được mô tả trong `reverse/reports/pgsroot-1.0.15-phan-tich-inject-pokemon-go.vi.md`.
- Theo report, `preAppSpecialize` lọc `nice_name` chứa `com.nianticlabs.pokemongo`; `postAppSpecialize` dựng đường dẫn trong app data, `dlopen` core và truyền loader symbol cho entry core.
- Param/ABI, RVA, body range và exact caller/callee chưa được nâng thành retained REA function Evidence trong lượt này. Do REA extraction bị chặn bởi case-collision của APK trên filesystem hiện tại, không dùng report để khẳng định thêm chi tiết opcode hoặc runtime postcondition.

### Kết luận ngắn của bước 3

PGS Root manager **không thực hiện một “connection startup” kiểu controller ↔ game runtime** trong các method đã quan sát. Nó chuẩn bị module/core/selector, hiển thị trạng thái, và nút Start chỉ launch package game. Việc đưa core vào process thuộc callback Zygisk ở lúc process được tạo; manager UI không phải client của một live session giống `RuntimeBridgeClient` trong repository này.

## Bước 4 — Luồng hành vi và điểm tích hợp

### Luồng PGS Root/PGSharp theo bằng chứng hiện có

1. `MainActivity.t()` kiểm module, game version, core và selector file; `F()` chỉ render manager state.
2. Khi user bấm Start, listener trong `MainActivity.onCreate(Bundle)` gọi `getLaunchIntentForPackage(A)` rồi `startActivity()`; không có handshake manager ↔ core.
3. Khi Android tạo process Pokémon GO, Zygisk gọi loader ở app-specialization. Theo report lịch sử, loader lọc process ở callback tương ứng `preAppSpecialize`, rồi `postAppSpecialize` `dlopen` core và đưa `android_dlopen_ext` cho core để hook thời điểm các thư viện game được load.
4. Manager có thể đóng hoặc không kết nối live; core đã ở trong process game và tự tiếp tục initialization.

Điểm chưa xác minh: core báo trạng thái về manager bằng cách nào, exact companion opcode, và phản ứng runtime nếu manager mở sau khi game đã chạy. Không có bằng chứng về một socket session tương đương `RUNTIME_READY`.

### Luồng hiện tại của `pogo-root-automation`

1. Zygisk gọi `PogoAutomationModule.onLoad(api, env)` rồi `preAppSpecialize(args)` trong process đang được tạo. `preAppSpecialize` so khớp đúng package/process, gọi `api_->connectCompanion()`, gửi `kTargetAttached`, rồi `exemptFd`; nếu companion connect/event/exemption lỗi thì đóng fd và chọn `DLCLOSE_MODULE_LIBRARY` (`zygisk/jni/shared/core/runtime_module.inc:3-101`).
2. `postAppSpecialize` tạo `ProbeContext`, khởi tạo main-thread bridge và spawn `binding_probe_thread`; không có đường attach muộn vào PID đã tồn tại (`runtime_module.inc:104-133`).
3. `binding_probe_thread` poll `/proc/self/maps` mỗi 500 ms tối đa 600 lần (tối đa khoảng 300 giây) để đợi `libil2cpp.so`/`libunity.so`; khi có IL2CPP exports hoặc hết budget, nó mới gửi event `kBindingProbe | kProbeComplete` (`runtime_binding_probe.inc:14-48`; constants ở `runtime_native_prelude.inc:57-65`).
4. Companion tạo `runtime_session_id` ngay khi nhận `kTargetAttached`, nhưng chỉ gọi `start_runtime_bridge_broker(...)` sau khi nhận `kProbeComplete` (`runtime_companion.inc:132-182`). Vì vậy abstract socket controller **chưa tồn tại trong toàn bộ cửa sổ probe ban đầu**.
5. Broker bind `pogo_root_automation_runtime`, chấp nhận controller, xác thực UID và gửi một `RUNTIME_READY` ban đầu với fingerprint `unverified|<process>` và capability rỗng (`runtime_bridge_broker.inc:266-342`; `runtime_bridge_protocol.inc:149-153`). Khi native gửi capability update về sau, broker mới gửi một `RUNTIME_READY` cập nhật có strong identity/capabilities (`runtime_bridge_broker.inc:75-100`).
6. Kotlin `RuntimeBridgeClient.connect()` đăng ký controller UID, connect abstract socket, chạy reader và chờ `RUNTIME_READY` tối đa 3 giây. Socket chưa tồn tại hoặc ready không đến trong timeout thì `disconnect()` và trả failure (`app/.../root/RuntimeBridgeClient.kt:78-114`).
7. `RuntimeUiClient.connect()` giữ desired snapshot và resubmit khi reconnect/session đổi (`RuntimeUiClient.kt:44-61`). `RuntimeUiEventRouter.ensureSession()` gọi connect mỗi tick khi service còn sống (`RuntimeUiEventRouter.kt:37-43,61-71`).
8. Tuy nhiên `RuntimeUiEventRouter.tick()` khi connect lỗi gọi `reset(...)`; callback `onRuntimeSessionLost` của facade gọi `onRuntimeInactive`, và `HeadlessAutomationService` thực hiện `stopSelf()` (`RuntimeUiEventRouter.kt:37-53`; `RuntimeUiAutomationFacade.kt:62-68`; `HeadlessAutomationService.kt:67-75`). Do đó đây không phải retry bền vững: lỗi kết nối đầu tiên có thể dừng service trước khi game/broker ready.
9. Service lại được start ở `BOOT_COMPLETED` nếu module state cho phép và khi MainActivity foreground nhận install snapshot (`AutomationBootReceiver.kt:8-14`; `MainActivity.kt:41-44`). Không có code app-side launch Pokémon GO. Vì vậy boot/controller-first dễ xảy ra trước game; sau khi service tự dừng, việc user mở game độc lập không tự khởi động lại controller.

### So sánh trực tiếp

| Khía cạnh | PGS Root/PGSharp | `pogo-root-automation` | Đánh giá |
|---|---|---|---|
| Điểm inject | Zygisk app specialization, rồi `dlopen` core trong game | Zygisk app specialization, runtime đã nằm trong module và spawn probe thread | Cùng họ kiến trúc; không phải attach PID muộn |
| Đợi thư viện game | Core được đưa `android_dlopen_ext` để hook loader theo report | Poll memory mappings 500 ms, tối đa 300 s | Khác kỹ thuật; polling làm broker xuất hiện trễ |
| Vai trò manager/controller | Chuẩn bị core/module/selector, launch game; chưa thấy live session handshake | Live UI client phải kết nối broker, nhận session, gửi desired snapshot, render native state | Khác rõ ràng và có chủ đích |
| Khi controller mở trước game | Manager vẫn usable và có nút launch game; không cần socket core để tồn tại | Service connect ngay; failure có thể reset và `stopSelf()` | Đây là startup race thực sự |
| Khi game đã chạy đúng module từ đầu | Core đã được inject; manager không cần attach lại | Controller có thể connect nếu broker đã qua probe và socket tồn tại | Hoạt động được, nhưng có cửa sổ 3 s timeout so với probe trễ |
| Khi game đã chạy trước lúc module có hiệu lực | Cần restart process/reboot theo vòng đời Zygisk; không có bằng chứng attach muộn | Cũng phải restart game; Zygisk không retroactively gọi specialize callback | Không phải khác biệt; đây là giới hạn chung |
| Readiness | Manager file/version state không chứng minh core live | Có session, identity, capability, UI status tách lớp | Repo hiện tại chặt chẽ hơn, nhưng lifecycle client đang quá “one-shot” |

### Race/failure branches

- **Controller-first/boot-first:** service connect khi socket chưa có → failure trong vài mili-giây hoặc timeout 3 giây → router reset → service dừng. Game mở sau đó không đánh thức service. Đây là khác biệt quan trọng nhất so với flow PGS.
- **Game-first nhưng IL2CPP load chậm:** native module đã inject nhưng broker chưa bind cho tới `kProbeComplete`; controller nhìn giống “module không chạy” dù thực tế probe đang chờ.
- **`connectCompanion()` lỗi ở `preAppSpecialize`:** module tự unload khỏi process và không retry trong cùng PID. Chỉ restart game/session mới có callback specialization mới.
- **Initial `RUNTIME_READY` yếu:** controller có thể kết nối thành công nhưng capability rỗng/identity chưa mạnh; đây không phải action-ready. Thiết kế native/UI status hiện có xử lý fail-closed, nhưng UI/notification “Runtime controller connected” có thể tạo cảm giác sẵn sàng sớm.
- **Controller reconnect sau khi broker đã sẵn sàng:** broker replay latest UI status, không replay command/result; `RuntimeUiClient` resubmit desired snapshot. Đây là đúng theo contract và tốt hơn việc suy từ status file.

### Kết luận bước 4

Nhận định của người dùng là **đúng ở tầng startup/control**: flow hiện tại không giống PGS Root. Hai bên đều dùng Zygisk ở lúc process Pokémon GO được tạo, nhưng PGS manager chuẩn bị payload rồi launch game và không phụ thuộc live socket; repo hiện tại khởi động một controller service độc lập và yêu cầu handshake với broker. Vấn đề không phải “thiếu attach vào game đang chạy” nếu module đã được inject; vấn đề chính là controller bỏ cuộc quá sớm trong khi broker chỉ xuất hiện sau probe.

## Bước 5 — Hướng xử lý

### Phương án A — Giữ service ở trạng thái chờ và reconnect có backoff (khuyến nghị)

Thay đổi lifecycle controller, không đổi transport và không bắt chước hook loader của PGS.

- `RuntimeUiEventRouter` cần phân biệt:
  - **chưa từng có session:** connect failure là `WAITING_FOR_RUNTIME`, không gọi `onRuntimeSessionLost`;
  - **đã có session rồi mất:** disarm/reset state theo session, nhưng báo một transient disconnect để service tiếp tục chờ;
  - **user stop / module install invalid / fatal protocol-auth mismatch:** mới dừng service.
- `RuntimeUiAutomationFacade` giữ poll sống, thêm reconnect backoff có giới hạn (ví dụ 0.5 s → 1 s → 2 s → 5 s), reset backoff khi nhận session mới. Không retry gameplay command; chỉ retry transport handshake và sau đó resubmit **full desired snapshot** theo contract hiện có.
- `HeadlessAutomationService.onRuntimeInactive` không `stopSelf()` cho socket unavailable, timeout, process exit hoặc session turnover. Service render `WAITING_FOR_GAME`/`WAITING_FOR_RUNTIME`; chỉ explicit `ACTION_STOP_SERVICE` hoặc module state không còn `gameplayAllowed` mới dừng.
- Catch/spin arm vẫn process/session-local. Khi session đổi hoặc mất, `CatchSpinArmState` phải disarm; reconnect không được tự arm session mới dù config bền vững còn `enabled=true`.
- `RuntimeBridgeClient` tiếp tục đăng ký UID, chờ `RUNTIME_READY`, kiểm protocol/session/identity và fail closed. Không đọc `runtime.status` để thay thế socket readiness.

API đề xuất — chưa tồn tại:

```kotlin
enum class RuntimeConnectionState {
    WAITING_FOR_GAME,
    CONNECTING,
    CONNECTED_UNVERIFIED,
    LIVE_READY,
    TRANSIENT_DISCONNECTED,
    FATAL,
}

sealed interface RuntimeConnectionFailure {
    data class Transient(val reason: String) : RuntimeConnectionFailure
    data class Fatal(val reason: String) : RuntimeConnectionFailure
}
```

Tên cụ thể có thể điều chỉnh theo model hiện có; mục tiêu là không dùng cùng callback `onRuntimeSessionLost` cho cả “chưa có session” và “session vừa mất”.

**Ưu:** thay đổi nhỏ nhất, phù hợp boundary hiện tại, giải quyết controller-first/game-start-slow, không đụng native hook.
**Nhược:** notification/service tồn tại khi game chưa mở; cần backoff và trạng thái UI để tránh spam/log/battery.

### Phương án B — UX gần PGS: enable/prepare rồi launch game, sau đó chờ session

- Thêm action UI rõ ràng “Mở Pokémon GO” bằng package launch intent sau khi module state/core config hợp lệ.
- Sau launch, vẫn dùng reconnect/backoff của phương án A; launch intent không chứng minh injection/readiness.
- Không dùng root shell để mở app, không force-stop game và không tự restart process nếu user không yêu cầu.

**Ưu:** trình tự dễ hiểu giống PGS manager: prepare → launch game → core/runtime attach.
**Nhược:** không giải quyết độc lập race nếu thiếu A; app có hai biến thể package, background-launch restriction và UX ownership cần xử lý; tự launch game không phải yêu cầu bắt buộc của framework.

### Phương án C — Bind controller broker ngay từ `kTargetAttached`

- Refactor companion thành state machine/multiplexer duy nhất: tạo session và bind socket ngay khi nhận target attached; gửi `RUNTIME_READY` dạng `ATTACHING/PROBE_PENDING`; cùng event loop tiếp tục nhận probe/capability/status và controller frames.
- Không được có hai thread cùng đọc một `runtime_fd`; hiện `companion_handler` chuyển ownership fd cho broker sau `kProbeComplete`, nên đây là refactor protocol/lifecycle thật sự chứ không phải dời một dòng gọi hàm.
- Controller chỉ gửi desired snapshot sau session; native giữ intent nhưng không apply mutation cho tới strong identity/map readiness.

**Ưu:** UI kết nối gần như ngay khi process game được specialize và thấy tiến độ thật, giảm mơ hồ trong cửa sổ probe.
**Nhược:** thay đổi native companion/broker lớn, rủi ro fd ownership/race/session replacement; vẫn không giúp nếu game chưa được tạo. Nên làm sau A nếu diagnostics tức thời thực sự cần.

### Phương án D — Hook `android_dlopen_ext` giống report PGS

Không khuyến nghị cho lỗi này. Có thể rút ngắn/định thời chính xác việc thấy `libil2cpp.so`, nhưng thêm native hook/trampoline và compatibility risk; không giải quyết việc Kotlin dừng service khi chưa có game. Poll mapping hiện đã có budget dài và fail-closed.

### Hướng chọn

1. Làm **A** trước: sửa semantics waiting/reconnect và giữ arm fail-closed.
2. Nếu muốn UX giống PGS, thêm **B** như tiện ích UI độc lập.
3. Chỉ cân nhắc **C** khi cần telemetry `ATTACHING` trước IL2CPP; không bắt buộc cho kết nối ổn định.
4. Không chọn **D** chỉ để “giống PGSharp”. Kiến trúc đúng không cần sao chép kỹ thuật hook nếu lifecycle controller đã hội tụ ổn định.

### Guards bắt buộc

- Runtime session ID do companion cấp; session mới luôn reset observation sequence, pending route và manual arm.
- Reconnect chỉ replay/resubmit desired config snapshot; không replay gameplay command/result.
- Socket connected/initial `RUNTIME_READY` yếu chỉ là `CONNECTED_UNVERIFIED`; mutation vẫn cần exact build, capability và native `RuntimeUiStatus.ready`.
- Auth/protocol mismatch, malformed frame và identity mismatch là fatal cho connection attempt, không retry nhanh vô hạn.
- Process exit là transient đối với service lifecycle nhưng terminal đối với session state; mọi pointer/pending action cũ phải bị loại bỏ.
- `runtime.status` vẫn chỉ là diagnostics; không dùng để authorize config/action hoặc dựng session giả.

## Bước 6 — Tiêu chí chấp nhận và xác minh

### Tiêu chí chấp nhận

| ID | Nguồn yêu cầu | Điều kiện đầu vào | Hành vi / postcondition | Method liên quan | Cách kiểm chứng |
|---|---|---|---|---|---|
| AC-01 | Đề xuất từ yêu cầu | Controller service chạy khi Pokémon GO chưa mở | Service ở `WAITING_FOR_GAME`/tương đương, không `stopSelf()`, không tạo session giả, không dispatch gameplay | `RuntimeUiAutomationFacade.pollRuntime`, `RuntimeUiEventRouter.tick/reset`, `HeadlessAutomationService.onRuntimeInactive` | Manual BlueStacks Air 1: mở controller trước, chờ lâu hơn connect timeout, xác nhận service/notification còn sống và automation chưa armed |
| AC-02 | Đề xuất từ yêu cầu | Sau AC-01, user mở Pokémon GO và Zygisk attach thành công | Controller tự kết nối khi broker xuất hiện, nhận session thật và gửi lại latest desired snapshot đúng một cách idempotent | `RuntimeBridgeClient.connect`, `RuntimeUiClient.connect/submitDesiredState` | Quan sát UI session/status; chỉ thu log bằng `./scripts/logcat-full.sh` nếu flow thất bại |
| AC-03 | Đề xuất từ yêu cầu | Pokémon GO mở trước, broker đã ready; controller mở sau | Kết nối trong cùng runtime session, broker replay latest UI status; không cần restart game | `handle_controller`, `RuntimeUiClient.connect` | Manual game-first trên BlueStacks Air 1; so session ID/status trước và sau controller connect |
| AC-04 | Ràng buộc fail-closed | Game process chết hoặc session đổi | Old session/pending route/observation/arm bị reset; config bền vững được giữ nhưng session mới không tự arm | `RuntimeUiEventRouter.reset/ensureSession`, `CatchSpinArmState`, `RuntimeUiClient.connect` | Kết thúc và mở lại game theo handoff; session ID phải đổi, Auto Catch vẫn OFF cho tới explicit arm |
| AC-05 | Ràng buộc protocol | Socket connect nhưng initial `RUNTIME_READY` có weak identity/capability rỗng | UI hiển thị attached/waiting; mutation permission vẫn false cho tới native status/capability mạnh | `send_runtime_ready`, `send_runtime_ready_with_values`, `RuntimeUiAutomationFacade.snapshot` | Kiểm UI/status; diagnostics chỉ đọc; không dùng ACK/connect làm success gameplay |
| AC-06 | Ràng buộc security | UID auth, protocol, frame hoặc identity mismatch | Connection attempt fail closed và được phân loại fatal/slow retry; không loop 500 ms vô hạn và không gửi desired/action vào session sai | `authorized_controller`, `RuntimeBridgeClient.connect/readLoop`, proposed failure classification | Existing protocol checks + failure diagnostics; không cần tạo test mới theo project rule |
| AC-07 | UX gần PGS (tùy chọn) | User chọn “Mở Pokémon GO” từ controller | Dùng Android launch intent cho đúng installed variant; controller vẫn chờ session, không coi launch success là runtime ready | API UI đề xuất ở phương án B | Manual trên BlueStacks Air 1; cancel/missing package không làm service crash |
| AC-08 | Ranh giới kiến trúc | Controller chưa kết nối hoặc reconnect | Không file queue, preference polling hay raw ADB transport; `runtime.status` chỉ diagnostics | Toàn bridge path | Source review đối chiếu `docs/ARCHITECTURE.md` và `git diff --check` |

### Kế hoạch xác minh khi triển khai

1. Chạy focused existing tests liên quan lifecycle/bridge nếu đã có trong module; không thêm hoặc sửa test theo quy tắc repository.
2. Chạy host native protocol checks hiện có:

   ```bash
   c++ -std=c++17 -Wall -Wextra -Werror -Izygisk/jni \
     zygisk/jni/runtime_command_protocol_test.cpp \
     -o /tmp/runtime_command_protocol_test
   /tmp/runtime_command_protocol_test

   c++ -std=c++17 -Wall -Wextra -Werror -Izygisk/jni \
     zygisk/jni/runtime_observation_protocol_test.cpp \
     -o /tmp/runtime_observation_protocol_test
   /tmp/runtime_observation_protocol_test
   ```

3. Chạy `./gradlew test assembleDebug` và `git diff --check`.
4. Nếu có thay đổi native/module, dùng đúng `ANDROID_NDK="$HOME/Library/Android/sdk/ndk/27.1.12297006" ./scripts/build-magisk.sh`.
5. Handoff manual chỉ trên **BlueStacks Air 1**, thực hiện đủ ba thứ tự: controller-first, game-first, session restart. Chỉ khi thất bại mới chạy `./scripts/collect-binding-diagnostics.sh` và `./scripts/logcat-full.sh`; không dùng raw ADB.

### Kết luận và câu hỏi còn mở

- **Đã xác nhận:** cách startup/control hiện tại không giống PGS Root manager. PGS chuẩn bị payload rồi launch game; core được Zygisk nạp trong app-specialization và manager không có live `RUNTIME_READY` handshake quan sát được. Repo này tách controller thành client của một broker/session sống.
- **Lỗi/rủi ro chính trong repo:** broker chỉ bind sau initial IL2CPP probe, trong khi Kotlin có connect timeout 3 giây và đường failure hiện tại có thể `stopSelf()` ngay lần đầu. Controller-first và slow game startup vì vậy không tự hội tụ.
- **Không phải lỗi:** nếu game đã được tạo trước khi module Zygisk có hiệu lực, cả hai kiến trúc đều cần tạo lại process game; không có attach retroactive từ source/bằng chứng hiện có.
- **Hướng khuyến nghị:** giữ service ở trạng thái waiting, reconnect có backoff, phân loại transient/fatal, resubmit desired snapshot khi có session mới, và luôn disarm manual arm qua session boundary. Không cần hook `android_dlopen_ext` chỉ để giống PGSharp.
- **Giới hạn:** chưa có runtime capture của PGS Root/core và chưa có exact function dossier cho loader nhúng; nhận định về loader/opcode dựa trên report lịch sử có ghi rõ phạm vi. Cần device verification để biết triệu chứng hiện tại có đúng là race này hay còn lỗi `connectCompanion`/auth/binding riêng.
