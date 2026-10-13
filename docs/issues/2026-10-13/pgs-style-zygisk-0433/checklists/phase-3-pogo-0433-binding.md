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

  Đã verify đủ 10 hash, import/query bundle canonical (17 records, 2 residual unknown), và nối provenance APKM→base/arm64 split→`libil2cpp.so`/metadata qua `ev_022695...`, `ev_9c80a3...`, `ev_d5331c...`. `inspect_android_package` báo thiếu JADX MCP 0.7.1; native session báo không có Ghidra/provider. Không có Evidence mới nên không export bundle giả và không dùng dump/script 0.427.0.

  </details>

- [x] **T-302** **[reverse]** *(native/reverse)* — Dùng REA Android/native analysis để trích evidence cho map/location owner của 0.433.0: `MapEntityCell`, `MapPokemon`, fort entity, map-cell collection/dictionary, player-location service và toàn bộ source type PGS-style cần kiểm tra (`WildMapPokemon`, `IncenseMapPokemon`, `TroyDiskMapPlacePokemon`, `DailyEncounterMapPokemon`, `PhotobombingMapPokemon`, `IncidentMapPokemon`, `PokestopSpawnPokemon`, `RouteMapPokemon`, `RaidMapPokemon`, `StationMapPokemon`). Với mỗi class ghi tồn tại/không tồn tại trong build, assembly/namespace, field/method, managed type, owner lifetime, thread requirement và Evidence ID. Không copy `eghw`/layout 0.427.0 hoặc report cũ sang 0.433.0 nếu chưa chứng minh. **AC:** `AC-14`; collector có inventory đầy đủ các source hiện diện trong build, phân biệt Pokémon/fort/vị trí/cell availability và đánh `UNRESOLVED` cho candidate chưa chắc chắn. **Phụ thuộc:** `T-301`. **Verify:** `search_android_classes` → `inspect_android_class/method` → `trace_android_references` và native xref/function dossier khi cần; ledger có một hàng/Evidence ID cho từng source type. Runtime owner/lifetime gắn `UNVERIFIED_DEVICE`, không chạy device.

  <details>
  <summary>Đã thực hiện — T-302</summary>

  Ledger đã liệt kê đủ 10 candidate source, `MapEntityCell`, fort và location. Mỗi hàng có verdict `UNRESOLVED`, residual Evidence/limitation và owner/lifetime `UNVERIFIED_DEVICE`. Không class/field/layout nào từ 0.427.0 được promote; vì provider thiếu, runtime source tương ứng phải bị loại khỏi capability.

  </details>

- [ ] **T-303** **[reverse]** *(native/reverse)* — Trích bằng REA contract catch trực tiếp 0.433.0: owner/service, method resolve tuple (assembly/namespace/class/name/arity/param types/return type), request/ball fields, Promise/result type, captured storage ID, server status và map-removal/postcondition. Với từng source type ở `T-302`, ghi khả năng resolve về live `MapPokemon`/subclass và direct-catch eligibility; source không hỗ trợ phải có reason. **AC:** `T-501`–`T-503` không cần đoán method/field/Promise và biết terminal success/failure/indeterminate cho mọi source được quảng bá. **Phụ thuộc:** `T-301`, `T-302`. **Verify:** Android method/reference Evidence kết hợp native `analyze_function`/call-path Evidence cùng exact artifact/profile; compile diagnostic guards. Phần owner/return/source eligibility chưa có runtime evidence ghi `UNVERIFIED_DEVICE`.

  <details>
  <summary>Bị chặn — T-303</summary>

  BLOCKED bởi residual unknown `unk_1aaa6e...`/`unk_27690a...`: chưa có JADX/native deep provider để chứng minh exact method tuple, request/result/Promise, captured storage ID và postcondition. Ledger đã ghi contract thiếu và `DIRECT_CATCH=UNAVAILABLE`; không triển khai binding đoán.

  </details>

- [ ] **T-304** **[reverse]** *(native/reverse)* — Hoàn thiện bằng REA contract storage cho transfer/discard: xác nhận `IPokemonBag`/`PokemonBagImpl.GetPokemon` và `ReleasePokemon`, `IItemBag`/`ItemBagImpl.GetItemCount` và `RecycleItem`; trích `ItemData` layout/constructor, `ExpiringItemsCopy` source/owner, Promise/result type, bag refresh event và postcondition. Token trong overview chỉ dùng làm identity, tuyệt đối không dùng làm RVA. **AC:** Transfer/discard có danh sách binding offline đã chứng minh và blocker runtime tách biệt; không bật mutation chỉ vì tìm thấy token/report. **Phụ thuộc:** `T-301`. **Verify:** Mỗi signature/slot/token/layout có artifact hash + REA Evidence ID từ class/method/reference hoặc native dossier; owner/lifetime/postcondition cần device ghi `UNVERIFIED_DEVICE` và capability live-ready giữ đóng.

  <details>
  <summary>Bị chặn — T-304</summary>

  BLOCKED bởi cùng provider gap. Bảng token/slot chỉ được giữ làm search identity; mọi storage tuple, `ItemData`, expiring items, Promise/result và refresh signal vẫn `UNRESOLVED`. Transfer/discard mutation không được mở.

  </details>

