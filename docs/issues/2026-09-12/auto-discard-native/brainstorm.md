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

## Reverse analysis: inventory và discard trong Pokémon GO 0.427.0

### Phạm vi và độ tin cậy

Đã đối chiếu các artifact trong `reverse/pogo-0.427.0/`: `dump.cs.gz`,
`script.json.gz`, `il2cpp.h.gz`, `stringliteral.json.gz`, các curated class trong
`classes/`, `OBSERVATION_RESEARCH.md` và binary
`native/libil2cpp.so`. Binary là ELF `aarch64`, stripped, nên tên private trong
metadata vẫn còn nhưng symbol native không còn; RVA dưới đây là RVA của build
0.427.0 arm64-v8a.

`classes/` chủ yếu là map/tap binding; không có implementation inventory. `MapPokemon`
chỉ giữ một field `IItemBag`. Phần inventory thực sự nằm trong `dump.cs`/metadata và
`libil2cpp.so`.

Quy ước trong phần này:

- **Exact metadata**: lấy trực tiếp từ `dump.cs`/`script.json`, gồm signature, field,
  tên public và RVA.
- **Exact `.so`**: logic đã được xác nhận bằng disassembly ARM64.
- **Suy luận có kiểm soát**: semantic name được đặt lại từ signature, field và call
  graph; không coi đó là tên gốc của source khi method đã bị obfuscate.

`dump.cs` là tên C# đã được metadata/decompiler khôi phục. Tên native generated trong
`script.json` có dạng `Niantic_Holoholo_Internal_ItemBagImpl__RecycleItem`; tên dạng
`yud`, `yue`, `yuj` là tên obfuscated còn lại, không có tên gốc semantic trong artifact.

### Đường đi dữ liệu đã xác nhận

```text
InventoryCache.UpdateInventory()
  -> GetHoloholoInventory (Method = 4)
  -> GetHoloholoInventoryOutProto.InventoryDelta
  -> NianticInventoryCache.HandleInventoryDelta()
  -> inventory<K,V> + prediction/serverState
  -> InventoryCache.OnFullInventoryUpdate / OnInventoryUpdate
  -> ItemBagImpl.yud / yuj
  -> ItemBagImpl.cwcq: Dictionary<Item, ItemProto>
  -> IItemBag.AllItems / GetItem / GetItemCount / category counters
```

```text
ItemBagImpl.RecycleItem(itemData, numToRecycle, expiringItemsCopy)
  -> tạo HoloInventoryItemProto prediction với Count = oldCount - numToRecycle
  -> InventoryCache.AddPredictedUpdates([prediction])
  -> tạo RecycleItemProto(Item, Count)
  -> RpcHandlerExt.Send(..., Method.RecycleInventoryItem = 137, request)
  -> Promise callback ItemBagImpl.wi.ytr / yts
     -> Success: yua(prediction, expiringItemsCopy)
     -> Error result: RollbackPredictedUpdates([prediction])
     -> rejected Promise: log lỗi
```

Điểm quan trọng: inventory không phải một `Dictionary<Item, int>` thuần. Server gửi
delta protobuf có timestamp; cache giữ `serverState` và `clientPrediction`. Vì vậy
đọc qua `ItemBagImpl.cwcq` sau khi wiring hoàn tất sẽ phù hợp với cách game hiển thị
inventory hơn việc tự gửi RPC rồi tự parse response.

### Các struct/proto cần biết

