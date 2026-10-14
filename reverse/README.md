# Reverse evidence index

REA là workflow reverse canonical của repo. Script Il2CppDumper cũ đã bị loại
bỏ; không tạo lại script tương đương.

## Nội dung

- `artifacts/`: artifact gốc và artifact do REA trích xuất để phân tích lại.
  Binary lớn được giữ cục bộ và ignore bởi Git; `SHA256SUMS` cùng README được
  commit để kiểm identity.
- `rea/rea-evidence-2026-10-13.json`: bundle Evidence deterministic, hiện có
  35 record, 2 residual-unknown identity và 4 revision record; SHA-256
  `62249426b6119f4f74a4490e2ba2202704de362fda2f6e1dce837a37efaa55bf`.
- `rea/rea-evidence-2026-10-14-startup-connection.json`: bundle kế tiếp cho
  phân tích startup connection so với PGS Root 1.0.15, gồm 48 record và 4
  unknown revision; SHA-256
  `fc3065d35500caaf914a77b70dbaec93026ea9af0986db88cc828a082ca956b2`.
  Bundle bổ sung JADX Evidence cho manager lifecycle, nút launch game, kiểm
  module/core và callback cài module; static evidence không chứng minh runtime.
- `rea/rea-evidence-2026-10-14-libhela-loader.json`: 5 record REA CLI từ
  Ghidra project `libhela-1.273.1/` read-only, gồm string/procedure search,
  xref và dossier `__cxa_guard_enter` tại địa chỉ phân tích `0x29b640`;
  SHA-256 `d761bcdbd18f8ef39aaf5663f92acf3506053675bede483b2917f94759138d43`.
  Snapshot làm việc ở `build/rea/` không thay thế bundle này; địa chỉ Ghidra
  cần đối chiếu image base và file mapping trước khi gọi là RVA.
- `rea/rea-evidence-2026-10-14-libhela-hook-path.json`: 10 record REA CLI
  read-only trên cùng artifact, mở rộng đường tĩnh từ `__cxa_guard_enter` qua
  wrapper `0x914084`, dispatcher `0x914090` và các routine quản lý hook;
  SHA-256 `96935db49d507bd722a93f754ce2048caafaff6c43757ce589daf2e882621cb2`.
  Đây là evidence về libhela, không xác nhận prologue/linker ABI hoặc thời điểm
  hook hoạt động trong runtime của project.
- `rea/rea-evidence-2026-10-14-pogo-il2cpp-init.json`: 2 record REA CLI từ
  Ghidra project PoGo 0.433.0 read-only, xác nhận export `il2cpp_init` tại
  địa chỉ phân tích `0x4fec28c` và dossier function dài 44 byte; SHA-256
  `789184a5a1446e5520a1021971d72254894c460798bc4d602ac7348275628c5e`.
  ELF symbol value/RVA `0x4eec28c` chỉ áp dụng cho artifact ARM64 đã pin;
  dossier không tự chứng minh hook lifecycle hoặc postcondition trên device.
- `rea/pogo-0.433.0-il2cpp-mapping.json`: mapping có provenance từ đúng cặp
  `libil2cpp.so` + `global-metadata.dat` 0.433.0. File giữ managed identity,
  full parameter/return types, token/slot, RVA, file offset và 6 selected type
  contracts cho regex Phase 3–5. Schema v2 có 672 method rows, SHA-256
  `3323117e8a2902996a6e0e60fc9d652df36f746f0e6e7cbc77dd9eff1f0bb59c`;
  đây là static-derived input, chưa tự nó chứng minh exact native entry,
  owner lifetime hoặc hành vi runtime.
- `rea/ghidra-existing-project/`: overlay version-scoped cho
  `rea-agents@6.4.0`; wrapper `scripts/rea-ghidra-project.sh` mở một Program từ
  bản snapshot `.gpr/.rep` bằng `-process -noanalysis -readOnly` và vẫn sinh
  REA Evidence theo exact target SHA-256.
- `reports/`: báo cáo phân tích tiếng Việt đã có. Đây là historical analyst
  reference; claim cần dùng cho binding phải được nâng thành REA Evidence.
- `pogo-0.427.0/`: material legacy, chỉ dùng tạo hypothesis, không dùng chứng
  minh binding 0.433.0.

## Quy trình dùng lại

1. Kiểm `shasum -a 256 -c reverse/artifacts/SHA256SUMS` từ root repo.
2. Import bundle trong `reverse/rea/` vào REA và query retained Evidence trước.
3. Chọn đúng artifact đã pin trong `reverse/artifacts/`.
4. Với APK/DEX, dùng các thao tác Android của REA; với ELF dùng provider native
   và phân tích function/xref có mục tiêu.
5. Ghi Evidence ID, artifact hash, authority, coverage và limitation vào ledger.
6. Export lại bundle deterministic vào `reverse/rea/` khi có evidence mới.

Mapping có thể tái tạo bằng mapper fail-closed (metadata v39, ELF64 AArch64):

```bash
node scripts/il2cpp-metadata-map.mjs \
  --binary reverse/artifacts/pogo-0.433.0/native/libil2cpp.so \
  --metadata reverse/artifacts/pogo-0.433.0/native/global-metadata.dat \
  --unity-version 6000.3.22f1 \
  --code-registration 0xe235508 \
  --metadata-registration 0xe6378c8 \
  --pattern 'MapEntityCell|MapPokemon|WildMapPokemon|IncenseMapPokemon|TroyDiskMapPlacePokemon|DailyEncounterMapPokemon|PhotobombingMapPokemon|IncidentMapPokemon|PokestopSpawnPokemon|RouteMapPokemon|RaidMapPokemon|StationMapPokemon|TryCapture|PokeballThrow|PokemonBagImpl|ReleasePokemon|ItemBagImpl|ItemData|RecycleItem|VirtualGoPlus|ExpiringItemsCopy' \
  --output build/rea/pogo-0.433.0-il2cpp-mapping.json
```

Hai registration address phải đến từ một lần phân tích có provenance trên đúng
cặp artifact; mapper không đoán hoặc scan heuristic địa chỉ này.

Không coi token metadata là RVA, pseudocode là source, tên method là call graph,
hay báo cáo cũ là bằng chứng runtime. Provider thiếu thì giữ unknown/capability
đóng.

## Trạng thái resolver 0.433.0

Runtime dùng hybrid descriptor resolver: exact image/class, instance/static,
full signature, unique match và metadata token để giữ `MethodInfo*`, sau đó gọi
qua `il2cpp_runtime_invoke`. RVA chỉ là profile evidence và hiện mang policy
`unavailable`, vì các dossier Ghidra mới chỉ chứng minh containment trong coarse
`FUN_*`, không phải exact entry/thunk. Snapshot read-only có SHA-256
`81c4c7a1fea5bf846c7a58ff0a8911168e7e7c18a56b9fac3f8671b56bf4eaab`;
focused evidence export có SHA-256
`b2201daf2535c8c222d9274248a08b22059df16fa328f8d7b8baaf04a361390e`.

Static identity và host build không đồng nghĩa `LIVE_READY`. Runtime owner,
Promise/result correlation và authoritative postcondition vẫn
`UNVERIFIED_DEVICE`; catch/transfer/discard mutation tiếp tục fail closed.
