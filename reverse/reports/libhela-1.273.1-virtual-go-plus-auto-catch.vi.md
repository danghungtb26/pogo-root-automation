# Phân tích `libhela.so`: Virtual GO Plus, auto catch, spin fort, ball và throw

## 1. Kết luận nhanh

Có. Core `libhela.so` tải theo API của PGS Root chứa đủ bằng chứng tĩnh để xác nhận tính năng **Virtual GO Plus**, gồm ít nhất:

- preference `autocatch` và `autospin`;
- công tắc `Nearby Pokémon` và `Nearby PokéStop`;
- thông báo `notifyCatch`, `notifySpin`, `notifyPush`;
- logic đọc các object Pokémon trên bản đồ, encounter ID, Pokédex ID, tọa độ, khoảng cách và proto;
- hook vào `MapPokestop`, `PoiItemSpinner` và `PokestopSpinnerInteractionState` để spin;
- điều kiện `Pause Go Plus on Shiny`;
- thông báo tắt Nearby Pokémon khi hết Poké Ball;
- một cụm `Enhanced Throw` riêng cho encounter bắt thủ công.

Điểm quan trọng: symbol ứng dụng trong core đã bị strip. Vì vậy không tồn tại export có thể chỉ thẳng là `catchPokemon()` hoặc `spinFort()`. Hai hành vi được hiện thực bằng các hook ẩn danh, lời gọi gián tiếp và bridge sang object/state machine managed của chính game.

## 2. Phạm vi mẫu

### APK manager ban đầu

```text
apk/pgsroot1.0.15_BIeEp(arm64).apk
package: m.u.va.man
```

APK manager không nhúng sẵn `libhela.so`. Nó:

1. Cài module Zygisk bằng root.
2. Kiểm tra phiên bản Pokémon GO.
3. Truy vấn API để lấy core tương thích.
4. Tải core vào vùng file của ứng dụng.
5. Zygisk nạp core vào process `com.nianticlabs.pokemongo`.

Chi tiết inject đã được mô tả riêng trong [pgsroot-1.0.15-phan-tich-inject-pokemon-go.vi.md](./pgsroot-1.0.15-phan-tich-inject-pokemon-go.vi.md).

### Core được phân tích

API trả về core cho Pokémon GO `0.433.0`:

```text
https://dl.pgsharp.com/pgsrt1.273.1_0.433.0.dt
https://dl.pgsharp.com/pgsrt1.273.0_0.433.0.dt
```

Lớp `.dt` được giải bằng cách bỏ 8 byte đầu rồi XOR phần còn lại với `0x0d`. Báo cáo dùng build mới hơn:

```text
apk/core/libhela-1.273.1-pogo-0.433.0.so
SHA-256: 893b84c077942b988e3419eeac54d973a35f9dd90a65e79b33bd894f081a6ef2
Format: ELF64 AArch64, stripped
SONAME: libhela.so
Build ID: 7369543908bf1c3e216306f5f3764fabebdcee78
```

Build `1.273.0` cũng chứa các cụm chuỗi/tính năng trọng tâm giống build `1.273.1`.

### APK UI nằm trong core

Core còn nhúng một APK/ZIP tại file offset `0x24dfca4`, đã tách thành:

```text
apk/core/libhela-1.273.1-embedded-ui.apk
SHA-256: 7bdb545aae4d3bb870da6c8afab8150979e27b5e1736f51e33d0d9e3239a00df
package: me.underw.hp
versionName: 1.273.1
```

Tài nguyên `assets/i18n/en.json` trong APK này cung cấp tên tính năng và thông báo trạng thái. Đây là bằng chứng UI bổ sung cho những hook native tìm thấy trong ELF.

## 3. Core được inject và bám vào game như thế nào?

Chuỗi tải tổng quát:

```text
PGS Root manager
    -> module Zygisk
    -> lọc process com.nianticlabs.pokemongo
    -> dlopen(.../files/libhela.so)
    -> resolve entry ngụy trang __cxa_guard_enter
    -> truyền địa chỉ android_dlopen_ext
    -> core hook loader và các object/state của game
```

Như vậy `libhela.so` chạy trong chính process Pokémon GO. Nó không cần đọc pixel hoặc tự nhận dạng icon trên màn hình; nó có thể quan sát object managed/native mà game vừa tạo.

## 4. Bằng chứng Virtual GO Plus

### Preference trong core

