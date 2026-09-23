# Brainstorm: inventory và discard từ reverse Pokémon GO 0.427.0

**Loại:** `feature` — phân tích luồng inventory và discard.
**Ngày phân tích đầu tiên:** 2026-09-23.
**Phạm vi game/build/ABI:** reverse 0.427.0, ELF AArch64 / arm64-v8a, BuildID `84ed21009899c84abca01e0b8c6f4b0b054f264e`; package/version code mục tiêu theo dự án, chưa kiểm lại APK hoặc thiết bị (chi tiết bước 2).

## Bước 1 — Yêu cầu và phạm vi

- Yêu cầu: tạo phân tích mới theo bản “Reverse analysis: inventory và discard trong Pokémon GO 0.427.0” người dùng gửi. Tạo tài liệu riêng theo chỉ dẫn rõ của người dùng, không viết vào issue cũ.
- Nguồn đầu vào: `/Users/admin/.codex/attachments/c97b6cff-2add-434b-874f-528aa38239b2/pasted-text.txt:1` (gọi tắt `INPUT`). Nhãn “Exact .so” trong đầu vào là kết quả được cung cấp, chưa mặc nhiên là kiểm chứng độc lập của lượt này.
- Hành vi cần đánh giá: native đọc inventory đã đồng bộ, xác định item vượt giới hạn người dùng, gọi luồng recycle của game, theo dõi kết quả và reconcile trước khi tiếp tục. Kotlin giữ cấu hình/UI theo `AGENTS.md`.
- Trigger nghiệp vụ: auto-discard bật và item vượt giới hạn. Trigger phân tích: bằng chứng về cache, prediction, rollback và expiration trong `INPUT`.
- Hiện trạng framework: chưa đối chiếu trong bước này. Không coi mô tả Kotlin planner trong issue lịch sử là source hiện hành.
- Phạm vi: kiểm chứng symbol/build; mô tả function, param và mức chắc chắn của hành vi; đối chiếu source tích hợp; đề xuất hướng xử lý và tiêu chí xác minh. Chỉ đọc, phân tích và ghi tài liệu.
- Điều kiện hoàn tất: đủ sáu bước có dẫn chứng `path:line`, phân biệt metadata, binary, suy luận và runtime; nêu khoảng trống trước khi bật binding.
- Giả định: đánh giá chính sách discard phần vượt limit có sẵn, không tự bổ sung chính sách item mới. Không triển khai hoặc gọi thử mutation trong lượt phân tích.
- Câu hỏi: owner/lifetime của `IItemBag` và cache; nguồn `ItemData`/`expiringItemsCopy`; nguồn freshness; cách phân biệt prediction với server success; lỗi transport/timeout/config reset có mở lại action chưa biết kết quả hay không.
- Template: `.agents/skills/brainstorm/references/feature.md`. Map-tap/camera/mock-location không áp dụng.

**Kết luận bước 1:** tạo phân tích riêng từ đầu vào, kiểm chứng reverse trước khi đọc điểm tích hợp framework.

## Bước 2 — Bằng chứng từ reverse

### Phạm vi và quy ước nguồn

- Đọc curated extract trước: tìm `IItemBag`, `ItemBagImpl`, `InventoryCache`, `RecycleItem`, `ItemData` trong `reverse/pogo-0.427.0/classes/`; chỉ thấy field `MapPokemon.itemBag` tại `classes/MapPokemon.cs:54`. Extract chưa đủ nên tra archive gốc ở thư mục cha. Thực tế archive nằm tại `reverse/pogo-0.427.0/`, không ở `classes/`.
- `D:L` bên dưới là dòng **nội dung giải nén qua pipe** của `reverse/pogo-0.427.0/dump.cs.gz`; không tạo dump text mới. `S:L` là cách đánh số tương tự cho `script.json.gz`. `BIN@RVA` là `reverse/pogo-0.427.0/native/libil2cpp.so` tại địa chỉ chỉ định.
- Version `0.427.0`: ghi trong `reverse/pogo-0.427.0/OBSERVATION_RESEARCH.md:17`. Package mục tiêu `com.nianticlabs.pokemongo`, version code `2026082702`: theo quy định dự án/đầu vào, **chưa xác minh độc lập từ APK** trong lượt này vì không thấy thư mục `pogo-apkm/`.
- `file reverse/pogo-0.427.0/native/libil2cpp.so` xác nhận ELF64 AArch64, stripped, BuildID `84ed21009899c84abca01e0b8c6f4b0b054f264e`. Đây là định danh binary đã đọc; chưa chứng minh binary đang chạy trên thiết bị trùng nó.
- `OBSERVATION_RESEARCH.md:24` nói không có `.so`, nhưng file hiện tại có và đọc được. Mô tả đó đã cũ. Quy tắc ownership Kotlin ở dòng 11–13 cũng không dùng làm chuẩn hiện hành; chuẩn là `AGENTS.md`.

### Symbol và dữ liệu đã tìm thấy

| Nguồn | Symbol / dữ liệu | Kết quả xác nhận và giới hạn |
|---|---|---|
| `D:26741`, `D:67998`, `D:68069`, `D:68100`, `D:68124` | `Niantic.Holoholo.IItemBag`, `Niantic.Holoholo.Internal.ItemBagImpl`; `get_AllItems`, `GetItemCount`, `GetItem` | Metadata khai báo read API; không chứng minh freshness. |
| `D:68008–68031` | `cwch: InventoryCache +0x28`, `cwci: IGameMasterData +0x30`, `cwcj: IRpcHandler +0x38`, `cwcq: Dictionary<Item,ItemProto> +0x70` | Có nhiều dependency inject và map phụ; không được coi constructor đơn lẻ là instance sẵn sàng. |
| `D:68169–68170` | `ItemBagImpl.RecycleItem` tại `0x81689A8` | Signature khớp đầu vào; trả `IPromise<RecycleItemOutProto>`. |
| `D:67912–67928`, `D:68113–68115` | closure `ItemBagImpl.wi`, `ytr`, `yts`, `yua` | Closure giữ owner, prediction và `ISet<Item>`; helper success nhận `ICollection<Item>`, không phải item list để gửi RPC. |
| `D:68175–68179`, `D:68203–68207` | `yud`, `yue`, `yuj`, `yuk` | Tên obfuscated và kiểu event khớp; quan hệ gọi cần binary, không suy từ tên. |
| `D:105339–105381` | `ItemData`, `ExpirationData`, constructor 10 tham số | `recyclable +0x21`, `itemExpirationData +0x38`, `ExpireTime +0x10`. Metadata chưa chứng minh cách UI xác định eligibility. |
| `D:270835–270935` | `IInventoryCache`, `InventoryCache.UpdateInventory`, `bqce`, `bqcg`, `drz.bqcb` | Refresh, event và prediction contract tồn tại. `UpdateInventory` tại `0x88DD138`. |
| `D:20008–20025` | `NianticInventoryCache.ClientInventoryItem<K,V>` | Có `serverState` và nullable `clientPrediction`; offset `0x0` trong generic dump không dùng trực tiếp làm layout concrete. |
| `D:705251–705272`, `D:639328` | `ItemProto` / `ItemSettingsProto` | `IgnoreInventoryCount` và `IgnoreInventorySpace` là hai field khác nhau. `ItemProto` còn có `TimePeriodCounter`, `StringTicketId` ngoài phần đầu vào nêu. |
| `D:836562–836569`, `D:836651–836662`, `D:836693–836700` | recycle request/result | Request `Item/Count`; result `Unset=0`, `Success=1`, `ErrorNotEnoughCopies=2`, `ErrorCannotRecycleIncubators=3`; response có `NewCount`. |
| `D:832313`, `D:561558–561563`, `D:828181`, `D:828243` | refresh/delta/RPC enum | Có timestamp request/delta; method `4` và `137` khớp. |
| `BIN@0x8163D78–0x8163E68` | `GetItemCount` / `GetItem` | Đã đọc disassembly: load dictionary `+0x70`, lookup, trả count `+0x14` hoặc zero; getter trả pointer lookup. Null dictionary đi nhánh lỗi, khác với không có item. |

**Kết luận bước 2:** cấu trúc inventory/prediction và entry point recycle khớp metadata cục bộ. Đã xác nhận sơ bộ reader bằng binary; bước 3 sẽ kiểm chứng các nhánh recycle/callback quan trọng. Không có bằng chứng runtime về owner, thread, wiring, GC lifetime hoặc calibration. Không gắn nhãn “đã xác nhận implementation” cho mọi mũi tên trong `INPUT` chỉ nhờ dump body rỗng.

## Bước 3 — Function/method, param và hành vi

### Kết quả kiểm chứng binary đợt 1

Ba chi tiết trong `INPUT` cần sửa trước khi dùng làm cơ sở binding:

