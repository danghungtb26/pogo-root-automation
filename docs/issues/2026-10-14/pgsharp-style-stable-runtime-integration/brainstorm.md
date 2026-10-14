# Brainstorm: Tích hợp runtime ổn định và đúng thời điểm theo hướng PGSharp

**Loại:** architecture
**Ngày phân tích đầu tiên:** 2026-10-14
**Phạm vi game/build/ABI:** Pokémon GO 0.433.0; Android/Zygisk; arm64-v8a và x86_64 của project; artifact tham chiếu PGS Root 1.0.15 ARM64 và libhela 1.273.1

## Bước 1 — Yêu cầu và phạm vi

### Vấn đề cần giải quyết

Thiết kế lại đường tích hợp native vào Pokémon GO để runtime được nạp ổn định, bắt đúng thời điểm Unity/IL2CPP lifecycle và không phụ thuộc vào những giả định yếu của kiến trúc startup hiện tại. Kiến trúc cũ của project không được xem là ràng buộc phải giữ nếu nó cản trở mục tiêu này.

Phân tích trước tại [startup-connection-vs-pgsharp](../startup-connection-vs-pgsharp/brainstorm.md) đã xác nhận:

- PGS Root dùng Zygisk app specialization để nạp một core riêng vào process game.
- Core nhận `android_dlopen_ext` và theo report lịch sử cài hook loader để bắt đúng lúc thư viện game được nạp.
- Project hiện tại spawn một thread poll memory mappings, chỉ tạo controller broker sau initial probe, còn Kotlin có thể dừng service khi connect sớm thất bại.

Lượt này không chỉ sửa reconnect Kotlin. Cần đánh giá lại toàn bộ chuỗi:

```text
Zygisk specialization
→ nạp runtime/core
→ quan sát library lifecycle
→ khởi tạo IL2CPP đúng thời điểm
→ resolve build binding
→ công bố runtime/session/readiness
→ controller đồng bộ config
```

### Hành vi mong muốn

1. Runtime native có mặt trong process game ngay từ app specialization.
2. Việc kích hoạt logic IL2CPP bám vào một lifecycle signal xác định, không dựa chủ yếu vào polling `/proc/self/maps` hoặc timeout cố định.
3. Controller có thể kết nối và thấy state `ATTACHING` sớm, nhưng gameplay chỉ mở khi exact build, IL2CPP runtime, main thread, owner lifetime và capability đều được xác minh.
4. Controller mở trước/sau game hoặc bị restart đều hội tụ về cùng một session đúng; không cần restart controller để bắt game.
5. Game process mới tạo session mới; state/pointer/pending action cũ không được reuse.
6. Thiết kế hỗ trợ cả ARM64 thật và x86_64/translation target mà project cam kết, hoặc fail closed rõ ràng theo ABI.

### Ràng buộc cứng

- Không repack Pokémon GO, không sửa APK game và không attach PID muộn bằng injector ngoài.
- Gameplay/runtime object vẫn do native trong process game sở hữu; Kotlin chỉ UI/config/fake-location theo `AGENTS.md`.
- Exact build/capability/session/main-thread/freshness guards phải giữ hoặc mạnh hơn.
- Không dùng file queue/SharedPreferences polling làm transport gameplay.
- Bằng chứng về PGSharp/PGS phải đến từ artifact/REA; report lịch sử chỉ là giả thuyết cho tới khi được nâng bằng Evidence phù hợp.
- Không triển khai trong lượt phân tích này.

### Ràng buộc có thể thay đổi

- Thời điểm tạo broker socket và cách companion quản lý session.
- Việc runtime nằm chung trong Zygisk module hay tách thành core `.so` riêng.
- Cơ chế phát hiện `libil2cpp.so`/`libunity.so` và trigger bootstrap.
- Kotlin foreground-service lifecycle, startup sequencing và UX launch game.
- Wire event/readiness state nếu cần version mới.

### Tiêu chí hoàn tất ban đầu

1. Xác định chính xác phần nào trong timing của PGS đáng học theo và phần nào không nên sao chép.
2. So sánh ít nhất ba kiến trúc mục tiêu với trade-off về stability, timing, ABI, maintainability và fail-closed behavior.
3. Chọn một phương án tốt nhất, mô tả owner/thread/lifecycle/data flow và API mới dự kiến.
4. Có lộ trình thay thế kiến trúc cũ theo phase, rollback boundary và tiêu chí xác minh trên BlueStacks Air 1.

