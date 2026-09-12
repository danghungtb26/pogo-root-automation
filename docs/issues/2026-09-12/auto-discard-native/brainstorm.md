# Brainstorm: chuyển auto-discard sang native

## Loại thay đổi

Refactor kèm feature runtime native.

## Câu hỏi định hướng

### Vì sao cần làm bây giờ?

Auto-discard hiện còn phụ thuộc vào Kotlin planner: Kotlin phải nhận inventory snapshot,
tính item vượt giới hạn rồi phát từng `DISCARD_ITEM`. Luồng native đã có inventory reader
và binding `IItemBag.RecycleItem`, nên việc giữ quyết định ở Kotlin làm tính năng không
liên tục, dễ bị bỏ qua khi structured controller không xử lý observation inventory, và
không đồng nhất với auto-transfer.

### Thay đổi gì và giữ lại gì?

- Kotlin giữ `autoDiscard` và `discardLimits` như nguồn cấu hình bền vững, đồng bộ một
  snapshot có revision/session/identity xuống native.
- Native tự đọc inventory theo tick, merge các item id đang được cấu hình với danh sách
  inventory chuẩn, tính số lượng vượt limit và xử lý tuần tự.
- `RecycleItem` chạy trên Unity main thread; Promise được giữ bằng GC handle và poll đến
  kết quả terminal hoặc timeout, tránh gọi lặp khi request trước chưa xong.
- Xóa auto-discard khỏi danh sách Kotlin planners; command discard thủ công vẫn được giữ
  để không phá protocol/API hiện có.
- Inventory telemetry vẫn được phát để chẩn đoán và làm nguồn dữ liệu quan sát.

### Dependency map

`AutomationConfig` → `RuntimeDiscardConfig` → discard config protocol/dispatcher →
native discard module → inventory reader → main-thread `RecycleItem` → Promise observer →
inventory refresh/automation event.

### Driver và lifecycle

Native discard module là driver chính. Observer tick đọc inventory ở cadence bảo trì;
module chỉ tạo một pending discard tại một thời điểm. Khi config tắt, revision/session
đổi hoặc runtime dừng, pending state và Promise handle được reset an toàn.

### Phạm vi triển khai

- Thêm wire codec và module control action `DISCARD_CONFIG_SET`.
- Thêm config mirror/dispatcher ở Kotlin.
- Thêm native config snapshot, candidate selection, main-thread task và Promise state
  machine.
- Bổ sung event/log đủ để xác nhận `inventory read → discard trigger → promise result`.
- Không thay đổi cách người dùng chỉnh giới hạn trong UI.

### Rủi ro và cách giảm thiểu

- Binding `ItemData`/`RecycleItem` chưa verified trên thiết bị: native fail closed, không
  tự gọi nếu binding không đạt guard.
- Promise không trả về hoặc server lỗi: giữ pending/blocked theo session, timeout và
  không spam request.
- Inventory read thiếu item id tùy cấu hình: reader hợp nhất id cấu hình, loại duplicate
  và vẫn giữ curated ids.
- Discard đụng catch/transfer: chỉ trigger khi không có pending transfer/catch operation;
  action Promise được serialize qua main-thread bridge.

## Acceptance criteria (inferred — cần BA xác nhận)

1. Khi `autoDiscard=true`, native đọc inventory định kỳ mà không cần Kotlin planner.
2. Với mỗi item có limit, native gửi `RecycleItem` đúng `count - limit`, không gửi nếu
   đang ở hoặc dưới limit.
3. Một item đang chờ Promise không bị gửi lại ở các tick tiếp theo; sau kết quả thành
   công, lần đọc inventory sau cho phép xử lý candidate kế tiếp.
4. Config thay đổi ở UI được native nhận theo revision và áp dụng trong session hiện tại.
5. Log có các mốc đọc inventory, trigger discard và Promise result; lỗi binding/timeout
   không làm crash hoặc loop vô hạn.
6. Auto-discard Kotlin planner không còn được gọi trong live automation flow.

## Tổng hợp quyết định

Thực hiện theo pattern auto-transfer nhưng dùng inventory làm nguồn trigger. Kotlin chỉ
là durable config bridge; native chịu trách nhiệm đọc, filter, quyết định, invoke và
đọc Promise. Việc giữ manual discard command là tương thích ngược, không phải giữ lại
auto-discard planner.
