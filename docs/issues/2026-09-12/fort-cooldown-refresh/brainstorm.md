# Brainstorm: Fort không được chọn lại sau khi hết cooldown

**Type:** bug
**Date:** 2026-09-12

---

## Analysis

### 1. Hành vi hiện tại

Native vẫn đọc `MapPokestop.IsCoolingDown()` trong mỗi map snapshot, nhưng sau
khi `FortSearch` trả về `SUCCESS`, coordinator thêm `fort_id` vào
`successful_spin_fort_ids`. Tập này không có thời điểm hết hạn, vì vậy fort bị
loại khỏi bộ lọc vĩnh viễn. Kết quả là sau cooldown, không còn fort hợp lệ để
spin lại dù game có thể đã cho phép spin.

### 2. Hành vi kỳ vọng

Mỗi lần scan phải dùng trạng thái cooldown mới nhất của game. Fort chỉ bị loại
trong thời gian server báo cooldown; sau `CooldownComplete`, fort được phép
được chọn lại nếu vẫn ở trong interaction range và các điều kiện filter khác
đều đạt.

### 3. Thời điểm và phạm vi xảy ra

Lỗi xảy ra trong native catch-spin flow, sau một lần spin thành công và khi
coordinator tiếp tục chạy các observer tick sau đó. Nó ảnh hưởng trực tiếp đến
auto-spin, đặc biệt khi chỉ có một fort trong vùng tương tác. Không phụ thuộc
Kotlin config hay việc bridge có kết nối hay không.

### 4. Có tái hiện được không?

Có thể tái hiện theo chuỗi: bật auto-spin, để native spin thành công một fort,
chờ hết cooldown, sau đó quan sát các scan tiếp theo. Log sẽ tiếp tục không
chọn fort đó vì `successful_spin_fort_ids` vẫn còn chứa id.

### 5. Nguyên nhân gốc

Có hai lớp trạng thái liên quan:

- `runtime_map_forts.inc` đọc `get_IsCoolingDown()` từ object `MapPokestop`.
- `coordinator.inc` áp dụng thêm một set “đã spin thành công” không có TTL.

Ngoài ra, reverse output của build 0.427.0 cho thấy response có
`FortSearchOutProto.CooldownComplete` và `MapPokestop` có
`SetCooldownTime(long timestampMs)`, nhưng native hiện chỉ đọc `Result`; nó
chưa đồng bộ thời điểm cooldown từ response về object map.

### 6. Dependency map

**Upstream:** observer tick → map snapshot trên Unity main thread →
`MapPokestop.get_IsCoolingDown()`; Promise `PoiItemSpinner.ckko()` →
`FortSearchOutProto.Result/CooldownComplete`.

**Downstream:** `select_spin_target()` → `request_main_thread_spin()` →
FortSearch RPC; map snapshot telemetry gửi qua bridge cho controller/Kotlin.

**Shared code:** `RuntimeBinding` chứa các method binding của MapPokestop và
PoiItemSpinner; thay đổi binding cần giữ guard exact build/capability.

### 7. Phạm vi ảnh hưởng

Ảnh hưởng việc spin lặp lại cùng một PokéStop. Catch, transfer, discard và
filter Pokémon không nằm trong phạm vi thay đổi. Cần tránh làm thay đổi giới
hạn interaction range 80 m hoặc điều kiện `spin_available`.

### 8. Vì sao lỗi tồn tại?

Set ban đầu được dùng để tránh gửi lại cùng fort trong lúc map/UI chưa kịp
phản ánh kết quả Promise. Nó được thiết kế như trạng thái chống lặp, nhưng bị
dùng thay cho cooldown state mà không có TTL hoặc cơ chế xóa khi cooldown hết.
Thiếu test state theo thời gian khiến lỗi chỉ lộ ra sau một chu kỳ cooldown.

### 9. Rủi ro của bản sửa

- Nếu timestamp response là epoch milliseconds nhưng bị hiểu nhầm là monotonic
  time, fort sẽ bị khóa sai thời lượng.
- Nếu `SetCooldownTime` không resolve được trên runtime khác build, phải giữ
  fail-closed và fallback bằng timestamp response, không gọi binding đoán.