1. **Đã xác nhận: `wi.yts(string a)` có rollback prediction.** `BIN@0x816C854–0x816C8A4` lấy cache từ owner `+0x28`, tạo array chứa `prediction +0x18`, rồi gọi `0x64650EC`. `S:18323–18324` xác định đích là `NianticInventoryCache<object,object>.RollbackPredictedUpdates`. Vì vậy nhận định “rejected Promise chỉ log lỗi” trong `INPUT:207` không đúng với binary hiện có. Không bổ sung rollback thủ công lần thứ hai.
2. **Đã xác nhận: `itemData.itemExpirationData` không được bỏ qua khi null trong entry point này.** `BIN@0x8168C98` load `itemData +0x38`; `0x8168C9C` nhảy đến chung nhánh lỗi `0x8168E98` nếu null; chỉ khi non-null mới đọc `ExpireTime +0x10` và ghi vào prediction `+0x30`. Pseudocode `if != null: copy` trong `INPUT:180` làm mất nhánh lỗi. Cần tìm cách game tạo context cho item thường, không truyền null chỉ vì item không hết hạn.
3. **Đã xác nhận giới hạn guard:** `BIN@0x8168B28–0x8168B38` coi lookup null như count zero rồi tính hiệu; không có nhánh từ chối “missing item” độc lập ở đoạn này. Guard hiệu âm đi đến nhánh dựng/throw lỗi `0x8168E9C–0x8168ECC`, chưa đủ để gọi là “return rejected Promise”. Caller phải chặn count không dương, item thiếu và bắt exception đồng bộ qua invoke hợp lệ.

Nguồn chữ ký hỗ trợ: `D:68170`, `D:67925`, `D:67928`, `S:60018`. Đã đọc toàn bộ vùng entry point `0x81689A8–0x8168ED4` và hai callback `0x816C6A0–0x816C8F4`; chưa xác minh runtime.

### Quy ước chung cho danh mục

Mọi RVA bên dưới thuộc binary AArch64 có BuildID ở bước 2. Signature C# giữ nguyên khai báo từ dump, bỏ body `{ }`; receiver không tính là param C#. Mọi method là **instance** nếu không ghi khác. Không có `ref`/`out` hoặc giá trị mặc định ngoài chỗ ghi rõ. Khi nguồn không có annotation thì nullability chưa xác minh; yêu cầu guard là đề xuất caller, không phải khẳng định game tự guard. Thread/lifetime thực tế của các owner chưa được thiết bị xác minh; mọi truy cập tích hợp dự kiến qua main-thread bridge, sao chép dữ liệu thuần trước khi rời callback.

### `Niantic.Holoholo.Internal.ItemBagImpl` — read API

| Signature nguyên văn | Param theo thứ tự / nguồn / đơn vị / ràng buộc | Return, hành vi và nguồn |
|---|---|---|
| `public ICollection<ItemProto> get_AllItems()` | Không có param. Owner bag đã inject. | `D:68070`, RVA `0x8163B44`. Binary load `cwcq` rồi gọi helper Values (`S:1377263–1377264`); không thấy tạo snapshot độc lập. Không giữ collection này như bản sao bất biến qua các tick. |
| `public int GetItemCount(Item itemType)` | `itemType: Holoholo.Rpc.Item`, enum id từ cấu hình đã đối chiếu catalog; không phải category. | `D:68101`, RVA `0x8163D78`; binary xác nhận count đơn vị bản sao, zero khi không có proto. Zero không tự chứng minh bag đã sync. Không side effect gameplay trong body đã đọc. |
| `public ItemProto GetItem(Item itemType)` | `itemType` như trên. | `D:68125`, RVA `0x8163E00`; nullable context và binary trả null nếu lookup không có; object thuộc game, không chuyển pointer qua IPC. |
| `public int GetCategoryCount(HoloItemCategory category)` | `category: HoloItemCategory`, enum nhóm từ settings; không dùng thay item id. | `D:68104`, RVA `0x8163E6C`. Signature xác nhận; thuật toán cộng theo category được `INPUT:91` mô tả, chưa kiểm chứng lại body. |
| `public int GetBallCategoryCount(bool excludeMasterBall = False, bool excludeWildBall = False)` | `excludeMasterBall`, rồi `excludeWildBall`: bool, mặc định `False`; cờ loại trừ theo tên/đầu vào. | `D:68118`, RVA `0x816484C`. Chỉ metadata trong lượt này, không dùng tổng này làm count từng item. |
| `public int GetTotalItems()` | Không có param. | `D:68121`, RVA `0x8164C44`. `INPUT:93` mô tả lọc `IgnoreInventorySpace`; chưa đọc lại body. Không có bằng chứng method trả capacity hoặc số slot trống. |
| `public bool HasItem(Item itemType)` | `itemType: Item`, như getter. | `D:68128`, RVA `0x8164FE0`; predicate chi tiết chưa đọc lại. |
| `public bool IsItemNew(Item itemType)` | `itemType: Item`, như getter. | `D:68131`, RVA `0x8165070`; không dùng làm readiness/freshness. Logic Unseen theo `INPUT:95`, chưa đọc lại body. |

Kết quả reader trả trực tiếp, không phải completion của RPC. Điều kiện chung: bag/cwcq còn sống, cùng session, không enumerate khi collection đang bị sửa; exception/null owner là lỗi đọc chứ không phải inventory rỗng. Caller game đầy đủ chưa lập call graph; các getter được xem là điểm đọc, không là tín hiệu server commit.

### `ItemBagImpl.RecycleItem` — metadata và nhánh binary đã xác nhận

- Nguồn: `D:68169–68170`; interface `Niantic.Holoholo.IItemBag` ở `D:26814` đặt tên param đầu là `itemProto`, implementation đặt `itemData`.
- Signature: `public IPromise<RecycleItemOutProto> RecycleItem(ItemInventoryItemWidget.ItemData itemData, int numToRecycle, ISet<Item> expiringItemsCopy)`.
- RVA `0x81689A8`; native signature tại `S:60018`: `Niantic_Promises_IPromise_RecycleItemOutProto__o* Niantic_Holoholo_Internal_ItemBagImpl__RecycleItem (Niantic_Holoholo_Internal_ItemBagImpl_o* __this, Niantic_Holoholo_Inventory_ItemInventoryItemWidget_ItemData_o* itemData, int32_t numToRecycle, System_Collections_Generic_ISet_Item__o* expiringItemsCopy, const MethodInfo* method);`. Metadata native xác nhận receiver/MethodInfo nhưng chưa là calibration ABI trên thiết bị.

| Param | Kiểu / modifier | Ý nghĩa / nguồn giá trị | Đơn vị / ràng buộc | Bằng chứng |
|---|---|---|---|---|
| `itemData` | `ItemInventoryItemWidget.ItemData` | Model của item được chọn; ưu tiên context do game tạo, cùng bag generation. | Non-null; `item +0x10` dùng cho lookup/RPC; `itemExpirationData +0x38` bắt buộc non-null ở path này. Không lấy `count +0x1C` làm count mới nhất. | `D:105367–105376`; `BIN@0x8168AE0–0x8168B38`, `0x8168C98–0x8168CA4` |
| `numToRecycle` | `int` | Phần vượt limit do native tính lại ngay trước gọi. | Số bản sao; đề xuất `0 < numToRecycle <= currentCount`, tính hiệu với kiểm tra miền số. Không thấy guard dương/recyclable trong body. | `BIN@0x8168B34`, `0x8168D50` |
| `expiringItemsCopy` | `ISet<Item>` | Context của inventory service, được closure giữ và đưa vào helper success. | Tập enum id, không phải request list. Nullability/khả năng dùng empty set cần xác minh theo helper/caller; không mặc định null an toàn. | `D:67917`, `D:68115`, `BIN@0x8168AD8`, `0x816C71C` |

- State/side effect đã xác nhận: telemetry xuất hiện trước prediction; tạo proto với `remaining = current - numToRecycle`; copy expiration; gọi `AddPredictedUpdates` tại `0x8168D18`; dựng request `Item/Count` rồi gửi route `137` tại `0x8168D44–0x8168D54`; nối callback sau send.
- Failure đồng bộ trước trả Promise và asynchronous error là hai loại khác nhau. Telemetry “deleted” xuất hiện trước server response, nên không dùng telemetry đó làm success.
- Return: Promise của chuỗi callback; assembly trả kết quả lời gọi cuối ở `0x8168E78`, chưa xác minh đối tượng đó có đồng nhất raw Promise từ `Send` hay không. Cần resolve concrete Promise/semantics `Then/Catch`, không cast mù vì interface cùng generic.
- Hoàn tất nghiệp vụ: cần response tương quan `Result=Success`, callback hậu xử lý và inventory reconcile; pointer Promise non-null hoặc count local giảm chưa đủ. Không tự gửi lại chỉ vì timeout.

