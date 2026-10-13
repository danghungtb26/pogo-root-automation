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
