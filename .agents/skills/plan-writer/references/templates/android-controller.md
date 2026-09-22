# Template — Controller và automation Android

Dùng cho engine, headless service, overlay/UI, runtime lifecycle hoặc mock location. Task/details theo `../../SKILL.md`; không mặc định tạo backend hoặc worker server.

## Luồng và lifecycle

- Xác định trigger, trạng thái engine/service, nguồn observation, dependency và hành vi người dùng.
- Đọc source app/core/adapter liên quan; nếu cần hành vi bên game thì tra reverse trước.
- Nêu start/stop, cancellation, foreground/background, reconnect, missing capability và stale observation theo phạm vi.

## Triển khai

- Đặt rule domain trong `core/`; dùng adapter contract để nối game, không nhúng binding theo build vào app.
- Sửa function/method/param ở path thực tế của controller/engine/service, mô tả state trước/sau và completion.
- Khi có mock-location/movement, dùng luồng controller và planner hiện có; giữ kiểm tra freshness/target hợp lệ.
- Khi có UI, nêu trạng thái loading/empty/error/disabled và feedback cần thiết. Dùng thiết kế/ảnh tham chiếu đã có khi phù hợp, không tạo gate xin duyệt UI mặc định.

## Kiểm chứng

- Test logic/lifecycle có ý nghĩa theo tooling Android/Kotlin hiện có; không tạo test chỉ để kiểm tra wording.
- Focused app/core/adapter tests phù hợp, full Gradle test/build.
- Khi cần thiết bị, dùng script hiện có với BlueStacks Air 1; ghi quan sát và giới hạn thực tế.
- Cập nhật từng checkbox/details ngay khi xong, không dồn vào cuối phase.