| Type/field | Ý nghĩa |
|---|---|
| `Item` | enum/int định danh item. Các giá trị liên quan: `PokeBall=1`, `GreatBall=2`, `UltraBall=3`, `MasterBall=4`, `WildBall=7`. |
| `HoloItemCategory` | enum nhóm item; `ItemCategoryPokeball=1`, `Food=2`, `Medicine=3`, `Incubator=8`, `Incense=9`, `Expiring=31`, v.v. |
| `ItemProto.Item` | item id, offset `0x10`. |
| `ItemProto.Count` | số lượng hiện tại, offset `0x14`. |
| `ItemProto.Unseen` | item mới/chưa xem, offset `0x18`. |
| `ItemProto.ExpirationTime` | expiration dạng string, offset `0x20`. |
| `ItemProto.IgnoreInventoryCount` | cờ item không tính vào giới hạn inventory, offset `0x28`. |
| `ItemProto.UnconvertedLocalExpirationTimeMs` | thời điểm hết hạn local, offset `0x30`. |
| `ItemProto.ClaimableExpirationConsolationCount` | số consolation có thể claim, offset `0x40`. |
| `ItemInventoryItemWidget.ItemData.item` | item được chọn để recycle, offset `0x10`. |
| `ItemInventoryItemWidget.ItemData.count` | count mà UI/model đang hiển thị, offset `0x1C`. |
| `ItemInventoryItemWidget.ItemData.recyclable` | UI eligibility; `RecycleItem` không chứng minh là tự kiểm tra cờ này trong `.so`, nên caller phải validate. |
| `ItemInventoryItemWidget.ItemData.itemExpirationData` | metadata expiration, offset `0x38`; `ExpireTime` nằm ở `+0x10`. |
| `RecycleItemProto.Item` / `Count` | request gửi server: item id và số lượng muốn discard, offsets `0x10`/`0x14`. |
| `RecycleItemOutProto.Result` | `Success=1`, `ErrorNotEnoughCopies=2`, `ErrorCannotRecycleIncubators=3`; `Unset=0`. |
| `RecycleItemOutProto.NewCount` | count server trả về sau recycle, offset `0x14`. |
| `InventoryDeltaProto` | `OriginalTimestamp`, `NewTimestamp`, danh sách `InventoryItemProto`. |
| `InventoryItemProto` | delta record có `ModifiedTimestamp` và oneof `DeletedItemKey` hoặc serialized `Item`. |

### Function đọc inventory

#### 1. Public read API của `IItemBag`/`ItemBagImpl`

| Tên semantic/original | Tên metadata/native mới | RVA | Parameters | Logic đã xác nhận |
|---|---|---:|---|---|
| `IItemBag.get_AllItems` | `ItemBagImpl.get_AllItems` / `Niantic_Holoholo_Internal_ItemBagImpl__get_AllItems` | `0x8163B44` | `this`: `ItemBagImpl`; `method`: IL2CPP `MethodInfo*` ẩn | Đọc field `cwcq` ở `this+0x70`, lấy `Dictionary<Item, ItemProto>.Values`, trả về `ICollection<ItemProto>`. `.so` xác nhận đây là snapshot view của map local. |
| `IItemBag.GetItem(Item itemType)` | `ItemBagImpl.GetItem` / `Niantic_Holoholo_Internal_ItemBagImpl__GetItem` | `0x8163E00` | `itemType`: enum `Item`; `this`; `method` | `TryGetValue(cwcq, itemType, out ItemProto)`, trả object nếu có, `null` nếu không có. |
| `IItemBag.GetItemCount(Item itemType)` | `ItemBagImpl.GetItemCount` / `Niantic_Holoholo_Internal_ItemBagImpl__GetItemCount` | `0x8163D78` | `itemType`: enum `Item`; `this`; `method` | Lookup `cwcq`; nếu có đọc `ItemProto.Count` ở `+0x14`, nếu không có trả `0`. Đây là function đơn giản nhất cho count một item. |
| `IItemBag.GetCategoryCount(HoloItemCategory category)` | `ItemBagImpl.GetCategoryCount` / `Niantic_Holoholo_Internal_ItemBagImpl__GetCategoryCount` | `0x8163E6C` | `category`: enum `HoloItemCategory`; `this`; `method` | Duyệt `cwcq.Values`; dùng `IGameMasterData` ở `this+0x30` để resolve category của từng item; nếu trùng category thì cộng `ItemProto.Count`. |
| `IItemBag.GetBallCategoryCount(bool excludeMasterBall, bool excludeWildBall)` | `ItemBagImpl.GetBallCategoryCount` / `Niantic_Holoholo_Internal_ItemBagImpl__GetBallCategoryCount` | `0x816484C` | `excludeMasterBall`: bỏ item enum `4`; `excludeWildBall`: bỏ item enum `7`; `this`; `method` | Duyệt values, áp dụng hai cờ loại trừ, resolve category bằng `IGameMasterData`, chỉ cộng item thuộc `ItemCategoryPokeball=1`. |
| `IItemBag.GetTotalItems()` | `ItemBagImpl.GetTotalItems` / `Niantic_Holoholo_Internal_ItemBagImpl__GetTotalItems` | `0x8164C44` | `this`; `method` | Duyệt các `ItemProto` trong `cwcq`, lấy `ItemSettingsProto` từ `IGameMasterData`, bỏ qua item có `IgnoreInventorySpace` (`ItemSettingsProto +0x98`), rồi cộng `ItemProto.Count`. Không nên tự diễn giải đây là capacity còn trống. |
| `IItemBag.HasItem(Item itemType)` | `ItemBagImpl.HasItem` / `Niantic_Holoholo_Internal_ItemBagImpl__HasItem` | `0x8164FE0` | `itemType`: enum `Item`; `this`; `method` | Helper boolean dựa trên item có trong bag/count; phù hợp để guard trước action, nhưng nếu cần count cụ thể nên dùng `GetItemCount`. |
| `IItemBag.IsItemNew(Item itemType)` | `ItemBagImpl.IsItemNew` / `Niantic_Holoholo_Internal_ItemBagImpl__IsItemNew` | `0x8165070` | `itemType`: enum `Item`; `this`; `method` | Đọc trạng thái `ItemProto.Unseen`; dùng cho UI/triage item mới, không phải count. |