### `ItemBagImpl.wi.ytr`, `wi.yts`, `ItemBagImpl.yua`

| Owner / signature nguyên văn / RVA | Param theo thứ tự | Hành vi và tín hiệu hoàn tất |
|---|---|---|
| `ItemBagImpl.wi`: `internal void ytr(RecycleItemOutProto a)`; `0x816C6A0`; `D:67925` | `a`: response game; có `Result/NewCount`, không có default; non-null ở path đã đọc. Receiver closure giữ owner/prediction/set. | `BIN@0x816C708–0x816C77C`: `Result=1` gọi `yua(prediction,set)`; mọi non-success gọi rollback array prediction. Không đọc `NewCount +0x14` trong body này. Return `void`, không có success bool. |
| `ItemBagImpl.wi`: `internal void yts(string a)`; `0x816C808`; `D:67928` | `a`: error string từ Promise; nullable chưa xác minh, không phải result enum. | `BIN@0x816C854–0x816C8CC`: rollback rồi chuyển error tới logging helper; không thấy tự retry. Return `void`; rollback local không chứng minh server chưa xử lý request. |
| `ItemBagImpl`: `private void yua(HoloInventoryItemProto a, ICollection<Item> b)`; `0x8164770`; `D:68115` | `a`: prediction từ closure; `b`: cùng tập expiration chuyển qua interface collection. Nullable context có ở metadata, nhưng điều kiện từng nhánh phải theo binary. | `BIN@0x8164770–0x8164848`: đọc predicted item/count/expiration, gọi helper `yty`/`ytz` khi cần, có nhánh phát `ForceBagUIRefresh +0x18`. Không thấy call “commit server state” trong body này; tên semantic `CommitSuccessfulRecyclePrediction` của đầu vào quá mạnh. Gọi từ success đã xác nhận; ý nghĩa đầy đủ helper chưa xác minh. |

Không gọi trực tiếp ba helper này để mô phỏng success/rollback. Chúng thuộc lifecycle của game; callback phải còn owner và prediction gốc. Không coi reset GC handle phía framework là hủy callback/request phía game.

### `Niantic.Holoholo.Inventory.ItemInventoryItemWidget.ItemData` và expiration

- `D:105381`, RVA `0x829F55C`: `public void .ctor(Item item, HoloItemType type, HoloItemCategory category, int count, bool usable, bool recyclable, bool displayCount, ItemInventoryItemWidget.ItemData.MedicineData medicineData, ItemInventoryItemWidget.ItemData.EquipmentState equipable, ItemInventoryItemWidget.ItemData.ExpirationData expirationData)`.

| Param, đúng thứ tự | Kiểu / nguồn / ý nghĩa và giới hạn |
|---|---|
| `item` | `Item`; id trong bag, không lấy tùy ý từ UI text. |
| `type` | `HoloItemType`; nguồn settings `ItemType` (`D:639292`). |
| `category` | `HoloItemCategory`; nguồn settings `Category` (`D:639294`). |
| `count` | `int`; count hiển thị tại lúc tạo, đơn vị bản sao; recycle đọc lại bag. |
| `usable` | `bool`; eligibility sử dụng, không đồng nghĩa recyclable. Nguồn phải theo game model. |
| `recyclable` | `bool`; eligibility discard; không tự set true để bỏ qua policy game. |
| `displayCount` | `bool`; cờ trình bày, không phải capacity. |
| `medicineData` | `ItemData.MedicineData`; context thuốc; nullability theo loại item chưa xác minh. |
| `equipable` | `ItemData.EquipmentState`; enum `NotEquipable=0`, `Equipped=1`, `UnEquipped=2` (`D:105354`). |
| `expirationData` | `ItemData.ExpirationData`; object có `ExpireTime: long`; phải non-null khi đi vào recycle đã đọc. Epoch/giá trị sentinel cần theo helper game, không tự dùng clock native. |

- Không param nào có default trong constructor này; return `void`, không gửi RPC. `ExpirationData.public void .ctor()` không có param, RVA `0x829F5B4`, `D:105350`; tự construct object chưa đủ để thiết lập expiration đúng.
- **Bằng chứng mới về nguồn model:** `ItemInventoryService.chwm` tạo `ExpirationData` tại `BIN@0x7E7EE9C`, điền field bằng kết quả helper game rồi đưa vào constructor `ItemData` tại `0x7E7EF40` (còn một nhánh dựng dòng category Expiring tại `0x7E7F020`). Không kết luận mọi nhánh/loại item đã được kiểm chứng; cần tránh chọn trùng model của cùng item từ nhiều category.

### `Niantic.Holoholo.UI.Items.ItemInventoryService` — nguồn eligibility/context tìm thêm

- `D:486527`, RVA `0x7E7F918`: `public bool CanItemBeRecycled(ItemSettingsProto itemRec)`. Param duy nhất `itemRec` là settings đúng item từ `IGameMasterData`; non-null, không default. Binary kiểm tra `ItemType +0x14` và `UniqueId +0x10`, có nhánh trả false cho item id `4` (MasterBall). Return bool trực tiếp; không thấy RPC trong body. Đây là predicate game thực có, không phải API tự đặt tên. Cần bind đúng owner/version và kiểm tra itemData flags/context bổ sung.
- `D:486484`, RVA `0x7E7D88C`: `public ISet<Item> get_ExpiringItemsCopy()`. Không param; binary chỉ trả field `eece +0x90`. **Tên Copy không chứng minh getter tạo một bản sao mới mỗi lần gọi.** `chwm` thay field đó bằng set mới tại `0x7E7E784`; cần gắn set với đúng lần tạo model và giữ managed lifetime.
- `D:486503`, RVA `0x7E7F4A8`: `public List<ItemInventoryItemWidget.ItemData> ListSortedPlayerInventory(Func<ValueTuple<Item, HoloItemType>, bool> usableFilter, bool cannotDeleteItems, Func<ValueTuple<Item, HoloItemType>, bool> showUnusableFilter, Func<Item, bool> showUnavailableFilter, Action<Item> itemExpiredCallback)`.

| Param | Ý nghĩa / nguồn / ràng buộc |
|---|---|
| `usableFilter` | Delegate `(Item,HoloItemType) -> bool`; context/filter của inventory game, không suy null là “allow all”. |
| `cannotDeleteItems` | `bool`; ngữ cảnh chặn xóa trong UI; chưa chứng minh chỉ đặt false là item được recycle. |
| `showUnusableFilter` | Delegate `(Item,HoloItemType) -> bool`; chính sách hiển thị item không dùng được; semantics body đầy đủ chưa xác minh. |
| `showUnavailableFilter` | Delegate `Item -> bool`; filter item unavailable; chưa xác minh nullability. |
| `itemExpiredCallback` | `Action<Item>`; callback liên quan expiration, lifetime phải dài đủ với model; không tự dùng delegate native không được GC quản lý. |

- Return list model local, không phải server refresh. `private List<ItemInventoryItemWidget.ItemData> chwm(Func<ValueTuple<Item, HoloItemType>, bool> usableFilter, bool a, Func<ValueTuple<Item, HoloItemType>, bool> showUnusableFilter, Func<Item, bool> b, Action<Item> c)` (`D:486509`, RVA `0x7E7E590`) có cùng thứ tự kiểu; mapping `a/b/c` với public param là suy luận cần xác nhận wrapper. Body trích đọc có allocation và ghi `eece`; không coi đây là getter thuần chỉ đọc hay gọi thử trong khảo sát runtime mặc định read-only.
- Dependency owner có bag `eebw +0x50` và nhiều service inject (`D:486427–486458`). Chưa biết có luôn tồn tại khi inventory UI đóng; đây là ứng viên để xác minh, chưa chốt dependency production.

### `Niantic.Holoholo.Collections.InventoryCache` — refresh đúng cache

| Signature / nguồn / RVA | Param và kết quả | Hành vi có bằng chứng |
|---|---|---|
| `public IPromise<GetHoloholoInventoryOutProto> UpdateInventory()`; `D:270918`; `0x88DD138` | Không param; cache đã inject; trả Promise completion async. | `BIN@0x88DD230–0x88DD26C`: dựng request, copy timestamp cache `+0x30` vào request `+0x10`, gửi method `4`. Nối callback và trả Promise riêng từ closure (`0x88DD398`). Không thấy write `IReceipt +0x48` trong body này; sự tồn tại field không chứng minh UpdateInventory giữ receipt. |
| `private GetHoloholoInventoryProto bqcg()`; `D:270935`; `0x88DDC40` | Không param; trả request proto local. | `BIN@0x88DDC94–0x88DDC9C` copy timestamp như trên. Không có lời gọi trực tiếp `GetLatestTimestamp` trong body đã đọc; có thể compiler inline. Không gọi đây là tên semantic gốc. |
| `private void bqce(GetHoloholoInventoryOutProto a)`; `D:270921`; `0x88DD3B4` | `a`: response non-null; return void. | `BIN@0x88DD3F0–0x88DD408`: nếu delta non-null, tail-call `HandleInventoryDelta`; không tự resolve Promise trong body này. |
| `InventoryCache.drz`: `internal void bqcb(GetHoloholoInventoryOutProto a)`; `D:270891`; `0x88DDCB0` | `a`: response; closure giữ cache và Promise (`D:270882–270883`). | `BIN@0x88DDD10–0x88DDD48`: xử lý delta nếu có rồi chuyển response vào resolve helper. Không thấy direct call `bqce`; logic tương đương được inline. Thread/callback ordering trên thiết bị chưa xác minh. |

