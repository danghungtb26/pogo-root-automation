---
name: brainstorm
description: "Phân tích tính năng, lỗi và hướng xử lý trong pogo-root-automation trước khi triển khai, đặc biệt các tính năng Pokémon GO cần tra reverse để xác định function/method, tham số và hành vi. Dùng khi người dùng yêu cầu brainstorm, phân tích, suy nghĩ hoặc đánh giá phương án; ghi kết quả từng bước trước khi phân tích bước tiếp theo."
---

# Brainstorm

Phân tích yêu cầu thành hành vi cụ thể, bằng chứng trong source và hướng xử lý có thể kiểm chứng. Với Pokémon GO, lấy thông tin game từ `reverse/`, giữ rõ ranh giới giữa binding theo phiên bản và logic automation ổn định.

## Ngôn ngữ và phạm vi

- Viết nội dung, tiêu đề, cập nhật tiến độ và tài liệu bằng tiếng Việt, trừ khi người dùng yêu cầu ngôn ngữ khác.
- Giữ nguyên code, đường dẫn, identifier, signature, tên tham số và lệnh.
- Yêu cầu phân tích chỉ bao gồm đọc, phân tích và ghi tài liệu. Nếu người dùng đã yêu cầu cả triển khai, hoàn thành phần phân tích rồi tiếp tục trong phạm vi đã được giao.

## Quy tắc bắt buộc: mỗi lượt phân tích mới tạo file mới

- Luôn tạo file mới cho mỗi yêu cầu phân tích mới, kể cả phân tích tiếp hoặc phân tích lại cùng chủ đề. Chỉ sửa hoặc bổ sung vào file cũ khi người dùng yêu cầu rõ ràng làm việc đó.
- File phân tích cũ chỉ là tài liệu tham khảo: có thể đọc và dẫn link trong file mới, không tự ghi đè, nối thêm hay cập nhật kết luận trong file cũ.
- Các bước của cùng một lượt phân tích ghi dần vào file đã chọn ở bước 1; không tạo file riêng cho từng bước.

## Quy tắc bắt buộc: ghi từng bước rồi mới phân tích tiếp

Thực hiện tuần tự từng bước trong quy trình bên dưới:

1. Chỉ đọc và phân tích những gì cần cho bước hiện tại.
2. Ghi ngay kết quả bước đó vào file phân tích đã chọn ở bước 1: phát hiện, bằng chứng, kết luận ngắn và điểm chưa rõ.
3. Gửi cập nhật ngắn cho người dùng về kết quả vừa ghi và mục đích bước tiếp theo.
4. Sau khi lưu và thông báo xong mới bắt đầu đọc hoặc phân tích bước tiếp theo.

Không phân tích hết rồi mới xuất toàn bộ các bước một lần. Không gộp các bước phụ thuộc nhau vào cùng một lượt đọc công cụ. Trong cùng một bước có thể gom các lượt tìm kiếm độc lập. Nếu bước dài, lưu từng phần có kết quả hữu ích và báo tiến độ trước khi tiếp tục.

Output là kết quả phân tích có dẫn chứng và lý do ngắn gọn, không phải bản ghi suy nghĩ nội bộ. Tự chuyển bước, không yêu cầu người dùng duyệt từng bước. Khi thiếu dữ liệu, ghi rõ giới hạn và tiếp tục phần độc lập; không biến giả định thành kết luận.

## Quy tắc đọc source Pokémon GO

