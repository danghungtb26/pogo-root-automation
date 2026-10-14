# Phase 4 — Native world state event-driven

Plan: [Tổng quan](00-overview.md)

## Điều kiện bắt đầu

- `T-305` có profile/fingerprint guard hoạt động.
- `T-302` và `T-304` đã mô tả owner, lifetime và thread requirement của state.
- Thay đổi native chưa commit hiện hữu đã được rebase logic bằng chỉnh sửa hẹp, không bị xóa.

> Cập nhật reverse 2026-10-14: exact artifact pair đã có focused metadata-to-RVA
> map, và REA đã xác nhận ba RVA đại diện nằm trong analyzed code
> (`ev_51dac350...`, `ev_7f6095cf...`, `ev_d7680400...`). Exact boundary/ABI,
> owner/lifetime, typed field/event và postcondition vẫn chưa được chứng minh;
> vì vậy T-401/T-402 giữ trạng thái triển khai nền chưa hoàn tất và T-403–T-408
> vẫn fail-closed, không được tick.

## Task

- [ ] **T-401** **[zygisk]** *(native)* — Thêm world-state store tách file (đề xuất `zygisk/jni/shared/runtime/state/runtime_world_state.h` và `.inc`) chứa snapshot bất biến cho session/location/map/inventory/encounter, `ownerGeneration`, `dirtyGeneration`, `revision`, timestamp và availability `NOT_READY`, `EMPTY`, `READY`, `STALE`, `ERROR`. Collector build bản tạm rồi commit nguyên tử; không lưu managed pointer trong snapshot. Wire declaration/include tối thiểu qua `runtime_native_declarations.inc`, `runtime_native_prelude.inc` và `main.cpp`. **AC:** Reader thấy một revision nhất quán và phân biệt empty hợp lệ với read failure. **Phụ thuộc:** `T-302`, `T-304`, `T-305`. **Verify:** Native compile + existing host checks; review memory ownership/concurrency; forced session reset làm revision/generation đổi và xóa snapshot cũ.

  <details>
  <summary>Đã triển khai phần store/build — T-401</summary>

  Đã thêm `runtime_world_state.h/.inc` với snapshot copy-out bất biến theo
  `session_generation`/`owner_generation`/`dirty_generation`/`revision`, năm trạng
  thái availability và commit nguyên tử riêng cho map/location/inventory/encounter.
  Reset được nối vào START/STOP và mỗi lần owner binding được dựng lại; các reader
  hiện tại commit dữ liệu POD đã copy, không giữ managed pointer. Native arm64 và
  x86_64 build/package đạt, toàn bộ existing host C++ checks và
  `./gradlew test assembleDebug --rerun-tasks` đạt, `git diff --check` sạch.
  Chưa đánh dấu `[x]`: T-304 và typed map-source contract vẫn thiếu qualifying
  IL2CPP Evidence, nên collector Phase 4 chưa được phép coi scanner cũ là authoritative.

  </details>

- [ ] **T-402** **[zygisk]** *(native)* — Thêm dirty-source/generation cho từng Pokémon source, fort, location, inventory và encounter; hook/callback từ contract `T-306` chỉ ghi cờ/generation bounded, không enumerate collection hoặc mutate gameplay. Reconcile timer chỉ làm fallback khi event bị mất, với interval và reason quan sát được; bỏ `kCatchSpinScanEveryTicks` như trigger full scan mặc định. **AC:** `AC-14`; code không còn full map/inventory scan khoảng 2 giây khi state không dirty, callback không giữ object/pointer qua frame và source update không làm mất các source khác. **Phụ thuộc:** `T-306`, `T-401`. **Verify:** Existing host-native checks + static call-path review cho dirty coalescing, bounded queue và idle path; compile log counters để user quan sát sau, không chạy device.

  <details>
  <summary>Đã triển khai dirty-generation nền — T-402</summary>

  World-state store hiện giữ generation riêng cho Pokémon/fort/location/inventory/
  encounter bằng bitmask bounded, không có queue managed object. Encounter transition,
  direct-catch terminal, spin terminal, transfer attempt và discard terminal chỉ mark
  source dirty; legacy observer chỉ reconcile map fallback mỗi 30 giây với log reason,
  không còn tự mark map mỗi ~200 ms. Native build hai ABI đạt. Chưa đánh dấu `[x]`:
  catch-spin coordinator vẫn có `kCatchSpinScanEveryTicks` ~2 giây và source callback
  typed của T-306 chưa có qualifying IL2CPP Evidence; phần đó phải được thay cùng
  T-403/T-406 thay vì giả hook/callback.

  </details>