Các field nằm liền nhau:

```text
0x9499b6 autocatch
0x9499c0 autospin
0x9499c9 notifySpin
0x9499d4 notifyCatch
0x9499e0 notifyPush
0x9499eb alwaysAdvertise
0x9499fb sleepTracking
```

Xref `autocatch`, `autospin`, `notifySpin` lần lượt nằm tại `0x7e4e4c`, `0x7e4e8c`, `0x7e4ecc`. Code quanh đó ánh xạ từng tên với byte field tương ứng trong object preference.

Đây là bước đăng ký/serialize preference, không phải ba “hàm thực thi” độc lập. Tuy nhiên khi ghép với UI và hook map/spinner, nó là bằng chứng rõ rằng core hỗ trợ chế độ GO Plus ảo.

### UI và trạng thái

APK nhúng chứa chính xác:

```text
Virtual Go Plus
GO Plus is not available for %s version.
Go Plus is connected, teleport may trigger a softban.
Nearby Pokémon
Nearby PokéStop
Pause Go Plus on Shiny
You're out of PokéBalls, "Nearby Pokémon" is turned off.
"Nearby Pokémon" is turned on.
```

### Mô hình hoạt động có khả năng cao

```text
Object map của game
   |                         |
   | Pokémon candidate       | MapPokestop candidate
   v                         v
Chuẩn hóa MapPokemon       MapPokestop/PoiItemSpinner
   |                         |
encounter id, dex,           | state hợp lệ + trong vùng
tọa độ, khoảng cách, proto    |
   |                         |
Nearby Pokémon bật?          Nearby PokéStop bật?
có Poké Ball?                autospin bật?
pause khi shiny?             spinner sẵn sàng?
   |                         |
   v                         v
GO Plus auto-catch path     game spinner/QuickSpin path
   |                         |
notifyCatch/push            notifySpin/push
```

## 5. Nhận biết Pokémon xung quanh

Core nhận diện các loại object map sau:

```text
WildMapPokemon
IncenseMapPokemon
TroyDiskMapPlacePokemon
DailyEncounterMapPokemon
PhotobombingMapPokemon
IncidentMapPokemon
PokestopSpawnPokemon
RouteMapPokemon
RaidMapPokemon
StationMapPokemon
```

Format log tại `0x93e6e3`, được tham chiếu từ code `0x7b5368`:

```text
MapPokemon type:%d eid:%lu dex:%d latlng:%.5f,%.5f dis:%.1f hasProto:%d
```

Luồng ở `0x7b51dc`–`0x7b5400` cho thấy:

1. Core gọi virtual method của object map để lấy tọa độ.
2. Nó đọc tọa độ hiện tại của người chơi từ state toàn cục.
3. Nó tính khoảng cách địa cầu bằng `sincos`, `cos` và `acos`.
4. Nó lấy type, encounter ID, Pokédex ID, latitude/longitude, distance và cờ `hasProto`.
5. Object không hợp lệ đi qua nhánh log `@MapPokemon type:%d invalid`.

Do đó “nhận biết Pokémon” ở đây là đọc dữ liệu model/runtime của game, không phải computer vision.

## 6. Nhận biết fort/PokéStop xung quanh

Các tên managed quan sát được:

```text
PoiItemSpinner
MapPokestop
PokestopSpinnerInteractionState
QuickSpin
MapGym
```

Các điểm tham chiếu chính:

| Địa chỉ | Bằng chứng |
|---|---|
| `0x79d9c4` | so khớp `PokestopSpinnerInteractionState` |
| `0x79da2c` | tham chiếu `QuickSpin` |
| `0x79db14` | so khớp type `PoiItemSpinner` |
| `0x79db64` | so khớp type `MapPokestop` |
| `0x79dd28` | dùng lại/cache type `MapPokestop` |

Hook ở vùng `0x79daa4` còn:

- kiểm tra flag tính năng tại state `+0x4e8`;
- xác nhận object đúng type;
- kiểm tra các điều kiện state qua lời gọi gián tiếp;
- giữ lại con trỏ spinner/pokestop;
- gọi bridge managed của game.

Như vậy fort được nhận ra từ `MapPokestop`; gym có type `MapGym` riêng. Core không dò màu/ảnh của đĩa PokéStop.

## 7. Cách chọn Pokémon và fort

### Điều xác nhận được

Candidate Pokémon có ít nhất:

