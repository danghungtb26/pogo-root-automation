---
name: plan-writer
description: "Chuyển brainstorm hoặc yêu cầu cụ thể trong pogo-root-automation thành kế hoạch triển khai có phase, dependency, checklist và cách kiểm chứng. Dùng khi người dùng yêu cầu viết plan, kế hoạch triển khai, chia task hoặc checklist thực hiện. Plan quy định agent ghi thay đổi thực tế bằng details/summary ngay dưới từng task trước khi chuyển sang task tiếp theo."
---

# Plan Writer

Viết kế hoạch mà agent có thể thực hiện theo thứ tự, biết sửa ở đâu, hành vi nào phải đạt và cần kiểm chứng gì. Kết nối bằng chứng Pokémon GO trong reverse với contract, logic automation và controller Android của dự án.

## Phạm vi và ngôn ngữ

- Viết tiếng Việt mặc định; giữ nguyên identifier, signature, path, CLI command và ID yêu cầu.
- Lập plan không tự cấp quyền triển khai hay chạy thao tác trên thiết bị. Nếu người dùng đã yêu cầu triển khai, tiếp tục trong phạm vi đó sau khi lưu plan; không hỏi lại chỉ vì đã xong bước lập plan.
- Đọc `AGENTS.md` hiện hành. Không áp dụng cấu trúc thư mục, công cụ test hay quy trình phê duyệt của dự án khác.
- Giữ code phụ thuộc Pokémon GO/Unity/IL2CPP theo phiên bản trong adapter/native binding; `core/` chứa logic domain độc lập.

## 1. Xác định nguồn, phạm vi và issue

- Đọc brainstorm cùng chủ đề, đặc biệt phần method/param/hành vi, hướng xử lý, tiêu chí chấp nhận và câu hỏi còn mở. Đọc tài liệu liên quan trong `docs/` khi cần.
- Yêu cầu/correction mới nhất của người dùng được ưu tiên nếu khác tài liệu cũ; ghi quyết định thay thế và nguồn.
- Tái sử dụng `docs/issues/{YYYY-MM-DD}/{issue-title}/` của brainstorm hoặc issue đã có; giữ ngày và slug ban đầu. Nếu chưa có, tạo issue theo ngày hiện tại và slug ngắn có nghĩa.
- Không chặn toàn bộ plan vì mọi câu hỏi chưa có đáp án. Chuyển thiếu bằng chứng kỹ thuật thành task discovery có đầu ra cụ thể; ghi dependency để chặn đúng phần triển khai phụ thuộc. Với lựa chọn sản phẩm không thể suy ra, nêu câu hỏi và tiếp tục lập phần độc lập.
- Gắn requirement với ID có sẵn; nếu chưa có, đặt `AC-01`, `AC-02`... kèm nguồn và ghi rõ tiêu chí nào là đề xuất.

## 2. Đối chiếu source và chọn template

- Khi cần xác định class, function/method, param, field, RVA hoặc hành vi game, đọc `reverse/pogo-0.427.0/classes/` trước theo `AGENTS.md`; dùng extract trước dump nén. Đối chiếu version/build/ABI, không suy ra implementation từ signature hoặc body rỗng.
- Tận dụng bằng chứng brainstorm đã có, mở lại đoạn reverse liên quan khi cần kiểm tra trước khi chốt task. Không dùng framework làm bằng chứng thay thế implementation game; không bắt đầu bằng dò live process.
- Đọc source framework cần thiết để xác nhận path, caller/callee, contract và test đang có. Dùng `rg`/`rg --files`; phân biệt file hiện có với file mới dự kiến tạo.
- Task dùng game method phải dẫn tới nguồn reverse hoặc mục brainstorm có nguồn, ghi signature, param và hành vi liên quan. Symbol chưa xác minh trở thành dependency discovery, không trở thành method/RVA được đoán sẵn trong task code.
- Đọc [phân công](references/agents.md), [quy tắc thực thi và ghi chú](references/execution-notes.md), [kiểm chứng](references/verification.md), rồi chỉ đọc template phù hợp:

| Scope | Dùng khi | Template |
|---|---|---|
| `feature` | Tính năng xuyên nhiều module | [feature](references/templates/feature.md) |
| `bridge-contract` | Capability, frame, event, payload, codec hoặc adapter contract | [bridge-contract](references/templates/bridge-contract.md) |
| `native-binding` | Binding theo build, owner/lifecycle, native hook hoặc game action | [native-binding](references/templates/native-binding.md) |
| `android-controller` | Headless service, engine, overlay, UI hoặc mock location | [android-controller](references/templates/android-controller.md) |
| `refactor` | Đổi cấu trúc, chia file hoặc chuyển contract/callsite | [refactor](references/templates/refactor.md) |