- [ ] **T-403** **[zygisk]** *(native)* — Thay scanner cũ trong `runtime_map_reader.inc`/`runtime_map_dictionary.inc` bằng collector + typed adapters tách file (đề xuất `runtime_map_sources.inc`) chạy qua `runtime_main_thread_bridge.inc`; resolve owner muộn và enumerate mọi source `SUPPORTED_OFFLINE` từ `T-306`, cùng fort/location. Normalize POD tối thiểu gồm `sourceType`, stable id, encounter id, species, coordinate/distance, expiry, proto/shiny-known flags, observed revision; unknown class không cast thành `WildMapPokemon`. Commit `MapSnapshot` với `READY_COMPLETE`, `READY_PARTIAL`, `EMPTY`, `STALE`, `ERROR`. **AC:** `AC-14`; scanner code không còn đồng nghĩa với đọc `MapEntityCell.eghw`, state chỉ được schedule trên Unity main thread và snapshot không chứa managed reference. **Phụ thuộc:** `T-306`, `T-401`, `T-402`. **Verify:** Native build + existing host checks; audit source registry/normalizer/availability branches và generated diagnostic fields. Đối chiếu live source/fort/revision/scene nằm trong handoff user.

  <details>
  <summary>Đã triển khai collector đa source nền — T-403</summary>

  Đã thay đường đọc riêng `MapEntityCell.eghw` bằng accessor public
  `MapEntityCell.get_AllCellWildPokemon()` của đúng build 0.433.0. Accessor được
  resolve bằng exact image/namespace/class/signature/return type và metadata token
  `0x0601378f`; mismatch làm `READ_NEARBY` fail-closed. Collector mới nằm trong
  `runtime_map_sources.inc`, enumerate trên Unity main thread và chỉ copy POD vào
  snapshot. Registry allowlist phân loại riêng `Wild`, `Incense`, `TroyDisk`,
  `DailyEncounter`, `Photobomb`, `Incident`, `PokestopSpawn`, `Route`, `Raid` và
  `Station`; object lạ không bị cast thành Wild. Mỗi object được normalize qua
  contract chung `get_EncounterId()`/`get_Pokemon()`/`get_Location()` với hierarchy
  lookup. Unknown/unreadable/capped item làm snapshot partial, có counter và log
  theo từng source; direct auto-catch hiện chỉ nhận source `Wild` để việc mở rộng
  read không tự mở mutation chưa chứng minh. Action lookup cũng dùng
  `MapEntityCell.GetMapPokemon(UInt64)` với exact token `0x0601379e`, không còn
  fallback `eghw`.

  Source type hiện là metadata nội bộ native và chưa serialize trong nearby payload
  v2; Kotlin vẫn nhận toàn bộ record normalized mà không có source label. Expiry,
  create/update/remove signal, membership thực tế của từng concrete source trong
  collection, owner lifetime và snapshot-completeness vẫn `UNVERIFIED_DEVICE`, nên
  giữ `[ ]` và capability mutation fail-closed cho tới handoff BlueStacks. Verify
  host: `./scripts/build-magisk.sh` đạt arm64/x86_64 + package; 6 existing native
  host checks đạt; `./gradlew test assembleDebug --rerun-tasks` đạt 52 tasks;
  `git diff --check` sạch.

  </details>

- [ ] **T-404** **[zygisk]** *(native)* — Chuẩn hóa owner lifecycle trong `runtime_scene_owners.inc`, `runtime_probe_service_owners.inc`, `runtime_encounter_owners.inc` và `runtime_main_thread_bridge_lifecycle.inc`: tăng generation khi scene/session/target process đổi, invalidate cache/pending cũ, rebind muộn và chỉ giữ GC handle cho Promise đang theo dõi. **AC:** Code không reuse owner/pointer của scene trước; reconnect tạo session mới và action cũ không thể complete nhầm. **Phụ thuộc:** `T-401`, `T-403`. **Verify:** Existing host-native lifecycle checks hoặc deterministic state walkthrough nếu test không có; static review mọi cached owner/pending có generation guard và reset path. Live reload chuyển sang handoff user.

  <details>
  <summary>Bị chặn — T-404</summary>

  BLOCKED: static RVA không chứng minh owner lifecycle/generation hoặc pointer
  validity. T-303/T-304 cần focused call-path và runtime owner evidence trước
  khi thay lifecycle code. Profile vẫn fail-closed; không đánh dấu hoàn tất.

  </details>

