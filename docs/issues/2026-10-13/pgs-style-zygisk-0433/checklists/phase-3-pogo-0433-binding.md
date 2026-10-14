# Phase 3 — Reverse và binding profile Pokémon GO 0.433.0

Plan: [Tổng quan](00-overview.md)

## Điều kiện bắt đầu

- Evidence ledger từ Phase 0 tồn tại.
- Có artifact 0.433.0 đúng identity dưới `reverse/artifacts/`; binary lớn là
  input read-only bị Git ignore, còn manifest/report/bundle REA được giữ trong repo.
- Đã import/query bundle REA trước khi tạo evidence mới; không dùng script
  reverse cũ hoặc Il2CppDumper output 0.427.0 làm bằng chứng.
- Mọi mutation capability vẫn đóng trong lúc discovery.

## Task

- [x] **T-301** **[reverse]** *(native/reverse)* — Dùng REA làm pipeline duy nhất: kiểm `reverse/artifacts/SHA256SUMS`, import/query `reverse/rea/rea-evidence-2026-10-13.json`, xác nhận inventory APKM/base/arm64 split và extraction provenance của `native/libil2cpp.so`/`global-metadata.dat`. Chạy `inspect_android_package` trên base APK khi JADX provider khả dụng; dùng `open_binary`/native provider cho ELF. Evidence mới phải được export deterministic về bundle canonical và index vào ledger; thiếu provider thì giữ residual unknown, không khôi phục `scripts/reverse-pogo-apk.sh` hay dùng dump 0.427.0. **AC:** Mọi discovery sau dùng đúng artifact đã pin và có thể tái dùng từ artifact + REA bundle trong repo. **Phụ thuộc:** `T-001`, `T-003`. **Verify:** hash manifest pass; REA Evidence `ev_022695...`, `ev_9c80a3...`, `ev_d5331c...` liên kết APKM → base/split → libil2cpp/metadata; bundle export có count/hash ghi trong ledger.

  <details>
  <summary>Đã thực hiện — T-301</summary>

  Đã verify đủ 10 hash và nối provenance APKM→base/arm64 split→`libil2cpp.so`/metadata qua `ev_022695...`, `ev_9c80a3...`, `ev_d5331c...`. Pass tiếp theo dùng JADX 0.7.1 xác nhận package/version/versionCode trong `ev_59ea3043...`; Ghidra 12.1.3 nhận đúng ELF arm64 nhưng truy vấn đầu tiên timeout trước khi sinh Evidence. Bundle canonical đã export lại thành 18 records/2 residual unknown; không dùng dump/script 0.427.0.

  </details>

- [x] **T-302** **[reverse]** *(native/reverse)* — Dùng REA Android/native analysis để trích evidence cho map/location owner của 0.433.0: `MapEntityCell`, `MapPokemon`, fort entity, map-cell collection/dictionary, player-location service và toàn bộ source type PGS-style cần kiểm tra (`WildMapPokemon`, `IncenseMapPokemon`, `TroyDiskMapPlacePokemon`, `DailyEncounterMapPokemon`, `PhotobombingMapPokemon`, `IncidentMapPokemon`, `PokestopSpawnPokemon`, `RouteMapPokemon`, `RaidMapPokemon`, `StationMapPokemon`). Với mỗi class ghi tồn tại/không tồn tại trong build, assembly/namespace, field/method, managed type, owner lifetime, thread requirement và Evidence ID. Không copy `eghw`/layout 0.427.0 hoặc report cũ sang 0.433.0 nếu chưa chứng minh. **AC:** `AC-14`; collector có inventory đầy đủ các source hiện diện trong build, phân biệt Pokémon/fort/vị trí/cell availability và đánh `UNRESOLVED` cho candidate chưa chắc chắn. **Phụ thuộc:** `T-301`. **Verify:** `search_android_classes` → `inspect_android_class/method` → `trace_android_references` và native xref/function dossier khi cần; ledger có một hàng/Evidence ID cho từng source type. Runtime owner/lifetime gắn `UNVERIFIED_DEVICE`, không chạy device.

  <details>
  <summary>Đã thực hiện — T-302</summary>

  Ledger đã liệt kê đủ 10 candidate source, `MapEntityCell`, fort và location.
  Mapping metadata v39 từ đúng cặp artifact xác nhận cả 10 source type và cung
  cấp RVA cho các method được lọc; ví dụ `MapEntityCell.GetMapPokemon` ở
  `0x9dd4b54`. Owner/lifetime/field/event/snapshot semantics vẫn
  `UNVERIFIED_DEVICE`/`UNRESOLVED`; không layout 0.427.0 nào được promote và
  runtime source tương ứng vẫn bị loại khỏi capability.

  </details>

