# Template — Adapter và bridge contract

Dùng cho capability, command/observation, versioned frame, payload và codec. Task sinh ra phải có khối details theo `../../SKILL.md`.

## Khảo sát contract

- Đọc frame/codec/interface và caller/consumer hiện có.
- Với mapping game, xác minh source trong reverse trước; ghi method signature và ý nghĩa từng param/field.
- Chốt dữ liệu, units, validation, version compatibility, correlation, freshness và completion/error semantics cần thay đổi.

## Triển khai theo dependency

- Cập nhật interface/capability trong `game-adapter/api` và fake tương ứng.
- Sửa encoder/decoder Kotlin trong `bridge/protocol` và đối ứng C++ ở path hiện hành dưới `zygisk/` nếu wire format đổi.
- Cập nhật decode/runtime mapping trong `game-adapter/pogo` và consumer app liên quan.
- Ghi cách từ chối unsupported version/payload/identity thay vì nới guard cho tương thích giả.

## Kiểm chứng

- Test contract/codec với dữ liệu đại diện và nhánh lỗi có ý nghĩa; đối chiếu Kotlin/C++ nếu giao thức dùng ở hai phía.
- Focused adapter/protocol tests, host C++ tests liên quan, native build khi C++ thay đổi, full Gradle.
- Device smoke chỉ khi cần và trong phạm vi; không coi fake success là live success.
- Mỗi task ghi file tạo/sửa/xóa, field/method/param thay đổi và kết quả Verify trong details ngay khi xong.