### `Niantic.Platform.NianticInventoryCache<K,V>` — giữ nguyên prediction/delta của game

| Signature nguyên văn / nguồn | Param đúng thứ tự / nguồn / kết quả |
|---|---|
| `public ICollection<NianticInventoryCache.ClientInventoryItem<K, V>> GetCurrentItems()`; `D:20163`, generic RVA `0x64646EC` | Không param; trả collection cache item (serverState/prediction), khác `ItemProto`. |
| `public NianticInventoryCache.ClientInventoryItem<K, V> GetItem(K key)`; `D:20174`, `0x646470C` | `key: K`, concrete `HoloInventoryKeyProto` do cache tạo; trả cache item; nullability chưa xác minh body. |
| `public void AddPredictedUpdates(V[] predictedUpdates)`; `D:20193`, `0x646473C` | `predictedUpdates: V[]`, concrete `HoloInventoryItemProto[]` do action game tạo; return void, side effect cache. Call-site từ recycle đã xác nhận; đầy đủ merge/event semantics chưa đọc lại. |
| `public void RollbackPredictedUpdates(V[] gameItems)`; `D:20237`, `0x64650EC` | `gameItems: V[]`, array chứa prediction cùng action; return void. Cả hai callback lỗi đều gọi; không gọi lại từ framework khi game đã quản lý. |
| `internal long GetLatestTimestamp()`; `D:20248`, `0x6465334` | Không param; timestamp cache để request kế tiếp, không đồng nhất với thời gian observer mới lấy snapshot. |
| `protected void HandleInventoryDelta(InventoryDeltaProto inventoryDelta)`; `D:20259`, `0x646533C` | `inventoryDelta`: delta server; return void; caller `bqce`/`bqcb` xác nhận. Chi tiết full-vs-delta, prediction reconciliation cần thêm body/runtime evidence. |
| `InventoryCache`: `protected override HoloInventoryKeyProto ExtractKeyFrom(HoloInventoryItemProto gameItem)`; `D:270928`, `0x88DD4EC` | `gameItem`: proto cache; trả key; nullable context. Không gọi override này để tự dựng cache mới. |

RVA generic `<object,object>` là shared implementation; phải có MethodInfo/rgctx concrete hợp lệ, không gọi thẳng RVA với null generic context. `S:3495376`, `S:3495401` ghi metadata reference cho specialization `HoloInventoryKeyProto,HoloInventoryItemProto` nhưng `MethodAddress=0`, không phải địa chỉ code callable mới.

### Callback nối cache và bag — biết chữ ký, chưa chứng minh toàn bộ call graph

Owner `ItemBagImpl`, nguồn `D:68176`, `D:68179`, `D:68189`, `D:68204`, `D:68207`:

| Signature / RVA | Param / vai trò / giới hạn |
|---|---|
| `private void yuk()` / `0x8163CBC` | Không param; `INPUT` suy luận wire events, lượt này chưa đọc body. |
| `private void yud(object a, NianticInventoryCache.FullInventoryUpdateEventArgs<HoloInventoryKeyProto, HoloInventoryItemProto> b)` / `0x81691C8` | `a`: sender, `b`: full-update event; tên type xác nhận nhưng thuật toán rebuild/event propagation chưa đọc lại. |
| `private void yuj(object a, NianticInventoryCache.InventoryUpdateEventArgs<HoloInventoryKeyProto, HoloInventoryItemProto> b)` / `0x816A6AC` | `a`: sender, `b`: delta event; apply/delete semantics theo INPUT, chưa độc lập xác minh toàn bộ. |
| `private void yue(ItemProto a)` / `0x8169B4C` | `a`: item proto; update/expiration role là suy luận từ INPUT. |
| `private bool yuh(ItemProto a)` / `0x816A168` | `a`: nullable context; predicate expiration/consolation theo INPUT, không dùng thay guard recyclable. |

Các method trên không có default; params do game event/cache tạo, không phải IPC. Return void trừ `yuh`; không phải completion request. Thread/lifetime và các side effect chưa đọc lại phải xác minh bằng wiring/caller và quan sát runtime. `RpcHandlerExt.Send`/constructor proto/telemetry là callee nội bộ, không chọn làm API tích hợp trực tiếp; route và request layout đã kiểm chứng tại call-site recycle/refresh.

**Kết luận bước 3:** giữ `GetItem/GetItemCount` và `RecycleItem` làm ứng viên entry point; đã sửa nhận định về rejection rollback, expiration null và success helper. Tìm thêm `CanItemBeRecycled` và `ExpiringItemsCopy` làm nguồn game có thật. Chưa có bằng chứng cho phép dựng `ItemData` giả, coi `AllItems` bất biến hoặc bật binding chỉ từ metadata.

## Bước 4 — Luồng hành vi và điểm tích hợp

### Đối chiếu source đợt 1 — đã có sai khác binding cụ thể

- Framework đã có native auto-discard: `zygisk/jni/modules/discard/module.inc:26–69` tự đọc inventory, lấy limit, tính amount và queue main-thread action. Phân tích này không cần đề xuất chuyển vòng discard từ Kotlin lần nữa.
- **Sai khác arity đã xác nhận:** game `ListSortedPlayerInventory` có **6 param** (`D:486503`). `zygisk/jni/modules/discard/item_inventory_lookup.inc:359` resolve **5 param**, và `:363–369` tạo array **5 argument**. Đây là mismatch source–reverse; nếu resolve đúng metadata này, lookup 5 param không tìm thấy overload đã xác nhận. Nếu sửa mỗi resolver nhưng không sửa array, runtime invoke vẫn thiếu param thứ sáu. Tác động thực tế trên thiết bị chưa chạy lại để xác nhận.
- Factory hiện tại đã dùng model game thật (`item_data_factory.inc:20–46`), không còn đường tự dựng ItemData từ offset. `recycle_item_action.inc:70–75` kiểm class/id/count/recyclable; **chưa có preflight `itemExpirationData`** trước invoke mặc dù binary yêu cầu non-null.
- `promise_observer.inc:84–92` nhận errorCalled thì hoàn tất và bỏ blocked; `:128–143` cũng bỏ blocked ở mọi result. `module.inc:44–69` không lưu suppression/backoff theo item/result. Vì vậy nếu lỗi lặp và count vẫn vượt limit, cadence sau có thể tạo request lại; chưa có bảo đảm “ErrorNotEnoughCopies/ErrorCannotRecycleIncubators không retry loop”.
- `execute.inc:104–111` reset cả pending/blocked và free handle; được gọi khi enable/disable (`module.inc:17–23`) và apply config mới (`config.inc:134`). Reset bookkeeping không hủy Promise/RPC game đã gửi. Cần xem thêm đường desired-state và main-thread dispatch trước khi chốt rủi ro cạnh tranh.

### Luồng hiện hành và mức bằng chứng

Quy ước đường dẫn phần framework: `DISCARD/` = `zygisk/jni/modules/discard/`; `RT/` = `zygisk/jni/shared/runtime/`; `APP/` = `app/src/main/java/dev/pogoroot/automation/`. Các dẫn chứng ngắn ở đợt 1 đều thuộc `DISCARD/`.