Plan nhỏ chỉ giữ phase liên quan. Không tạo task ở mọi module nếu yêu cầu không cần.

## 3. Viết task cụ thể và có dependency

Mỗi checkbox là một thay đổi hoặc một kết quả có thể kiểm chứng:

```markdown
- [ ] **T-001** **[{layer}]** *({owner})* — {việc cần làm, path và function/method liên quan}. **AC:** {ID}. **Phụ thuộc:** {task ID hoặc Không}. **Verify:** {lệnh/quan sát và kết quả mong đợi}.

  <details>
  <summary>Chưa thực hiện — T-001</summary>

  Chưa có thay đổi hoặc kết quả kiểm chứng. Agent cập nhật khối này ngay sau khi xử lý task, trước khi sang task tiếp theo.

  </details>
```

- ID task duy nhất và ổn định trong cả plan; dùng ID khi tham chiếu giữa phase.
- Layer là module thực tế, owner là vai trò theo `references/agents.md`. Nhãn vai trò không phải yêu cầu tạo subagent.
- Nêu cụ thể file tạo/sửa/xóa dự kiến, function/method và param cần đổi, hành vi trước/sau, guards và completion semantics khi liên quan. API mới ghi `Đề xuất — chưa tồn tại`.
- **Verify** phải kiểm tra được AC bằng kết quả test hoặc postcondition cụ thể. Với game action bất đồng bộ, gọi method thành công chưa chứng minh action hoàn tất.
- Task discovery phải nêu bằng chứng cần thu thập, nơi ghi kết quả và điều kiện để task phụ thuộc được bắt đầu.
- Chia task theo phần việc có thể xác minh; không tạo checkbox chỉ để mở hoặc tạo một file rỗng. Dự kiến tách file nếu thay đổi có thể vượt 500 dòng ở source không phải Markdown.
- Với bug hoặc logic mới có rủi ro, lập test hồi quy/hành vi phù hợp; dùng test trước implementation khi giúp tái hiện vấn đề. Không ép mọi thay đổi thành red/green/refactor hoặc tạo test chỉ lặp lại implementation. Với tài liệu/đổi tên đơn giản, dùng diff và kiểm tra tham chiếu phù hợp.
- Gắn kiểm tra focused với task/phase liên quan; phase cuối chạy kiểm tra tích hợp bắt buộc theo phạm vi. Không chỉ để một task “test tất cả” mơ hồ ở cuối.

## 4. Sắp phase theo thứ tự thực hiện

Ưu tiên dependency thực tế; khung tham khảo:

1. Bằng chứng và contract: xác minh phần chưa rõ, pin build/ABI, chốt signature/param/AC.
2. Phần triển khai độc lập: domain, contract hoặc fake adapter cần thiết.
3. Adapter/native binding và bridge liên quan, kèm guards và test phù hợp.
4. Tích hợp Android/controller nếu có, kiểm chứng hành vi người dùng.
5. Kiểm tra tổng thể, native build/device verification khi cần, cập nhật tài liệu.

Không buộc mọi tính năng theo khung này. Mỗi phase ghi điều kiện đầu vào và điều kiện hoàn tất; prerequisite phải đứng trước task phụ thuộc. Không bật capability trước khi binding/identity/freshness/lifecycle guards và bằng chứng thiết bị cần thiết đã đạt.

Nếu có UI, ghi màn hình/trạng thái/hành vi cần thay đổi và cách kiểm tra trên Android. Chỉ thêm bản vẽ hoặc bước duyệt thiết kế khi yêu cầu thực tế cần; không tạo gate phê duyệt UI mặc định hay phụ thuộc vào skill chưa có.

## 5. Quy tắc ghi chú bắt buộc cho agent thực thi

Đọc mẫu đầy đủ trong [execution-notes.md](references/execution-notes.md). **Phải đưa quy tắc này vào plan sinh ra**, không chỉ giữ trong skill, để agent nhận riêng file plan vẫn làm đúng:

> Sau mỗi task, kiểm tra diff và kết quả Verify, cập nhật ngay checkbox cùng khối `<details>` nằm dưới nó, rồi mới chuyển task. Chỉ đánh dấu `[x]` khi đạt Verify. Trong `<details><summary>Đã hoàn thành — T-ID</summary>...</details>`, ghi file đã tạo/sửa/xóa/di chuyển, function/method/param hoặc hành vi đã thêm/đổi/bỏ, và lệnh kiểm chứng cùng kết quả thực tế. Mục không phát sinh ghi “Không”. Nếu còn lỗi, chưa kiểm chứng hoặc bị chặn, giữ `[ ]`, ghi trạng thái và phần đã làm/điểm còn thiếu. Không dồn ghi chú tới cuối phase, không đánh dấu hoàn tất dựa trên dự kiến.

