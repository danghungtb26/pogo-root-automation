# Phân tích Auto Transfer và Auto Discard của `libhela.so`

## Kết luận ngắn

Trong build PGSharp 1.273.1 dành cho Pokémon GO 0.433.0:

- **Auto Transfer** chạy ngay trên luồng Pokémon vừa bắt được. Nó áp dụng một
  bitmask các điều kiện **giữ lại**, rồi gọi trực tiếp
  `Niantic.Holoholo.IPokemonBag.ReleasePokemon(pokemon)` nếu không có điều kiện
  bảo vệ nào khớp.
- **Auto Discard** đọc số lượng bằng
  `Niantic.Holoholo.IItemBag.GetItemCount(itemType)`, tính phần dư so với số cần
  giữ, rồi gọi `Niantic.Holoholo.IItemBag.RecycleItem(...)`.
- Cả hai không điều khiển nút trên giao diện. `PokemonInfoGuiController.TransferPokemon`
  và Pokémon HOME không phải đường gọi của hai tính năng này.
- Cấu hình được lưu trong Android `SharedPreferences`, file có tên ngụy trang
  `com.google.android.gms.safetynet`.

## Artifact và độ khớp phiên bản

| Artifact | Thông tin |
|---|---|
| PGSharp core | `libhela-1.273.1-pogo-0.433.0.so` |
| SHA-256 core | `893b84c077942b988e3419eeac54d973a35f9dd90a65e79b33bd894f081a6ef2` |
| Pokémon GO | `com.nianticlabs.pokemongo` 0.433.0, versionCode `2026100702` |
| Kiến trúc | arm64/AArch64 |
| IL2CPP metadata | version 39 |

APK Pokémon GO dùng để đối chiếu có chứng thư ký `OU="Niantic, Inc.", O="Niantic,
Inc."`, SHA-256 chứng thư
`944bda466eaf64be1c93896d5c8e9a454a07c8d8fcbc4adeeefb3545b335e979`.

## 1. Cấu hình được lưu như thế nào?

Module gọi:

```java
context.getSharedPreferences("com.google.android.gms.safetynet", 0)
```

Sau đó ghi bằng `SharedPreferences.Editor.putInt/putString(...).apply()`.
Khi chạy trong package Pokémon GO, file thường tương ứng với:

```text
/data/user/0/com.nianticlabs.pokemongo/shared_prefs/
  com.google.android.gms.safetynet.xml
```

### Auto Transfer

```text
key:  hlautotrans
type: int
```

Giá trị là bitmask các loại Pokémon cần **giữ lại**:

| Giá trị | Điều kiện được bảo vệ |
|---:|---|
| 1 | IV 100% |
| 2 | Shiny |
| 4 | IV 0% |
| 8 | IV 90% trở lên |
| 16 | PvP Rank Top 10 |
| 32 | Legendary |
| 64 | XXL |
| 128 | XXS |
| 256 | Level 30 trở lên |
| 512 | Có background |

Default mask là `803 = 1|2|32|256|512`. Ba bit `1|2|32 = 35` bị ép bật khi
Auto Transfer hoạt động, nghĩa là IV 100, shiny và legendary luôn được bảo vệ.

Số âm biểu diễn switch tắt nhưng vẫn giữ lựa chọn. Ví dụ `-803` là trạng thái mặc
định: tính năng tắt, mask nhớ là 803. Listener Java chuyển mọi giá trị âm thành `0`
trước khi gửi cho native. Config ID native là `46` (`AUTOTRANS`).

### Auto Discard

```text
key:     hlautodiscard
type:    JSON string
default: {}
```

Ví dụ:

```json
{
  "notify": true,
  "items": {
    "1": 100,
    "2": 50,
    "3": 200,
    "701": 30
  }
}
```