1. **Đã xác nhận source framework:** `APP/service/RuntimeUiAutomationFacade.kt:121–145` đọc config repository, map desired snapshot rồi gửi client; `APP/config/RuntimeDesiredStateMapper.kt:18–19` mang `autoDiscard/discardLimits`. `RT/control/runtime_desired_state_reconcile.inc:42–58` apply mirror native. Kotlin không tính candidate discard trong đường này. Persistence vẫn do `APP/config/AutomationConfig.kt:105–106,158–159` và owner ở `docs/ARCHITECTURE.md` quản lý.
2. **Đã xác nhận source framework:** `DiscardModule::available` kiểm exact build/read/discard flags (`DISCARD/module.inc:9–15`); observer cadence 300 ticks, nominal khoảng 30 giây; có thể lâu hơn nếu tick bị chặn. `read_runtime_inventory` hợp nhất curated ids và config ids, trả false nếu bất kỳ read fail (`DISCARD/inventory_reader.inc:17–77`).
3. **Đã xác nhận:** reader đọc `GetItemCount` nhiều lần trên attached observer thread; thời điểm poll mới không chứng minh server cache mới. Không có timestamp/revision inventory hoặc cờ prediction trong snapshot này. Câu “data service nên không cần main thread” ở comment/tài liệu không chứng minh dictionary được phép đọc đồng thời với mutation game. Đã có `request_main_thread_inventory` để dùng lại (`RT/mainthread/runtime_inventory_request.inc:1–55`).
4. **Đã xác nhận:** module chặn nhiều action cạnh tranh rồi chọn item đầu tiên vượt limit (`DISCARD/module.inc:38–69`). Queue chỉ chứa `item_id`, `amount`, `command_id` (`RT/mainthread/runtime_main_thread_actions.inc:203–224`), không mang config revision/owner generation. Main-thread executor đọc lại count nhưng chỉ kiểm `amount <= stack_count` (`DISCARD/recycle_item_action.inc:20–31`), không tính lại excess theo limit mới.
5. **Hệ quả suy luận có ví dụ:** snapshot count=100, limit=50 → queue amount=50. Nếu trước invoke count còn 70, guard hiện tại vẫn cho phép bỏ 50, còn 20, thấp hơn limit 50. Đây là kịch bản source cho phép, chưa tái hiện device. Tương tự config thay trong lúc task đang chờ không được đối chiếu revision trong action này.
6. **Đã xác nhận:** factory resolve inventory service/filter, gọi `ListSortedPlayerInventory`, tìm row theo id, kiểm exact class/id/count/recyclable, lấy expiration set rồi invoke `RecycleItem` bằng `runtime_invoke`. Nhưng resolver chính cũng dùng arity 5 (`RT/probe/runtime_probe_discovery.inc:760–764`), nên cần sửa cả probe, fallback resolver và argument array cùng nhau.
7. **Đã xác nhận game binary:** `RecycleItem` đọc count local → telemetry → prediction → RPC 137 → callback. `ytr` success gọi `yua`, non-success rollback; `yts` rejection cũng rollback. **Chưa xác minh đầy đủ:** callback/delta nào làm bag converge và lúc nào mọi prediction liên quan đã sạch. Không tự vẽ đường `yua → commit serverState`.
8. **Đã xác nhận framework:** Promise được giữ GC handle; poll trên main thread, timeout 15 giây. Đọc `completeCalled/errorCalled/completedValue/errorValue`; kiểm response type và `Result`, `NewCount`. `Result=1` phát success ngay; chưa đợi bước reconcile độc lập (`DISCARD/promise_observer.inc:44–145`, `execute.inc:77–83`). Event này có thể mang nghĩa “server trả SUCCESS”, nhưng không chứng minh snapshot kế tiếp đã authoritative.
9. **Đã xác nhận:** lỗi layout/timeout/target lost chặn thêm action trong state hiện tại. Tuy nhiên reset do config/enable/disable xóa state ấy. Desired path xem thay **revision bất kỳ** là discard changed (`RT/control/runtime_desired_state_reconcile.inc:42–45,88,153`), nên không chỉ thay limit mới reset pending. **Rủi ro suy luận:** bỏ theo dõi request cũ, mở mutation gate và mất correlation trước callback muộn. Cần lifecycle generation, không chỉ `item_id/amount`.

### Khoảng trống còn lại và điều gì đã làm đúng

| Chủ đề | Đã có trong source | Khoảng trống / hậu quả |
|---|---|---|
| Build | `shared/core/runtime_native_prelude.inc:55–59` pin fingerprint chứa đúng BuildID đã đọc; `RT/probe/runtime_probe_discovery.inc:177` kiểm BuildID | APK/version code và metadata hash chưa độc lập kiểm ở lượt này; metadata flag không phải runtime calibration. |
| Readiness | Có owner và exact build; `inventory_read_verified` khi tìm được GetItemCount (`RT/probe/runtime_probe_discovery.inc:338–344`) | Không chứng minh full cache đã sync, dependency/event wiring hoặc collection thread safety. |
| Discard capability | `discard_verified` khi thấy RecycleItem và class param đầu (`RT/probe/runtime_probe_discovery.inc:367–382`) | Chưa bao gồm ListSorted 6 param, filter, expiration field, Promise shape và context readiness. Có thể báo available rồi chỉ preparation fail. |
| Model | Dùng row game thật, không fabricate recyclable; lỗi prepare không gắn nhãn đã invoke | Row lookup chọn match đầu tiên (`DISCARD/item_inventory_lookup.inc:312–331`); model Expiring có thể trùng id. Cần kiểm context/expiration, không chỉ count. |
| Expiration set | Lấy service getter trước (`DISCARD/item_data_factory.inc:108–126`) | Fallback sang supplier `RequiredMoveRerollPromptService.cfrw(false)` rồi empty set (`:129–135`, `hash_set_binding.inc:62–73`). Đúng kiểu `HashSet<Item>` chưa chứng minh đúng nội dung/nguồn expiration; semantics supplier chưa kiểm nên không coi fallback tương đương. |
| Lỗi và retry | Phân biệt preparation fail, invoked unknown, retained Promise, success/non-success | Non-success/rejection không bị suppress; candidate đầu lỗi có thể lặp mỗi cadence và chặn item khác. Local rollback không khẳng định server chưa commit sau network error. |
| Freshness | Re-read count ngay trước dựng row; native xử lý pending | Chưa revalidate config/limit ở invoke; chưa có reconcile phase và server cache epoch. Reader thành công không đủ để bỏ blocked sau lỗi unknown. |
| Capacity | Discard dựa per-item limits nên không cần capacity | Reader đặt `used_slots = capacity = curated_sum` (`DISCARD/inventory_reader.inc:78–82`), không phải capacity thực. Không dùng payload này để kết luận bag đầy/trống. |
| Concurrency | Auto path tránh pending catch/spin/transfer/encounter; main-thread bridge có token, một task mỗi lần | Task serialization chỉ bao phủ lúc invoke, không tự serialize RPC đang chạy. Manual path và lifecycle reset cần cùng một mutation barrier; chưa chứng minh mọi race xảy ra thực tế. |

`docs/architecture/inventory-and-discard.md` có giá trị lịch sử nhưng một số đường dẫn/luồng adapter Kotlin và mô tả dựng synthetic ItemData đã cũ; `docs/discard-failure-and-toast-names.md` khớp hướng row game thật, đồng thời nói rõ chưa hoàn tất live validation. Phân tích này dẫn source hiện hành, không sửa các tài liệu đó.

### Danh mục method framework trực tiếp liên quan

Không có RVA game cho các API sau. Param `ProbeContext &context` là runtime session/IL2CPP/main-thread state hiện hành, không được lưu bền vững. Mọi pointer output bên dưới phải hợp lệ theo caller; `int32_t item_id/amount` dùng enum id và số bản sao, không phải byte count.

