# Câu hỏi phân tích tính năng Pokémon GO

Dùng cùng quy trình trong `../SKILL.md`. Trả lời ở bước tương ứng và lưu kết quả trước khi chuyển bước; không đợi trả lời hết bộ câu hỏi mới ghi tài liệu.

## Bước 1 — Yêu cầu

- Người dùng muốn hành vi gì? Trigger, dữ liệu đầu vào và kết quả quan sát được là gì?
- Luồng hiện tại khác hành vi mong muốn ở đâu? Phần nào nằm trong phạm vi lần này?

## Bước 2 — Bằng chứng game

- Bộ reverse nào khớp phiên bản/build/ABI đang phân tích?
- Class/interface/model/enum nào tham gia? Đã đọc extract nào trong `reverse/`?
- Extract đã đủ chưa? Nếu chưa, phần nào trong dump nén cần tra thêm?
- Bằng chứng là implementation, metadata/signature hay chỉ là suy luận?

## Bước 3 — Function/method và param

- Entry point và method trực tiếp liên quan có signature chính xác là gì?
- Mỗi param có tên, kiểu, ý nghĩa, nguồn giá trị, đơn vị và ràng buộc nào? Phần nào chưa xác minh?
- Method trả về gì, báo hoàn tất qua đâu và thay đổi state nào?
- Điều kiện gọi, owner, lifetime, thread và nhánh lỗi được chứng minh bởi nguồn nào?

## Bước 4 — Luồng hành vi

- Trigger đi qua những method nào, truyền param gì và kết thúc tại postcondition nào?
- Quan hệ caller/callee nào đã xác nhận, quan hệ nào mới là giả thuyết?
- Mất kết nối, stale data, object bị hủy, đổi scene, timeout hoặc thao tác trùng ảnh hưởng ra sao?
- Dữ liệu/command đi qua adapter, native binding, bridge, domain và app như thế nào? Chỉ đọc source framework sau lượt reverse khi cần trả lời phần tích hợp.

## Bước 5 — Hướng xử lý

- Có thể tận dụng method/luồng hiện có không? Binding hay contract nào còn thiếu?
- Method nào dùng lại, API nào là đề xuất mới, param lấy từ đâu, hành vi dự kiến là gì?
- Vì sao chọn phương án này? Có phương án khác đáng cân nhắc không?
- Guards nào phải đạt trước khi bật capability? Điều kiện nào buộc fail-closed?

## Bước 6 — Kiểm chứng

- Tiêu chí Given/When/Then nào chứng minh yêu cầu đã đạt?
- Điều gì kiểm tra được bằng test domain/protocol/adapter, điều gì cần runtime evidence?
- Cần xác minh thêm gì để chuyển từ đề xuất sang triển khai? Không ghi kiểm tra chưa chạy là đã thành công.