- [ ] **T-303** **[reverse]** *(native/reverse)* — Trích bằng REA contract catch trực tiếp 0.433.0: owner/service, method resolve tuple (assembly/namespace/class/name/arity/param types/return type), request/ball fields, Promise/result type, captured storage ID, server status và map-removal/postcondition. Với từng source type ở `T-302`, ghi khả năng resolve về live `MapPokemon`/subclass và direct-catch eligibility; source không hỗ trợ phải có reason. **AC:** `T-501`–`T-503` không cần đoán method/field/Promise và biết terminal success/failure/indeterminate cho mọi source được quảng bá. **Phụ thuộc:** `T-301`, `T-302`. **Verify:** Android method/reference Evidence kết hợp native `analyze_function`/call-path Evidence cùng exact artifact/profile; compile diagnostic guards. Phần owner/return/source eligibility chưa có runtime evidence ghi `UNVERIFIED_DEVICE`.

  <details>
  <summary>Đang điều tra — T-303</summary>

  Cập nhật 2026-10-14: mapping metadata v39 schema v2 xác nhận exact instance
  signature `MapPokemon.TryCapture(PokeballThrow,
  ARPlusEncounterValuesProto) -> IPromise<CatchPokemonOutProto>`, token
  `0x06013ab5`, slot 44 và RVA `0x9e1caa4`. Type contracts ghi layout value-type
  `PokeballThrow` cùng `CatchPokemonOutProto.Status`/
  `CapturedPokemonId`. Hybrid resolver đã enforce image/class/full signature,
  instance flag, unique match và token; `runtime_invoke` vẫn là call path.
  REA dossier `ev_51dac350...` chỉ chứng minh RVA nằm trong coarse function
  `FUN_09e1c6cc`, nên RVA equality/direct-call bị vô hiệu. Live owner lifetime,
  concrete Promise lifecycle, source eligibility và authoritative map/storage
  postcondition vẫn `UNVERIFIED_DEVICE`; `DIRECT_CATCH` giữ `UNAVAILABLE` và
  task chưa đạt AC.

  </details>

- [ ] **T-304** **[reverse]** *(native/reverse)* — Hoàn thiện bằng REA contract storage cho transfer/discard: xác nhận `IPokemonBag`/`PokemonBagImpl.GetPokemon` và `ReleasePokemon`, `IItemBag`/`ItemBagImpl.GetItemCount` và `RecycleItem`; trích `ItemData` layout/constructor, `ExpiringItemsCopy` source/owner, Promise/result type, bag refresh event và postcondition. Token trong overview chỉ dùng làm identity, tuyệt đối không dùng làm RVA. **AC:** Transfer/discard có danh sách binding offline đã chứng minh và blocker runtime tách biệt; không bật mutation chỉ vì tìm thấy token/report. **Phụ thuộc:** `T-301`. **Verify:** Mỗi signature/slot/token/layout có artifact hash + REA Evidence ID từ class/method/reference hoặc native dossier; owner/lifetime/postcondition cần device ghi `UNVERIFIED_DEVICE` và capability live-ready giữ đóng.

  <details>
  <summary>Đang điều tra — T-304</summary>

  Cập nhật 2026-10-14: metadata mapping schema v2 đã resolve exact full
  signatures/return types cho `PokemonBagImpl.GetPokemon` `0xa04bd58`,
  `ReleasePokemon` `0xa04cf68`, `ItemBagImpl.GetItemCount` `0x9ffe438` và
  `RecycleItem` `0xa006bec`. Mapping cũng ghi `ItemData` typeDef 2769,
  fields/offsets, constructor 10 tham số và
  `ItemInventoryService.get_ExpiringItemsCopy()`. Hybrid resolver enforce
  image/class/signature/token và giữ RVA ở mức evidence-only vì dossiers
  `ev_987116...`, `ev_7f6095...`, `ev_2dfe76...`, `ev_d76804...` chưa chứng
  minh exact native entry. Live service owner/lifetime, concrete Promise
  lifecycle, refresh signal và authoritative storage/inventory postcondition
  vẫn `UNVERIFIED_DEVICE`. Transfer/discard mutation không được mở và task
  chưa đạt AC.

  </details>