### Giả định và câu hỏi còn thiếu

- “PGSharp” trong phạm vi artifact local được đại diện bởi PGS Root 1.0.15 + core libhela 1.273.1 cho PoGo 0.433.0; không khẳng định mọi version PGSharp dùng cùng implementation.
- Chưa có runtime capture của PGS loader/core trên thiết bị. Static evidence có thể chứng minh điểm vào/hook shape nhưng không tự chứng minh mọi race đã được xử lý.
- Cần xác định liệu hook loader trực tiếp có ổn định trên cả x86_64/translation target hay nên dùng linker callback/interposition khác theo ABI.

## Bước 2 — Bằng chứng từ reverse

### Nguồn đã kiểm lại

| Nguồn | Identity / Evidence | Điều chứng minh | Giới hạn |
|---|---|---|---|
| PGS Root manager | APK SHA-256 `2e0f328a15094ebc0f2f91cfac72fb94024abdce95ac48b229b4be7b3dce7297`; REA inventory `ev_0248662389cf472ffa1f5c7a1d0a0cad9b3b8a48b16952e74fa6b23580854f08`; JADX package `ev_6b5217b536c9152d88d60ad0ef68875ed270484cc3a36437aea220a2c91a0c9c` | Package `m.u.va.man`, chỉ có launcher activity cho manager, không có service injection riêng | Static; không quan sát Zygisk runtime |
| Manager bootstrap | `MainActivity.<clinit>` `ev_b6077421d51f51e6194d16f1cffe947acf0773df4bded030b25b750025c795cf` | Manager load `libman.so` trong chính process manager | Không phải core game |
| Manager start flow | `MainActivity.onCreate` `ev_d1e8e756d9510c239c7e324d198ae71ca20fc301f6fc9cd97005e622c40cac3a`; `t()` `ev_cf6e34e02f695c61c1c1567cccc7f3e0643c1cbcf059240e4fa8010177d192b`; `F()` `ev_3fef6d9b5fe09752c68fbc45404dbb35bb9615fef2a1f6a461635f7359eadbf7` | Manager kiểm module/core/game version/selector; nút Start gọi package launch intent. Không có live controller↔core handshake trong các method này | Static source reconstruction; exact native strings vẫn làm rối |
| Module install flow | `e3.i.a(g2.b)` `ev_5bf134c185993fc0371c2a83347473bcdd621d55ccd2ca32e12d50662fe4b134` | Materialize ZIP module tạm, cài qua root backend, yêu cầu reboot khi phù hợp | JADX có warning duplicated CFG; command strings nằm sau `gs(id)` |
| Core PGS cho PoGo 0.433.0 | `reverse/artifacts/libhela-1.273.1/libhela-1.273.1-pogo-0.433.0.so`; SHA-256 `893b84c077942b988e3419eeac54d973a35f9dd90a65e79b33bd894f081a6ef2` | Artifact identity đã pin và hash đã xác nhận bởi `SHA256SUMS` | Chưa có retained function dossier cho bootstrap entry |
| Loader/core report | `reverse/reports/pgsroot-1.0.15-phan-tich-inject-pokemon-go.vi.md` | Historical analysis mô tả `postAppSpecialize → dlopen(libhela.so) → dlsym(__cxa_guard_enter) → truyền android_dlopen_ext → hook loader` | Historical reference, không thay thế function Evidence/runtime capture |

### Retained evidence và correction quy trình

- Bundle `reverse/rea/rea-evidence-2026-10-14-startup-connection.json` có 48 record và 4 unknown revision, đã đủ cho câu hỏi kiến trúc này.
- Một focused search mới trên `libhela` đã bị khởi chạy không cần thiết và được dừng; các timeout/partial output không được dùng làm evidence, không được export vào bundle và không tham gia kết luận.
- Residual unknown vẫn là câu hỏi đã có từ phân tích trước: exact body/ABI/call path của bootstrap entry và callback xác nhận library readiness. Theo rule của project, unknown này được giữ mở thay vì tiếp tục analyze khi chưa cần cho quyết định kiến trúc.

### Điều có thể học chắc chắn từ PGS ở thời điểm này