`items[itemId]` là **số cần giữ lại**, không phải số cần xóa. Với cấu hình
`"1": 100`, nếu đang có 137 Poké Ball thì lượng dự kiến bỏ là `137 - 100 = 37`.
Nếu đang có 80 thì không bỏ gì. Giá trị `-1` trong UI nghĩa là không đặt rule.

Config ID native là `47` (`AUTODISCARD`). `notify` mặc định `true` và điều khiển
thông báo kết quả.

Các item được UI hỗ trợ:

```text
1    Poké Ball             2    Great Ball
3    Ultra Ball            101  Potion
102  Super Potion          103  Hyper Potion
104  Max Potion            201  Revive
202  Max Revive            701  Razz Berry
703  Nanab Berry           705  Pinap Berry
706  Golden Razz Berry     708  Silver Pinap Berry
1201 Rare Candy            1202 Rare Candy XL
```

## 2. Logic thực thi Auto Transfer

Luồng native nằm trong vùng function `0x8061bc`–`0x8067f4` của core đã nêu.

```text
Pokémon vừa bắt được
        │
        ▼
Auto Transfer đang bật và service sẵn sàng?
        │ không → dừng
        ▼ có
Bỏ qua species đặc biệt 132/201/251/570
        │
        ▼
Kiểm tra IV, shiny, legendary, PvP rank, size, level, background
        │
        ├─ khớp một rule giữ lại → dừng, không transfer
        │
        ▼
IPokemonBag.ReleasePokemon(pokemon)
        │
        ▼
notification "notif_trans" + log "autoTransfer"
```

Chi tiết các phép kiểm tra native:

- IV = `(attack + defense + stamina) / 45`;
- IV 100 khi tổng bằng 45;
- IV 0 khi tổng bằng 0;
- IV 90+ khi `sum * 100 >= 4050`;
- PvP Top 10 dùng helper tính rank và giữ khi rank dưới 11;
- XXL/XXS dùng helper phân loại size;
- level 30+ so với mốc `58` trong cách biểu diễn nửa-level nội bộ;
- background được nhận biết qua danh sách/field background không rỗng.

Core luôn bỏ qua các species ID `132` (Ditto), `201` (Unown), `251` (Celebi) và
`570` (Zorua). Bằng chứng chỉ cho thấy điều kiện bỏ qua; lý do chính xác không có
symbol nên không nên diễn giải quá mức.

### Class và method Pokémon GO thực sự được gọi

| Vai trò | Interface | Implementation | Metadata |
|---|---|---|---|
| Transfer Pokémon | `Niantic.Holoholo.IPokemonBag.ReleasePokemon(pokemon)` | `Niantic.Holoholo.Internal.PokemonBagImpl.ReleasePokemon(pokemon)` | interface slot 15, token `0x0600105f`; implementation slot 19, token `0x06002a43` |

Tên “Transfer on Catch” là tên tính năng của module; API nội bộ của game dùng từ
`ReleasePokemon`. Nó không gọi:

- `Niantic.Holoholo.UI.Pokemon.PokemonInfoGuiController.TransferPokemon()`;
- `PokemonHomeRpcService.TransferPokemon(transferData)`;
- `BelugaTransferService.TransferPokemon(pokemonList)`.

## 3. Logic thực thi Auto Discard

Luồng native nằm trong vùng function `0x802ee4`–`0x8035a4`.

```text
Đọc JSON hlautodiscard
        │
        ▼
GetItemCount cho 16 item ID
        │
        ▼
Với từng rule: discard = max(0, current - keep)
        │
        ▼
Áp dụng cap/reserve riêng cho nhóm Potion và Revive
        │
        ├─ discard = 0 → bỏ qua
        ▼
IItemBag.RecycleItem(itemData, discard, expiringItemsCopy)
        │
        ▼
notification "notif_disc" nếu notify=true + log "autoDiscard"
```

Màn hình settings gửi event `cm.itemCounts`; native trả mảng `itemCounts` gồm 16
giá trị để hiển thị tồn kho hiện tại. Trong core còn có điều chỉnh tổng cho nhóm
Potion (`101`–`104`) và Revive (`201`–`202`). Có thể xác nhận đây là bước cap theo
dung lượng/reserve chung, nhưng không đủ symbol để đặt tên chính xác mọi biến.