- [x] **T-306** **[reverse]** *(native/reverse)* — Chốt PGS-style scan contract vào evidence ledger bằng REA: với từng source đã tìm ở `T-302`, xác định collection/service sở hữu, create/update/remove event hoặc callback, stable identity, expiry, proto/species/coordinate accessor, cell/scene generation và semantics “snapshot complete”. Xác định fort update/cooldown source riêng; field wild dictionary cũ không được đại diện cho toàn bộ map. **AC:** `AC-14`; implementation biết cách thu full source set và khi nào được đánh `READY_COMPLETE`, `READY_PARTIAL`, `EMPTY` hoặc `STALE`. **Phụ thuộc:** `T-302`. **Verify:** REA class/method/reference và native call-path evidence; mỗi source có verdict `SUPPORTED_OFFLINE`, `ABSENT_IN_BUILD` hoặc `UNRESOLVED` kèm Evidence ID/limitation. Event trace thật chuyển handoff user.

  <details>
  <summary>Đã thực hiện — T-306</summary>

  Ledger có một verdict cho từng source. Metadata map xác nhận cả 10 type tồn
  tại trong 0.433.0, nhưng collection owner/event/stable-id/expiry/accessor/
  generation/snapshot semantics vẫn thiếu. Scan contract runtime hợp lệ duy
  nhất hiện tại là `UNRESOLVED`; runtime không được quảng bá READY/partial từ
  dictionary cũ.

  </details>

- [ ] **T-307** **[reverse]** *(native/reverse)* — Discovery riêng bằng REA cho Virtual GO Plus fallback trong 0.433.0: tìm accessory/session owner, connect/disconnect state, nearby notification/select/catch request, Poké Ball item 1 rule, result callback/Promise, captured storage ID và cooldown/rate semantics. Không giả định tên protocol từ bản cũ và không tái sử dụng `TryCapture` outcome làm bằng chứng cho fallback. **AC:** `AC-13`; có contract offline đủ để phân biệt fallback thật với direct catch và để capability giữ `UNAVAILABLE` nếu thiếu evidence. **Phụ thuộc:** `T-301`, `T-303`. **Verify:** REA class/method/reference/native dossier ghi exact class/method/params/result/Evidence ID hoặc residual unknown; owner/lifetime runtime được instrument thành diagnostic và ghi `UNVERIFIED_DEVICE`, không dispatch catch.

  <details>
  <summary>Đang điều tra — T-307</summary>

  Chưa có Evidence cho accessory/session owner, nearby/select/catch request,
  ball rule, result ID hoặc cooldown. Ledger tách riêng fallback và giữ
  capability `UNAVAILABLE`; không tái sử dụng `TryCapture` làm bằng chứng.

  Cập nhật 2026-10-14: focused metadata map cũng không có row chứa exact key
  `VirtualGoPlus`; điều này chỉ phủ regex đã chọn, không chứng minh accessory
  protocol/type vắng mặt hoặc tên khác. Owner/session/request/result/cooldown
  vẫn chưa có qualifying Evidence. Fallback tiếp tục `UNAVAILABLE` và task
  chưa đạt AC.

  </details>

- [x] **T-305** **[zygisk]** *(native/reviewer)* — Thêm profile build cố định (đề xuất `zygisk/jni/shared/runtime/probe/runtime_build_profile.h` và `.inc`), nối vào `runtime_binding_probe.inc`, `runtime_capabilities.inc`, `main.cpp` và allowlist app để kiểm package/versionCode/build ID/metadata hash 0.433.0 trước mutation. Build khác chỉ diagnostic/read-only và trả `unsupported_build`; không có network/update fallback. **AC:** Chỉ đúng fingerprint mới đi qua code path capability; trạng thái vẫn `UNVERIFIED_DEVICE` tới user test, mismatch không gọi action method. **Phụ thuộc:** `T-301`–`T-304`. **Verify:** Native build/package; existing host check + offline positive/negative fingerprint fixtures nếu đã có; `git diff --check` và line-count audit. Probe live chuyển sang handoff user.

  <details>
  <summary>Đã thực hiện — T-305</summary>

  Đã thêm `runtime_build_profile.h` pin package/version/ABI/build-id/metadata/
  base APK của 0.433.0. Runtime tự quan sát package + versionCode qua JNI,
  GNU build-id từ loaded ELF và SHA-256 của installed `global-metadata.dat`;
  controller không cấp fingerprint. Mismatch trả reason riêng và fail closed.
  Native arm64/x86_64 build/package đạt; execution/outcome gates vẫn tách khỏi
  build identity, nên live status giữ `UNVERIFIED_DEVICE` tới handoff device.

  </details>

## Điều kiện hoàn tất

- Binding ledger 0.433.0 đủ nguồn, tuple, type, owner/lifetime và unknown.
- Full-source scan inventory và Virtual GO Plus fallback có verdict/evidence riêng;
  thiếu evidence không chặn direct catch nhưng giữ fallback capability đóng.
- Runtime fingerprint guard fail closed trên build khác.
- Catch/transfer/discard capability chưa được bật nếu evidence tương ứng chưa đạt.

Sau mỗi task, agent phải cập nhật checkbox và khối `<details>` của chính task đó
trước khi chuyển sang task kế tiếp.