- [x] **T-306** **[reverse]** *(native/reverse)* — Chốt PGS-style scan contract vào evidence ledger bằng REA: với từng source đã tìm ở `T-302`, xác định collection/service sở hữu, create/update/remove event hoặc callback, stable identity, expiry, proto/species/coordinate accessor, cell/scene generation và semantics “snapshot complete”. Xác định fort update/cooldown source riêng; field wild dictionary cũ không được đại diện cho toàn bộ map. **AC:** `AC-14`; implementation biết cách thu full source set và khi nào được đánh `READY_COMPLETE`, `READY_PARTIAL`, `EMPTY` hoặc `STALE`. **Phụ thuộc:** `T-302`. **Verify:** REA class/method/reference và native call-path evidence; mỗi source có verdict `SUPPORTED_OFFLINE`, `ABSENT_IN_BUILD` hoặc `UNRESOLVED` kèm Evidence ID/limitation. Event trace thật chuyển handoff user.

  <details>
  <summary>Đã thực hiện — T-306</summary>

  Ledger có một verdict cho từng source và ghi rõ toàn bộ collection owner/event/stable-id/expiry/accessor/generation/snapshot semantics còn thiếu. Vì không source nào có Evidence 0.433.0, scan contract hợp lệ duy nhất hiện tại là `UNRESOLVED`; runtime không được quảng bá READY/partial từ dictionary cũ.

  </details>

- [ ] **T-307** **[reverse]** *(native/reverse)* — Discovery riêng bằng REA cho Virtual GO Plus fallback trong 0.433.0: tìm accessory/session owner, connect/disconnect state, nearby notification/select/catch request, Poké Ball item 1 rule, result callback/Promise, captured storage ID và cooldown/rate semantics. Không giả định tên protocol từ bản cũ và không tái sử dụng `TryCapture` outcome làm bằng chứng cho fallback. **AC:** `AC-13`; có contract offline đủ để phân biệt fallback thật với direct catch và để capability giữ `UNAVAILABLE` nếu thiếu evidence. **Phụ thuộc:** `T-301`, `T-303`. **Verify:** REA class/method/reference/native dossier ghi exact class/method/params/result/Evidence ID hoặc residual unknown; owner/lifetime runtime được instrument thành diagnostic và ghi `UNVERIFIED_DEVICE`, không dispatch catch.

  <details>
  <summary>Bị chặn — T-307</summary>

  BLOCKED bởi provider gap: chưa có Evidence cho accessory/session owner, nearby/select/catch request, ball rule, result ID hoặc cooldown. Ledger tách riêng fallback và giữ capability `UNAVAILABLE`; không tái sử dụng `TryCapture` lịch sử.

  </details>

- [x] **T-305** **[zygisk]** *(native/reviewer)* — Thêm profile build cố định (đề xuất `zygisk/jni/shared/runtime/probe/runtime_build_profile.h` và `.inc`), nối vào `runtime_binding_probe.inc`, `runtime_capabilities.inc`, `main.cpp` và allowlist app để kiểm package/versionCode/build ID/metadata hash 0.433.0 trước mutation. Build khác chỉ diagnostic/read-only và trả `unsupported_build`; không có network/update fallback. **AC:** Chỉ đúng fingerprint mới đi qua code path capability; trạng thái vẫn `UNVERIFIED_DEVICE` tới user test, mismatch không gọi action method. **Phụ thuộc:** `T-301`–`T-304`. **Verify:** Native build/package; existing host check + offline positive/negative fingerprint fixtures nếu đã có; `git diff --check` và line-count audit. Probe live chuyển sang handoff user.

  <details>
  <summary>Đã thực hiện — T-305</summary>

  Đã thêm `runtime_build_profile.h` pin package/version/ABI/build-id/metadata/base APK của 0.433.0, thay hoàn toàn fingerprint 0.427.0 và wire vào probe. Runtime hiện xác minh package + GNU build-id nhưng cố ý để versionCode/metadata chưa chứng minh là false; mutation chỉ cho phép khi cả bốn true, nên build hiện diagnostic-only với reason rõ. Native arm64/x86_64 build và package đạt; live status `UNVERIFIED_DEVICE`.

  </details>

## Điều kiện hoàn tất

- Binding ledger 0.433.0 đủ nguồn, tuple, type, owner/lifetime và unknown.
- Full-source scan inventory và Virtual GO Plus fallback có verdict/evidence riêng;
  thiếu evidence không chặn direct catch nhưng giữ fallback capability đóng.
- Runtime fingerprint guard fail closed trên build khác.
- Catch/transfer/discard capability chưa được bật nếu evidence tương ứng chưa đạt.

Sau mỗi task, agent phải cập nhật checkbox và khối `<details>` của chính task đó
trước khi chuyển sang task kế tiếp.