1. **Tách loader nhỏ khỏi core lớn:** Zygisk loader làm công việc sớm, tối thiểu; core gameplay là `.so` riêng được nạp sau specialization.
2. **Đưa lifecycle signal vào core:** loader truyền điểm vào dynamic loader cho core, thay vì để manager đoán game đã sẵn sàng.
3. **Manager không nằm trên critical path runtime:** manager chuẩn bị/install/select core và launch game; core trong process tự bootstrap.
4. **Version selection xảy ra trước process startup:** core phù hợp game version/ABI được chuẩn bị trước khi user launch game.

### Điều chưa được phép kết luận

- Không khẳng định PGSharp hook `android_dlopen_ext` là nguyên nhân duy nhất tạo stability; chưa có runtime timing/postcondition capture.
- Không biết hook được cài bằng inline patch, PLT/GOT, trampoline library hay cơ chế khác trong core hiện tại.
- Không biết exact event mà core dùng sau khi thấy `libil2cpp.so`: return từ `dlopen`, `JNI_OnLoad`, `il2cpp_init`, Unity player loop hay một chuỗi nhiều gate.
- Không có bằng chứng x86_64 tương đương cho PGS Root 1.0.15 local artifact; không thể lấy ARM64 hook bytes làm thiết kế portable.

### Kết luận ngắn của bước 2

Phần đáng học theo không phải tên symbol ngụy trang hay code hook cụ thể, mà là kiến trúc **early loader + lifecycle-driven core bootstrap + manager off critical path**. Exact hook implementation vẫn là unknown, nên phương án đích phải định nghĩa contract lifecycle của project và cho phép backend khác nhau theo ABI, thay vì copy một trampoline ARM64 chưa xác minh.

## Bước 3 — Function/method, param và hành vi

### PGS manager và loader — dùng retained evidence, không probe lại

- `MainActivity.<clinit>(): void` nạp `libman.so` trong process manager; Evidence `ev_b6077421d51f51e6194d16f1cffe947acf0773df4bded030b25b750025c795cf`.
- `MainActivity.onCreate(Bundle): void` nối nút Start với `getLaunchIntentForPackage(...)`/`startActivity(...)`; Evidence `ev_d1e8e756d9510c239c7e324d198ae71ca20fc301f6fc9cd97005e622c40cac3a`. Đây là launch game, không phải late attach vào PID.
- `MainActivity.t(): void` và `F(): void` kiểm package/version/core selector rồi render readiness của manager; Evidence `ev_cf6e34e02f695c61c1c1567cccc7f3e0643c1cbcf059240e4fa8010177d192b` và `ev_3fef6d9b5fe09752c68fbc45404dbb35bb9615fef2a1f6a461635f7359eadbf7`.
- `e3.i.a(g2.b): void` materialize/cài module qua root backend; Evidence `ev_5bf134c185993fc0371c2a83347473bcdd621d55ccd2ca32e12d50662fe4b134`.
- Historical report mô tả Zygisk `postAppSpecialize` nạp core `libhela.so` rồi truyền địa chỉ dynamic-loader vào entry của core. Exact ABI, trampoline và callback sau load vẫn là residual unknown đã ghi ở phân tích trước; không cần giải unknown này để chọn kiến trúc.

### Runtime hiện tại của project

| Function/method | Param quan trọng | Hành vi quan sát được | Hệ quả startup |
|---|---|---|---|
| `PogoAutomationModule::preAppSpecialize(AppSpecializeArgs *args)` | `args->nice_name` | Lọc exact process, `connectCompanion()`, gửi `kTargetAttached`, giữ/exempt FD | Điểm attach sớm là đúng; session đã có thể được tạo ngay tại đây |
| `PogoAutomationModule::postAppSpecialize(const AppSpecializeArgs *)` | Không dùng param | Khởi tạo bridge Java rồi tạo `binding_probe_thread` | Toàn bộ readiness phụ thuộc một worker polling |
| `binding_probe_thread(void *opaque_context)` | `ProbeContext*` giữ FD, PID, process | Tối đa `600` lần, mỗi lần `500 ms`, scan mapping `libil2cpp.so`/`libunity.so`, resolve exports; sau đó mới gửi `kProbeComplete`, register module và vào command loop | Có cửa sổ chờ tối đa khoảng 300 giây; không bám đúng lifecycle event |
| `probe_il2cpp_runtime(RuntimeEvent *, Il2CppApi *)` | Event và output API | Chỉ resolve ELF exports; chủ động không gọi managed API trong cửa sổ `il2cpp_init` | Safety đúng, nhưng thiếu signal xác nhận `il2cpp_init` đã hoàn tất |
| `companion_handler(int fd)` | Zygisk companion FD | Tạo session ở `kTargetAttached`, nhưng chỉ `start_runtime_bridge_broker(...)` khi nhận `kProbeComplete` | Session tồn tại nhưng controller chưa quan sát được trạng thái `ATTACHING` |
| `RuntimeBridgeClient.connect()` | socket abstract; timeout mặc định `3_000 ms` | Connect, chạy reader, chờ `RuntimeReady`, failure thì disconnect | Controller mở trước broker có thể fail do timing thay vì lỗi runtime thật |