| Signature hiện có / nguồn | Param, kết quả, side effect và giới hạn |
|---|---|
| `void DiscardModule::observe(ObserverTickContext &tick)`; `DISCARD/module.inc:26` | `tick` chứa context/binding/tick counter; đọc inventory, chọn/queue một action; không return completion. |
| `bool read_runtime_inventory(const Il2CppApi &api, const RuntimeBinding &binding, pogo_runtime::RuntimeInventoryObservation *observation)`; `DISCARD/inventory_reader.inc:9` | API, binding snapshot, output thuần; true khi các count đọc được; false nếu source/read fail. Không có server freshness token. |
| `int32_t read_runtime_item_count(const Il2CppApi &api, const RuntimeBinding &binding, int32_t item_id)`; `RT/map/runtime_map_forts.inc:72` | Gọi getter, unbox; count >=0 hoặc -1 unknown/error. Không tự refresh. |
| `bool request_main_thread_inventory(ProbeContext &context, pogo_runtime::RuntimeInventoryObservation *observation)`; `RT/mainthread/runtime_inventory_request.inc:1` | Context và output; post task/wait tối đa 5 giây, false nếu busy/fail/timeout. Trả snapshot copied, không chứng minh cache server mới. |
| `void *invoke_inventory_list_sorted(const Il2CppApi &api, void *service, const void *list_sorted, void *filter_delegate, void **exception)`; `DISCARD/item_inventory_lookup.inc:343` | API; owner; MethodInfo đã resolve hoặc fallback; managed filter; output exception. Trả list hoặc null; hiện sai arity/argument count. Không giữ pointer list lâu dài. |
| `void *create_recycle_item_data_on_main_thread(ProbeContext &context, int32_t item_id, int32_t stack_count, void **exception)`; `DISCARD/item_data_factory.inc:4` | Context/id/count mới; exception output. Trả row game; factory xóa exception sau fail và trả null, khiến caller chỉ còn thông báo generic. |
| `void *resolve_recycle_expiring_items_copy(const Il2CppApi &api, const RuntimeBinding &binding, void **exception)`; `DISCARD/item_data_factory.inc:108` | API/binding/exception output; getter → supplier → empty set. Return pointer chưa là bằng chứng semantic context đúng. |
| `MainThreadActionOutcome request_main_thread_start_runtime_discard(ProbeContext &context, int32_t item_id, int32_t amount, const char *command_id)`; `RT/mainthread/runtime_main_thread_actions.inc:291` | Context/id/amount/correlation id; auto truyền null; wrapper queue/wait 5 giây; return invoked/rejected/indeterminate, không phải server result. |
| `MainThreadActionOutcome start_runtime_discard_on_main_thread(ProbeContext &context, int32_t item_id, int32_t amount, const char *command_id)`; `DISCARD/execute.inc:116` | Cùng tham số; đặt pending, gọi recycle, giữ GC handle, poll lần đầu. Không revalidate limit/config revision hiện hành. |
| `void *recycle_runtime_item_on_main_thread(ProbeContext &context, int32_t item_id, int32_t amount, void **exception, bool *invoked, const char **failure)`; `DISCARD/recycle_item_action.inc:2` | Context/id/amount; 3 output: exception, có thể đã gọi mutation, lý do lỗi. Return managed Promise hoặc null; mark invoked trước runtime_invoke nên exception được xử lý thận trọng. |
| `RuntimeDiscardPromisePollResult poll_pending_runtime_discard_on_main_thread(ProbeContext &context)`; `DISCARD/promise_observer.inc:2` | Context; đọc Promise do GC handle giữ, finish/event khi terminal. Deadline dùng elapsed ns, không timestamp server. |
| `void finish_runtime_discard(ProbeContext &context, uint32_t phase, const char *error_code, const char *message, bool blocked)`; `DISCARD/execute.inc:37` | Context; phase protocol; code/text; cờ barrier. Xóa active/free handle, phát event/result; không ghi cache game. Chưa nhận action-generation riêng để chống completion cũ. |
| `void reset_runtime_discard_state(ProbeContext &context)`; `DISCARD/execute.inc:104` | Context; reset toàn bộ pending/blocked/free handle, không hủy game RPC. |
| `fun submitCurrentDesiredState(): Result<DesiredStateReceipt>`; `APP/service/RuntimeUiAutomationFacade.kt:88` | Không param; map config repository, gửi intent. Receipt không chứng minh gameplay success. |

**Kết luận bước 4:** ưu tiên sửa mismatch 6/5 trước; tiếp theo là context/preflight, freshness/recompute amount và lifecycle pending. Luồng native hiện có phù hợp ownership; vấn đề nằm ở binding contract và độ tin cậy của action state, không phải thiếu một vòng Kotlin mới. Chưa chạy thiết bị nên các race/loop được ghi là kịch bản suy ra từ source.

## Bước 5 — Hướng xử lý

### Phương án đề xuất

**Sửa và hoàn thiện luồng native hiện có, tiếp tục dùng public entry point `ItemBagImpl.RecycleItem`; lấy model/context từ game, giữ prediction/rollback do game quản lý.** Chưa bật thêm capability hoặc coi source flag `verified` là calibration. Không cần thay protocol config/UI chỉ để sửa binding 6/5.

| Phương án | Lợi ích / chi phí | Quyết định |
|---|---|---|
| Sửa binding `ItemInventoryService` và tái dùng row/context game | Giữ eligibility, expiration và state machine của game; source đã theo hướng này. Phải xác minh sáu param/filter/lifecycle. | Chọn làm hướng chính. |
| Tự dựng `ItemData` qua constructor 10 param | Có thể bỏ dependency inventory service, nhưng phải tái tạo eligibility/expiration/medicine/equipment và lifetime. Sai null expiration đã là nguy cơ cụ thể. | Chỉ nghiên cứu nếu service thật không dùng được khi UI đóng; chưa đủ bằng chứng để chọn. |
| Gửi RPC 137 trực tiếp / tự ghi `cwcq` | Giảm phụ thuộc UI model nhưng phải tự chịu prediction, rollback, event, expiration và correlation. | Không chọn trong phạm vi này; mất lợi ích entry point game đã có. |
| Chuyển mọi reader sang full `AllItems` | Tăng coverage, nhưng thêm enumeration/lifetime; `Values` không là snapshot bất biến. | Chưa cần để sửa discard: curated ids đã hợp nhất config ids. Sao chép trên main thread nếu cần full inventory về sau. |

### Phần việc cụ thể và nguồn param

1. **Khớp signature đầy đủ trước mọi invoke.** Tại `RT/probe/runtime_probe_discovery.inc` và `DISCARD/item_inventory_lookup.inc`, resolve `ListSortedPlayerInventory` **6 param**, kiểm cả return type, từng kiểu param/instance owner, sau đó truyền array sáu phần tử đúng thứ tự bước 3. `usableFilter` tiếp tục lấy managed delegate đã được resolver hiện có giữ GC; `cannotDeleteItems` theo context recycle; ba delegate còn lại (`showUnusableFilter`, `showUnavailableFilter`, `itemExpiredCallback`) phải xác minh null contract từ game caller/body. Không coi việc thêm một `nullptr` là đủ chứng minh binding hợp lệ. Khả năng prepare model phải phản ánh trong readiness thay vì báo discard available chỉ từ RecycleItem/class.
2. **Ràng buộc row và expiration cùng một lần chuẩn bị.** Tại `item_data_factory.inc`, `item_inventory_lookup.inc`, `recycle_item_action.inc`, lấy row và `get_ExpiringItemsCopy` từ cùng owner/service generation trên cùng main-thread task. Giữ managed references đúng GC qua mọi allocation; kiểm class/id/count/recyclable, non-null `itemExpirationData` đúng class, giá trị expiration theo helper game; nếu có hai row cùng item (category Expiring), chọn bằng ngữ nghĩa đã xác minh, thiếu thì reject. Dùng `CanItemBeRecycled(ItemSettingsProto itemRec)` để đối chiếu eligibility khi settings/owner hợp lệ; không set `recyclable=true` bằng tay.
3. **Bỏ giả định “HashSet đúng type thì context đúng”.** `resolve_recycle_expiring_items_copy` phải fail closed khi context gốc không lấy được. Supplier `RequiredMoveRerollPromptService.cfrw(false)`/empty set chỉ được chấp nhận nếu chứng minh bằng reverse và runtime rằng chúng tương đương context cần thiết cho loại item đó. Với bằng chứng hiện có, không dùng fallback này để vượt lỗi service. Không gọi `yua`, `ytr`, `yts` hay `RollbackPredictedUpdates` trực tiếp từ module để sửa state game.
4. **Đọc và tính lại action tại thời điểm thực thi.** Dùng `request_main_thread_inventory` thay đọc dictionary từ observer khi chưa chứng minh thread safety; giữ cadence thấp. Với auto-discard, main-thread task phải lấy config đang áp dụng, check enabled/revision/session/generation và action barrier, đọc count mới, rồi tính `amount=max(0,currentCount-limit)` bằng miền số an toàn. Snapshot count chỉ gợi ý candidate. Nếu config/generation đổi hoặc freshness không đủ, bỏ candidate và chờ poll mới; không giữ amount cũ như lệnh auto. Manual amount giữ semantics riêng, vẫn kiểm eligibility/count và chung barrier.
5. **Tách kết quả server khỏi trạng thái sẵn sàng cho action kế tiếp.** `promise_observer.inc` có thể phát “server trả SUCCESS” khi `Result=1`, nhưng module chuyển sang đợi reconcile; không tự gán count bằng `NewCount` hay tự sửa dictionary. Nếu cần refresh, bind `InventoryCache.UpdateInventory()` trên cache cùng owner `cwch`, dùng timestamp/cache game và theo dõi Promise, không tự gửi route 4. Rate limit/coalesce refresh; không gọi mỗi tick. Completion refresh cần đối chiếu cache epoch/prediction state, không bắt buộc timestamp phải tăng khi response hợp lệ không có delta và cũng không coi no-delta đủ chứng minh prediction đã sạch.
6. **Xử lý lỗi và tránh starvation.** `ErrorNotEnoughCopies` → chờ refresh tin cậy rồi tính candidate mới, không lặp request cũ; `ErrorCannotRecycleIncubators`/item không recyclable → suppress item trong context đã xác minh và cho phép xét item hợp lệ khác. Unknown result/Unset → không success, chặn/reconcile tùy bằng chứng. Promise rejection → giữ uncertainty cho đến khi xác nhận trạng thái server, dù game đã rollback local. Preparation fail trước invoke không cần barrier “đã gửi”, nhưng phải tránh spam cùng lỗi mỗi cadence. Chính sách retry tối đa/backoff cụ thể là tham số còn cần chọn khi triển khai; mặc định fail closed nếu không có bằng chứng mới.
7. **Giữ pending xuyên thay config trong cùng game session.** Sửa `execute.inc`, `module.inc`, `config.inc`, `RT/control/runtime_desired_state_reconcile.inc`: đổi config hoặc STOP ngăn action mới nhưng không xóa bằng chứng action đã invoke; tiếp tục observe/cleanup pending dưới ownership module native. Tách cancellation task chưa invoke khỏi trạng thái mutation đã gửi. Theo dõi `action_id`, runtime session và owner generation để callback/poll cũ không finish action mới cùng item/amount. Chỉ hủy reference khi an toàn; process/owner mất thì chuyển unknown, không khôi phục pointer từ persistence. Session mới cần full readiness/cache baseline mới, không replay request cũ.
8. **Giữ IPC/UI mỏng.** Dùng desired snapshot, result/event/status hiện có; sửa diagnostic detail để phân biệt prepare/exception/transport/result/reconcile. `APP/` chỉ lưu config và render native state. Nếu cần thêm status field/wire enum, sửa codec native + Kotlin và version/limits cùng nhau; không thêm filesystem queue hay decoder gameplay phía app. `used_slots/capacity=curated_sum` phải được ghi rõ là chưa biết, không dùng làm capacity thật; đây là chỉnh contract riêng nếu muốn hiển thị chính xác, không phải điều kiện phải đọc toàn inventory để sửa discard.