- Khi cần đọc source để xác định class, function/method, param, field, RVA hoặc hành vi của Pokémon GO, **bắt buộc đọc trong `reverse/` trước**.
- Theo `AGENTS.md`, bộ reverse hiện tại nằm ở `reverse/pogo-0.427.0/classes/`, ứng với Pokémon GO `0.427.0`, version code `2026082702`, `arm64-v8a`. Đối chiếu metadata thực tế; không mặc định kết quả đúng cho phiên bản hay ABI khác.
- Tìm bằng `rg` trong các class extract đã tuyển chọn trước. Chỉ tra dump đầy đủ dạng nén trong cùng thư mục khi extract chưa đủ. Không giải nén toàn bộ thành file text vượt giới hạn 500 dòng của dự án.
- Không lấy code trong `app/`, `core/`, `game-adapter/` hoặc `zygisk/` làm bằng chứng thay thế cho implementation của game. Chỉ đọc các phần đó khi cần đối chiếu điểm tích hợp, sau lượt tra reverse; ghi rõ đâu là game, đâu là framework.
- Nếu reverse thiếu hoặc không khớp build, ghi chính xác dữ liệu thiếu và phần kết luận bị chặn. Khi cần tái tạo, dùng entry point `./scripts/reverse-pogo-apk.sh` theo `AGENTS.md`; không sửa APK đầu vào.
- Không bắt đầu bằng dò tên trên live process. Sau lượt reverse, chỉ dùng runtime để xác minh instance, owner, lifetime, ABI/layout và postcondition khi cần. Khảo sát mặc định chỉ đọc và dùng script hiện có theo `AGENTS.md`.
- Dump chỉ có signature, metadata hoặc method body rỗng **không chứng minh implementation hay call graph**. Phân biệt `Đã xác nhận`, `Suy luận` và `Chưa xác minh` cho từng nhận định; nêu bằng chứng cần bổ sung. Không suy ra chắc chắn hành vi chỉ từ tên method.

## Quy trình

### Bước 1 — Chốt yêu cầu và tạo tài liệu

- Ghi tính năng/lỗi cần phân tích, hành vi hiện tại và mong muốn, trigger, phạm vi và điều kiện hoàn tất ban đầu.
- Xác định loại: `feature`, `bug`, `refactor`, `architecture`, `performance`, `security`, `ux` hoặc `design-api`.
- Đọc template khớp loại trong bảng tham chiếu bên dưới. Dùng câu hỏi làm checklist cho từng bước tương ứng; chưa phân tích nội dung của bước sau. Câu không áp dụng ghi lý do ngắn.
- Tìm issue cùng chủ đề trong `docs/issues/*/` để tham khảo. Mặc định tạo file mới tại `docs/issues/{YYYY-MM-DD}/{issue-title}/brainstorm.md`; ngày là ngày bắt đầu lượt phân tích mới, slug ngắn và ổn định. Nếu file đã tồn tại, chọn tên chưa tồn tại theo thứ tự `brainstorm-02.md`, `brainstorm-03.md`... trong cùng thư mục, không ghi đè file có sẵn.
- Nếu người dùng yêu cầu rõ ràng sửa hoặc bổ sung vào file cũ, dùng đúng file được yêu cầu theo mục “Phân tích tiếp cùng chủ đề” bên dưới.
- Ghi tiêu đề, loại, ngày, yêu cầu và kết quả bước 1 ngay. Các bước chưa làm chỉ ghi `Chưa phân tích`, không điền sẵn kết luận.

**Output bước 1:** phạm vi, yêu cầu đã biết, giả định và câu hỏi còn thiếu. Lưu file và thông báo trước bước 2.

### Bước 2 — Thu thập bằng chứng từ reverse

- Xác định package/version/version code/ABI/build mà bằng chứng áp dụng; giá trị thiếu ghi `Chưa xác minh`.
- Tra các class, interface, model, enum và method liên quan trong reverse theo quy tắc trên.
- Ghi đường dẫn và số dòng cho từng bằng chứng. Với dump nén, ghi đường dẫn archive, symbol và số dòng của nội dung giải nén được trích đọc.
- Lập danh sách symbol liên quan cùng vai trò sơ bộ, nguồn và giới hạn dữ liệu. Chưa thấy symbol thì ghi từ khóa/phạm vi đã tìm; không tự đặt tên game method cho đủ output.

**Output bước 2:** bảng nguồn reverse, symbol tìm được, phạm vi phiên bản và dữ liệu còn thiếu. Lưu và thông báo trước bước 3.

### Bước 3 — Mô tả function/method và param

Với mỗi function/method trực tiếp liên quan đến tính năng hoặc hướng xử lý, ghi đầy đủ:

