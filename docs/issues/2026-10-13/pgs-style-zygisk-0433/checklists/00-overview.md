# Kế hoạch — APK tự cài Zygisk và automation kiểu PGS cho Pokémon GO 0.433.0

Ngày lập: 2026-10-13

Issue: `pgs-style-zygisk-0433`

Target: `com.nianticlabs.pokemongo` 0.433.0, versionCode `2026100702`, arm64-v8a

Plan nguồn: [Ghi chú triển khai PGS-style](../../../../PGS_STYLE_ZYGISK_PORT_0.433.0.md)

Research bổ sung: [TryCapture, PGS-style scan và lifecycle Auto Catch](../../../../../research/2026-10-13-autocatch-strategy-scan-lifecycle.md)

Research REA/artifact: [Migration reverse sang REA](../../../../../research/2026-10-13-rea-artifact-migration.md)

## Bối cảnh và quyết định

Mục tiêu là dùng repo hiện tại để tạo controller APK tự chứa Magisk/Zygisk
module, yêu cầu `su` khi người dùng mở app lần đầu, cài module và sau reboot để
Zygisk nạp runtime vào Pokémon GO. Runtime native đọc object/state IL2CPP, giữ
state theo session và tự điều phối Auto Catch, Auto Transfer, Auto Discard.

Không triển khai server kiểm tra version, tải core hoặc auto-update. Runtime vẫn
phải kiểm tra fingerprint cục bộ của đúng build 0.433.0 và fail closed trên build
khác. Không triển khai root hiding, DenyList mutation, property spoofing, Play
Integrity bypass hoặc screen/tap/swipe automation.

`pogo-root-automation` hiện có thay đổi chưa commit trong native catch/map. Agent
thực thi phải kiểm tra diff trước mỗi task, giữ nguyên thay đổi của người dùng và
không dùng reset/checkout để xóa chúng.

## Quyết định bổ sung từ người dùng — 2026-10-13

- `MapPokemon.TryCapture` là đường Auto Catch chính vì mục tiêu là kết quả bắt
  tự động, không phải mô phỏng đầy đủ accessory protocol. Đường này chỉ được coi
  là live-ready khi exact binding 0.433.0, authoritative Promise outcome,
  `capturedPokemonId` và map postcondition đều được chứng minh.
- Virtual GO Plus là fallback, không phải đường mặc định. Fallback chỉ được chọn
  trước khi dispatch khi direct capability không sẵn sàng; không chuyển đường sau
  một dispatch `TryCapture` đang pending/indeterminate để tránh bắt trùng.
- Scan cũ dựa vào `MapEntityCell.eghw`/wild dictionary và full poll khoảng 2 giây
  không còn là contract đích. Scan mới phải lấy state đầy đủ theo owner/service,
  event/dirty generation và typed adapter cho từng nguồn Pokémon giống mô hình
  PGS; nguồn chưa reverse được phải bị đánh `UNSUPPORTED`, không ép thành wild.
- Cấu hình policy/filter được phép persist, nhưng quyền chạy Auto Catch là
  session-scoped và chỉ bật bằng thao tác người dùng. Mở app/controller hoặc mở
  Pokémon GO không tự arm. Khi process/runtime session Pokémon GO chết, disconnect
  hoặc đổi session, Auto Catch phải disarm ngay và session mới luôn bắt đầu OFF.
- Phạm vi agent dừng ở code hoàn chỉnh, host/native checks, Gradle build và đóng
  gói artifact. Agent không cài/chạy APK hoặc module, không reboot, không mở game
  và không chạy E2E/BlueStacks. Sau khi build xong, agent phải báo artifact và
  đưa checklist/lệnh để người dùng tự chạy trên BlueStacks Air 1.
- Reverse build 0.433.0 chỉ dùng REA và artifact/evidence đã pin dưới `reverse/`.
  `scripts/reverse-pogo-apk.sh` đã bị loại bỏ; dump 0.427.0 và báo cáo PGS cũ chỉ
  được dùng tạo hypothesis, không được dùng làm binding evidence.