### API nội bộ có thể cần thêm — Đề xuất, chưa tồn tại

Các tên/kiểu sau là thiết kế framework, **không phải symbol game và không có RVA**. Có thể điều chỉnh khi viết plan; ưu tiên sửa API hiện có nếu không cần seam mới.

```cpp
// Đề xuất — chưa tồn tại; modules/discard/, chạy trên main thread.
DiscardPreparationResult prepare_runtime_auto_discard_on_main_thread(
    ProbeContext &context,
    int32_t item_id,
    uint64_t expected_config_revision,
    uint64_t expected_owner_generation,
    PreparedRuntimeDiscard *out);

// Đề xuất — chưa tồn tại; modules/discard/, chỉ xử lý reconcile của action này.
DiscardReconcileResult reconcile_runtime_discard_on_main_thread(
    ProbeContext &context,
    uint64_t action_id);
```

- `item_id` lấy từ candidate native; revision/generation chụp khi queue, kiểm lại ngay lúc chạy; chúng không cấp quyền dùng snapshot cũ. `PreparedRuntimeDiscard` dự kiến giữ amount mới, config/session/generation và managed roots row/set đúng owner, có cleanup rõ. Enum/struct trên cũng chưa tồn tại.
- `action_id` do native tăng trong session; `DiscardReconcileResult` phải phân biệt còn chờ, đã xác minh, không xác định và mất owner. Không dùng `item_id + amount` làm identity duy nhất. Session/generation và deadline lấy từ runtime context; thời gian local dùng monotonic ns, tách khỏi timestamp cache game.
- Chưa đề xuất API mutation mới trong Kotlin. Binding `UpdateInventory`, `CanItemBeRecycled`, expiration/helper theo build nằm sau native adapter/probe boundary; logic limit/state machine ổn định ở `modules/discard/`. `main.cpp` chỉ wiring include, giữ single translation unit và giới hạn 500 dòng/source.

### Guards và tiêu chí dừng

- **Identity/capability:** đúng package/version/ABI/BuildID; concrete method signatures/return type/field layout; GC/invoke/main-thread bridge; row/context/Promise ready. Thiếu một mắt xích thì action unavailable, diagnostic nói đúng dependency thiếu.
- **Freshness/lifetime:** owner cùng session/generation; count/settings/config hiện hành; known cache baseline; không còn uncertainty của mutation trước. `inventory_read_verified=true` hoặc snapshot mới đọc không thay thế điều kiện này.
- **Mutation barrier:** một action discard pending/reconciling/unknown; chung điều kiện chặn catch/spin/transfer/manual đã xác minh. Config update không tự reset barrier; task đã post cần kiểm lại quyền khi callback bắt đầu.
- **Dừng:** mismatch signature 6/5; expiration/context thiếu; stale revision; owner đổi; invoke exception; mất GC target; unknown result; timeout hoặc không chứng minh reconcile. Không fallback RPC/raw memory để tiếp tục.

**Có thể làm ngay khi triển khai được giao:** sửa arity/array đồng bộ, thêm preflight rõ, giữ pending qua config, tính lại excess và phân loại lỗi. **Cần xác minh trước enable:** nullable filter/callback, model/set lifecycle khi UI đóng, concrete Promise chain, cache full/delta/prediction postcondition. Lượt này chỉ ghi đề xuất, không sửa source hay chạy mutation.

## Bước 6 — Tiêu chí chấp nhận và xác minh

### Tiêu chí chấp nhận

Các ID dưới đây là **Đề xuất từ yêu cầu phân tích và hành vi discard hiện có**, không thêm điều kiện duyệt BA. `INPUT` là yêu cầu/evidence đầu vào cần hiệu chỉnh theo binary ở bước 3; không lấy nhận định sai trong INPUT làm expected behavior.

| ID | Nguồn yêu cầu | Điều kiện đầu vào | Hành vi / postcondition | Method liên quan | Cách kiểm chứng |
|---|---|---|---|---|---|
| AC-01 | Chỉ dẫn mới của người dùng | Có issue cũ và thay đổi người dùng | Phân tích nằm ở file mới ngày 2026-09-23; không bổ sung nội dung vào file cũ | Không áp dụng | Kiểm file/diff và đối chiếu phần cũ trước lượt này |
| AC-02 | AGENTS.md; INPUT binding | Khác build/ABI hoặc thiếu dependency | Capability unavailable; không invoke mutation | `DiscardModule::available`, probe | Đối chiếu reverse/metadata và diagnostic trên đúng BlueStacks Air 1 |
| AC-03 | `D:486503`; mismatch bước 4 | Resolve inventory list ở build này | Đúng 6 kiểu param/return/owner và array đủ 6 phần tử; không chọn overload 5 | `invoke_inventory_list_sorted`, `ListSortedPlayerInventory` | Review source/signature, runtime diagnostic typed binding; test filter cũ không chứng minh AC này |
| AC-04 | INPUT reader; cache metadata | Bag chưa sync, owner stale hoặc read fail | Báo unknown/unavailable; không diễn giải là zero hợp lệ và không discard | `GetItemCount`, `read_runtime_inventory` | Quan sát cache baseline/full/delta/lifecycle; test logic hiện có chỉ kiểm phần mock |
| AC-05 | Policy per-item hiện có | count=100, limit=50; trước invoke count=70 | Auto tính amount mới 20; không gửi amount cũ 50 | `observe`, preparation/invoke | Rà đường truyền revision/count; runtime có kiểm soát sau khi guards hợp lệ |
| AC-06 | INPUT positive/count guard | amount<=0, amount>count, item thiếu hoặc protected | Reject trước RecycleItem, không prediction/telemetry mutation | `recycle_runtime_item_on_main_thread`, `CanItemBeRecycled` | Chạy existing preparation test trong phạm vi có; kiểm preflight/read-only cho ca chưa được test phủ |
| AC-07 | Binary expiration correction | Row non-null nhưng thiếu/sai `itemExpirationData`; row/set khác generation | Fail closed trước invoke; không fallback null hoặc set tùy ý | factory, `get_ExpiringItemsCopy`, `RecycleItem` | Layout/context diagnostic và review; không cố truyền null vào game chỉ để tái hiện lỗi |
| AC-08 | INPUT route/count | Item hợp lệ, config hợp lệ, main thread/GC ready | Một RPC 137 cho item và amount đã tính; prediction local trước response không phát success | `RecycleItem`, pending observer | Correlation action id/response, UI count và snapshot; không suy từ telemetry deleted |
| AC-09 | Binary `ytr/yts` | Response non-success hoặc Promise rejection | Game rollback prediction đúng callback; framework không rollback lần hai; rejection giữ uncertainty phù hợp | `ytr`, `yts`, `RollbackPredictedUpdates` | Binary call-site đã đối chiếu; runtime outcome khi điều kiện xảy ra hợp lệ, không cưỡng ép request sai |
| AC-10 | INPUT result policy; bước 4 | Result 2, 3 hoặc Unset lặp; vẫn vượt limit | Phân loại lỗi; không replay mù mỗi cadence; item bị chặn không làm starvation item khác | `poll_pending_runtime_discard_on_main_thread`, coordinator | Review suppression/retry eligibility, quan sát nhiều cadence; chưa có test runtime cho toàn bộ ca |
| AC-11 | Prediction/delta semantics | Result=1 nhưng cache/prediction chưa reconcile | Báo server success đúng nghĩa; chỉ mở action kế sau khi baseline đáng tin; không tự write `NewCount` vào cache | `UpdateInventory`, `HandleInventoryDelta`, reconcile | Đối chiếu response, cache generation/prediction và count; count không bắt buộc bằng before-amount nếu có biến động hợp lệ khác |
| AC-12 | AGENTS identity/freshness | Config revision đổi, disable/enable hoặc owner/session đổi khi pending | Không xóa barrier như thể RPC đã hủy; không action trùng; callback cũ không kết thúc action mới | `reset_runtime_discard_state`, reconcile config, finish | Review action/session/generation; lifecycle scenario sau khi triển khai guards |
| AC-13 | Timeout fail closed | Timeout, mất Promise target, sai result shape hoặc thread task indeterminate | Unknown/blocked; không suy failure chắc chắn, không auto replay sau đổi config | Promise observer/main-thread bridge | Existing checks + runtime lifecycle diagnostic; host compile không chứng minh GC lifetime |
| AC-14 | AGENTS Kotlin/native boundary | Người dùng sửa limit/toggle, reconnect controller | Kotlin persist/gửi desired/render result; native quyết định; không filesystem queue | facade, desired reconcile | Review đường production, bridge/protocol tests sẵn có |
| AC-15 | INPUT count/capacity | Curated snapshot không có capacity thật | Không dùng curated sum hoặc GetTotalItems làm capacity/free slots | inventory reader / consumer | Review contract/consumer; nếu đổi payload thì kiểm hai codec và compatibility |
| AC-16 | Tính năng hoạt động độc lập UI | Game vừa mở, item bag UI chưa mở; relaunch/scene change | Resolve context và discard đúng limit nếu đủ guards; nếu thiếu thì chặn với lý do rõ, không crash/retry vô hạn | list/filter factory, recycle/pending | Scenario chính trên BlueStacks Air 1 sau khi fix; smoke/probe PASS không tự chứng minh AC này |

