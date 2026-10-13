# Research — migration artifact và reverse workflow sang REA

Ngày: 2026-10-13

Repo: `pogo-root-automation`

## Mục tiêu

Loại bỏ dependency vào `scripts/reverse-pogo-apk.sh`/Il2CppDumper local, mang
artifact đã phân tích và báo cáo cũ vào repo, rồi dùng REA Evidence làm nguồn
canonical cho discovery 0.433.0.

## Artifact đã đưa vào repo

Binary lớn nằm dưới `reverse/artifacts/`, read-only và bị Git ignore. Identity
được commit trong `reverse/artifacts/SHA256SUMS`.

| Artifact | SHA-256 | Ghi chú |
|---|---|---|
| Pokémon GO 0.433.0 APKM | `1b120bb485310777f196fdb8cb827b63887e91f83fe81dc0cdb7e67a2d518278` | Container nguồn |
| `base.apk` | `1c8f198cd212e7303e5a5e38e34857f4ced42eb432ac62b07807bfd286503690` | REA trích từ APKM |
| arm64 split APK | `8f4a472ca8fd59cc794f4f0b32fff3920c55211bbab955ec8b79f7aa4fe5f0a0` | REA trích từ APKM |
| `libil2cpp.so` | `6b1e11619362ddd53f9dbcd7be1d4296bdec5dda27b9f742c16b8403359c25a0` | AArch64; BuildID `da59ce3024c4ccfe16fa6616281a4d111de67aa2` |
| `global-metadata.dat` | `f5961a8548256da287b4f79466a169c302b90ec59be7f6461e8e58b482c0fb0d` | REA trích từ base APK |
| PGSRoot 1.0.15 APK | `2e0f328a15094ebc0f2f91cfac72fb94024abdce95ac48b229b4be7b3dce7297` | Comparison artifact |
| libhela 1.273.1 ELF | `893b84c077942b988e3419eeac54d973a35f9dd90a65e79b33bd894f081a6ef2` | BuildID `7369543908bf1c3e216306f5f3764fabebdcee78` |
| libhela embedded UI APK | `7bdb545aae4d3bb870da6c8afab8150979e27b5e1736f51e33d0d9e3239a00df` | Comparison artifact |
| libhela `.dt` | `9fc15ebd95f454a2428bb613873de8acef68d71344cbfaf8cda50fcd88708f39` | Opaque comparison data |
| PGTools 6.0.6 APK | `b30b5c45a6425bdc74a13254dac5e22e8e58f827d098562b12fa1cd41660f78f` | Comparison artifact |

Bốn báo cáo tiếng Việt trước đó được sao chép nguyên trạng vào
`reverse/reports/`. Chúng là historical analyst reference, không phải observed
Evidence.

## REA evidence đã tạo

Bundle canonical: `reverse/rea/rea-evidence-2026-10-13.json`.

- 17 Evidence record, 2 residual unknown.
- SHA-256 bundle:
  `c04debdc7da20fecc727fa0b23f4f8928a41dada25d020cdb36ce5fe142330f4`.
- APKM inventory: `ev_bf551a9b48a19cbee50d4e93ceb72f11ca28b3056da53d00499f20da266d4357`.
- APKM extraction: `ev_022695538201a1d4233b7d5813d22103d622d633ad10051eed198b4ead340bf8`.
- arm64 split inventory/extraction: `ev_011ef4b59ff7f527f18b1645d286f01f3b2dae58c7fb23ba4cede78364cec805`,
  `ev_9c80a3ad49968607e7aed4b2b984e865e07f06ef3a8f0efc0fd481f74fcfa1c4`.
- base APK inventory/extraction: `ev_42ee22de57c2c42ba683030e8c8fd10e81b956da10c93f123ef1fafb7f17bc0c`,
  `ev_d5331c763c7416b306953a92edb4396a701d55e8948cd61746445c9516929cfe`.
- PGSRoot inventory: `ev_0248662389cf472ffa1f5c7a1d0a0cad9b3b8a48b16952e74fa6b23580854f08`.
- libhela embedded UI inventory: `ev_a521a2041426c310e84315583bd3857e0cec92f3bb2d5af1ed3ce650946e91ac`.
- PGTools inventory: `ev_dc2cdfaefe0ff0e3fd96b0c81afc1952ccb2942fae12f36dd5fd04fa265dd330`.

REA xác nhận artifact identity, containment và extraction provenance. Các kết
quả này không tự chứng minh semantics của class/method hoặc runtime call.

## Residual unknown và giới hạn hiện tại

1. `unk_1aaa6e2b3989ed4a5950c08b9758be4a174e525ea7d9358e3738748b7d70f5b4`:
   Android class/method analysis chưa chạy vì môi trường chưa có caller-supplied
   `jadx-headless-mcp` 0.7.1 qua `REA_JADX_MCP_JAR`.
2. `unk_27690aa58af5fcf5f26981acb0bbbc7636021a154a7cb88a664b9f48d7826028`:
   native deep analysis chưa chạy vì Ghidra 12.1.x/Hopper/IDA provider chưa
   khả dụng. REA đã nhận diện libhela là ELF arm64 nhưng chưa tạo call graph hay
   function dossier.

Vì vậy các claim về scan source, `TryCapture`, GO Plus, transfer và discard trong
báo cáo cũ vẫn là hypothesis cho tới khi có Android/native REA Evidence. Không
được dùng absence trong bundle hiện tại để kết luận class/method không tồn tại.

## Workflow đích

1. Verify `reverse/artifacts/SHA256SUMS`.
2. Import bundle canonical và query retained Evidence.
3. Dùng `inspect_android_package`, `search_android_classes`,
   `inspect_android_class`, `inspect_android_method`,
   `trace_android_references` cho APK/DEX khi JADX provider sẵn sàng.
4. Dùng `open_binary`, search/xref và `analyze_function` cho ELF. Chỉ dùng
   `build_call_path` với function dossier cùng artifact/provider.
5. Ghi Evidence ID, hash, authority, coverage, limitation và residual unknown
   vào binding ledger.
6. Export lại bundle deterministic; không tạo dump/script reverse song song.

Live/device chỉ dùng sau static pass để xác minh owner instance, lifetime,
thread và postcondition. Theo scope hiện tại, agent không chạy E2E/BlueStacks;
những phần đó giữ `UNVERIFIED_DEVICE` và được bàn giao cho người dùng.