- Nếu xóa guard quá sớm, map snapshot stale có thể làm native gửi lại request
  trước cooldown; vì vậy cần giữ guard đến đúng `CooldownComplete`.

### 10. Bản sửa tối thiểu đúng

Đọc `CooldownComplete` từ `FortSearchOutProto`, truyền kết quả về coordinator,
đồng bộ nó qua `MapPokestop.SetCooldownTime(long)` khi binding đã verify, và
thay set vĩnh viễn bằng map `fort_id → cooldown deadline epoch ms`. Bộ lọc chỉ
bỏ qua entry khi deadline còn trong tương lai; entry hết hạn được xóa và fort
được chọn lại. Với response không có deadline hợp lệ, giữ hành vi fail-closed
theo `IsCoolingDown()` hiện tại và ghi log chẩn đoán.

### 11. Cách verify

- Unit/compile: kiểm tra signature Promise consume và binding mới không phá
  protocol; chạy focused tests và full Gradle build.
- Static: `git diff --check`, kiểm tra không còn `successful_spin_fort_ids`
  dạng set vĩnh viễn.
- Manual: log phải thể hiện `CooldownComplete`, `SetCooldownTime` được gọi,
  các scan trong cooldown bị bỏ qua, và scan sau deadline chọn lại cùng
  `fort_id` rồi gửi `FORT_SEARCH`.

---

## Acceptance Criteria (from spec)

> Source: no spec found → inferred — needs BA confirm.

| ID | Rule / Requirement | Formula / Expected | Acceptance note |
|----|--------------------|--------------------|-----------------|
| AC-1 | Đồng bộ cooldown sau spin | `FortSearchOutProto.CooldownComplete` được đọc và áp dụng cho fort tương ứng | Không dùng trạng thái đã spin vĩnh viễn |
| AC-2 | Không spin trong cooldown | Bỏ qua khi `nowEpochMs < cooldownCompleteEpochMs` hoặc `IsCoolingDown == true` | Không tạo request lặp sớm |
| AC-3 | Spin lại sau cooldown | Cho phép chọn khi `nowEpochMs >= cooldownCompleteEpochMs` và fort còn trong range | Cùng fort phải được chọn lại nếu là candidate hợp lệ |
| AC-4 | Giữ filter hiện có | `spin_available`, tọa độ hợp lệ và khoảng cách `<= 80 m` vẫn được kiểm tra | Không mở rộng phạm vi tương tác |
| AC-5 | Fail-closed binding | Chỉ gọi `SetCooldownTime` khi method đã resolve từ exact build; nếu không thì không gọi pointer đoán | Không ảnh hưởng runtime ngoài binding đã verify |

- Bug được xem là hoàn tất khi AC-1 đến AC-4 được chứng minh bằng log/manual
  test; AC-5 được chứng minh bằng capability guard và build verification.

---

## Synthesis

### Key Insight

Map không hẳn không được đọc lại; chính set `successful_spin_fort_ids` đã che
mất kết quả đọc mới và khóa fort vĩnh viễn. Response server đã cung cấp đúng
thời điểm cần thiết, nên có thể sửa theo dữ liệu thật thay vì hardcode thời gian
cooldown.

### Recommended Approach

Giữ map scan định kỳ, đọc và áp dụng `CooldownComplete` vào object MapPokestop,
đồng thời lưu deadline theo epoch milliseconds làm guard native. Xóa deadline
hết hạn trước khi chọn target; chỉ fort đang cooldown mới bị loại. Vẫn giữ các
guard Promise, lifecycle, exact build và interaction range hiện có.

### Risks to Watch

- Sai đơn vị thời gian giữa epoch ms và monotonic ns.
- Method setter có thể không được bind nếu discovery không đi qua inherited
  method; cần log binding và fallback an toàn.
- Cần bảo đảm response của fort nào được áp dụng cho đúng `fort_id`.

### Open Questions

- Chưa có spec BA riêng cho chu kỳ auto-spin; acceptance ở trên là
  `inferred — needs BA confirm`.
- Cần xác nhận trên emulator rằng `MapPokestop.get_IsCoolingDown()` trả về
  `false` sau deadline khi `SetCooldownTime` được gọi; nếu không, native
  deadline map vẫn là nguồn quyết định phụ.
