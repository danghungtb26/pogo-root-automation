# Phase 4 — Native world state event-driven

Plan: [Tổng quan](00-overview.md)

## Điều kiện bắt đầu

- `T-305` có profile/fingerprint guard hoạt động.
- `T-302` và `T-304` đã mô tả owner, lifetime và thread requirement của state.
- Thay đổi native chưa commit hiện hữu đã được rebase logic bằng chỉnh sửa hẹp, không bị xóa.

## Task

- [ ] **T-401** **[zygisk]** *(native)* — Thêm world-state store tách file (đề xuất `zygisk/jni/shared/runtime/state/runtime_world_state.h` và `.inc`) chứa snapshot bất biến cho session/location/map/inventory/encounter, `ownerGeneration`, `dirtyGeneration`, `revision`, timestamp và availability `NOT_READY`, `EMPTY`, `READY`, `STALE`, `ERROR`. Collector build bản tạm rồi commit nguyên tử; không lưu managed pointer trong snapshot. Wire declaration/include tối thiểu qua `runtime_native_declarations.inc`, `runtime_native_prelude.inc` và `main.cpp`. **AC:** Reader thấy một revision nhất quán và phân biệt empty hợp lệ với read failure. **Phụ thuộc:** `T-302`, `T-304`, `T-305`. **Verify:** Native compile + existing host checks; review memory ownership/concurrency; forced session reset làm revision/generation đổi và xóa snapshot cũ.

  <details>
  <summary>Bị chặn — T-401</summary>

  BLOCKED: storage/map owner contracts T-303/T-304 chưa có REA provider evidence, nên không thể xây collector/arbiter/protocol dựa trên type/owner đoán. Profile 0.433.0 đã giữ toàn bộ mutation fail-closed; scanner 0.427.0 không được coi là implementation hợp lệ. Không đánh dấu task hoàn tất.

  </details>

- [ ] **T-402** **[zygisk]** *(native)* — Thêm dirty-source/generation cho từng Pokémon source, fort, location, inventory và encounter; hook/callback từ contract `T-306` chỉ ghi cờ/generation bounded, không enumerate collection hoặc mutate gameplay. Reconcile timer chỉ làm fallback khi event bị mất, với interval và reason quan sát được; bỏ `kCatchSpinScanEveryTicks` như trigger full scan mặc định. **AC:** `AC-14`; code không còn full map/inventory scan khoảng 2 giây khi state không dirty, callback không giữ object/pointer qua frame và source update không làm mất các source khác. **Phụ thuộc:** `T-306`, `T-401`. **Verify:** Existing host-native checks + static call-path review cho dirty coalescing, bounded queue và idle path; compile log counters để user quan sát sau, không chạy device.

  <details>
  <summary>Bị chặn — T-402</summary>

  BLOCKED: storage/map owner contracts T-303/T-304 chưa có REA provider evidence, nên không thể xây collector/arbiter/protocol dựa trên type/owner đoán. Profile 0.433.0 đã giữ toàn bộ mutation fail-closed; scanner 0.427.0 không được coi là implementation hợp lệ. Không đánh dấu task hoàn tất.

  </details>

- [ ] **T-403** **[zygisk]** *(native)* — Thay scanner cũ trong `runtime_map_reader.inc`/`runtime_map_dictionary.inc` bằng collector + typed adapters tách file (đề xuất `runtime_map_sources.inc`) chạy qua `runtime_main_thread_bridge.inc`; resolve owner muộn và enumerate mọi source `SUPPORTED_OFFLINE` từ `T-306`, cùng fort/location. Normalize POD tối thiểu gồm `sourceType`, stable id, encounter id, species, coordinate/distance, expiry, proto/shiny-known flags, observed revision; unknown class không cast thành `WildMapPokemon`. Commit `MapSnapshot` với `READY_COMPLETE`, `READY_PARTIAL`, `EMPTY`, `STALE`, `ERROR`. **AC:** `AC-14`; scanner code không còn đồng nghĩa với đọc `MapEntityCell.eghw`, state chỉ được schedule trên Unity main thread và snapshot không chứa managed reference. **Phụ thuộc:** `T-306`, `T-401`, `T-402`. **Verify:** Native build + existing host checks; audit source registry/normalizer/availability branches và generated diagnostic fields. Đối chiếu live source/fort/revision/scene nằm trong handoff user.

  <details>
  <summary>Bị chặn — T-403</summary>

  BLOCKED: storage/map owner contracts T-303/T-304 chưa có REA provider evidence, nên không thể xây collector/arbiter/protocol dựa trên type/owner đoán. Profile 0.433.0 đã giữ toàn bộ mutation fail-closed; scanner 0.427.0 không được coi là implementation hợp lệ. Không đánh dấu task hoàn tất.

  </details>

- [ ] **T-404** **[zygisk]** *(native)* — Chuẩn hóa owner lifecycle trong `runtime_scene_owners.inc`, `runtime_probe_service_owners.inc`, `runtime_encounter_owners.inc` và `runtime_main_thread_bridge_lifecycle.inc`: tăng generation khi scene/session/target process đổi, invalidate cache/pending cũ, rebind muộn và chỉ giữ GC handle cho Promise đang theo dõi. **AC:** Code không reuse owner/pointer của scene trước; reconnect tạo session mới và action cũ không thể complete nhầm. **Phụ thuộc:** `T-401`, `T-403`. **Verify:** Existing host-native lifecycle checks hoặc deterministic state walkthrough nếu test không có; static review mọi cached owner/pending có generation guard và reset path. Live reload chuyển sang handoff user.

  <details>
  <summary>Bị chặn — T-404</summary>

  BLOCKED: storage/map owner contracts T-303/T-304 chưa có REA provider evidence, nên không thể xây collector/arbiter/protocol dựa trên type/owner đoán. Profile 0.433.0 đã giữ toàn bộ mutation fail-closed; scanner 0.427.0 không được coi là implementation hợp lệ. Không đánh dấu task hoàn tất.

  </details>

