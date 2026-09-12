# Brainstorm: Lỗi decode automation event do ID unsigned

**Type:** bug
**Date:** 2026-09-12

---

## Analysis

### 1. Hành vi hiện tại

UI hiển thị lỗi `automation event decode: runtime automation event primary id is invalid` thay vì hiển thị toast tìm thấy/catch Pokémon.

### 2. Hành vi mong đợi

Các event native `POKEMON_FOUND`, `POKEMON_CAUGHT`, `POKEMON_FLED` và `POKEMON_TRANSFERRED` phải được Kotlin decode thành công, kể cả khi ID có bit cao nhất bằng 1, sau đó hiển thị toast tương ứng.

### 3. Khi nào và ở đâu xảy ra

Lỗi xảy ra ở `RuntimeAutomationEventPayloadCodec.decode()` khi native auto-catch phát telemetry. Log runtime ghi nhận encounter ID `14328597840097567882`, lớn hơn `Long.MAX_VALUE` (`9223372036854775807`).

### 4. Có tái hiện được không

Có thể tái hiện ổn định với event có `primary_id` lớn hơn `Long.MAX_VALUE`. Native encode ID này dưới dạng 64-bit unsigned; Kotlin `DataInputStream.readLong()` giữ nguyên bit nhưng biểu diễn kết quả dưới dạng signed `Long`, nên giá trị trở thành số âm.

### 5. Nguyên nhân gốc

Native protocol dùng `uint64_t` và `append_u64()`, trong khi Kotlin validate:

```kotlin
require(primaryId > 0L) { "runtime automation event primary id is invalid" }
```

Với ID unsigned có bit cao bằng 1, `readLong()` trả về `Long` âm dù payload không hỏng. Vì vậy validation signed nhận nhầm ID hợp lệ là invalid. Đây là lỗi tương thích signedness giữa hai ngôn ngữ, không phải lỗi magic, byte order hay kích thước payload.

### 6. Dependency map

- **Upstream:** native catch-spin chọn `target.spawn_id` (`uint64_t`), native catch Promise dùng `pending.encounter_id`, native transfer dùng `pokemon_id`; encoder ghi các giá trị bằng `append_u64()`.
- **Bridge:** `runtime_bridge_broker.inc` trích payload và forward nguyên bytes sang Kotlin.
- **Downstream:** `StructuredAutomationController` gọi `RuntimeAutomationEventPayloadCodec.decode()`. Decode fail sẽ set `lastError` và publish event `ERROR`, từ đó custom toast hiển thị lỗi.
- **Shared code:** codec được dùng cho cả bốn event telemetry; sửa validation/formatting có thể ảnh hưởng found, caught, fled và transferred.

### 7. Phạm vi ảnh hưởng

Các event có ID không vượt `Long.MAX_VALUE` vẫn hoạt động. Các encounter/pokemon ID có bit cao bằng 1 sẽ fail decode; log hiện tại cho thấy encounter ID thực tế đã thuộc trường hợp này. Failing event không làm native catch tự thất bại, nhưng làm mất toast và tạo toast lỗi decode.

### 8. Vì sao bug lọt vào

Protocol mới chỉ kiểm thử giá trị dương thông thường và chưa có test boundary cho unsigned 64-bit. Kotlin `Long` được dùng như container bit-pattern nhưng validation lại áp dụng semantics signed.

### 9. Rủi ro khi sửa

- Nếu đổi sang `ULong` toàn bộ model, có thể tạo thêm thay đổi API và formatting không cần thiết.
- Nếu chỉ bỏ validation mà vẫn format `Long` trực tiếp, toast sẽ hiển thị số âm gây hiểu nhầm.
- Không được chuyển ID sang `Int` hoặc giới hạn về signed 64-bit vì sẽ làm mất tính đúng của ID native.

### 10. Sửa tối thiểu đúng

Giữ ID dưới dạng `Long` để bảo toàn 64 bit, nhưng validate `primaryId != 0L` và `secondaryId >= 0L` theo raw-bit semantics phù hợp với field optional hiện tại. Khi hiển thị ID unsigned, dùng `toULong().toString()`. Bổ sung test decode với một ID lớn hơn `Long.MAX_VALUE` và test vẫn reject primary ID bằng 0.

### 11. Cách xác minh

- Unit test codec với payload chứa `0xC...` hoặc giá trị `14328597840097567882`; decode phải success và giữ nguyên bit pattern.
- Unit test payload có primary ID bằng 0; decode phải fail.
- Build `./gradlew test assembleDebug`.
- Build native bằng `scripts/build-magisk.sh`.
- Sau khi cài và restart, log phải có `bridge automation event forwarded` mà không có `automation event decode: ... primary id is invalid`; UI phải hiển thị toast event tương ứng.

---

## Acceptance Criteria (from spec)

> Source: no product spec found → inferred from the reported bug.

| ID | Rule / Requirement | Formula / Expected | Acceptance note |
|----|--------------------|-------------------|-----------------|
| AC-1 | Decode unsigned primary ID | Any non-zero 64-bit bit pattern, including values `> Long.MAX_VALUE`, decodes successfully | Fixes the reported decode error for native encounter/pokemon IDs |
| AC-2 | Reject only invalid zero primary ID | `primaryId == 0` is rejected; signed interpretation must not reject a valid high-bit ID | Preserves payload validation |
| AC-3 | Preserve event routing | FOUND/CAUGHT/FLED/TRANSFERRED map to the same `AutomationEventType` | No event is silently lost after decode |
| AC-4 | Display correct ID | `Long.toULong().toString()` is used for unsigned ID text | Prevents negative IDs in custom toast |
| AC-5 | Regression verification | Kotlin and native builds/tests pass; runtime log has no primary-ID decode error | Confirms protocol and UI integration |

- Bug requirement → **AC-1/AC-2**: high-bit native ID phải decode, zero ID vẫn fail.
- Toast requirement → **AC-3/AC-4**: event phải đi tới đúng toast và ID không bị hiển thị âm.
- Runtime verification → **AC-5**: build và log xác nhận end-to-end.

---

## Synthesis

### Key Insight

Payload đang được encode/forward đúng; lỗi nằm ở việc Kotlin diễn giải `uint64_t` như signed `Long`. Encounter ID trong log đã chứng minh trường hợp high-bit thực tế, nên việc kiểm tra `primaryId > 0L` là nguyên nhân trực tiếp.

### Recommended Approach

Sửa codec theo raw-bit unsigned semantics, giữ `Long` để tránh lan rộng thay đổi kiểu dữ liệu, và format ID bằng `toULong()` khi đưa vào toast. Thêm boundary tests cho high-bit ID và zero ID trước khi build/install lại.

### Risks to Watch

- Secondary ID cũng là uint64 ở protocol; không nên áp dụng validation signed tùy tiện.
- Các event cũ/manual có thể đi qua cùng codec và phải giữ routing hiện tại.
- Cần restart native module sau install để kiểm thử đúng binary mới.

### Open Questions

- Có cần hiển thị encounter ID hay species/pokemon ID trong nội dung toast không? Hiện tại đây là vấn đề hiển thị, không ảnh hưởng decode.
- Có cần đổi model Kotlin từ `Long` sang `ULong` về lâu dài không? Với phạm vi bug hiện tại, raw-bit `Long` + unsigned formatting là đủ.