Nguồn source: `zygisk/jni/shared/core/runtime_module.inc:9-133`, `zygisk/jni/shared/runtime/control/runtime_binding_probe.inc:1-82`, `zygisk/jni/shared/bridge_appproc/runtime_companion.inc:1-31,104-183`, `app/src/main/java/dev/pogoroot/automation/root/RuntimeBridgeClient.kt:45-115`.

### Contract còn thiếu

Project hiện có hai mốc quá thô: `TargetAttached` và `ProbeComplete`. Phương án ổn định cần một state machine đơn điệu, trong đó "library đã xuất hiện", "`il2cpp_init` đã return", "build đã verify", "main thread đã sẵn sàng" và "gameplay bindings đã sẵn sàng" là các mốc riêng. Hook loader chỉ được phát signal; mọi resolve/binding hoặc gọi Unity/IL2CPP phải chạy ngoài callback hook và đúng thread owner.

### Kết luận ngắn của bước 3

Điểm yếu không nằm ở việc Kotlin connect bằng socket, mà ở việc native chỉ công bố broker sau một polling gate và không có lifecycle gate chính xác sau `il2cpp_init`. Phần đáng thay là bootstrap contract và thời điểm publish state, không phải copy symbol/hook bytes chưa biết của PGSharp.

## Bước 4 — Luồng hành vi và điểm tích hợp

### Luồng tham chiếu PGS đã đủ bằng chứng

1. Manager kiểm module/core/version và để user launch Pokémon GO.
2. Khi process game được tạo, Zygisk loader chạy theo app-specialization.
3. Theo historical report, loader nhỏ nạp core riêng và cung cấp dynamic-loader entry cho core.
4. Core tiếp tục bootstrap trong process game; manager không nằm trên critical path.

Static evidence không cho phép khẳng định core hook bằng kỹ thuật gì hoặc gate cuối là gì. Vì vậy phương án của project chỉ học **shape của lifecycle**, không sao chép implementation chưa biết.

### Luồng đích đề xuất

```text
Zygisk preAppSpecialize
  -> TARGET_ATTACHED + tạo session/broker ngay
  -> lấy core FD đã verify theo ABI/build

Zygisk postAppSpecialize
  -> load core tối thiểu
  -> CORE_LOADED
  -> cài loader observer trước khi game load libil2cpp

loader callback thấy libil2cpp.so return thành công
  -> chỉ ghi event/đánh thức bootstrap worker
  -> worker resolve export + cài gate il2cpp_init

il2cpp_init wrapper
  -> gọi original
  -> original return thành công
  -> chỉ phát IL2CPP_INITIALIZED

bootstrap worker
  -> attach IL2CPP thread
  -> verify exact build/profile/descriptors
  -> cài Unity/main-thread bridge
  -> đợi owner/scene/map readiness trên main thread
  -> BINDINGS_READY / LIVE_READY

Kotlin controller (có thể đến trước hoặc sau)
  -> subscribe session state
  -> gửi full desired snapshot
  -> native chỉ apply tại gate phù hợp
```

### State machine phải là nguồn sự thật duy nhất

```text
CREATED
  -> CORE_LOADED
  -> LOADER_OBSERVER_ACTIVE
  -> IL2CPP_LIBRARY_SEEN
  -> IL2CPP_INITIALIZED
  -> BUILD_VERIFIED
  -> MAIN_THREAD_READY
  -> BINDINGS_READY
  -> LIVE_READY
```