Trong tất cả native signature, `this` và `MethodInfo* method` là tham số IL2CPP; không
phải input nghiệp vụ. `itemType`/`category` mới là input cần bind chính xác.

#### 2. Refresh từ server và cache delta

| Tên semantic/original | Tên obfuscated/native | RVA | Parameters | Logic |
|---|---|---:|---|---|
| `IInventoryCache.UpdateInventory()` | `InventoryCache.UpdateInventory` / `Niantic_Holoholo_Collections_InventoryCache__UpdateInventory` | `0x88DD138` | `this`; `method` | Tạo `GetHoloholoInventoryProto`, gửi RPC method `4` (`GET_HOLOHOLO_INVENTORY`) qua `dmoe` (`IRpcHandler`), giữ `IReceipt`, trả `IPromise<GetHoloholoInventoryOutProto>`. `.so` cho thấy request mang timestamp hiện tại và response được nối callback. |
| `BuildGetHoloholoInventoryRequest` *(semantic label suy luận)* | `InventoryCache.bqcg` / `Niantic_Holoholo_Collections_InventoryCache__bqcg` | `0x88DDC40` | `this`; `method` | Tạo `GetHoloholoInventoryProto`; ghi `GetLatestTimestamp()` vào `TimestampMillis` (`+0x10`). `ItemBeenSeen` (`+0x18`) được giữ theo protobuf và là danh sách item đã seen nếu caller/cache populate. Tên gốc trong dump chỉ là `bqcg`. |
| `HandleGetInventoryResponse` *(semantic label suy luận)* | `InventoryCache.bqce` / `Niantic_Holoholo_Collections_InventoryCache__bqce` | `0x88DD3B4` | `a`: `GetHoloholoInventoryOutProto`; `this`; `method` | Nếu response có `InventoryDelta` (`a+0x18`) thì tail-call `NianticInventoryCache.HandleInventoryDelta`; sau đó resolve luồng `Promise`. Không có delta thì không tự dựng item map. |
| `ResolveInventoryPromise` *(semantic label suy luận)* | `InventoryCache.drz.bqcb` / `InventoryCache_drz__bqcb` | `0x88DDCB0` | `a`: `GetHoloholoInventoryOutProto`; closure giữ `<>4__this` và `promise` | Callback Promise: xử lý response rồi resolve/reject promise đã tạo bởi `UpdateInventory`; `InventoryDelta` vẫn là nguồn cập nhật cache. |
| `ExtractKeyFrom(HoloInventoryItemProto gameItem)` | `InventoryCache.ExtractKeyFrom` / `Niantic_Holoholo_Collections_InventoryCache__ExtractKeyFrom` | `0x88DD4EC` | `gameItem`: record inventory protobuf; `this`; `method` | Tạo `HoloInventoryKeyProto`, chọn oneof case `Item`, gán key từ `gameItem.Item.Item`. Đây là key `Item` để dictionary generic index theo item id. |
| `NianticInventoryCache.GetCurrentItems()` | generic `NianticInventoryCache<object,object>...GetCurrentItems` | `0x64646EC` | `this`; `method` | Trả collection các `ClientInventoryItem<K,V>` trong dictionary cache; mỗi entry có server state và prediction state. |
| `NianticInventoryCache.GetItem(K key)` | generic `...GetItem` | `0x646470C` | `key`: `HoloInventoryKeyProto`; `this`; `method` | Lookup cache theo key; kết quả là `ClientInventoryItem`, không trực tiếp là `ItemProto`. Cần gọi/đọc model hiện hành để phân biệt server và prediction. |
| `NianticInventoryCache.HandleInventoryDelta(InventoryDeltaProto inventoryDelta)` | generic `...HandleInventoryDelta` | `0x646533C` | `inventoryDelta`: server delta; `this`; `method` | Parse từng `InventoryItemProto`, giải mã key/value, cập nhật timestamp và dictionary; tạo `ItemUpdate` rồi phát `OnInventoryUpdate` hoặc full-update event. Đây là nơi phải giữ nguyên semantics delta/prediction. |
| `NianticInventoryCache.GetLatestTimestamp()` | generic `...GetLatestTimestamp` | `0x6465334` | `this`; `method` | Đọc timestamp cache để đưa vào request kế tiếp, tránh yêu cầu lại toàn bộ state một cách mù quáng. |

