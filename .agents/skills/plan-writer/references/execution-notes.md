# Ghi chú ngay dưới từng checklist

Quy tắc này phải xuất hiện trong plan được tạo. Agent code từ plan phải cập nhật chính file phase đang làm, không chỉ báo thay đổi ở câu trả lời cuối.

## Chu trình thực thi

1. Đọc task, AC, dependency, Verify và ghi chú hiện tại; kiểm tra thay đổi có sẵn để không nhận công việc của người khác là của mình.
2. Thực hiện đúng phần việc, kiểm tra diff và chạy Verify phù hợp.
3. **Trước khi chuyển task**, ghi thay đổi thực tế vào khối `<details>` ngay dưới checkbox đó. Đổi summary theo trạng thái và chỉ tick `[x]` nếu Verify đạt.
4. Nếu bị gián đoạn hoặc còn dependency chưa đạt, giữ `[ ]` và ghi phần đã làm, chưa kiểm chứng/lỗi, nguyên nhân và bước tiếp theo. Có thể tiếp tục task độc lập.

Không để một task đã hoàn thành thiếu ghi chú rồi dồn ghi vào cuối phase. Không gộp nhiều task vào một khối details. Không dùng ghi chú chung cuối file thay cho ghi chú của từng task.

## Nội dung bắt buộc khi hoàn thành

- **Tạo file:** path cụ thể và mục đích.
- **Sửa file:** path và nội dung thay đổi.
- **Xóa file:** path và lý do.
- **Di chuyển/đổi tên:** path cũ → path mới và cập nhật tham chiếu liên quan.
- **Thay đổi chức năng:** function/method/param/config/contract đã thêm, sửa hoặc bỏ; hành vi trước/sau và guards liên quan. Ghi signature khi có đổi signature, không dán toàn bộ source hay diff.
- **Kiểm chứng:** lệnh đã chạy hoặc quan sát đã thực hiện, kết quả thực tế (pass/fail, số test nếu có), artifact cần đối chiếu và hạn chế. Không ghi test chưa chạy là pass; không đọc log khi thao tác thành công, theo `AGENTS.md`.
- **Sai khác so với plan:** phát sinh và lý do hoặc `Không`.

Loại thay đổi không phát sinh ghi `Không`. Với task chỉ đọc/review/test, vẫn ghi rõ không có file source thay đổi; phân biệt artifact/ghi chú tạo ra với code. Không coi output build bị ignore là source mới được thêm vào repo.

## Mẫu định dạng

Ví dụ dưới đây là **khung để điền sau khi thực hiện**, không phải bằng chứng task đã chạy. Khi tạo plan mới phải dùng `[ ]` và summary `Chưa thực hiện — T-ID` như `SKILL.md`.

```markdown
- [x] **T-001** **[{layer}]** *({owner})* — {mô tả task}. **AC:** {ID}. **Phụ thuộc:** {ID hoặc Không}. **Verify:** {điều kiện đã đạt}.

  <details>
  <summary>Đã hoàn thành — T-001</summary>

  - **Tạo file:** `{path}` — {mục đích}; hoặc `Không`.
  - **Sửa file:** `{path}` — {thay đổi cụ thể}; hoặc `Không`.
  - **Xóa file:** `{path}` — {lý do}; hoặc `Không`.
  - **Di chuyển/đổi tên:** `{old}` → `{new}` — {tham chiếu đã cập nhật}; hoặc `Không`.
  - **Thay đổi chức năng:** `{Class.method(param: Type): ReturnType}` — {đã thêm/sửa/bỏ gì, hành vi và guards}; hoặc `Không`.
  - **Kiểm chứng:** `{lệnh thực tế}` → {kết quả thực tế}; {artifact/giới hạn nếu có}.
  - **Sai khác so với plan:** {nội dung và lý do}; hoặc `Không`.

  </details>
```

Nếu Verify fail/chưa chạy, dùng summary `Chưa hoàn tất — T-ID` hoặc `Bị chặn — T-ID`, giữ `[ ]`, bổ sung **Còn lại** và **Nguyên nhân**. Nếu một task chỉ yêu cầu chứng minh test tái hiện bug, kết quả fail đúng nguyên nhân có thể đạt Verify của task đó; không coi nó là bằng chứng implementation đã đúng.

Nếu kết quả cũ không còn đúng sau thay đổi tiếp theo, mở lại checkbox và bổ sung ghi chú mới, giữ lịch sử cũ có đánh dấu đã bị thay thế. Không xóa ghi chú của người khác hay lặng lẽ giảm Verify để tick task.