- State chỉ tiến tới hoặc chuyển sang `FAILED_<reason>`; không dùng một boolean `ready` để đại diện mọi tầng.
- `TARGET_ATTACHED`/socket connected không cấp mutation permission.
- `IL2CPP_LIBRARY_SEEN` chỉ chứng minh ELF đã load; cấm gọi managed API.
- Chỉ sau `IL2CPP_INITIALIZED` worker mới được `il2cpp_thread_attach` và inspect domain.
- `BUILD_VERIFIED` không đồng nghĩa owner/scene đã sống; việc cần Unity main thread phải qua main-thread bridge.
- `LIVE_READY` được tính từ exact identity + capability + owner/lifecycle state, không từ ACK hoặc timeout.

### Ownership theo thread

| Context | Được làm | Không được làm |
|---|---|---|
| Zygisk callback | Lọc process, mở companion, nhận/verify core payload, load core | Resolve gameplay binding, gọi Unity/IL2CPP managed API |
| Dynamic-loader observer | So tên library, lưu handle/base, phát event không blocking | IPC dài, allocation phức tạp, scan class, gọi managed API |
| `il2cpp_init` wrapper | Gọi original đúng ABI, sau return phát event một lần | Discover binding hoặc chạy gameplay trước khi original hoàn tất |
| Bootstrap worker | Resolve export, attach/detach thread, verify build và descriptors | Giữ game object sống vô hạn hoặc gọi API main-thread-only trực tiếp |
| Unity/main thread bridge | Resolve owner sống, cài hook/action cần main thread | Quyết định policy từ Kotlin hoặc bỏ qua session/lease guard |
| Companion broker | Xác thực controller, multiplex state/config/result, quản lý session | Suy gameplay readiness hoặc giữ raw game pointer |
| Kotlin | Render phase, giữ desired config, fake-location UI/control | Tự kết luận game ready hoặc chạy gameplay scheduler |

### Race và cách đóng

- **Controller-first:** broker đã có ngay từ `TARGET_ATTACHED`; trước khi game tồn tại, app hiển thị `WAITING_FOR_GAME`. App không tự dừng do socket chưa có.
- **Game-first:** core bootstrap độc lập; controller kết nối sau sẽ nhận snapshot phase mới nhất và gửi lại full desired config.
- **Library load rất nhanh:** loader observer phải được cài synchronously khi core start. Callback sau successful load cài/arm `il2cpp_init` gate trước khi trả control về caller; polling chỉ được giữ làm watchdog chẩn đoán, không là primary path.
- **Missed lifecycle event:** không đoán rằng runtime đã initialized. Phát lỗi có mã, giữ capabilities đóng và yêu cầu restart process nếu không có một recovery gate đã được verify riêng.
- **Controller chết/restart:** core/session vẫn sống; pending gameplay state không được chuyển quyền sở hữu sang Kotlin. Controller mới chỉ resubmit desired snapshot.
- **Game process chết/restart:** đóng session, hủy pointer/hook-owned state, process mới nhận session ID mới; không restore live pointer từ persistence.
- **ABI/backend không hỗ trợ:** loader/core vẫn báo phase và lý do, nhưng không mở capability. ARM64 implementation của PGS không được suy ra cho x86_64/translation.

### Kết luận ngắn của bước 4

Điểm tích hợp ổn định nhất là một chuỗi event-driven hai gate: **loader thấy `libil2cpp.so`** rồi **`il2cpp_init` đã return**. Broker/session phải xuất hiện độc lập và sớm hơn gameplay readiness. Như vậy thứ tự user mở controller hay game không còn quyết định thành bại, còn native vẫn không chạm IL2CPP quá sớm.

## Bước 5 — Hướng xử lý

### So sánh phương án

| Phương án | Timing | Độ cô lập | Khả năng phục hồi | Kết luận |
|---|---|---|---|---|
| A. Giữ monolith + polling, chỉ tăng timeout/retry Kotlin | Vẫn phụ thuộc polling 500 ms và cửa sổ 300 s | Thấp; bootstrap/gameplay chung một `.so` | Che triệu chứng controller-first nhưng không tạo gate `il2cpp_init` | Loại |
| B. Monolith hiện tại + hook loader/`il2cpp_init` + broker sớm | Đúng event | Trung bình; giảm thay đổi build/package | Core nặng vẫn gắn chặt Zygisk entry, khó rollback/backend theo ABI | Có thể làm bước chuyển tiếp, không phải đích tốt nhất |
| C. **Tiny Zygisk loader + runtime core riêng + lifecycle gates + session broker sớm** | Đúng event, có phase rõ | Cao; loader, hook backend, gameplay tách nhau | Core fail không phá manager; controller đến lúc nào cũng hội tụ | **Chọn** |
| D. Manager late-inject/attach PID đang chạy | Luôn có race với app startup và SELinux/linker namespace | Thấp, quyền root rộng | Khó chứng minh sạch/repeatable, trái mô hình Zygisk đã có | Loại |