`GetCurrentItems()` và `GetItem()` trả lớp cache generic, còn `IItemBag.AllItems`
trả các `ItemProto` đã được `ItemBagImpl` materialize vào `cwcq`. Nếu mục tiêu là
auto-discard, đường đọc ổn định hơn là `ItemBagImpl.GetItem/GetItemCount` sau khi
`InventoryCache` đã được inject và event wiring hoàn tất.

#### 3. Helper obfuscated nối cache với `ItemBagImpl`

| Tên gốc trong artifact | Semantic label đề xuất | RVA | Parameters | Mức chắc chắn và logic |
|---|---|---:|---|---|
| `ItemBagImpl.yuk` | `WireInventoryCacheEvents` | `0x8163CBC` | `this`; `method` | **Suy luận mạnh**: `.so` khởi tạo generic/event metadata rồi đăng ký callback vào `InventoryCache` (`cwch` ở `+0x28`), phù hợp với hai callback `yud`/`yuj`. |
| `ItemBagImpl.yud` | `OnFullInventoryUpdate/RebuildItemDictionary` | `0x81691C8` | `a`: sender object; `b`: `FullInventoryUpdateEventArgs<HoloInventoryKeyProto,HoloInventoryItemProto>`; `this`; `method` | **Suy luận từ signature/fields**: nhận collection full items, rebuild/refresh `cwcq`, xử lý expiration và phát `OnItemsUpdated`. Cần thiết sau full refresh; chưa nên gọi trực tiếp từ native module. |
| `ItemBagImpl.yuj` | `OnInventoryUpdate/ApplyItemUpdates` | `0x816A6AC` | `a`: sender object; `b`: `InventoryUpdateEventArgs<...>`; `this`; `method` | **Suy luận từ signature/graph**: áp dụng item update/delete vào `cwcq`, cập nhật listeners và schedule expiration cho item thay đổi. |
| `ItemBagImpl.yue` | `UpdateItemProtoAndExpiration` | `0x8169B4C` | `a`: `ItemProto`; `this`; `method` | **Suy luận có bằng chứng field**: đọc `ItemProto.Item/Count`, dictionary `cwcq`, expiration fields và scheduler `cwcp`; likely update item map plus expiration bookkeeping. |
| `ItemBagImpl.yuh` | `IsExpirationConsolationEligible` | `0x816A168` | `a`: nullable `ItemProto`; `this`; `method` | **`.so` xác nhận phần lớn predicate**: null thì false; `ClaimableExpirationConsolationCount > 0` thì true; nếu không thì kiểm tra expiration/settings qua `IGameMasterData`, sau đó count/expiration eligibility. Đây không phải guard recycle chính. |
| `ItemBagImpl.yua` | `CommitSuccessfulRecyclePrediction` | `0x8164770` | `a`: predicted `HoloInventoryItemProto`; `b`: `ICollection<Item>` expiring item copy; `this`; `method` | **Call-site xác nhận, body semantic suy luận**: được gọi chỉ sau result `Success`; hoàn tất local update và xử lý danh sách item hết hạn. Không nên dùng thay cho server delta nếu chưa có calibration. |

Các label đề xuất ở bảng trên không phải tên gốc. Khi tạo binding nên lưu cả hai:
`native_name = yud/yue/yuj/...` và `semantic_role = ...`, để không nhầm semantic label
là symbol ổn định qua version.

### Function discard/recycle item

#### 1. Entry point và request