- loại nguồn spawn;
- encounter ID;
- species/Pokédex ID;
- latitude/longitude;
- khoảng cách tới người chơi;
- cờ có proto hợp lệ.

Các gate người dùng/hệ thống quan sát được:

- `Nearby Pokémon` phải bật;
- `autocatch` phải bật;
- phải còn Poké Ball;
- tùy chọn `Pause Go Plus on Shiny` có thể dừng luồng;
- object phải hợp lệ và trong state game phù hợp.

Candidate fort phải là `MapPokestop`, có `PoiItemSpinner`, Nearby PokéStop/autospin bật và spinner không ở trạng thái bị loại.

### Điều chưa phục hồi được

Static build không đủ để khẳng định comparator chính xác khi có nhiều candidate:

- gần nhất trước;
- spawn cũ nhất/mới nhất trước;
- thứ tự object do game trả về;
- hay một queue nội bộ của GO Plus.

Khoảng cách chắc chắn được tính, nên nó có thể dùng để lọc hoặc xếp hạng; nhưng chưa có bằng chứng đủ mạnh để nói “luôn chọn con gần nhất”. Cũng không thấy UI chọn species/IV riêng nằm trong nhóm Virtual GO Plus. Cách mô tả thận trọng nhất là core thu thập candidate hợp lệ, còn thứ tự cuối nhiều khả năng bám theo queue/state machine GO Plus của game.

## 8. `spin fort` là function nào?

Không có export tên `spinFort`.

Mapping gần nhất theo bằng chứng là hook bắt đầu quanh `0x79daa4`:

```text
feature enabled
  -> type == PoiItemSpinner
  -> target == MapPokestop
  -> spinner/state hợp lệ
  -> giữ target
  -> gọi bridge/state machine của game
```

Phần `QuickSpin` quanh `0x79d97c` phát hiện/điều khiển `PokestopSpinnerInteractionState`. Một hook khác quanh `0x79dca4` nhận object `MapPokestop` và chuyển tiếp lời gọi gián tiếp.

Vì vậy “function spin fort” không phải một RPC độc lập dễ đặt tên. Nó là cụm hook điều khiển flow `MapPokestop -> PoiItemSpinner -> PokestopSpinnerInteractionState` của game.

## 9. `catch Pokémon` là function nào?

Không có export tên `catchPokemon`, và không nên coi xref `autocatch` tại `0x7e4e4c` là hàm bắt: xref đó thuộc phần preference/serialization.

Mapping theo bằng chứng:

```text
MapPokemon candidate
  -> kiểm tra object/proto/encounter id
  -> gate Nearby Pokémon + autocatch + ball + shiny policy
  -> interaction path của Virtual GO Plus
  -> game xử lý kết quả catch/flee
  -> notifyCatch / notifyPush
```

Tên method managed/RPC cuối cùng không phục hồi được do code bị strip và phần bridge gọi gián tiếp. Kết luận chắc chắn là core tự động hóa **đường tương tác GO Plus của game**, không phải tự động tap vào Pokémon rồi mô phỏng một cú vuốt thông thường.

## 10. Chọn ball như thế nào?

### Bảng enum/item core hiểu được

Jump table quanh `0x7be308` ánh xạ item enum sang tên:

```text
0x941574 PokeBall
0x94157d GreatBall
0x941587 UltraBall
0x941591 MasterBall
0x94159c PremierBall
0x9415a8 BeastBall
0x9415b2 WildBall
...
```

Trong APK UI:

```text
item 1 = Poké Ball
item 2 = Great Ball
item 3 = Ultra Ball
```

### Ball của Virtual GO Plus

Thông báo dành riêng cho luồng này nói rằng khi hết `PokéBalls`, `Nearby Pokémon` bị tắt. Không có thông báo hay nhánh UI cho fallback sang Great/Ultra Ball.

Kết luận có độ tin cậy cao cho build này:

- Virtual GO Plus dùng Poké Ball thường theo semantics phụ kiện;
- khi hết bóng, auto catch dừng/tắt Nearby Pokémon;
- không thấy bằng chứng tự đổi sang Great Ball, Ultra Ball hoặc Master Ball.

Sự hiện diện của Great/Ultra/Master trong bảng enum chỉ cho biết core hiểu các item đó ở những luồng khác. `Remember Pokéball` là tính năng riêng của encounter thủ công và không đủ bằng chứng để gán cho GO Plus.

## 11. Throw là gì?