- [ ] **T-405** **[zygisk]** *(native)* — Chuyển `runtime_inventory_common.inc`, `runtime_inventory_full.inc`, `runtime_inventory_request.inc` sang inventory collector main-thread; trigger khi inventory/bag dirty và ngay sau spin/transfer/discard terminal result, dùng fallback thưa (mốc đề xuất 30 giây) chỉ khi cần reconcile. Commit full immutable counts/storage summary vào world state. **AC:** Policy không đọc count cache cũ ngay trước mutation; refresh failure thành `STALE/ERROR`, không thành count 0. **Phụ thuộc:** `T-304`, `T-401`, `T-402`, `T-404`. **Verify:** Existing host-native checks + call-path/timer review chứng minh dirty trigger, bounded fallback và empty/error branches; compile diagnostic counters cho handoff user.

  <details>
  <summary>Bị chặn — T-405</summary>

  BLOCKED: storage implementation RVAs đã có, nhưng signature/ABI,
  expiring-items source, refresh event và postcondition chưa được chứng minh.
  Inventory collector không được gọi binding chỉ dựa trên RVA.

  </details>

- [ ] **T-406** **[zygisk]** *(native)* — Sửa `modules/catch_spin/coordinator.inc`, `shared/runtime/control/runtime_desired_state_reconcile.inc` và `runtime_action_common.inc` để module consume snapshot theo revision/freshness, không tự enumerate map mỗi chu kỳ. Tạo một action arbiter chung cho catch/spin/transfer/discard với pending owner/session/revision, timeout và lý do block. **AC:** Chỉ một gameplay mutation active; config mới được replay nhưng pending/pointer không persist qua reconnect; stale snapshot không dispatch. **Phụ thuộc:** `T-401`–`T-405`. **Verify:** Existing host checks và deterministic concurrent-intent/session-reset paths; native build chứng minh integration compile, không chạy mutation/device diagnostic.

  <details>
  <summary>Bị chặn — T-406</summary>

  BLOCKED: action arbiter phụ thuộc typed collectors và terminal contracts còn
  thiếu exact ABI/owner/outcome evidence. Static RVA không đủ để dispatch hoặc
  correlate completion. Profile vẫn fail-closed; không đánh dấu hoàn tất.

  </details>

- [ ] **T-407** **[bridge]** *(native/android)* — Mở rộng `runtime_observation_protocol.h`, `runtime_ui_status_protocol.h`, `runtime_ui_status_bridge.inc`, `RuntimeUiStateStore.kt` và `RuntimeUiEventRouter.kt` để truyền session/revision, availability/freshness, dirty/read error, active action, `UNVERIFIED_DEVICE` và capability reason. Bump protocol version nếu wire format đổi; decoder cũ phải fail rõ. **AC:** UI model phân biệt not-ready/empty/stale/error/unverified và không suy “ready” chỉ từ kết nối socket. **Phụ thuộc:** `T-401`, `T-406`. **Verify:** Native + Gradle existing checks; encode/decode cùng protocol version và static mapper review. UI live rendering nằm trong handoff user.

  <details>
  <summary>Bị chặn — T-407</summary>

  BLOCKED: protocol/UI state phụ thuộc availability và arbiter semantics của
  T-403–T-406, vốn chưa thể hoàn tất từ static RVA. Profile vẫn fail-closed;
  không đánh dấu hoàn tất.

  </details>

- [ ] **T-408** **[build]** *(reviewer)* — Verify code/build phase với live capability gắn `UNVERIFIED_DEVICE`: native build/package, existing host checks, `./gradlew test assembleDebug`, protocol/source registry inspection, `git diff --check` và line-count audit. Không gọi script BlueStacks/diagnostics. **AC:** `AC-05`, `AC-06`, `AC-12`, `AC-14` đạt ở mức implementation/build; code idle path không full-scan, typed source coverage và lifecycle reset có host/static evidence. **Phụ thuộc:** `T-401`–`T-407`. **Verify:** Lưu command/exit code/artifact/hash và danh sách observability fields để user kiểm tra live; task không phụ thuộc log thiết bị.

  <details>
  <summary>Bị chặn — T-408</summary>

  BLOCKED: build-level completion phụ thuộc T-403–T-407. Mapping đã loại bỏ
  token-to-RVA gap nhưng chưa cung cấp typed/runtime contracts cần để triển
  khai an toàn. Không đánh dấu task hoàn tất.

  </details>

## Điều kiện hoàn tất

- World state theo session/revision hoạt động và không giữ managed pointer.
- Event/dirty signal là đường chính; reconcile polling chỉ là fallback quan sát được.
- Snapshot bao phủ mọi source 0.433.0 đã xác minh và công khai partial/unsupported,
  không tiếp tục coi wild dictionary là toàn bộ Pokémon xung quanh.
- Build/host checks đạt; live source coverage, main-thread behavior và scene reload
  được ghi vào handoff user, không chạy bởi agent.
- UI nhận đúng availability/freshness và mutation arbiter đã sẵn sàng.

Sau mỗi task, agent phải cập nhật checkbox và khối `<details>` của chính task đó
trước khi chuyển sang task kế tiếp.