| Tên semantic/original | Tên metadata/native | RVA | Parameters | Logic `.so` |
|---|---|---:|---|---|
| `IItemBag.RecycleItem(...)` | abstract interface, không có RVA riêng | `-1` | `itemData`, `numToRecycle`, `expiringItemsCopy` | Contract của game; implementation thật là `ItemBagImpl.RecycleItem`. |
| `ItemBagImpl.RecycleItem(...)` | `Niantic_Holoholo_Internal_ItemBagImpl__RecycleItem` | `0x81689A8` | `this`: `ItemBagImpl`; `itemData`: `ItemInventoryItemWidget.ItemData*`; `numToRecycle`: `int`; `expiringItemsCopy`: `ISet<Item>*`; `method`: `MethodInfo*` | Đây là function chính có thể dùng cho discard. Xem pseudocode đầy đủ bên dưới. |
| `RpcHandlerExt.Send<TIn,TOut>` | `Niantic_Holoholo_Rpc_RpcHandlerExt__Send<object,object>` | `0x53BED0C` | `rpcHandler`: `IRpcHandler`; `method`: int enum value `137`; `inProto`: request protobuf; `methodInfo` | Serialize/send RPC, trả `IPromise<RecycleItemOutProto>`. Đây là transport helper, không tự quyết định item/count. |
| `Method.RecycleInventoryItem` | original enum `RECYCLE_INVENTORY_ITEM` | `137` | Không có object parameter; được truyền vào `Send` | Server route dành cho discard inventory. `Method.GetHoloholoInventory=4` là route refresh đọc inventory. |
| `RecycleItemProto::.ctor()` | `Holoholo_Rpc_RecycleItemProto___ctor` | `0x919AC50` | `this`; `method` | Khởi tạo request protobuf; caller ghi `Item` và `Count`. |

`ItemInventoryItemWidget.ItemData` có các parameter/field nghiệp vụ sau:

| Parameter/field | Type | Mô tả |
|---|---|---|
| `itemData.item` | `Item` | Item enum thực sự gửi lên server; `.so` đọc từ `itemData + 0x10`. |
| `itemData.type` | `HoloItemType` | Loại UI/game item; không thấy được dùng làm route trong function chính. |
| `itemData.category` | `HoloItemCategory` | Nhóm item dùng cho UI/filter. |
| `itemData.count` | `int` | Count UI/model tại thời điểm tạo `ItemData`; function chính vẫn lookup count mới trong `cwcq`. |
| `itemData.usable` | `bool` | Có thể dùng item hay không. |
| `itemData.recyclable` | `bool` | Có thể recycle hay không; caller nên guard. `.so` của `RecycleItem` không đủ bằng chứng để nói nó tự reject mọi item không recyclable. |
| `itemData.displayCount` | `bool` | UI có hiển thị count hay không. |
| `itemData.itemExpirationData` | `ExpirationData*` | Expiration context; `ExpireTime` được copy sang prediction nếu object tồn tại. |
| `numToRecycle` | `int` | Số lượng muốn discard. `.so` tính `remaining = currentCount - numToRecycle` và fail path nếu remaining âm. Nên enforce `> 0` ở caller vì không thấy guard zero trong path đã đọc. |
| `expiringItemsCopy` | `ISet<Item>` | Snapshot item đang theo dõi expiration; giữ trong closure callback và truyền vào `yua` khi success. Không được nhầm nó là danh sách item request gửi server. |

Pseudocode gần với `.so`:

```text
RecycleItem(itemData, numToRecycle, expiringItemsCopy):
    item = itemData.item
    current = cwcq.TryGetValue(item)
    if current is missing:
        fail/return rejected promise path

    remaining = current.Count - numToRecycle
    if remaining < 0:
        fail/return rejected promise path

    log ItemDeletedTelemetry(ItemId=item, Quantity=numToRecycle)

    prediction = new HoloInventoryItemProto()
    predictedItem = new ItemProto()
    predictedItem.Item = item
    predictedItem.Count = remaining
    if itemData.itemExpirationData != null:
        predictedItem.UnconvertedLocalExpirationTimeMs =
            itemData.itemExpirationData.ExpireTime
    prediction.Item = predictedItem

    cwch.AddPredictedUpdates([prediction])

    request = new RecycleItemProto()
    request.Item = item
    request.Count = numToRecycle
    promise = RpcHandlerExt.Send(cwcj, 137, request)

    promise.Then(wi.ytr(prediction, expiringItemsCopy))
           .Catch(wi.yts)
    return promise
```

