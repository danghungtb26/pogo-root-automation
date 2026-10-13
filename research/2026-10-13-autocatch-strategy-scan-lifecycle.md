# Research — TryCapture, PGS-style scan và lifecycle Auto Catch

Ngày: 2026-10-13

Repo: `pogo-root-automation`

Phạm vi: đọc source hiện tại; chưa dispatch gameplay action và chưa kiểm chứng live
trên Pokémon GO 0.433.0.

## Phạm vi thực thi cập nhật

Theo yêu cầu người dùng, agent chỉ hoàn thiện source, host checks, Gradle/native
build và package artifact. Agent không cài/chạy APK hoặc module, không reboot,
không mở/kill Pokémon GO và không chạy E2E trên BlueStacks. Những kết luận phụ
thuộc owner lifetime, callback/server result và postcondition thật phải mang
trạng thái `UNVERIFIED_DEVICE`; khi code/build xong agent bàn giao APK/ZIP/hash,
lệnh script và expected result để người dùng tự kiểm thử.

## Kết luận

Có thể dùng `MapPokemon.TryCapture` thay cho GO Plus ở vai trò **đường Auto Catch
chính**, nếu mục tiêu là tự động bắt Pokémon chứ không phải giả lập đầy đủ
accessory protocol. Nó không tương đương GO Plus về connection/session/notification
state, nhưng có đường direct action ngắn hơn và repo đã có phần lớn scaffold cho
Promise/outcome/map sync.

Quyết định kiến trúc:

1. `DIRECT_TRY_CAPTURE` là primary.
2. `VIRTUAL_GO_PLUS` là fallback capability-gated.
3. Strategy được chọn trước dispatch. Khi direct đã dispatch, pending hoặc
   `INDETERMINATE`, tuyệt đối không chuyển sang fallback cho cùng target.
4. Auto Catch chỉ arm bằng tay cho runtime session hiện tại; kill/disconnect hoặc
   session mới phải disarm và không tự restore.

## Bằng chứng TryCapture hiện có

- `zygisk/jni/modules/catch_spin/direct_map_bindings.inc` resolve
  `MapPokemon.TryCapture(PokeballThrow, ARPlusEncounterValuesProto)` và
  `WildMapPokemon.get_SpawnPointId()`.
- `zygisk/jni/modules/catch_spin/direct_map_actions.inc` gọi method trên Unity
  main thread và giữ Promise.
- `direct_catch_promise_observer.inc` có pending state, GC handle, timeout,
  outcome và `captured_pokemon_id`.
- `direct_catch_map_sync.inc` có bước đồng bộ map sau terminal result.
- `catch.inc` đặt `kDirectCatchExecutionEnabled = true`.

Giới hạn quan trọng: `zygisk/jni/host/runtime_capabilities.inc` và
`runtime_ui_status_bridge.inc` hiện publish `DIRECT_CATCH` khi
`direct_catch_verified` đúng, chưa bắt buộc `direct_catch_outcome_verified` và
map postcondition. Gate này phải sửa trước khi gọi đường direct là live-ready.

## Virtual GO Plus hiện chưa tồn tại

Không thấy implementation accessory/GO Plus riêng trong repo. Vì vậy fallback
chưa thể chỉ “bật flag”; cần reverse build 0.433.0 để tìm:

- accessory/session owner và connect/disconnect state;
- nearby notification/selection/catch request;
- rule Poké Ball item id `1`;
- result callback/Promise và `capturedPokemonId`;
- cooldown/rate và lifecycle khi scene/session đổi.

Thiếu một trong các bằng chứng trên thì capability fallback phải giữ
`UNAVAILABLE`; không được dùng direct result để giả làm bằng chứng GO Plus.

## Vì sao scanner cũ chưa đúng

- `modules/catch_spin/coordinator.inc` dùng `kCatchSpinScanEveryTicks = 20`, tức
  full snapshot khoảng mỗi 2 giây ở tick 100 ms.
- `runtime_map_reader.inc` đọc field obfuscate `MapEntityCell.eghw`, enumerate
  dictionary và chỉ nhận object exact class `WildMapPokemon`.
- Cách này bỏ sót hoặc không phân loại rõ các source PGS-style như incense,
  daily encounter, photobomb, incident, PokéStop spawn, route, raid, station và
  Troy disk.
- Một lỗi ở cell traversal có thể làm nearby snapshot fail; `is_complete` hiện
  chưa diễn đạt coverage của từng source.

Scanner đích phải dùng event/dirty generation, đọc managed object trên main
thread và có typed adapter cho từng source hiện diện trong 0.433.0. Snapshot cần
phân biệt `READY_COMPLETE`, `READY_PARTIAL`, `EMPTY`, `STALE`, `ERROR`; unknown
class không được cast thành `WildMapPokemon`.

## Vì sao lifecycle hiện chưa đáp ứng yêu cầu bật tay

- `HeadlessAutomationConfig` mặc định `enabled=true`, `catchSpinArmed=true`,
  `autoCatch=true`.
- `AutomationConfigRepository.read()` cũng dùng default `true` và persist key
  `catch_spin_armed`.
- `AutomationBootReceiver` tự start `HeadlessAutomationService` sau boot.
- `RuntimeUiAutomationFacade` poll mỗi 500 ms và submit desired state khi revision
  thay đổi; native `sync_auto_enabled_modules()` có thể enable module khi state
  persisted vẫn armed.

Thiết kế cần đổi:

- policy/filter/strategy có thể persist;
- arm state là RAM-only, gắn exact `runtimeSessionId`;
- controller/service/boot chỉ connect và đọc state, không arm;
- chỉ nút/toggle user mới `arm(sessionId)`;
- PID chết, bridge disconnect, session đổi, STOP/error hoặc service shutdown đều
  `disarm(reason)`;
- session Pokémon GO mới luôn OFF, kể cả preferences cũ từng lưu armed=true.

## State machine đề xuất

```text
DISARMED
  -- user enable + runtime ready/session S --> ARMED(S)

ARMED(S)
  -- direct live-ready --> DIRECT_TRY_CAPTURE
  -- direct unavailable + fallback live-ready --> VIRTUAL_GO_PLUS
  -- no capability --> ARMED_BLOCKED
  -- PID death/disconnect/session != S/stop/error --> DISARMED

DIRECT pending/indeterminate
  -- không fallback cho cùng target --> terminal hoặc manual recovery
```

Kế hoạch triển khai chi tiết nằm tại
`docs/issues/2026-10-13/pgs-style-zygisk-0433/checklists/00-overview.md`.

Nguồn artifact và workflow reverse đã được chuyển sang REA; xem
`research/2026-10-13-rea-artifact-migration.md`. Những kết luận trong file này
chưa có REA Evidence ID phải được coi là hypothesis cho tới Phase 3.
