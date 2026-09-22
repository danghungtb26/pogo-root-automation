# Template — Binding Pokémon GO và native runtime

Dùng cho method/field binding theo build, native hook, owner resolution, lifecycle hoặc game action. Áp dụng task/details và dependency theo `../../SKILL.md`.

## Bằng chứng trước implementation

- Đọc curated extract trong `reverse/pogo-0.427.0/classes/` trước dump nén; đối chiếu metadata build/ABI theo `AGENTS.md`.
- Ghi class/owner, signature, param, return/callback, field/RVA cần thiết và nguồn; không dựng call graph từ tên method.
- Task runtime discovery chỉ sau reverse, ưu tiên read-only; xác minh instance owner, lifetime, thread, ABI/layout và postcondition còn thiếu bằng script hiện có.

## Binding và guards

- Nêu file native theo cấu trúc thực tế, giữ trách nhiệm gọn và giới hạn 500 dòng.
- Chốt cách lấy instance/param, cách xác nhận object còn sống, thread gọi và cách xử lý session/scene thay đổi.
- Phân biệt dispatched, pending, completed, failed hoặc indeterminate theo contract thực tế; không biến call thành công thành game action thành công.
- Chỉ publish capability khi identity/build/ABI, binding guards và bằng chứng thiết bị cần thiết đạt; fail-closed khi không hợp lệ.
- Với map-tap walk, giữ `READ_MAP_TARGET` tắt khi chưa calibration/device verification; không dùng screenshot suy tọa độ hoặc input tap/swipe fallback.

## Kiểm chứng và tích hợp

- Host C++ tests cho phần logic/protocol test được trên host; native build/package bằng script.
- Kotlin adapter/bridge tests khi contract bị ảnh hưởng và full Gradle trước bàn giao code khi môi trường cho phép.
- Device checks trên BlueStacks Air 1 bằng script hiện có; ghi build/ABI, guards và kết quả postcondition thực tế.
- Cập nhật docs khi mức hỗ trợ capability đổi; ghi details dưới từng task và giữ `[ ]` cho phần runtime còn chưa xác minh.