Điểm `.so` xác nhận trực tiếp: field offset `cwcq=0x70`, `ItemProto.Count=0x14`,
`ItemData.item=0x10`, prediction proto được tạo trước khi `AddPredictedUpdates`,
request count là `numToRecycle`, method integer là `137`, và Promise callback được
gắn sau `Send`.

#### 2. Result callback, commit và rollback

| Tên gốc | Tên native | RVA | Parameters | Logic |
|---|---|---:|---|---|
| `ItemBagImpl.wi.ytr` | `ItemBagImpl_wi__ytr` | `0x816C6A0` | `a`: `RecycleItemOutProto`; closure giữ `<>4__this`, `prediction`, `expiringItemsCopy`; `method` | Đọc `a.Result`. Nếu `Success=1`, gọi `ItemBagImpl.yua(prediction, expiringItemsCopy)`. Nếu result khác success, tạo array `[prediction]` rồi gọi `InventoryCache.RollbackPredictedUpdates`. Có breadcrumb log result. |
| `ItemBagImpl.wi.yts` | `ItemBagImpl_wi__yts` | `0x816C808` | `a`: error string; closure; `method` | Promise rejection handler; log/breadcrumb lỗi. `.so` không cho thấy nó tự sửa inventory hoặc retry. |
| `NianticInventoryCache.AddPredictedUpdates(V[] predictedUpdates)` | generic `...AddPredictedUpdates` | `0x646473C` | `predictedUpdates`: array `HoloInventoryItemProto[]`; `this`; `method` | Gắn client prediction vào cache theo key. Sau call, các consumer đọc effective item có thể thấy count đã giảm trước server response. |
| `NianticInventoryCache.RollbackPredictedUpdates(V[] gameItems)` | generic `...RollbackPredictedUpdates` | `0x64650EC` | `gameItems`: array prediction cần rollback; `this`; `method` | Xóa/khôi phục prediction khi server trả lỗi result. Không được bỏ qua nếu request Promise đã trả non-success. |
| `ItemBagImpl.yua` | `Niantic_Holoholo_Internal_ItemBagImpl__yua` | `0x8164770` | `a`: prediction; `b`: expiring item collection; `this`; `method` | Success-side local bookkeeping; sau đó server delta vẫn là nguồn reconcile cuối cùng. |
| `ItemDeletedTelemetry::.ctor` | `Holoholo_Rpc_Telemetry_ItemDeletedTelemetry___ctor` | `0x92D0594` | telemetry object; `ItemId`; `Quantity`; `method` | Tạo telemetry trước khi gửi recycle; không phải inventory mutation. |

`RecycleItemOutProto.NewCount` tồn tại trong metadata nhưng callback `ytr` mà `.so`
đã đọc quyết định bằng `Result` rồi gọi prediction helper; không có bằng chứng trong
path này rằng `NewCount` được dùng thay cho delta server. Vì vậy không nên lấy
`NewCount` làm nguồn duy nhất để cập nhật local cache.

### Mapping tên gốc/tên mới nên dùng khi bind

| Semantic role | Tên gốc public hoặc tên decompiler | Native generated name | Binding stability |
|---|---|---|---|
| Read all item protos | `ItemBagImpl.get_AllItems` | `Niantic_Holoholo_Internal_ItemBagImpl__get_AllItems` | Cao trong đúng build; vẫn phải guard version/ABI. |
| Read one count | `ItemBagImpl.GetItemCount` | `Niantic_Holoholo_Internal_ItemBagImpl__GetItemCount` | Cao trong đúng build. |
| Read one proto | `ItemBagImpl.GetItem` | `Niantic_Holoholo_Internal_ItemBagImpl__GetItem` | Cao trong đúng build. |
| Refresh server delta | `InventoryCache.UpdateInventory` | `Niantic_Holoholo_Collections_InventoryCache__UpdateInventory` | Cao trong đúng build; preferred entry point. |
| Apply full cache event | `ItemBagImpl.yud` | `Niantic_Holoholo_Internal_ItemBagImpl__yud` | Thấp qua version; private/obfuscated. |
| Apply delta cache event | `ItemBagImpl.yuj` | `Niantic_Holoholo_Internal_ItemBagImpl__yuj` | Thấp qua version; private/obfuscated. |
| Recycle item | `ItemBagImpl.RecycleItem` | `Niantic_Holoholo_Internal_ItemBagImpl__RecycleItem` | Cao trong đúng build; public semantic name. |
| Recycle success | `ItemBagImpl.wi.ytr` | `Niantic_Holoholo_Internal_ItemBagImpl_wi__ytr` | Thấp qua version; compiler-generated closure. |
| Recycle failure | `ItemBagImpl.wi.yts` | `Niantic_Holoholo_Internal_ItemBagImpl_wi__yts` | Thấp qua version; compiler-generated closure. |