- Thêm phase cleanup trước release-candidate: xóa deploy/E2E/headless scripts
  không còn consumer, bỏ loopback HTTP control surface, giữ build/package và
  diagnostic tối thiểu. CI/local dùng một flow build → host test → artifact
  inspect; BlueStacks là manual handoff của user, không phải gate của agent.

## Bằng chứng build cố định

```text
package:             com.nianticlabs.pokemongo
versionName:         0.433.0
versionCode:         2026100702
libil2cpp build ID:  da59ce3024c4ccfe16fa6616281a4d111de67aa2
global metadata:     f5961a8548256da287b4f79466a169c302b90ec59be7f6461e8e58b482c0fb0d
base APK:            1c8f198cd212e7303e5a5e38e34857f4ced42eb432ac62b07807bfd286503690
APKM:                1b120bb485310777f196fdb8cb827b63887e91f83fe81dc0cdb7e67a2d518278
arm64 split APK:     8f4a472ca8fd59cc794f4f0b32fff3920c55211bbab955ec8b79f7aa4fe5f0a0
libil2cpp SHA-256:   6b1e11619362ddd53f9dbcd7be1d4296bdec5dda27b9f742c16b8403359c25a0
```

Method metadata search identities supplied by the plan (not binding evidence):

| Class | Method | Slot | Token |
|---|---|---:|---:|
| `Niantic.Holoholo.IPokemonBag` | `GetPokemon(id)` | 8 | `0x06001058` |
| `Niantic.Holoholo.Internal.PokemonBagImpl` | `GetPokemon(id)` | 12 | `0x06002a39` |
| `Niantic.Holoholo.IPokemonBag` | `ReleasePokemon(pokemon)` | 15 | `0x0600105f` |
| `Niantic.Holoholo.Internal.PokemonBagImpl` | `ReleasePokemon(pokemon)` | 19 | `0x06002a43` |
| `Niantic.Holoholo.IItemBag` | `GetItemCount(itemType)` | 5 | `0x06000f85` |
| `Niantic.Holoholo.Internal.ItemBagImpl` | `GetItemCount(itemType)` | 9 | `0x0600295b` |
| `Niantic.Holoholo.IItemBag` | `RecycleItem(itemProto, numToRecycle, expiringItemsCopy)` | 19 | `0x06000f93` |
| `Niantic.Holoholo.Internal.ItemBagImpl` | `RecycleItem(itemData, numToRecycle, expiringItemsCopy)` | 23 | `0x06002971` |

Token/slot chỉ là search identity, không phải RVA hay địa chỉ runtime. REA
providers hiện thiếu nên các tuple này vẫn `UNRESOLVED`; native không được
resolve/call chúng cho 0.433.0 tới khi ledger có Evidence tương ứng.

## Trạng thái implementation hiện tại

Phase 0–2 và cleanup/build Phase 8 đã hoàn tất. Phase 3 chốt artifact/profile
fail-closed nhưng direct-catch, storage và Virtual GO Plus contracts còn bị chặn
bởi REA provider gap. Vì vậy Phase 4–7 không được port bằng layout 0.427.0 và
vẫn để mở với blocker chi tiết. Artifact hiện tại dùng để kiểm installer,
lifecycle và manual session arm; gameplay mutation không `LIVE_READY`.

## Acceptance criteria

