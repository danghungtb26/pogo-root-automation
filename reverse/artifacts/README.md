# Artifact catalog

Các binary trong cây này là input read-only để REA phân tích lại và được Git
ignore vì kích thước/nguồn phân phối. Không sửa tại chỗ. Identity canonical nằm
trong `SHA256SUMS`.

| Thư mục | Nội dung | Vai trò |
|---|---|---|
| `pogo-0.433.0/` | APKM gốc, `base.apk`, arm64 split và `native/` do REA trích | Target build |
| `pgsroot-1.0.15/` | APK loader/root đã phân tích | Comparison/reference |
| `libhela-1.273.1/` | core ELF, embedded UI APK, `.dt` đúng PoGo 0.433.0 | Comparison/reference |
| `pgtools-6.0.6/` | APK PGTools đã phân tích | Comparison/reference |

`base.apk` và arm64 split được REA trích từ APKM với extraction Evidence
`ev_022695538201a1d4233b7d5813d22103d622d633ad10051eed198b4ead340bf8`.
`native/libil2cpp.so` được trích từ arm64 split bằng Evidence
`ev_9c80a3ad49968607e7aed4b2b984e865e07f06ef3a8f0efc0fd481f74fcfa1c4`;
`native/global-metadata.dat` được trích từ base APK bằng Evidence
`ev_d5331c763c7416b306953a92edb4396a701d55e8948cd61746445c9516929cfe`.

## Rehydrate trên máy khác

Binary trong thư mục này không được đẩy lên Git. Để tiếp tục Phase 4 trên máy
khác, artifact bắt buộc duy nhất cần tải lại là bundle Pokémon GO 0.433.0:

- Nguồn: <https://www.apkmirror.com/apk/niantic-inc/pokemon-go/pokemon-go-0-433-0-release/pokemon-go-0-433-0-android-apk-download/>
- Chọn đúng variant: `0.433.0`, versionCode `2026100702`, `arm64-v8a`,
  `nodpi`, Android 9.0+, bundle gồm base APK và một split.
- Đặt bundle tải về tại
  `reverse/artifacts/pogo-0.433.0/com.nianticlabs.pokemongo_arm64-v8a_0.433.0.apkm`.

Container APKM do nguồn tải tạo có thể khác byte/metadata với container đã dùng
trong lượt REA ban đầu. Không dùng riêng hash của wrapper để thay thế identity
của APK bên trong. Sau khi REA inventory/extract bundle mới, hai file bắt buộc
phải được đặt đúng đường dẫn và khớp identity sau:

| File dẫn xuất | SHA-256 | MD5 đối chiếu nguồn |
|---|---|---|
| `pogo-0.433.0/base.apk` | `1c8f198cd212e7303e5a5e38e34857f4ced42eb432ac62b07807bfd286503690` | `bfe94c69d52f34889751c659388bf99e` |
| `pogo-0.433.0/split_config.arm64_v8a.apk` | `8f4a472ca8fd59cc794f4f0b32fff3920c55211bbab955ec8b79f7aa4fe5f0a0` | `efc0b500f795477338febfec3658bef6` |

Từ hai APK trên, dùng REA để trích lại, không dùng script reverse cũ:

| File native dẫn xuất | APK chứa file | SHA-256 bắt buộc |
|---|---|---|
| `pogo-0.433.0/native/libil2cpp.so` | `split_config.arm64_v8a.apk` | `6b1e11619362ddd53f9dbcd7be1d4296bdec5dda27b9f742c16b8403359c25a0` |
| `pogo-0.433.0/native/global-metadata.dat` | `base.apk` | `f5961a8548256da287b4f79466a169c302b90ec59be7f6461e8e58b482c0fb0d` |

Kiểm toàn bộ file đã rehydrate:

```bash
shasum -a 256 -c reverse/artifacts/SHA256SUMS
```

Nếu APKM tải mới khác hash dòng APKM trong `SHA256SUMS` nhưng bốn artifact con
ở trên đều khớp, giữ wrapper mới là artifact riêng và tạo REA Evidence mới cho
inventory/extraction của wrapper đó; không sửa Evidence cũ để giả vờ cùng
identity.

### Artifact comparison tùy chọn

Các file dưới đây không cần để chạy Ghidra trên Pokémon GO 0.433.0:

- PGTools 6.0.6: tải từ trang chính thức
  <https://pgtools.net/download/pgtools>, đặt tại
  `reverse/artifacts/pgtools-6.0.6/pgtools-6.0.6-079d8b42e6c6cfaae64033341f861ac6.apk`,
  rồi yêu cầu SHA-256
  `b30b5c45a6425bdc74a13254dac5e22e8e58f827d098562b12fa1cd41660f78f`.
- PGSharp core 1.273.1 cho PoGo 0.433.0: URL đã quan sát là
  <https://dl.pgsharp.com/pgsrt1.273.1_0.433.0.dt>. Đặt file `.dt` tại
  `reverse/artifacts/libhela-1.273.1/pgsrt1.273.1_0.433.0.dt`, SHA-256
  `9fc15ebd95f454a2428bb613873de8acef68d71344cbfaf8cda50fcd88708f39`.
- Nguồn tải chính xác của PGSRoot 1.0.15 và embedded UI APK không được lưu trong
  repo. Nếu không copy được từ máy hiện tại thì bỏ qua; chúng chỉ là comparison
  artifact, không phải dependency của Phase 4.

APK controller của dự án không cần tải. Build lại bằng `./gradlew assembleDebug`;
output nằm tại `app/build/outputs/apk/debug/app-debug.apk`.