- Namespace/class/owner, tên chính xác, overload, static hay instance và signature nguyên văn từ source nếu có.
- Tất cả param theo đúng thứ tự: tên, kiểu, modifier như `ref`/`out`, ý nghĩa, nguồn giá trị, đơn vị, giá trị mặc định và ràng buộc/nullability nếu xác định được. Không có param thì ghi rõ; chi tiết chưa biết ghi `Chưa xác minh`.
- Kiểu trả về và ý nghĩa kết quả; phân biệt trả về trực tiếp với callback/task/event báo hoàn tất.
- Hành vi: điều kiện trước khi gọi, dữ liệu đọc/ghi, thay đổi state, side effect, nhánh lỗi và điều kiện hoàn tất. Mỗi mô tả phải chỉ rõ điều nào đã xác nhận, suy luận hoặc chưa xác minh.
- Caller/callee, dependency owner, thread và object lifetime nếu có bằng chứng; không bịa quan hệ gọi từ danh sách signature.
- RVA/offset khi nguồn cung cấp và cần cho binding; luôn gắn với build/ABI. Không trộn param C# với receiver hoặc param ngầm của native ABI chưa được xác minh.
- Nguồn `path:line` hỗ trợ signature và nguồn hỗ trợ hành vi; hai loại bằng chứng có thể khác nhau.

Dùng mẫu ngắn cho từng symbol:

```markdown
### {Namespace.Class.Method} — {trạng thái bằng chứng}
- Nguồn: {path:line}; build/ABI: {giá trị hoặc Chưa xác minh}
- Signature: `{signature chính xác hoặc Chưa tìm thấy}`
- Vai trò và hành vi: {giải thích có dẫn chứng}

| Param | Kiểu / modifier | Ý nghĩa / nguồn giá trị | Đơn vị / ràng buộc | Bằng chứng |
|---|---|---|---|---|
| ... | ... | ... | ... | ... |

- Kết quả / tín hiệu hoàn tất: ...
- Điều kiện gọi / state / side effect / lỗi: ...
- Caller/callee / owner / thread / lifetime: ...
- Điểm chưa xác minh và cách xác minh: ...
```

**Output bước 3:** danh mục function/method và param kèm giải thích hành vi. Nếu không tìm thấy, vẫn giữ mục này và ghi thiếu gì, ảnh hưởng gì. Lưu và thông báo trước bước 4.

### Bước 4 — Ghép luồng hành vi và điểm tích hợp

- Mô tả theo thứ tự: trigger → kiểm tra điều kiện → function/method với nguồn param → thay đổi state → callback/response/event → postcondition quan sát được.
- Dùng tên method chính xác từ bước 3. Đánh dấu cạnh gọi nào chưa được xác minh; không trình bày luồng suy luận như call graph đã biết.
- Nêu các nhánh liên quan: thiếu dữ liệu, object hết lifetime, dữ liệu cũ, timeout, lỗi game/server, thao tác trùng hoặc cạnh tranh.
- Khi cần, đọc source framework để đối chiếu luồng qua adapter/native binding, bridge, domain và app. Dẫn nguồn cho method framework được sử dụng; bổ sung signature/param/hành vi theo mẫu bước 3 và ghi rõ nguồn framework.

**Output bước 4:** luồng hành vi có bằng chứng, dependency và điểm tích hợp/rủi ro. Lưu và thông báo trước bước 5.

### Bước 5 — Đề xuất hướng xử lý

- Đề xuất cách xử lý phù hợp với bằng chứng: tận dụng luồng có sẵn, bổ sung binding/contract, hoặc xác minh thêm trước khi triển khai. So sánh phương án khác khi có lựa chọn thực sự.
- Với mỗi phần việc, chỉ rõ module, function/method liên quan, param lấy từ đâu, hành vi mong muốn và lý do chọn.
- Tách rõ symbol hiện có với API mới đề xuất. API mới phải có signature dự kiến và được ghi `Đề xuất — chưa tồn tại`; không gắn nó với source/RVA của game.
- Nêu guards còn cần và tiêu chí dừng: capability, runtime identity, freshness, lifetime và fail-closed. Để phần phụ thuộc build trong adapter/native binding, giữ domain ổn định.
- Với map-tap walk, tuân thủ giới hạn `READ_MAP_TARGET` trong `AGENTS.md`; không đề xuất suy tọa độ từ screenshot hay fallback `input tap`/`input swipe`.
- Không gọi thử method làm thay đổi trạng thái game chỉ để chứng minh giả thuyết khi chưa có binding/guards hợp lệ và phạm vi cho phép.

**Output bước 5:** hướng xử lý cụ thể, lý do, phần có thể làm ngay và phần cần xác minh. Lưu và thông báo trước bước 6.