### Phương án chọn: split loader/core, event-driven bootstrap

#### 1. Tiny Zygisk loader

- Chỉ giữ process filter, companion handshake, core acquisition/load và bootstrap status tối thiểu.
- Trong `preAppSpecialize`, gửi `TARGET_ATTACHED`, nhận một core artifact đúng ABI từ companion dưới dạng read-only FD cùng manifest `{protocol, abi, size, sha256}`.
- Trong `postAppSpecialize`, verify manifest/hash rồi load core qua FD bằng Android linker API phù hợp. Cơ chế FD là lựa chọn ưu tiên để tránh hard-code app-data path và linker namespace; tính khả dụng trên minSdk/BlueStacks phải được xác minh trước implementation.
- Resolve duy nhất một ABI-stable export do project sở hữu, ví dụ `pogo_runtime_core_start_v1(const CoreBootstrapArgsV1*)`. Nếu ABI version/hash/export sai, publish lỗi và fail closed.
- Sau khi core nhận ownership, loader không giữ gameplay state. Có thể unload Zygisk loader chỉ khi lifecycle callback/hook backend không còn trỏ vào code của loader.

Không tải core động từ mạng như một updater. Core được bundle trong Magisk ZIP theo ABI, có digest cố định và chỉ có hiệu lực cho process mới.

#### 2. Runtime core và hook backend

- Core có `BootstrapCoordinator` làm state machine duy nhất và `LibraryLifecycleObserver` interface.
- Backend ARM64 và x86_64/translation là implementation riêng. Không dùng trampoline ARM64 của PGS làm giả định portable.
- Observer phải bắt successful return của loader cho `libil2cpp.so`; ngay trong cửa sổ trước khi control trở lại caller, nó arm gate cho `il2cpp_init`.
- Wrapper `il2cpp_init` gọi original trước, preserve ABI/return/errno cần thiết, rồi phát event một lần. Mọi việc nặng được đưa sang bootstrap worker.
- Một one-shot mapping check sau khi observer được cài có thể phát hiện library đã có sẵn, nhưng không được tự suy `IL2CPP_INITIALIZED`. Nếu init event đã bị miss và không có post-init proof được verify, capability giữ đóng và yêu cầu restart process.
- Polling hiện tại chỉ giữ làm bounded watchdog/diagnostic để báo `loader_event_missing`; nó không được chuyển state sang ready.

#### 3. Companion `SessionBroker` sớm

- Tạo/bind broker ngay khi nhận `TARGET_ATTACHED`, không đợi probe/binding.
- Một reactor sở hữu duy nhất runtime FD và các controller FD; dùng `poll`/event queue để multiplex. Không để hai thread cùng đọc các bản sao của cùng stream vì frame có thể bị chia sai owner.
- Lưu snapshot in-memory của phase, identity, capability, error và sequence để replay cho controller mới.
- Session ID được tạo tại attach; process chết thì đóng broker session và loại toàn bộ volatile state.
- Wire protocol thêm frame versioned `RUNTIME_BOOTSTRAP_STATUS` với tối thiểu:
  - `sessionId`, `pid`, `processName`, `sequence`, `phase`;
  - `gameBuild`, `abi`, `translationLayer` khi đã biết;
  - `recoverable`, `errorCode`, `message`;
  - `strongIdentityVerified` và capabilities chỉ xuất hiện tại phase phù hợp.

`RUNTIME_READY` có thể được giữ tương thích tạm thời, nhưng về nghĩa phải là snapshot transport/session; mutation vẫn chỉ dựa vào `RuntimeUiStatus.ready && strongIdentityVerified`.

#### 4. Kotlin thành connection supervisor, không phải bootstrap owner