### Class và method Pokémon GO thực sự được gọi

| Vai trò | Interface | Implementation | Slot/token |
|---|---|---|---|
| Đọc số lượng | `Niantic.Holoholo.IItemBag.GetItemCount(itemType)` | `Niantic.Holoholo.Internal.ItemBagImpl.GetItemCount(itemType)` | 5 / `0x06000f85`; 9 / `0x0600295b` |
| Bỏ item | `Niantic.Holoholo.IItemBag.RecycleItem(itemProto, numToRecycle, expiringItemsCopy)` | `Niantic.Holoholo.Internal.ItemBagImpl.RecycleItem(itemData, numToRecycle, expiringItemsCopy)` | 19 / `0x06000f93`; 23 / `0x06002971` |

`Niantic.Holoholo.UI.Items.ItemInventoryService` cũng được module resolve/cache.
Nó hỗ trợ context/callback và dữ liệu UI, nhưng operation xóa item thật đi qua
`IItemBag.RecycleItem`/`ItemBagImpl.RecycleItem`.

Các message RPC liên quan trong metadata:

```text
Holoholo.Rpc.ReleasePokemonProto / ReleasePokemonOutProto
Holoholo.Rpc.RecycleItemProto    / RecycleItemOutProto
```

## 4. Bảng method chuẩn đã đối chiếu IL2CPP

| Class | Method | Slot | Token |
|---|---|---:|---:|
| `Niantic.Holoholo.IPokemonBag` | `ReleasePokemon(pokemon)` | 15 | `0x0600105f` |
| `Niantic.Holoholo.Internal.PokemonBagImpl` | `ReleasePokemon(pokemon)` | 19 | `0x06002a43` |
| `Niantic.Holoholo.IItemBag` | `GetItemCount(itemType)` | 5 | `0x06000f85` |
| `Niantic.Holoholo.Internal.ItemBagImpl` | `GetItemCount(itemType)` | 9 | `0x0600295b` |
| `Niantic.Holoholo.IItemBag` | `RecycleItem(itemProto, numToRecycle, expiringItemsCopy)` | 19 | `0x06000f93` |
| `Niantic.Holoholo.Internal.ItemBagImpl` | `RecycleItem(itemData, numToRecycle, expiringItemsCopy)` | 23 | `0x06002971` |

## 5. Mức độ chắc chắn và giới hạn

- **Cao:** khóa/kiểu SharedPreferences, JSON, bitmask, item ID, luồng tính lượng
  bỏ, tên class/method/tham số/slot/token IL2CPP và việc gọi bag services.
- **Khá cao:** ánh xạ từng bit với nhánh native, do đã đối chiếu nhãn UI, bitmask
  và phép kiểm tra assembly.
- **Trung bình:** tên gọi chi tiết của các biến reserve/cap nhóm Potion/Revive;
  binary đã strip symbol nên chỉ mô tả hành vi quan sát được.

Môi trường REA nhận dạng artifact nhưng provider decompiler sâu không khả dụng
(Ghidra yêu cầu JDK mới hơn, IDA/Hopper chưa cấu hình). Vì vậy native được kiểm tra
bằng AArch64 disassembly/xref; tên method được khóa lại bằng metadata IL2CPP v39
của đúng Pokémon GO 0.433.0.

## 6. Hồ sơ research

- `research/06-cau-hinh-auto-transfer-auto-discard.md`
- `research/07-luong-thuc-thi-auto-transfer-auto-discard.md`
- `research/08-doi-chieu-pokemon-go-0.433.0.md`
- `research/parse_il2cpp_v39_metadata.py`

Các file trên tách riêng bằng chứng settings, native flow và đối chiếu class/method
để có thể tái kiểm tra độc lập.
