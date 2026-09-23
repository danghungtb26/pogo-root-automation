# Câu hỏi phân tích contract adapter/bridge Pokémon GO

Dùng khi tính năng cần thiết kế hoặc sửa contract giữa domain, game adapter, bridge và native binding. Tuân thủ thứ tự đọc reverse và ghi output từng bước trong `../SKILL.md`.

## Bước 1 — Phạm vi contract

- Bên gọi và bên xử lý là ai? Cần observation, command hay cả hai?
- Hành vi nào cần hỗ trợ? Đâu là tín hiệu hoàn tất mà bên gọi có thể kiểm chứng?

## Bước 2 — Nguồn game

- Class/method/model nào trong `reverse/` cung cấp dữ liệu hoặc thực hiện hành vi?
- Source áp dụng cho build/ABI nào? Hành vi nào chưa được chứng minh bởi dump?

## Bước 3 — Signature và dữ liệu

- Signature, param, return/callback/event của từng game method là gì?
- Param của contract ánh xạ sang param/model của game như thế nào? Nêu kiểu, ý nghĩa, đơn vị, nullability và nguồn từng giá trị.
- Khi cần đọc contract framework hiện có, làm sau lượt reverse, dẫn nguồn riêng và bổ sung signature/param/hành vi theo mẫu của `SKILL.md`.

## Bước 4 — Luồng và vòng đời

- Capability và runtime identity được kiểm tra ở đâu?
- Request/observation được encode, truyền, decode, xử lý và trả kết quả qua method nào?
- Correlation, freshness, deadline, lỗi, disconnect, cancellation và thao tác trùng được xử lý thế nào nếu có liên quan?
- Ai sở hữu instance/handle? Điều gì xảy ra khi object hoặc session hết lifetime?

## Bước 5 — Contract đề xuất

Với mỗi API mới hoặc thay đổi, ghi:

- Tên và signature; đánh dấu rõ `Hiện có` hay `Đề xuất — chưa tồn tại`.
- Từng param/payload field: kiểu, ý nghĩa, nguồn, ràng buộc và cách ánh xạ sang game.
- Kết quả, completion event và nhánh lỗi; không tự đặt mã lỗi rồi mô tả như đã tồn tại.
- Preconditions, side effect, postconditions, capability/identity/freshness guards và fail-closed.
- Thay đổi protocol version hoặc tương thích ngược nếu cần; giữ chi tiết theo build sau adapter/native boundary.

## Bước 6 — Xác minh

- Tiêu chí nào kiểm chứng mapping param, codec, lỗi và completion semantics?
- Fake adapter/protocol test có thể xác minh gì? Binding/lifecycle cần bằng chứng runtime nào?
- Những phần chưa có nguồn game phải được xác minh thế nào trước khi triển khai?

Ghi kết quả trong file phân tích đã chọn ở bước 1 theo `SKILL.md`, mặc định là file mới; không tách thêm tài liệu chỉ vì đang phân tích API.