### Các điểm cần tránh khi triển khai

1. Không bind chỉ vào `cwcq` rồi bỏ qua `InventoryCache` event/prediction. `cwcq` là
   map phụ của `ItemBagImpl`, không phải wire protocol cache độc lập.
2. Không tự gửi method `137` trước khi xác nhận `ItemData.item`, count hiện tại,
   `recyclable`, `numToRecycle > 0`, và không có request discard đang pending.
3. Không coi `ItemData.count` là count authoritative; `.so` của `RecycleItem` lookup
   `cwcq` để tính remaining.
4. Không rollback sau mọi Promise rejection bằng cách tự trừ thêm lần nữa. Prediction
   đã được thêm trước request; failure path phải gọi đúng `RollbackPredictedUpdates`.
5. Không recycle incubator hoặc item có server policy cấm; response có
   `ErrorCannotRecycleIncubators=3`.
6. `GetTotalItems` có filter `ItemSettingsProto.IgnoreInventorySpace`; không dùng nó
   để suy ra inventory capacity hoặc số item có thể discard. `ItemProto.IgnoreInventoryCount`
   và `ItemSettingsProto.IgnoreInventorySpace` là hai cờ khác nhau.
7. Private RVA phải guard exact package/version/ABI/build. Các tên `yud`, `yuj`, `yua`
   có thể đổi dù function role nhìn giống nhau.
8. Việc gọi `RecycleItem` cần Unity/game main thread và object lifetime hợp lệ; bản
   reverse này chưa phải bằng chứng device calibration cho mọi lifecycle.

### Acceptance criteria bổ sung cho reverse/binding

Các tiêu chí dưới đây được suy ra từ binary; BA vẫn cần xác nhận policy discard:

1. Với một item hợp lệ, reader lấy được `ItemProto.Item` và `ItemProto.Count` qua
   `GetItem`/`GetItemCount`, và kết quả khớp effective cache sau full/delta update.
2. `UpdateInventory` dùng route `GET_HOLOHOLO_INVENTORY=4`, giữ timestamp và đi qua
   `HandleInventoryDelta`, không parse response bằng shortcut riêng.
3. Discard gửi `RECYCLE_INVENTORY_ITEM=137` với `RecycleItemProto.Item=item` và
   `RecycleItemProto.Count=numToRecycle`.
4. Không gửi discard khi `numToRecycle <= 0`, item thiếu trong cache, hoặc
   `currentCount < numToRecycle`; local `.so` chỉ chứng minh guard remaining âm,
   các guard còn lại phải nằm ở caller.
5. Trước response, prediction count là `currentCount - numToRecycle`; khi result
   không phải `Success`, prediction được rollback; khi success, callback `ytr` gọi
   `yua` và chờ server delta reconcile.
6. `ErrorNotEnoughCopies` và `ErrorCannotRecycleIncubators` kết thúc request mà không
   retry loop; log phải phân biệt Promise rejection với response non-success.
7. Binding chỉ được enable sau guard đúng version `0.427.0`, arm64-v8a, method RVA,
   object layout và main-thread lifecycle; nếu guard fail thì fail closed.

### Kết luận reverse

Function đáng dùng nhất cho inventory là `ItemBagImpl.GetItemCount`/`GetItem` hoặc
`get_AllItems`, nhưng chúng chỉ đáng tin sau khi `InventoryCache` đã được khởi tạo và
đồng bộ. Function discard thực sự là `ItemBagImpl.RecycleItem` tại `0x81689A8`.
Nó đã có đủ logic client cần: lấy count hiện tại, tạo prediction, gửi RPC `137`, xử lý
success/rollback và giữ context expiration. Phần còn thiếu trước khi bật runtime là
calibration lifecycle/main-thread và guard chính xác trên thiết bị; reverse không tự
chứng minh rằng việc gọi trực tiếp từ mọi thời điểm của game là an toàn.