### Virtual GO Plus

Auto catch GO Plus không dựng một đường vuốt trên màn hình. Nó dùng accessory interaction path của game; do đó không có “tọa độ throw” kiểu gesture trong luồng này.

### Enhanced Throw cho bắt thủ công

Core có bốn cấu hình:

```text
0x94b972 enhancedThrowCurve
0x94b985 enhancedThrowExcellent
0x94b99c enhancedThrowARPlus
0x94b9b0 enhancedThrowHit
```

Tài nguyên UI hiển thị:

```text
Enhanced Throw
Nice
Great
Excellent
100% Hit
AR+
Curveball
```

Vùng native `0x81313c`–`0x81329c`:

1. Đọc bitmask tính năng tại state `+0x430`.
2. Tạo danh sách grade đang bật.
3. Gọi `rand()` để chọn một grade khả dụng.
4. Gọi `rand()` lần nữa cùng các bảng tại `0x94b9c8`/`0x94b9d4` để tạo hệ số float.
5. Ghi hệ số và cờ trực tiếp vào object kết quả throw.

Tại `0x8134e8`, nhánh `enhancedThrowHit` đặt cờ hit, xóa một trường trạng thái số nguyên rồi chuyển tiếp qua hook/bridge.

Kết luận: Enhanced Throw sửa dữ liệu/kết quả throw trong object của game. Không có bằng chứng nó ghi và phát lại gesture ngón tay. Cơ chế này thuộc encounter thủ công, tách biệt với Virtual GO Plus auto catch.

## 12. Bảng mức độ chắc chắn

| Nhận định | Mức | Cơ sở |
|---|---:|---|
| Core có Virtual GO Plus | Cao | UI, preference `autocatch/autospin`, notify và hook spinner |
| Pokémon được đọc từ object game, không từ ảnh | Cao | các type `*MapPokemon`, field log và code tính khoảng cách |
| Fort/PokéStop được đọc từ `MapPokestop` | Cao | type comparison và hook spinner |
| Spin dùng flow spinner/state machine của game | Cao | `PoiItemSpinner`, `PokestopSpinnerInteractionState`, `QuickSpin` |
| Auto catch dùng accessory/GO Plus path | Cao | preference + UI + tách biệt khỏi Enhanced Throw |
| Auto catch dùng Poké Ball thường, không fallback | Cao nhưng chưa có trace runtime | thông báo hết Poké Ball tắt Nearby Pokémon, không có fallback UI |
| Core luôn chọn candidate gần nhất | Chưa xác định | có tính distance nhưng chưa phục hồi comparator |
| Tên hàm chính xác `spinFort`/`catchPokemon` | Không có bằng chứng | symbol strip, lời gọi managed gián tiếp |
| Enhanced Throw giả lập gesture | Không | quan sát thấy sửa object/field, không thấy trajectory replay |

## 13. Giới hạn phân tích

- Đây là phân tích tĩnh; không chạy core trong Pokémon GO và không gửi request tới server game.
- REA xác nhận artifact/hash, nhưng provider native sâu không sẵn có: Ghidra chưa cấu hình và cần JDK 21+, Hopper chưa cài, IDA MCP chưa cấu hình.
- Disassembly bổ sung dùng `objdump`, bảng string và script tìm xref AArch64 `ADRP + ADD`.
- Các địa chỉ chỉ đúng với `libhela.so` 1.273.1 dành cho game 0.433.0; build mới có thể thay đổi hoàn toàn offset.
- Chuỗi/tên class chứng minh core biết hoặc hook thành phần đó; riêng chuỗi đơn lẻ không đủ chứng minh toàn bộ semantics. Báo cáo chỉ nâng mức kết luận khi có thêm xref/code/UI tương ứng.
- Can thiệp process game có thể vi phạm điều khoản dịch vụ và gây khóa tài khoản.

## 14. Hồ sơ nghiên cứu

- [Research 01 - Phạm vi và metadata](../research/01-pham-vi-va-metadata.md)
- [Research 02 - Giải mã config và chọn core](../research/02-giai-ma-config-va-chon-core.md)
- [Research 03 - Tải và giải lớp core](../research/03-tai-va-giai-ma-core.md)
- [Research 04 - Auto catch, map Pokémon và fort](../research/04-bang-chung-auto-catch-map-fort.md)
- [Research 05 - Ball và throw](../research/05-ball-va-throw.md)