- [ ] **T-405** **[zygisk]** *(native)* — Chuyển `runtime_inventory_common.inc`, `runtime_inventory_full.inc`, `runtime_inventory_request.inc` sang inventory collector main-thread; trigger khi inventory/bag dirty và ngay sau spin/transfer/discard terminal result, dùng fallback thưa (mốc đề xuất 30 giây) chỉ khi cần reconcile. Commit full immutable counts/storage summary vào world state. **AC:** Policy không đọc count cache cũ ngay trước mutation; refresh failure thành `STALE/ERROR`, không thành count 0. **Phụ thuộc:** `T-304`, `T-401`, `T-402`, `T-404`. **Verify:** Existing host-native checks + call-path/timer review chứng minh dirty trigger, bounded fallback và empty/error branches; compile diagnostic counters cho handoff user.

  <details>
  <summary>Bị chặn — T-405</summary>

  BLOCKED: storage/map owner contracts T-303/T-304 chưa có REA provider evidence, nên không thể xây collector/arbiter/protocol dựa trên type/owner đoán. Profile 0.433.0 đã giữ toàn bộ mutation fail-closed; scanner 0.427.0 không được coi là implementation hợp lệ. Không đánh dấu task hoàn tất.

  </details>

- [ ] **T-406** **[zygisk]** *(native)* — Sửa `modules/catch_spin/coordinator.inc`, `shared/runtime/control/runtime_desired_state_reconcile.inc` và `runtime_action_common.inc` để module consume snapshot theo revision/freshness, không tự enumerate map mỗi chu kỳ. Tạo một action arbiter chung cho catch/spin/transfer/discard với pending owner/session/revision, timeout và lý do block. **AC:** Chỉ một gameplay mutation active; config mới được replay nhưng pending/pointer không persist qua reconnect; stale snapshot không dispatch. **Phụ thuộc:** `T-401`–`T-405`. **Verify:** Existing host checks và deterministic concurrent-intent/session-reset paths; native build chứng minh integration compile, không chạy mutation/device diagnostic.

  <details>
  <summary>Bị chặn — T-406</summary>

  BLOCKED: storage/map owner contracts T-303/T-304 chưa có REA provider evidence, nên không thể xây collector/arbiter/protocol dựa trên type/owner đoán. Profile 0.433.0 đã giữ toàn bộ mutation fail-closed; scanner 0.427.0 không được coi là implementation hợp lệ. Không đánh dấu task hoàn tất.

  </details>

- [ ] **T-407** **[bridge]** *(native/android)* — Mở rộng `runtime_observation_protocol.h`, `runtime_ui_status_protocol.h`, `runtime_ui_status_bridge.inc`, `RuntimeUiStateStore.kt` và `RuntimeUiEventRouter.kt` để truyền session/revision, availability/freshness, dirty/read error, active action, `UNVERIFIED_DEVICE` và capability reason. Bump protocol version nếu wire format đổi; decoder cũ phải fail rõ. **AC:** UI model phân biệt not-ready/empty/stale/error/unverified và không suy “ready” chỉ từ kết nối socket. **Phụ thuộc:** `T-401`, `T-406`. **Verify:** Native + Gradle existing checks; encode/decode cùng protocol version và static mapper review. UI live rendering nằm trong handoff user.

  <details>
  <summary>Bị chặn — T-407</summary>

  BLOCKED: storage/map owner contracts T-303/T-304 chưa có REA provider evidence, nên không thể xây collector/arbiter/protocol dựa trên type/owner đoán. Profile 0.433.0 đã giữ toàn bộ mutation fail-closed; scanner 0.427.0 không được coi là implementation hợp lệ. Không đánh dấu task hoàn tất.

  </details>

- [ ] **T-408** **[build]** *(reviewer)* — Verify code/build phase với live capability gắn `UNVERIFIED_DEVICE`: native build/package, existing host checks, `./gradlew test assembleDebug`, protocol/source registry inspection, `git diff --check` và line-count audit. Không gọi script BlueStacks/diagnostics. **AC:** `AC-05`, `AC-06`, `AC-12`, `AC-14` đạt ở mức implementation/build; code idle path không full-scan, typed source coverage và lifecycle reset có host/static evidence. **Phụ thuộc:** `T-401`–`T-407`. **Verify:** Lưu command/exit code/artifact/hash và danh sách observability fields để user kiểm tra live; task không phụ thuộc log thiết bị.

  <details>
  <summary>Bị chặn — T-408</summary>

  BLOCKED: storage/map owner contracts T-303/T-304 chưa có REA provider evidence, nên không thể xây collector/arbiter/protocol dựa trên type/owner đoán. Profile 0.433.0 đã giữ toàn bộ mutation fail-closed; scanner 0.427.0 không được coi là implementation hợp lệ. Không đánh dấu task hoàn tất.

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