| ID | Tiêu chí |
|---|---|
| `AC-01` | Debug APK chứa đúng Magisk ZIP sinh từ cùng source/build, có manifest/hash và không cần network để lấy module. |
| `AC-02` | Lần mở app đầu trong foreground yêu cầu root tối đa một lần, cài module idempotent qua Magisk và báo lỗi rõ khi root/Magisk/Zygisk không sẵn sàng. |
| `AC-03` | App phân biệt `INSTALLED`, `REBOOT_REQUIRED`, `ACTIVE`; reboot chỉ xảy ra sau xác nhận người dùng và service không chạy gameplay trước `ACTIVE`. |
| `AC-04` | Runtime chỉ cho mutation trên fingerprint 0.433.0 đã pin; build khác attach diagnostic/read-only hoặc trả `unsupported_build`, không tải update. |
| `AC-05` | Native có world-state store theo session/generation/revision, commit snapshot nguyên tử và phân biệt `EMPTY`, `STALE`, `NOT_READY`, `ERROR`. |
| `AC-06` | Map, location, inventory và encounter được đọc từ object/service game trên Unity main thread; hook chỉ đánh dấu dirty, reconcile poll chỉ là fallback. |
| `AC-07` | Auto Catch dùng Pokémon map hợp lệ, Poké Ball item 1, range/freshness guards, một pending action, authoritative Promise outcome và map postcondition. |
| `AC-08` | Auto Transfer chỉ bắt đầu từ caught storage ID xác thực, áp dụng keep policy fail-closed, gọi `GetPokemon`/`ReleasePokemon`, theo dõi Promise và storage postcondition. |
| `AC-09` | Auto Discard dùng `keepCount`, đọc lại count trước mutation, gọi `RecycleItem` với `ItemData`/`ExpiringItemsCopy` đúng owner và xác minh Promise + inventory refresh. |
| `AC-10` | Catch/spin/transfer/discard không chạy đồng thời; config revision/session được replay sau reconnect nhưng pointer/pending state không persist. |
| `AC-11` | UI/status cho biết install/runtime/state/capability/error; không báo action hoàn tất khi mới dispatch hoặc khi Promise indeterminate. |
| `AC-12` | Existing host checks, full Gradle build, native package build và static artifact inspection đạt; tài liệu/handoff ghi rõ phần code-complete và phần BlueStacks chưa chạy do người dùng tự kiểm thử. |
| `AC-13` | Auto Catch ưu tiên direct `MapPokemon.TryCapture`; Virtual GO Plus chỉ là fallback capability-gated, không failover sau dispatch/pending/indeterminate và không tạo double catch. |
| `AC-14` | Scanner thay thế đường cũ bằng PGS-style state acquisition: event-driven, full-cell/full-source typed inventory của Pokémon/fort, snapshot complete/incomplete rõ và không coi một field wild là toàn bộ nearby. |
| `AC-15` | Auto Catch mặc định/disconnect/session mới đều OFF; chỉ thao tác người dùng mới arm cho đúng runtime session, còn policy/filter có thể persist độc lập. |
| `AC-16` | Repo không còn deploy/E2E/headless script hoặc HTTP control surface đã bị thay thế; script còn giữ có consumer rõ. CI/local dùng một flow không duplicate native build/package và dừng trước device test. |

Các AC trên được agent chốt ở mức code/host checks/build artifact. Quan sát cần
process game thật được ghi `UNVERIFIED_DEVICE` và chuyển sang handoff user; không
được dùng kết quả chưa chạy để tuyên bố `LIVE_READY`.

## Mapping AC → task

| AC | Task |
|---|---|
| `AC-01` | `T-101`–`T-104`, `T-808` |
| `AC-02` | `T-201`–`T-204` |
| `AC-03` | `T-202`, `T-205`, `T-206` |
| `AC-04` | `T-001`, `T-002`, `T-301`–`T-305` |
| `AC-05` | `T-401`, `T-402`, `T-405`, `T-406` |
| `AC-06` | `T-302`–`T-305`, `T-403`–`T-405` |
| `AC-07` | `T-501`–`T-504`, `T-506` |
| `AC-08` | `T-601`–`T-603` |
| `AC-09` | `T-701`–`T-703` |
| `AC-10` | `T-406`, `T-502`, `T-602`, `T-702`, `T-801` |
| `AC-11` | `T-205`, `T-407`, `T-504`, `T-603`, `T-703` |
| `AC-12` | `T-003`, `T-104`, `T-206`, `T-305`, `T-408`, `T-808`, `T-809`, `T-801`, `T-802`, `T-804` |
| `AC-13` | `T-303`, `T-307`, `T-501`–`T-503`, `T-506` |
| `AC-14` | `T-302`, `T-306`, `T-401`–`T-405`, `T-501` |
| `AC-15` | `T-207`, `T-208`, `T-407`, `T-807`, `T-801` |
| `AC-16` | `T-805`–`T-809`, `T-801`, `T-804` |

## Phạm vi