- Service/UI có các trạng thái `WAITING_FOR_GAME`, `ATTACHING`, `INITIALIZING`, `VERIFYING`, `LIVE_READY`, `FAILED`.
- Socket unavailable, game chưa chạy hoặc session turnover là trạng thái transient; reconnect có backoff và không `stopSelf()`.
- Khi session mới xuất hiện, Kotlin gửi lại full desired config snapshot có revision. Native apply theo gate; không replay lệnh gameplay cũ và không auto-arm session mới.
- Controller chết không làm core unload. Core có thể giữ `LIVE_READY` nhưng gameplay cần config/lease phù hợp; khi controller quay lại nó sync lại source-of-truth Android.
- Có thể thêm nút “Start Pokémon GO” giống PGS để UX có thứ tự chuẩn bị → launch, nhưng đây chỉ là convenience. User mở game trực tiếp hoặc mở controller sau vẫn phải hoạt động.

#### 5. Phân lớp source đề xuất

```text
zygisk/loader/                 Zygisk callbacks, core FD/manifest, core ABI entry
zygisk/runtime-core/bootstrap lifecycle state machine và worker
zygisk/runtime-core/hooks/     backend loader/il2cpp_init theo ABI
zygisk/runtime-core/runtime/   IL2CPP, Unity main-thread và binding verification
zygisk/runtime-core/modules/   catch/spin/discard/transfer policy và execution
zygisk/companion/              SessionBroker, auth, multiplex và diagnostics
bridge/protocol/               BootstrapStatus codec/version
app/                           connection supervisor, desired snapshot và render phase
```

Tên thư mục là target architecture, chưa phải yêu cầu đổi toàn bộ source trong một commit. Migration nên giữ mỗi bước buildable và không có hai gameplay scheduler chạy song song.

### Trình tự migration đề xuất

1. Thêm phase model + early `SessionBroker`; chuyển Kotlin sang supervisor bền vững nhưng chưa đổi probe.
2. Tách core ABI contract/artifact packaging; loader nạp core riêng nhưng core còn dùng watchdog cũ.
3. Thêm `LibraryLifecycleObserver` và `il2cpp_init` gate cho ARM64; chỉ tắt polling-primary sau device verification.
4. Thêm backend x86_64/translation và fail-closed matrix.
5. Chuyển main-thread/binding/module ownership vào core, rồi làm mỏng Zygisk loader.
6. Xóa đường monolith/polling cũ sau khi cold-start, restart và controller-order matrix đều đạt.

### Kết luận ngắn của bước 5

Phương án tốt nhất không phải vá timeout của kiến trúc hiện tại và cũng không phải copy PGSharp nguyên xi. Đích nên là **loader rất nhỏ + core riêng + hai lifecycle gate + broker sớm + controller hội tụ theo session**. Đây là phần PGSharp làm đúng về timing, nhưng được bổ sung identity/capability/fail-closed mà retained evidence chưa chứng minh ở PGS.

## Bước 6 — Tiêu chí chấp nhận và xác minh

### Invariant bắt buộc

| ID | Tiêu chí |
|---|---|
| AC-01 | Không có Unity/IL2CPP managed API call trước event `IL2CPP_INITIALIZED` phát sau khi original `il2cpp_init` return |
| AC-02 | Phase/sequence của một session đơn điệu; duplicate loader/init callback không khởi tạo core hoặc bindings lần hai |
| AC-03 | Broker quan sát được `ATTACHING` ngay sau `TARGET_ATTACHED`, không phụ thuộc probe/gameplay readiness |
| AC-04 | Socket connected, START/CONFIG ACK hoặc `IL2CPP_LIBRARY_SEEN` không cấp mutation permission |
| AC-05 | `LIVE_READY` yêu cầu exact build identity, main-thread readiness và ít nhất capability đã verify theo profile |
| AC-06 | Process mới luôn có session ID mới; pointer, pending action, lease và arm state của session cũ không được reuse |
| AC-07 | Unsupported build/ABI, core digest sai, core ABI sai hoặc lifecycle event bị miss đều fail closed với error code cụ thể |
| AC-08 | Controller chết/mở muộn không ảnh hưởng bootstrap core; reconnect chỉ resubmit full desired snapshot, không replay gameplay command |
| AC-09 | Không có hai reader cạnh tranh trên runtime stream; một `SessionBroker` sở hữu và multiplex frame theo sequence/correlation |
| AC-10 | Polling không thể chuyển runtime sang ready; chỉ lifecycle event đã verify mới được đổi phase readiness |