- Đặt đoạn quy tắc trên trong `00-overview.md`; mỗi file phase nhắc lại yêu cầu cập nhật ngay dưới từng task và chỉ tick khi Verify đạt.
- Tạo sẵn khối `<details>` trạng thái `Chưa thực hiện` dưới **mọi** checkbox, kể cả task docs, review và verification. Không điền sẵn kết quả thành công khi mới lập plan.
- `<summary>` nằm **bên trong** `<details>`, có thẻ đóng đúng thứ tự. Thụt khối hai dấu cách để gắn vào list item; để dòng trống quanh phần Markdown bên trong để hiển thị đúng.
- Dùng danh sách link thường cho index phase và tiêu chí DoD, tránh checkbox tổng hợp trùng với task thực thi.

## 6. Lưu plan và duy trì trạng thái

Lưu trong issue đã xác định, một file mỗi phase:

```text
docs/issues/{YYYY-MM-DD}/{issue-title}/checklists/
├── 00-overview.md
├── phase-0-{slug}.md
├── phase-1-{slug}.md
└── phase-N-verification.md
```

`00-overview.md` gồm:

- Bối cảnh, nguồn brainstorm/yêu cầu, scope và ngày; build/ABI nếu liên quan.
- Kết quả mong muốn và mapping AC → task ID.
- Phạm vi, phần loại trừ, dependency, câu hỏi còn mở và task giải quyết tương ứng.
- Index liên kết tới các file phase theo thứ tự; rủi ro và cách xử lý.
- Quy tắc thực thi/ghi chú bắt buộc ở mục 5.
- DoD: task trong phạm vi đạt Verify, mỗi task hoàn thành có ghi chú thực tế, focused checks và kiểm tra tổng thể theo phạm vi đạt; không còn dependency bị chặn nhưng được báo là hoàn tất.

Mỗi file phase gồm:

```markdown
# Phase {N} — {tên}

Plan: [Tổng quan](00-overview.md)

Điều kiện bắt đầu: {dependency/bằng chứng cần đạt}.
Điều kiện hoàn tất: {kết quả kiểm chứng của phase}.

Sau mỗi task, cập nhật ngay khối details dưới checkbox với file tạo/sửa/xóa/di chuyển, thay đổi method/param/hành vi và kiểm chứng thực tế trước khi chuyển task. Chỉ tick [x] khi Verify đạt; chưa đạt thì giữ [ ] và ghi phần còn thiếu.

{các task theo mẫu mục 3, mỗi task có một khối details riêng}
```

Khi sửa plan đang thực thi, giữ ID, checkbox và lịch sử ghi chú đã có; chỉ chỉnh phần bị ảnh hưởng. Nếu phát hiện task đã tick không còn đạt Verify, mở lại `[ ]` và ghi lý do, không xóa kết quả trước đó. Task bỏ khỏi phạm vi phải ghi quyết định/nguồn, giữ lịch sử và chuyển thành mục thông tin, không tick như đã thực hiện. Không tạo bản plan mới trùng chủ đề hoặc ghi đè công việc khác của người dùng.

## 7. Tự kiểm tra và trả lời

Trước khi bàn giao, rà soát:

- Task dùng đúng module/path/tooling của repo; tên file mới được ghi rõ là dự kiến tạo.
- Method game có bằng chứng reverse/build/ABI; phần chưa xác minh có discovery dependency.
- Mỗi task có ID, layer, owner, AC phù hợp, dependency và Verify; nếu không ánh xạ AC trực tiếp, nêu điều kiện kỹ thuật mà task hỗ trợ.
- Dependency không vòng; task code không chạy trước guard/bằng chứng cần thiết.
- Mọi checkbox có `<details><summary>` ngay dưới; task mới vẫn `[ ]`, không có ghi chú thành công bịa sẵn.
- Plan có quy tắc ghi chú cho executor, kiểm tra Kotlin/native riêng khi cần, giới hạn 500 dòng và chính sách dùng script thiết bị theo `AGENTS.md`.
- Link overview/phase/source đúng; `git diff --check` sạch đối với thay đổi đã tạo.

Trả lời bằng link `00-overview.md`, số task/phase và tóm tắt thứ tự thực hiện, chỉ rõ dependency chưa giải quyết nếu có. Không chép toàn bộ checklist và không mặc định kết thúc bằng câu hỏi xin phép bắt đầu.