- Build/package self-contained APK và Magisk module từ cùng checkout.
- Installer root foreground, install state, reboot UX và service gate.
- Reverse/discovery bằng REA và binding profile 0.433.0 arm64; tái dùng artifact,
  report và Evidence bundle đã đặt dưới `reverse/`.
- Native event-driven world state và main-thread collection.
- Auto Catch, Auto Transfer, Auto Discard cùng outcome/postcondition.
- Direct `TryCapture` primary, Virtual GO Plus fallback và manual per-session arm.
- Bridge/status cần thiết để UI hiển thị state thật.
- Existing host checks, native build/package, Gradle build và artifact inspection.
- Cleanup code/script và canonical local/CI build → host-test flow.
- Handoff riêng ghi APK/ZIP/hash, hướng dẫn UI và expected postcondition để người
  dùng tự cài/test trên BlueStacks Air 1 sau khi agent báo code/build hoàn tất.

## Ngoài phạm vi

- Backend update/version API hoặc tải core động.
- Hỗ trợ nhiều Pokémon GO build trong cùng artifact.
- Root hiding, anti-detection, DenyList/property/attestation changes.
- Repack hoặc ký lại Pokémon GO.
- Pokémon HOME/Beluga transfer.
- Mô phỏng ảnh, tap, swipe hoặc throw gesture cho Virtual GO Plus-like catch.
- Tạo hoặc sửa test source: `AGENTS.md` hiện hành cấm thêm/sửa test; plan chỉ chạy
  existing host/Gradle tests và kiểm tra artifact tĩnh.
- Deploy production hoặc kiểm thử trên tài khoản chính.
- Mọi thao tác trên BlueStacks/device: cài APK/module, reboot, mở/kill Pokémon GO,
  root prompt, runtime diagnostics, smoke/E2E và mutation catch/transfer/discard.

## Dependency và câu hỏi còn mở

1. Field obfuscate/map owner/catch outcome của 0.433.0 chưa được trích đầy đủ.
   `T-302`–`T-304` là discovery bắt buộc; `T-501`–`T-503` bị chặn tới khi có
   REA evidence. Bundle hiện giữ residual unknown vì provider JADX/native deep
   analysis chưa cấu hình; không được lấp khoảng trống bằng script/dump cũ.
2. `RecycleItem` đã rõ signature nhưng layout `ItemData`, owner của
   `ExpiringItemsCopy` và full-refresh postcondition chưa có runtime proof.
   `T-701` chặn implementation ở `T-702`–`T-703`; live verification nằm trong handoff user.
3. `ReleasePokemon` đã rõ signature nhưng caught storage ID và storage refresh
   postcondition phải được chứng minh. `T-304`/`T-601` chặn capability transfer.
4. Package Installer không thể tự chạy app. “Tự động” được định nghĩa là tự chạy
   preflight/install khi Activity được mở lần đầu; root prompt phải ở foreground.
5. Reboot là hành vi phá phiên hiện tại; plan chỉ thêm nút xác nhận, không tự reboot.
6. Repo chưa có Virtual GO Plus implementation. `T-307` phải tìm đúng
   accessory/game-state API và completion contract của 0.433.0; nếu không có
   evidence, fallback giữ `UNAVAILABLE` nhưng direct catch vẫn có thể hoàn tất.
7. Scanner cũ chỉ đọc `MapEntityCell.eghw` và `WildMapPokemon`, vì vậy chưa đủ để
   gọi là PGS-style full scan. `T-306` chặn việc quảng bá scan-complete cho tới khi
   từng source/owner và semantics complete/incomplete được xác minh.
8. Owner lifetime, callback thật và server postcondition chỉ có thể chốt trên
   device. Agent implement guard/diagnostic theo offline evidence nhưng giữ
   trạng thái `UNVERIFIED_DEVICE`; người dùng chạy handoff sau build và cung cấp
   kết quả nếu cần mở `LIVE_READY`.
9. Script có historical-doc reference nhưng không có production/CI consumer
   chưa tự động bị coi là unused. `T-805` phải phân biệt maintained consumer với
   archival evidence trước khi `T-806` xóa file.

## Thứ tự phase