### Kế hoạch xác minh — chưa chạy trong lượt phân tích

**Kiểm tra host hiện có:** các lệnh dưới lấy từ `.github/workflows/ci.yml:59–78` và `AGENTS.md`. Chỉ chạy test có sẵn, không tạo/sửa test code.

```bash
c++ -std=c++17 -Wall -Wextra -Werror -Izygisk/jni \
  zygisk/tests/runtime_discard_filter_factory_test.cpp -o /tmp/runtime_discard_filter_factory_test
/tmp/runtime_discard_filter_factory_test
c++ -std=c++17 -Wall -Wextra -Werror -Izygisk/jni \
  zygisk/tests/runtime_discard_preparation_test.cpp -o /tmp/runtime_discard_preparation_test
/tmp/runtime_discard_preparation_test
./gradlew :bridge:protocol:test --rerun-tasks
./gradlew test assembleDebug
```

- Test factory hiện kiểm `Delegate.CreateDelegate`/GC retention; test preparation kiểm class/id/count/recyclable/expiring set và invoke exception. Chúng dùng mock, không gọi `ItemInventoryService.ListSortedPlayerInventory` thật và không chứng minh signature sáu tham số, expiration field, reconcile hoặc state sau config reset. Không báo pass các AC ấy chỉ vì hai test này pass.
- Nếu đổi result/event contract, chạy thêm `runtime_automation_event_protocol_test.cpp` đúng command CI và các protocol checks sẵn có. Gradle không build Zygisk; build native phải qua `scripts/build-magisk.sh`.
- Khi triển khai, dùng entry point build/package/install hiện có theo `AGENTS.md`. Override NDK được tài liệu dự án ghi là `ANDROID_NDK="$HOME/Library/Android/sdk/ndk/27.1.12297006" ./scripts/build-magisk.sh`; lần đọc này không thấy thư mục NDK đó, nên phải kiểm môi trường trước, không khẳng định build chạy được và không dựng workflow thay thế.

**Xác minh thiết bị:** chỉ dùng instance **BlueStacks Air 1**, target cấu hình mặc định được `scripts/headless-control.sh:32` ghi là `127.0.0.1:5565`; xác thực target bằng workflow script, không raw ADB hoặc tự chuyển sang instance khác. Chưa chạy lệnh nào dưới đây trong lượt này.

```bash
ANDROID_SERIAL=127.0.0.1:5565 ./scripts/bluestacks-smoke-test.sh
ANDROID_SERIAL=127.0.0.1:5565 ./scripts/headless-control.sh status
ANDROID_SERIAL=127.0.0.1:5565 ./scripts/headless-control.sh diagnostic
ANDROID_SERIAL=127.0.0.1:5565 ./scripts/binding-probe-test.sh
```

- Các script smoke/probe chỉ xác minh nền/lifecycle/probe, không assert sáu tham số hoặc inventory reconciliation. `binding-probe-test.sh` còn launch game; `headless-control.sh status` có thể bootstrap app nếu API chưa ready. Không gọi chúng là thao tác chỉ đọc tuyệt đối.
- Trước mutation scenario: xác minh metadata/build, owner, thread, row/expiration/set, Promise shape, cache baseline; auto-discard vẫn tắt cho tới khi guards đủ. Chỉ kiểm action thực khi đã có phạm vi triển khai/kiểm thử tương ứng và binding hợp lệ. Phân tích này không tự cấp quyền gọi thử.
- Nếu một thao tác kiểm thử thất bại: dùng `./scripts/collect-binding-diagnostics.sh` cho chẩn đoán và `./scripts/logcat-full.sh` để thu log theo AGENTS; chỉ đọc file log liên quan khi thao tác thất bại. Thành công đánh giá bằng structured status/result/UI postcondition, không mở log để kiểm thêm. Script hiện chưa xuất đủ cache/prediction evidence thì ghi khoảng trống cần bổ sung diagnostic, không dùng raw ADB hoặc inline workflow để bypass.
- Lifecycle smoke `./scripts/device-smoke-test.sh` có force-stop/relaunch; chỉ dùng ở giai đoạn kiểm chứng lifecycle được giao. `scripts/lib/emulator.sh` là thư viện source bởi entry point, không phải script thay thế để chạy ad hoc. Không sửa hành vi script trong lượt này.

### Kết luận và câu hỏi còn mở

**Phát hiện chính:** source đang resolve/invoke `ListSortedPlayerInventory` với 5 tham số trong khi reverse xác nhận 6. Hai kết luận trong bản đầu vào cần thay bằng bằng chứng binary: rejection handler `yts` có rollback; `itemExpirationData` null đi vào nhánh lỗi. `yua` có nhánh refresh UI, chưa có bằng chứng là commit server state; `AllItems` không là snapshot bất biến.

**Hướng chọn:** sửa binding/context trong module native có sẵn; giữ game xử lý prediction/rollback, tính amount mới tại invoke, tách server outcome với reconcile và giữ barrier qua config/lifecycle. Không mở thêm vòng orchestration Kotlin, không fabricate context hoặc gọi RPC thay thế để vượt lỗi binding.

**Câu hỏi chưa đóng:**

1. Ba delegate tùy ngữ cảnh của `ListSortedPlayerInventory` chấp nhận null trong những nhánh nào; filter `chvt` hiện dùng có phù hợp mọi item configured không?
2. Inventory service/row/set có lifecycle và ownership chính xác thế nào khi UI đóng, scene/account đổi hoặc khi `chwm` chạy lại? Những pointer hiện giữ đã có managed roots đủ chưa?
3. Promise được trả sau `Then/Catch` có concrete layout/ordering nào; đọc completion fields có bảo đảm game callbacks đã xử lý xong không?
4. Cách quan sát cache baseline, server delta và prediction sạch để mở lại action, đặc biệt response no-delta hoặc network rejection, cần binding/diagnostic nào thêm?
5. Các kịch bản reset/queue stale/retry từ source có biểu hiện thế nào trên thiết bị? Chưa có bằng chứng live của lượt này; không kết luận đã tái hiện.

**Giới hạn:** chỉ đọc source/reverse/binary và viết tài liệu. Không sửa code/test, không build, không chạy emulator, không gọi mutation. Binary đủ để sửa một số nhận định tĩnh, chưa đủ chứng minh binding an toàn ở mọi lifecycle.

**Kiểm tra tài liệu đã thực hiện:** `git diff --check` sạch; kiểm whitespace file mới bằng `git diff --no-index --check` không có báo lỗi (exit 1 do file mới khác `/dev/null`). File cũ đã bỏ đúng phần thêm nhầm, giữ 360 dòng; 281 dòng bản reverse đã có trong file cũ được `cmp` xác nhận trùng byte với attachment. Các thay đổi người dùng có sẵn và thay đổi đồng thời ngoài tài liệu mới được giữ nguyên.