### Bước 6 — Tiêu chí chấp nhận và xác minh

- Luôn có mục `Tiêu chí chấp nhận`. Lấy yêu cầu người dùng, tài liệu tính năng liên quan trong `docs/` và ràng buộc dự án làm nguồn; không tìm spec của dự án khác.
- Giữ ID yêu cầu có sẵn hoặc dùng `AC-01`, `AC-02`... Nếu phải suy ra, ghi `Đề xuất từ yêu cầu`; không tự tạo yêu cầu phê duyệt BA.
- Mỗi tiêu chí nêu điều kiện đầu vào, hành vi mong muốn/postcondition, function/method liên quan và cách kiểm chứng.
- Phân biệt kiểm tra logic/protocol bằng test với xác minh binding/lifecycle trên thiết bị. Đề xuất lệnh/script có sẵn theo `AGENTS.md`; không khẳng định đã chạy khi mới lập kế hoạch kiểm tra.
- Chốt phát hiện chính, hướng xử lý được đề xuất, rủi ro và câu hỏi còn mở. Nếu thiếu reverse/runtime evidence, kết luận phải giữ giới hạn đó.

**Output bước 6:** bảng tiêu chí, kế hoạch xác minh và kết luận. Lưu xong mới trả lời cuối.

## Cấu trúc file kết quả

```markdown
# Brainstorm: {tên vấn đề}

**Loại:** {type}
**Ngày phân tích đầu tiên:** {YYYY-MM-DD}
**Phạm vi game/build/ABI:** {giá trị hoặc Chưa xác minh}

## Bước 1 — Yêu cầu và phạm vi
## Bước 2 — Bằng chứng từ reverse
## Bước 3 — Function/method, param và hành vi
## Bước 4 — Luồng hành vi và điểm tích hợp
## Bước 5 — Hướng xử lý
## Bước 6 — Tiêu chí chấp nhận và xác minh

| ID | Nguồn yêu cầu | Điều kiện đầu vào | Hành vi / postcondition | Method liên quan | Cách kiểm chứng |
|---|---|---|---|---|---|

### Kết luận và câu hỏi còn mở
```

Mỗi bước phải có kết quả, dẫn nguồn và điểm chưa rõ; mẫu là khung để điền dần theo tiến độ. Trả lời cuối bằng link tới file và tóm tắt ngắn phát hiện chính, hướng xử lý, giới hạn quan trọng. Không chép lại toàn bộ tài liệu.

## Phân tích tiếp cùng chủ đề

Mặc định vẫn tạo file mới theo bước 1 và dẫn link tới phân tích trước nếu có liên quan. Yêu cầu “phân tích tiếp” hoặc nhắc tới file cũ không tự cho phép sửa file đó. Ghi câu hỏi đã giải quyết và kết luận thay thế trong file mới, kèm nguồn mới và tham chiếu tới kết luận cũ.

Chỉ khi người dùng yêu cầu rõ ràng sửa hoặc bổ sung vào file cũ mới dùng lại file đó. Giữ nội dung người dùng đã viết ngoài phạm vi chỉnh sửa; thêm mục tiếp nối có số thứ tự hoặc sửa phần được yêu cầu, vẫn lưu/báo kết quả từng bước. Trong phạm vi được yêu cầu, cập nhật câu hỏi đã giải quyết và đánh dấu kết luận cũ bị thay thế kèm nguồn mới; không để hai kết luận mâu thuẫn cùng mang trạng thái đã xác nhận.

## Template bổ trợ

Chỉ đọc template khớp loại yêu cầu; quy trình, nguồn reverse và quy tắc output ở trên áp dụng cho tất cả template. Template là bộ câu hỏi kiểm tra, không thay thế trình tự ghi từng bước.

| Loại | Template |
|---|---|
| Tính năng Pokémon GO | [feature](references/feature.md) |
| Lỗi | [bug](references/bug.md) |
| Refactor | [refactor](references/refactor.md) |
| Kiến trúc | [architecture](references/architecture.md) |
| Hiệu năng | [performance](references/performance.md) |
| Bảo mật | [security](references/security.md) |
| Trải nghiệm người dùng | [ux](references/ux.md) |
| Contract adapter/bridge | [design-api](references/design-api.md) |