1. [Phase 0 — Baseline và evidence contract](phase-0-baseline-evidence.md)
2. [Phase 1 — Build và nhúng Magisk module](phase-1-bundled-module-build.md)
3. [Phase 2 — Installer APK và lifecycle](phase-2-android-installer.md)
4. [Phase 3 — Reverse và binding profile 0.433.0](phase-3-pogo-0433-binding.md)
5. [Phase 4 — Native world state event-driven](phase-4-native-world-state.md)
6. [Phase 5 — Auto Catch](phase-5-auto-catch.md)
7. [Phase 6 — Auto Transfer](phase-6-auto-transfer.md)
8. [Phase 7 — Auto Discard](phase-7-auto-discard.md)
9. [Phase 8 — Cleanup code, script và build → test](phase-8-cleanup-build-test-flow.md)
10. [Phase 9 — Tích hợp, build và handoff](phase-9-integration-verification.md)

## Handoff BlueStacks do người dùng thực hiện

Các ID cũ `T-505`, `T-507`, `T-604`, `T-704`, `T-803` được giữ trong file phase
dưới dạng mục thông tin để bảo toàn lịch sử, nhưng không còn là checkbox/task
agent và không chặn Definition of Done. `T-804` phải tổng hợp chúng thành một
file handoff duy nhất với:

- APK/ZIP path, SHA-256 và build identity;
- prerequisite root/Magisk/Zygisk và cảnh báo dùng test account;
- cài APK bằng UI BlueStacks, first-open `su`/self-install, reboot thủ công và
  retained diagnostic/log commands chỉ khi test thất bại;
- thứ tự test installer → attach OFF → manual arm → scan → direct/fallback →
  transfer/discard → kill/relaunch OFF;
- expected status/postcondition và stop condition nếu gặp crash, binding mismatch
  hoặc outcome `INDETERMINATE`;
- log/artifact tối thiểu người dùng cần gửi lại để tiếp tục sửa.

Agent chỉ báo “code/build hoàn tất, sẵn sàng để bạn test” sau khi `T-802`/`T-804`
đạt; không tự chạy bất kỳ bước handoff nào.

## Rủi ro và cách xử lý

| Rủi ro | Xử lý |
|---|---|
| Ghi đè thay đổi native chưa commit | Kiểm tra diff trước từng task; chỉnh hẹp; không reset/checkout; ghi sai khác trong details. |
| APK assemble trước khi module ZIP được sinh | Build task có dependency rõ và fail nếu asset/hash thiếu hoặc stale. |
| Root prompt từ background bị treo/từ chối | Chỉ bắt đầu install từ foreground Activity; persist trạng thái từ chối; retry do user kích hoạt. |
| Zygisk chưa bật hoặc cần reboot | Code preflight/state machine đầy đủ; artifact handoff để người dùng tự xác minh `REBOOT_REQUIRED`/`ACTIVE`. |
| Field obfuscate đổi giữa 0.427 và 0.433 | Dùng REA trên đúng artifact/hash trước implementation; dump/report cũ chỉ tạo hypothesis; runtime guard class/type/arity/owner; capability ghi `UNVERIFIED_DEVICE` tới khi user test. |
| Direct catch và Virtual GO Plus cùng dispatch | Chọn strategy trước action; một action id/arbiter; cấm fallback khi direct đã dispatch, pending hoặc indeterminate. |
| Persisted `enabled/armed` tự bật lại sau relaunch | Tách policy bền vững khỏi session arm; default/migration OFF; process-death/session-change bắt buộc disarm. |
| Scanner cũ bỏ sót nguồn Pokémon | Reverse từng typed source, commit snapshot complete/incomplete; unknown source không bị cast thành `WildMapPokemon`. |
| Managed object hết lifetime | `ownerGeneration`, scene/session invalidation, GC handle chỉ cho Promise cần giữ. |
| Callback hook làm nặng main thread | Hook chỉ tăng dirty generation; bounded collector chạy qua bridge; fallback poll thưa. |
| Call thành công nhưng server action chưa xong | Promise/result/postcondition bắt buộc; timeout trả `INDETERMINATE`, không retry mù. |
| File native vượt 500 dòng | Tách store/collector/binding/policy thành `.h`/`.inc` riêng trước khi thêm logic. |
| Xóa script còn consumer ẩn | `T-805` inventory caller/CI/docs trước; verdict chưa chắc giữ `UNRESOLVED`, không xóa. |
| CI và local build khác thứ tự hoặc package hai lần | `T-808` dùng `build-magisk.sh` một lần, Gradle consume đúng output, artifact inspect trước upload. |
| HTTP/headless path còn khả năng arm ngoài UI | Xóa server/CLI cùng lúc, `rg` port/route/callsite và audit manual per-session arm trước final build. |

