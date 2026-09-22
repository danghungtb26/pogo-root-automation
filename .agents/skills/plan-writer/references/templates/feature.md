# Template — Tính năng automation

Dùng cho tính năng xuyên domain, adapter, bridge/native và controller. Chỉ chọn phần liên quan, xếp lại theo dependency. Mọi task sinh ra phải theo mẫu ID/AC/Verify và khối details trong `../../SKILL.md`.

## Bằng chứng và contract

- Đối chiếu brainstorm: signature, param, hành vi, build/ABI và giới hạn chưa xác minh.
- Khi cần thông tin game, tra reverse trước; đưa owner/lifecycle/completion chưa rõ thành discovery task.
- Chốt input/output, capability, tiêu chí hoàn tất và các nhánh fail-closed.

## Logic ổn định và kiểm thử

- Bổ sung model/rule trong `core/` nếu cần, không kéo offset/game class vào domain.
- Cập nhật `game-adapter/api` và `game-adapter/fake` đồng bộ khi contract thay đổi.
- Dùng test có ý nghĩa cho thành công, lỗi, thiếu capability, stale data hoặc cancel theo hành vi thực tế.

## Runtime và tích hợp

- Cập nhật `bridge/protocol`, `game-adapter/pogo` hoặc native binding theo phần việc cần thiết.
- Nêu từng method, nguồn param, state thay đổi, callback/outcome và guards.
- Tích hợp vào engine/headless service/overlay/mock location trong `app/` nếu thuộc yêu cầu.
- Giữ capability đóng cho tới khi bằng chứng binding/runtime cần thiết đạt.

## Xác minh

- Focused Kotlin/native checks tương ứng, full Gradle khi thay đổi code.
- Native build/package và device verification khi feature phụ thuộc native/game binding.
- Cập nhật tài liệu readiness/contract nếu hành vi hoặc mức hỗ trợ thay đổi; mỗi checklist hoàn thành có ghi chú thực tế ngay bên dưới.
