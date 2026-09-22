# Vai trò và module

Nhãn owner xác định trách nhiệm trong plan, không giả định có agent/plugin cùng tên đã được cài. Mặc định một agent thực hiện tuần tự và có thể đảm nhận nhiều vai trò. Không tự spawn subagent từ bảng này; chỉ phân công song song khi người dùng hoặc hướng dẫn áp dụng cho phép rõ ràng.

| Owner | Trách nhiệm |
|---|---|
| `coder` | Domain Kotlin, contract, adapter, codec, fake, test và tài liệu liên quan |
| `native` | Reverse evidence, binding theo build, bridge C++, lifetime/thread và native checks |
| `android` | Controller, engine, headless service, overlay, UI và mock location |
| `reviewer` | Rà soát kết quả, guards, dependency và bằng chứng Verify; agent hiện tại có thể tự rà soát |

Review chỉ yêu cầu kết quả ghi vào plan/tài liệu trong repo; nhãn `reviewer` không tự cho phép gửi comment ra PR hoặc nhắn người khác.

| Layer | Phạm vi |
|---|---|
| `core` | Model domain, geo, movement, rule automation độc lập game build |
| `bridge/protocol` | Frame/event/payload/codec versioned phía Kotlin |
| `game-adapter/api` | Interface và capability contract |
| `game-adapter/fake` | Adapter xác định cho test, đồng bộ với contract |
| `game-adapter/pogo` | Runtime source, decode protobuf, bridge adapter cho Pokémon GO |
| `app` | Android controller/service/overlay/mock location và test app |
| `zygisk` | Native binding/bridge và host-side C++ test |
| `reverse` | Task đọc/xác minh artifact reverse; không chỉnh generated output như source duy trì |
| `scripts` | Script vận hành hiện có, chỉ sửa khi thuộc phạm vi yêu cầu |
| `docs` | Brainstorm, checklist, kiến trúc và bằng chứng |
| `build` | Gradle/CMake/package/CI khi thay đổi thực sự liên quan |

Gắn test vào layer sở hữu hành vi, nêu đường dẫn test cụ thể. Task phối hợp nhiều module chọn layer chính và liệt kê phần còn lại; tách task nếu có đầu ra/Verify độc lập. Mọi owner đều phải cập nhật ghi chú dưới checklist theo [execution-notes.md](execution-notes.md).