## Quy tắc thực thi và ghi chú bắt buộc

Sau mỗi task, kiểm tra diff và kết quả Verify, cập nhật ngay checkbox cùng khối
`<details>` nằm dưới nó, rồi mới chuyển task. Chỉ đánh dấu `[x]` khi đạt Verify.
Trong `<details><summary>Đã hoàn thành — T-ID</summary>...</details>`, ghi file đã
tạo/sửa/xóa/di chuyển, function/method/param hoặc hành vi đã thêm/đổi/bỏ, và
lệnh kiểm chứng cùng kết quả thực tế. Mục không phát sinh ghi “Không”. Nếu còn
lỗi, chưa kiểm chứng hoặc bị chặn, giữ `[ ]`, ghi trạng thái và phần đã làm/điểm
còn thiếu. Không dồn ghi chú tới cuối phase, không đánh dấu hoàn tất dựa trên dự
kiến.

Quy tắc bổ sung của repo:

- Agent không chạy device/emulator. Tài liệu handoff dùng UI BlueStacks cho
  install/test và chỉ đưa retained diagnostic/log scripts khi lỗi; không đưa
  raw ADB hoặc script E2E đã xóa.
- Không tạo/sửa test source; được chạy existing tests.
- Source không phải Markdown phải giữ tối đa 500 dòng.
- Native build/package qua `scripts/build-magisk.sh`; APK/full Kotlin qua Gradle.
- Code phải phân biệt `IMPLEMENTED_UNVERIFIED_DEVICE` với `LIVE_READY`; không mô
  tả device postcondition là đã đạt khi agent chỉ build/host-check.

## Definition of Done

- Mọi task thuộc phạm vi đạt Verify và task `[x]` có ghi chú thực tế ngay dưới.
- APK build chứa đúng module/hash và installer state machine compile đúng các trạng thái yêu cầu.
- Binding 0.433.0 có artifact hash + REA Evidence ID/coverage/limitation; build khác fail closed.
- State reader không poll full map liên tục khi không dirty và không đọc managed
  object sai thread.
- Catch/transfer/discard có code theo dõi terminal outcome/postcondition và fail
  closed; trạng thái live vẫn `UNVERIFIED_DEVICE` tới khi user chạy handoff.
- Auto Catch code chỉ arm theo runtime session; disconnect/session-change path
  disarm và không persist arm dù policy/filter vẫn còn.
- Direct `TryCapture` là primary; Virtual GO Plus fallback không được gây duplicate
  mutation và chỉ quảng bá sau khi contract riêng được verify.
- Sáu deploy/E2E/headless scripts và loopback HTTP server đã bị xóa sau consumer
  audit; retained scripts có mục đích/build hoặc diagnostic rõ.
- CI/local build đúng Mermaid Phase 8, không duplicate native build/package và
  không chứa device/E2E gate.
- Existing focused checks, `./gradlew test assembleDebug`, native build/package,
  static artifact inspection và `git diff --check` đạt hoặc task vẫn để `[ ]`
  với giới hạn ghi rõ.
- `T-804` bàn giao APK/ZIP/hash cùng checklist BlueStacks cho user; agent không
  chạy các lệnh device trong lượt implementation và không lấy E2E làm điều kiện tick.
- `docs/ARCHITECTURE.md`, `docs/LIVE_AUTOMATION_READINESS.md`, README và tài liệu
  feature phản ánh đúng trạng thái cuối, không mô tả scaffold là đã chạy live.