### Matrix hành vi startup

| Scenario | Kết quả mong đợi |
|---|---|
| Controller mở, game chưa chạy | UI ở `WAITING_FOR_GAME`, service không tự dừng, không spam reconnect |
| Game mở sau controller | Session mới xuất hiện; phase tiến dần đến `LIVE_READY`; desired snapshot được apply đúng revision |
| Game mở trước, controller mở sau | Core tự bootstrap; controller nhận snapshot phase hiện tại và hội tụ mà không restart game |
| Controller bị kill khi core đang init | Core tiếp tục; controller mới reconnect cùng session và không tự arm gameplay |
| Game bị kill rồi mở lại | Session cũ đóng; session mới không kế thừa pointer/pending mutation |
| `libil2cpp.so` load chậm hơn 3 giây | Không timeout fatal; phase vẫn quan sát được và không cần tăng một magic timeout toàn cục |
| `il2cpp_init` hook không cài được hoặc event bị miss | `FAILED_LIFECYCLE_GATE`, capabilities rỗng, yêu cầu restart; không fallback đoán bằng maps/domain |
| Build/ABI/translation không có backend đã verify | Runtime báo unsupported rõ ràng, không crash và không expose gameplay capability |
| Core manifest/hash/export không khớp | Dừng ở loader, broker báo `FAILED_CORE_INTEGRITY`/`FAILED_CORE_ABI` |
| Duplicate/multiple `dlopen` cùng library | Initialization exactly-once; refcount/load lại không tạo worker/module thứ hai |

### Xác minh source/build

- Không tạo hoặc sửa test theo rule repository; chạy toàn bộ test hiện có.
- Chạy hai native protocol test host được liệt kê trong `AGENTS.md`.
- Chạy `./gradlew test assembleDebug --rerun-tasks` và `git diff --check`.
- Build multi-ABI bằng `ANDROID_NDK="$HOME/Library/Android/sdk/ndk/27.1.12297006" ./scripts/build-magisk.sh`; package phải có loader/core đúng cho `arm64-v8a` và `x86_64`, cùng manifest/digest.
- Thêm compile-time/static guards cho core ABI struct size/version và frame size; runtime vẫn phải kiểm tra lại trước khi dùng.

### Xác minh runtime

- Target duy nhất: `BlueStacks Air 1`; không chọn emulator bằng raw ADB.
- Install/reboot và feature check là manual handoff theo workflow hiện có. Dùng phase/status qua UI/broker để xác nhận thứ tự; không đọc log file sau một run thành công.
- Với từng scenario trong matrix, xác nhận phase timestamp/order và rằng không có gameplay action trước `LIVE_READY`.
- Chỉ khi run thất bại mới dùng `./scripts/logcat-full.sh` và `./scripts/collect-binding-diagnostics.sh`; không thay bằng lệnh ADB thủ công.
- ARM64 thành công không đủ đóng AC cho x86_64/translation; mỗi backend cần evidence/runtime verification riêng. Backend chưa verify giữ capability đóng.

### Exit condition

Chỉ xóa monolith/polling-primary sau khi toàn bộ startup-order matrix đạt trên ABI mục tiêu, controller restart không làm mất core session, game restart tạo session sạch, và mọi nhánh unsupported/missed-event đều fail closed. Nếu exact hook backend chưa có evidence/runtime postcondition, dừng migration ở phase model + split core và không tuyên bố lifecycle gate hoàn tất.

### Kết luận và câu hỏi còn mở

**Kết luận:** Kiến trúc hiện tại không nên được giữ làm đích. Phương án tốt nhất là mô hình gần PGS ở đúng phần tạo stability: Zygisk loader nhỏ nạp core riêng thật sớm, core theo dõi library lifecycle và bootstrap độc lập với manager. Project nên cải tiến thêm bằng gate sau `il2cpp_init`, state machine có phase, broker sớm và identity/capability fail-closed.

Không nên copy tên symbol, trampoline hoặc hook bytes của PGS vì retained evidence chưa chứng minh ABI/call path, đặc biệt không có bằng chứng x86_64 tương đương. Phần còn mở trước implementation là chọn/vet hook backend theo từng ABI và xác minh khả năng load core qua read-only FD trên Android/BlueStacks target. Hai câu hỏi này ảnh hưởng implementation, không thay đổi target architecture đã chọn.
