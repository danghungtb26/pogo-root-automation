# Template — Refactor hoặc chuyển contract

Dùng khi chia file, thay cấu trúc module, đổi interface/protocol hoặc di chuyển callsite. Task/details theo `../../SKILL.md`; không mặc định cần database migration.

## Inventory và invariants

- Tìm toàn bộ consumer/caller/include/import của symbol/path đổi bằng `rg`.
- Ghi hành vi/contract phải giữ, phần chủ ý thay đổi và yêu cầu tương thích nếu có.
- Với game binding, dẫn nguồn reverse; không thay binding bằng tên/offset suy đoán trong quá trình di chuyển.

## Các bước chuyển đổi

- Chia trách nhiệm/file thành phần theo boundary domain/adapter/native; source không phải Markdown tối đa 500 dòng.
- Di chuyển declaration/implementation rồi cập nhật import/include/callsite và build config liên quan theo thứ tự có thể kiểm tra.
- Nếu wire format đổi, nêu phiên bản, consumer và fail-closed khi không tương thích; không giữ fallback vượt identity/capability guards.
- Chỉ xóa file/symbol cũ khi consumer đã chuyển và xác minh tham chiếu; không xóa thay đổi không thuộc nhiệm vụ.

## Kiểm chứng

- Dùng tests hiện có để xác minh invariants; bổ sung test khi có khoảng trống hành vi đáng kể, không ép test mới cho rename đơn giản.
- Focused checks, full Gradle, native checks/build nếu C++ bị ảnh hưởng và `git diff --check`.
- Khi di chuyển file, details từng task ghi path cũ → mới, tham chiếu đã cập nhật, file đã xóa và kết quả kiểm tra.
