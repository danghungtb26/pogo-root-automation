# Brainstorm: Auto catch ngầm từ map, báo kết quả và cập nhật map

**Loại:** feature — điều tra lại tính năng hiện có trước khi sửa.
**Ngày:** 2026-09-09.
**Phạm vi lượt này:** đọc source, reverse và mã máy; không triển khai tính năng, không viết UT, không gọi mutation trên thiết bị.

## Phân tích

### 1. Tính năng giải quyết vấn đề gì?

Yêu cầu: Pokémon xuất hiện xung quanh thì tool tự gọi method game để bắt ngầm, không mở màn hình encounter và không cần thao tác quăng bóng; nhận biết đã bắt hoặc chạy mất, báo toast rồi cập nhật map.

Có hai ý nghĩa cần phân biệt để đánh giá đúng khả năng:

- **Không mở encounter UI/animation:** có thể khảo sát luồng bắt trực tiếp từ map. Repo đã có một phần luồng này.
- **Không dùng cả dữ liệu ném bóng hoặc bước encounter nội bộ:** tìm được ứng viên phù hợp hơn là `SoftSfidaCaptureRpc(ulong encounter_id, string spawnpoint_id)`. Chưa chứng minh tài khoản/runtime hiện tại đủ điều kiện sử dụng.

Giả định làm việc: “bg” nghĩa là bắt âm thầm khi game đang chạy và map vẫn hiển thị. Chạy khi Android đưa game xuống nền hoặc tắt màn hình cần kiểm chứng vòng đời riêng; foreground service của controller không chứng minh Unity vẫn thực thi được.

### 2. Ai sử dụng, hiện tại đang phải làm gì?

Người vận hành tool root trên Pokémon GO, mục tiêu kiểm chứng là BlueStacks Air 1. Mong muốn là ở lại map, bật auto catch rồi theo dõi thông báo kết quả thay vì vào từng encounter.

Đây là binding theo bản game `0.427.0`, version code `2026082702`, arm64-v8a. Không suy rộng kết quả reverse này sang bản game hoặc ABI khác chỉ vì ZIP module chứa nhiều ABI.

### 3. Những luồng bắt buộc là gì?

1. Đọc spawn còn hiện diện trong map, có định danh hợp lệ và observation còn mới.
2. Chọn một mục tiêu được route hiện tại hỗ trợ, kiểm tra lifecycle/capability/identity rồi thực hiện một yêu cầu bắt.
3. Giữ nguyên map UI trong suốt thao tác.
4. Theo dõi kết quả bất đồng bộ của đúng yêu cầu; không coi method trả về là bắt thành công.
5. Báo toast đúng kết quả, tối đa một thông báo kết quả cho mỗi attempt.
6. Đồng bộ trạng thái map sau kết quả; chỉ chọn mục tiêu tiếp theo khi map đủ mới.
7. Khi thiếu điều kiện hoặc mất kết quả, hiển thị trạng thái chưa xác định và dừng retry mù.

Các mở rộng chưa bắt buộc: chọn loại bóng, berry, shiny/IV filter, incense/lure/daily spawn, encounter đặc biệt, chạy khi màn hình tắt. Bản đầu nên giới hạn đúng loại spawn đã được xác minh; không quảng bá mọi Pokémon nhìn thấy đều bắt được.

### 4. Những trường hợp biên cần xử lý?

| Tình huống | Hành vi cần có |
|---|---|
| Không có spawn, mới vào game, map chưa tải | Đợi observation hợp lệ; không spam toast hoặc gọi method với target rỗng. |
| Spawn hết hạn, rời vùng nhìn thấy, object bị huỷ | Kiểm tra lại trước invocation; không giữ raw pointer qua chuyển scene mà thiếu quản lý lifetime. |
| Spawn biến mất sau invocation | Chỉ là bằng chứng thay đổi map; không đủ kết luận bắt thành công hoặc chạy mất. |
| Mạng mất hoặc deadline hết sau khi gửi | `INDETERMINATE`; không tự gửi thêm catch cùng target hoặc đổi route để thử lại. |
| Hết bóng, đầy kho, ngoài tầm, feature bị tắt hoặc hết hạn mức | Phân loại lỗi có bằng chứng; không gán tất cả thành `FLED`. |
| Người dùng tự mở encounter, teleport, spin hoặc dùng tính năng bắt tự động khác | Dừng dispatch khi lifecycle/position/target thay đổi; bảo đảm một owner điều khiển mutation và tránh hai vòng auto catch chạy đồng thời. |
| Callback trùng, đến muộn hoặc từ session cũ | Correlate bằng session + command + attempt/target; loại trùng, không gắn kết quả cũ cho con mới. |
| `CATCH_ESCAPE` từ route bắt thường | Thoát bóng, chưa phải chạy mất; xử lý `BREAKOUT`, chỉ retry theo policy có giới hạn sau kết quả xác định. |
| `CATCH_ITEM_REPLACEMENT` hoặc enum chưa hỗ trợ | Giữ raw status và trạng thái đặc biệt/chưa hỗ trợ; không giả thành caught, không retry tự động. |
| Chỉ đọc được một phần danh sách map | Không dùng sự vắng mặt trong snapshot thiếu dữ liệu làm bằng chứng target đã được xử lý. |
| Game bị đưa xuống nền hoặc kill | Không cam kết tiếp tục bắt; tạm ngừng theo lifecycle và không phát lại command cũ khi attach session mới. |

Định danh `ulong` phải giữ nguyên độ chính xác 64-bit. Chuỗi ID không được đi qua số thực. Tên loài trong toast dùng metadata đã có hoặc ID dự phòng; không bịa tên/IV/shiny.

### 5. Các ràng buộc và bằng chứng hiện tại là gì?

#### 5.1. Repo đã có đường direct catch nhưng chưa khép kín

| Vị trí | Điều đã đọc được | Hệ quả |
|---|---|---|
| [AutomationCoordinator.kt](../../../../core/src/main/kotlin/dev/pogoroot/automation/core/automation/AutomationCoordinator.kt) | Khi `OVERWORLD`, `autoCatch && catchAll` tạo `CatchMode.DIRECT_MAP`. Nhánh này không yêu cầu `autoEncounter=false`; nó còn chặn nhánh auto-encounter phía dưới. | Chỉ tắt auto-encounter không giải quyết phần còn thiếu. Mô tả trong tài liệu hiện có hẹp hơn điều kiện thực tế của source. |
| [runtime_direct_map_bindings.inc](../../../../zygisk/jni/runtime_direct_map_bindings.inc) | Resolve `MapPokemon.TryCapture`, hai lớp proto và getter spawn point; từ đó đặt `direct_catch_verified`. | Tìm thấy binding không chứng minh asynchronous outcome hoặc toàn bộ postcondition đã đúng. |
| [runtime_direct_map_actions.inc](../../../../zygisk/jni/runtime_direct_map_actions.inc) | Chỉ nhận exact `WildMapPokemon`; tạo `DirectMapPokeballThrow` với ball type `1`; gọi method và chỉ kiểm tra promise null/non-null. | Promise bị bỏ khỏi luồng theo dõi; chưa biết caught/fled/error. Route này vẫn sử dụng dữ liệu throw nội bộ dù không mở UI. |
| [runtime_action_catch.inc](../../../../zygisk/jni/runtime_action_catch.inc) | `kDirectCatchExecutionEnabled=true`; khi đã gọi trả `INDETERMINATE`, error `direct_catch_outcome_unavailable`. | “Đã gửi” đang đi thẳng vào trạng thái recovery thay vì chờ một kết quả bất đồng bộ thông thường. |
| [AutomationRunner.kt](../../../../core/src/main/kotlin/dev/pogoroot/automation/core/automation/AutomationRunner.kt) | `resolveIndeterminateMapAction` gỡ suspension khi target không còn trong nearby; không tạo `CatchOutcome`. `isValidTransition` không cho đi tiếp từ `INDETERMINATE` bằng result thông thường. | Có thể tiếp tục queue mà vẫn không biết bắt hay chạy. Chỉ thêm callback native rồi gửi `COMPLETED` đến muộn vẫn chưa đủ. |
| [runtime_action_common.inc](../../../../zygisk/jni/runtime_action_common.inc), [runtime_bridge_protocol.inc](../../../../zygisk/jni/runtime_bridge_protocol.inc) | Native result hiện chỉ mang command ID, phase, error, message; parser nội bộ yêu cầu đọc hết payload. Broker chưa serialize catch outcome. | Phải nối cả target → companion → controller; chỉ sửa decoder Kotlin không giải quyết được. |
| [BridgePayloadDecoder.kt](../../../../bridge/protocol/src/main/kotlin/dev/pogoroot/automation/bridge/BridgePayloadDecoder.kt) | Kotlin đã đọc optional `catchOutcome`; core đã có `CAUGHT`, `MISSED`, `BREAKOUT`, `FLED`, `NO_BALL`, `INDETERMINATE`. | Có sẵn domain seam, nhưng chưa có nguồn kết quả native thực. Không copy nguyên số enum của game vào wire enum. |
| [StructuredAutomationController.kt](../../../../app/src/main/java/dev/pogoroot/automation/headless/StructuredAutomationController.kt), [AutomationEvents.kt](../../../../app/src/main/java/dev/pogoroot/automation/headless/AutomationEvents.kt) | Sink toast và event `CAUGHT`/`RAN_AWAY` đã tồn tại. `consumeResult` đưa kết quả vào runner và ghi last action; chưa phát toast caught/fled tại đây. | Cần nối event sau khi kết quả được runner chấp nhận; không toast từ raw callback chưa validate. |

Log đã lưu tại `build/logs/logcat-full-20260909-210154-38120.txt`, dòng 19034, ghi runtime quảng bá `DIRECT_CATCH` cùng `READ_NEARBY` vào 20:54:45 ngày 09-09. Đây là bằng chứng capability từng được công bố, không phải bằng chứng đã bắt thành công. Tài liệu `docs/LIVE_AUTOMATION_READINESS.md` chứa baseline cũ hơn; không dùng baseline đó để kết luận trạng thái thiết bị hiện tại.

#### 5.2. Hai method đáng khảo sát

Đã đọc curated [MapPokemon.cs](../../../../reverse/pogo-0.427.0/classes/MapPokemon.cs) và [WildMapPokemon.cs](../../../../reverse/pogo-0.427.0/classes/WildMapPokemon.cs) trước. Vì curated chưa có `SoftSfida` và proto kết quả, mới đọc `reverse/pogo-0.427.0/dump.cs.gz` bằng luồng giải nén, không tạo dump text lớn.

| Route | Binding theo reverse | Mức bằng chứng |
|---|---|---|
| Bắt thường không qua UI | `MapPokemon.TryCapture(PokeballThrow, ARPlusEncounterValuesProto) -> IPromise<CatchPokemonOutProto>`, RVA `0x7F91ECC` | Có metadata, có caller trong repo và đã đọc mã máy. Chưa chứng minh server chấp nhận mọi wild spawn khi chưa có encounter context. |
| Bắt trực tiếp không truyền throw | `Niantic.Holoholo.Services.SoftSfidaService.SoftSfidaRpcHandler.SoftSfidaCaptureRpc(ulong encounter_id, string spawnpoint_id) -> IPromise<SoftSfidaCaptureOutProto>`, RVA `0x8443E50` | Có metadata và mã máy gửi RPC riêng. Chưa có binding/owner/capability cho route này trong repo, chưa xác minh điều kiện tài khoản. |

**Bằng chứng mã máy:** dùng `llvm-objdump` của NDK đã cài để đọc bản `.so` có sẵn tại `/private/tmp/pogo-disasm.xliRwr/libil2cpp.so`. SHA-256 đã khớp reverse: `f2ad4ed85ac56e0068bb1096a94ee1b63786b5f7b2bdf57bac1a38fa53071603`.

- `TryCapture`: tại `0x7F92230` và `0x7F922B4` đặt method ID `103`; đối chiếu enum là `Method.CatchPokemon`. Hai nhánh gọi `RpcHandlerExt.Send`/`SendReliable`. Có nhánh timeout `5` giây. Vì vậy đây là gửi yêu cầu bắt đến game server, không phải sửa trạng thái caught tại client.
- `SoftSfidaCaptureRpc`: tại `0x8443F30` đặt method ID `833`, đối chiếu là `Method.SoftSfidaCapture`; chuyển vào helper `SoftSfidaRpcHandler.bfbv`/`SendRpcWithLogging`. Signature và đoạn code đã đọc không dùng `PokeballThrow` hoặc mở encounter UI trực tiếp.
- `SoftSfidaRpcHandler.<>c.bfbr` tại `0x844465C` chấp nhận `Result` bằng `1` hoặc `2`.
- Callback generic `<SendRpcWithLogging>b__0` tại `0x6FE3080` gọi `Promise<object>.Complete` khi predicate đúng, còn nhánh sai tạo message rồi gọi `Promise<object>.Error`. Do đó phải theo dõi cả success và error. Structured mã lỗi có thể đã bị helper chuyển thành chuỗi; muốn phân loại đầy đủ phải xác minh seam trước chuyển đổi, không mặc định error callback luôn mang proto.
- `TryCapture` còn dereference `xpAwardService` ở offset `0xE0`, `codeGateService` ở `0xC8`, và dùng scheduler ở nhánh timeout. Guard direct catch hiện chủ yếu kiểm tra bốn dependency khác. Đây là khoảng trống cần audit, chưa phải kết luận nguyên nhân một crash đã tái hiện.

Đã xác định thêm service điều phối `SoftSfidaForegroundService`, với `bexk(IMapPokemon)`/iterator `byp`, callback `byl.beva(SoftSfidaCaptureOutProto)` và nhiều feature gate. Chưa đọc đủ call graph để kết luận gọi riêng RPC sẽ tự xử lý map, loot và state như gọi service đầy đủ.

#### 5.3. Kết quả phải phân loại theo đúng route

**Route `TryCapture`** — `dump.cs.gz` sau giải nén, dòng 625351–625365:

| Status game | Giá trị | Ý nghĩa trong tool |
|---|---:|---|
| `CATCH_ERROR` | 0 | Lỗi bắt; cần thông tin bổ sung, không suy ra chạy mất. |
| `CATCH_SUCCESS` | 1 | `CAUGHT`; proto có `CapturedPokemonId` để đối chiếu nếu hợp lệ. |
| `CATCH_ESCAPE` | 2 | `BREAKOUT`; thoát bóng. |
| `CATCH_FLEE` | 3 | `FLED`; chạy mất. |
| `CATCH_MISSED` | 4 | `MISSED`; attempt trượt. |
| `CATCH_ITEM_REPLACEMENT` | 5 | Kết quả đặc biệt; domain hiện chưa biểu diễn đầy đủ. |

**Route `SoftSfidaCaptureRpc`** — `dump.cs.gz` sau giải nén, dòng 862732–862803:

| Result game | Giá trị | Ý nghĩa trong tool |
|---|---:|---|
| `UNSET` | 0 | Chưa có kết luận hợp lệ. |
| `POKEMON_CAPTURED` | 1 | `CAUGHT`. |
| `POKEMON_FLED` | 2 | `FLED`. |
| `ERROR_FEATURE_DISABLED` | 3 | Route không khả dụng theo điều kiện game. |
| `ERROR_NOT_FOUND` | 4 | Không tìm được mục tiêu. |
| `ERROR_ENCOUNTER_ALREADY_FINISHED` | 5 | Encounter đã kết thúc; không đủ biết caught hay fled của attempt hiện tại. |
| `ERROR_NOT_IN_RANGE` | 6 | Ngoài tầm. |
| `ERROR_NO_MORE_POKEBALLS` | 7 | Hết bóng. |
| `ERROR_POKEMON_INVENTORY_FULL` | 8 | Kho Pokémon đầy. |
| `ERROR_LIMIT_REACHED` | 9 | Chạm hạn mức. |
| `ERROR_DAY_NOT_MATCH` | 10 | Trạng thái ngày không khớp. |
| `ERROR_CAN_NOT_CATCH` | 11 | Mục tiêu không được bắt bằng route này. |

Proto SoftSfida trả `Result`, `DisplayPokedexId`, `PokemonDisplay`, `Loot`, `State`; không có field `CapturedPokemonId` như proto bắt thường. Correlation phải giữ target/command từ thời điểm gửi, không đợi lấy lại từ response.

`ISoftSfidaService` có `IsEnabled`, `IsActive`, `IsInUse`, `CurrentState` và `StartSoftSfida()`. `SoftSfidaStartOutProto` có `CatchLimit`, `SpinLimit` và lỗi feature/state/kho đầy. Đây là bằng chứng route có điều kiện kích hoạt và hạn mức; sự hiện diện trong APK không chứng minh đang dùng được. Không đề xuất sửa flag client để coi như server đã bật tính năng.

#### 5.4. “Load map lại” cần hoàn tất gì?

Tìm được các binding sau trong cùng dump:

| Method | RVA arm64 | Vai trò cần xác minh |
|---|---|---|
| `MapContentHandler.RegisterPokemonCaughtOrFled(ulong)` | `0x7F54EBC` | Ghi nhận encounter đã kết thúc trong quản lý nội dung map. |
| `MapEntityService.RemoveWildPokemon(ulong)` | `0x7F636B4` | Loại wild entity theo encounter ID. |
| `MapContentHandler.ForceRefreshVisibleCells()` | `0x7F53FAC` | Yêu cầu làm mới cell đang hiển thị; không phải bằng chứng refresh đã xong. |
| `MapContentHandler.set_ForceImmediateGetObjects(bool)` | `0x7F53FA4` | Cờ yêu cầu lấy map objects sớm; cần xác minh vòng poll và throttle. |

Đây là các điểm khảo sát, **không phải danh sách gọi liên tiếp vô điều kiện**. Cần lần theo callback mà game vốn dùng sau caught/fled để tránh remove hai lần, bỏ sót tappable, loot hoặc cập nhật inventory. Owner `map_content_handler` đã có trong `RuntimeBinding`, nhưng các binding refresh/result lifecycle còn thiếu.

Sau authoritative result: toast kết quả → yêu cầu/quan sát đồng bộ map qua luồng game → xác nhận snapshot phản ánh cập nhật → mới bắt tiếp. Nếu caught đã xác nhận nhưng refresh thất bại, vẫn giữ kết quả caught và báo lỗi đồng bộ riêng; không catch lại.

Không dùng reload scene, khởi động lại game, teleport hay thao tác màn hình để thay cho refresh. Snapshot có message sequence lớn hơn chỉ chứng minh được đọc/phát muộn hơn; cần kiểm chứng dữ liệu map đã áp dụng kết quả, không chỉ poll lại cache cũ.

#### 5.5. Ràng buộc triển khai

- Giữ domain độc lập với tên obfuscated, RVA và offset; chỉ adapter/native binding biết bản game.
- Mọi managed call, callback, object lifetime phải tuân thủ thread/scope của game. Dump generic `Promise<T>` có nhiều offset `0x0`; không lấy các số đó làm layout runtime thật. Cần exact instantiated class hoặc cơ chế callback đã xác minh và giữ GC reference đúng.
- Capability bắt ngầm chỉ được công bố khi cả invocation, nhận outcome và lifecycle cần thiết đã qua guard; `method != nullptr` là chưa đủ.
- Dùng native async state `ACCEPTED`/`STARTED` khi đã gửi và đang chờ kết quả. Timeout thực sự mới chuyển recovery. Kết quả đến muộn cần một nhánh reconcile có correlation, không mở mọi transition từ `INDETERMINATE`.
- Cập nhật cả internal native protocol lẫn broker và Kotlin khi thêm payload; giữ version/compatibility. Map enum bằng ý nghĩa vì giá trị `FLED` giữa các proto và wire khác nhau.
- Không có deadline sản phẩm được cung cấp. Chia theo bằng chứng để tránh hứa tính năng trước khi biết route dùng được.
- Không viết UT theo yêu cầu. Mỗi source file không quá 500 dòng; khi triển khai phải tách trách nhiệm hợp lý.

### 6. Những phương án đã cân nhắc?

| Phương án | Điểm phù hợp | Hạn chế / quyết định |
|---|---|---|
| **A. Khảo sát `SoftSfidaCaptureRpc` và service sở hữu** | Không truyền throw, kết quả chính là captured/fled; gần nhất với yêu cầu bắt trực tiếp ngầm. | Ưu tiên kiểm tra feasibility: feature/state/limit/owner và callback map. Chưa chốt dùng production trước kiểm chứng. |
| **B. Hoàn thiện `MapPokemon.TryCapture` hiện có** | Tận dụng direct-map branch và game RPC; không cần encounter UI trong caller hiện tại. | Vẫn truyền throw data; còn câu hỏi encounter context phía server. Là phương án có điều kiện nếu mục tiêu là bỏ UI và route được xác minh. |
| **C. `SendEncounterRequest` nội bộ rồi `TryCapture`, giữ UI ở map** | Có thể cung cấp context nếu route B cần handshake. | Chỉ là giả thuyết cần reverse/call graph. Nếu “không encounter” cấm cả request nội bộ thì không đáp ứng; không âm thầm thêm làm fallback. |
| **D. Mở encounter rồi ném và đóng nhanh** | Có thể giảm thời gian UI. | Không đạt yêu cầu người dùng nên loại khỏi đề xuất. |
| **E. Chỉ xoá entity, sửa status hoặc toast “đã bắt”** | Tạo cảm giác thao tác hoàn tất ở client. | Không chứng minh server đã bắt; không đạt yêu cầu nên loại. |

Không chuyển giữa A/B/C để retry một attempt có kết quả chưa rõ. Chọn route trước khi gửi dựa trên capability đã kiểm chứng.

### 7. Tương tác với các tính năng hiện có?

- **Auto spin:** chung FIFO một mutation; bắt đang chờ kết quả sẽ chặn spin. Recovery và timeout cần trạng thái dễ hiểu để người dùng không tưởng tool đứng im.
- **Auto encounter/manual catch:** ở map, catch-all hiện đã ưu tiên direct route. Khi người dùng vào encounter, phải để lifecycle guard quyết định, không gọi map catch đồng thời.
- **IV/shiny filter:** nearby model hiện không chứa đủ metadata cho các điều kiện này. Không tuyên bố catch-all ngầm vẫn đáp ứng selective catch; không mở encounter để lấy metadata mà không thể hiện rõ đổi hành vi.
- **Excellent/AR+/snapshot/close-preview:** là intent của encounter route, không tự áp dụng vào SoftSfida. Không báo đã dùng excellent ở route không có throw outcome.
- **Joystick/teleport:** thay đổi position có thể làm target ngoài tầm; revalidate khi dispatch và không suy ra flee chỉ vì rời cell.
- **Inventory/transfer:** kết quả capture/loot phải theo pipeline game; không tự cộng Pokémon hoặc sửa bag từ client để làm bằng chứng thành công.
- **Toast/config:** tái sử dụng sink hiện có và `showActionToasts`; log/status giữ kết quả ngay cả khi tắt toast. Chỉ phát event sau validation và dedupe.

### 8. Những dependency nào còn thiếu?

Không cần HTTP endpoint mới để bật auto catch: controller đã có config/start và `AutomationRunner`. Cần hoàn thiện dependency bên trong:

1. Route native được xác minh theo exact build; nếu dùng SoftSfida thì thêm resolver cho service và trạng thái sẵn sàng.
2. Theo dõi promise success/error với GC/thread/lifetime đúng; giữ session, command, encounter ID và route.
3. Chuyển authoritative outcome qua target process, companion, bridge event và runner.
4. Phân biệt pending thông thường với outcome bị mất; xử lý late result, duplicate và reconnect.
5. Map synchronization có postcondition riêng, không suy diễn outcome từ target disappearance.
6. Adapter/controller phát toast đúng một lần sau khi kết quả đã được chấp nhận.

Không cần thư viện bên thứ ba mới được xác định ở bước phân tích. NDK/IL2CPP resolver và bridge hiện có là nền tảng; APK chính xác cùng owner/scope thực tế là dependency quyết định.

### 9. Rủi ro lớn nhất là gì?

1. **Nhầm khả năng gọi với khả năng bắt:** có method/RPC trong binary nhưng server có thể từ chối theo feature, range, inventory hoặc state. Chỉ live result mới chốt feasibility.
2. **Mất hoặc gán sai asynchronous result:** bỏ promise, sai GC lifetime, callback session cũ hoặc runner không nhận late result có thể làm kẹt queue, toast sai, hoặc bắt lặp.
3. **Map và kết quả bị trộn thành một bằng chứng:** tự remove entity rồi dùng disappearance để kết luận caught tạo vòng xác nhận giả. Cần authoritative outcome trước, map sync sau và được kiểm chứng riêng.

Chưa tái hiện một ca lỗi auto catch cụ thể trên thiết bị trong lượt này, nên các khoảng trống source là phát hiện tĩnh; không gán chúng thành nguyên nhân duy nhất của mọi lỗi thực tế.

### 10. Điều kiện nào chứng minh đã làm xong?

Các tiêu chí bên dưới gắn trực tiếp với yêu cầu; chúng là kế hoạch kiểm chứng khi triển khai, chưa phải kết quả test đã chạy.

## Tiêu chí nghiệm thu (Acceptance Criteria)

**Nguồn:** yêu cầu người dùng trong hội thoại. Không tìm thấy `docs/newspec/` hoặc `docs/specs/`. `docs/headless-autocatch-spin.md` là tài liệu hành vi hiện có, không thay thế spec cho yêu cầu mới. Các tiêu chí suy ra được đánh dấu **`inferred — needs BA confirm`**; đây là nhãn về giả định sản phẩm, không phải yêu cầu mở thêm bước xin phép cho việc phân tích đã được giao.

| ID | Quy tắc / yêu cầu | Kết quả mong đợi | Cách nghiệm thu |
|---|---|---|---|
| BG-01 | “pokemon sẽ xuất hiện xung quanh” | Chỉ chọn spawn có ID hợp lệ, observation mới và route hỗ trợ. | Nhiều spawn, spawn hết hạn và danh sách rỗng; không gọi target cũ. |
| BG-02 | “không muốn encounter rồi quăng ball” | Giữ map UI; không mở encounter, không gọi luồng throw UI. Nếu cấm cả throw data, route phải không nhận `PokeballThrow`. | Quan sát UI thủ công kết hợp trace exact methods, không dựa vào toast. |
| BG-03 | “gọi đến method game để tự catch luôn ngầm bg” | Method thuộc game xử lý yêu cầu thật, không sửa status/local bag giả. | Ghi route + command + target + authoritative result. `bg` khi vẫn ở map là `inferred — needs BA confirm`. |
| BG-04 | “nhận biết trạng thái đã bắt hay chạy mất” | `CAUGHT`/`FLED` chỉ từ result đã correlate; error, timeout, breakout không bị gán nhầm. | Có bằng chứng captured và fled riêng; negative cases giữ đúng trạng thái. |
| BG-05 | “báo toast” | Toast đúng kết quả và target, tối đa một lần mỗi attempt; tôn trọng setting toast hiện có. | Callback duplicate/late không phát toast sai hoặc lặp. Dedupe là `inferred — needs BA confirm`. |
| BG-06 | “rồi load map lại” | Sau terminal result, map cập nhật qua pipeline game, không reselect target đã xử lý; kết quả caught vẫn giữ nếu refresh lỗi. | Xác nhận dữ liệu map đã áp dụng thay đổi, không chỉ sequence tăng. Refresh cell thay reload scene là `inferred — needs BA confirm`. |
| BG-07 | Một mutation tại một thời điểm | Không bắt/spin/catch thủ công chồng nhau trong owner được tool điều phối; không đổi route để retry outcome chưa rõ. | Quan sát dispatch và result cùng session. `inferred — needs BA confirm`. |
| BG-08 | Guard và readiness | Sai build/ABI/owner, feature disabled hoặc thiếu observer thì không gửi catch; lý do đọc được qua status. | Read-only readiness và trường hợp điều kiện không đạt. Theo quy tắc repository. |
| BG-09 | Kết quả đến muộn / mất kết nối | Pending không bị biến ngay thành indeterminate; late result chỉ reconcile đúng attempt; không replay session cũ. | Manual/harness scenario khi timeout/reconnect. `inferred — needs BA confirm`. |
| BG-10 | “không viết UT” | Không thêm UT. | Review diff; kiểm chứng bằng build, kiểm tra native phù hợp và thử thực tế khi triển khai. |

Liên kết yêu cầu → tiêu chí: phát hiện xung quanh → BG-01; không encounter/throw UI → BG-02; gọi method bắt ngầm → BG-03/BG-08; biết kết quả → BG-04/BG-09; toast → BG-05; load map → BG-06; chạy liên tục an toàn → BG-07; không UT → BG-10.

**Trình tự kiểm chứng khi triển khai:**

1. Read-only: xác nhận ADB target Air 1 qua `adb devices -l`, exact build, owner, state/capability và các binding reverse-derived. Không bắt đầu bằng live enumeration để đoán tên.
2. Với route đã qua guard, ghi một attempt có target và correlation đầy đủ; xác minh map UI không chuyển encounter, result thật và toast đúng.
3. Đối chiếu caught với dữ liệu inventory/journal của game nếu có; kiểm chứng flee riêng. Không lấy việc entity biến mất làm bằng chứng duy nhất.
4. Xác minh refresh và target tiếp theo; thử trường hợp hết bóng, thiếu feature/range, callback trùng, timeout và chuyển scene phù hợp điều kiện thực tế.
5. Không viết UT. Khi sửa code, chạy kiểm tra/build hiện có phù hợp và `./gradlew test assembleDebug` nếu môi trường cho phép; chạy test có sẵn không đồng nghĩa viết UT mới. Native build/package/install/logcat dùng đúng scripts repository.

## Tổng hợp

### Phát hiện chính

**Có một method khớp sát yêu cầu bắt ngầm không truyền dữ liệu quăng bóng: `SoftSfidaCaptureRpc(encounter_id, spawnpoint_id)`.** Kết quả có `POKEMON_CAPTURED` và `POKEMON_FLED`, nhưng route bị ràng buộc bởi feature/state/limit phía game. Nhánh `MapPokemon.TryCapture` hiện có là route khác, vẫn gửi catch request với throw data và đang thiếu observer/result/toast/map synchronization.

### Hướng đề xuất

Ưu tiên một bước xác minh khả năng dùng SoftSfida trên đúng Air 1: owner sống, trạng thái service, điều kiện kích hoạt, semantics success/error và callback cập nhật map. Nếu route này dùng được, tích hợp qua runner hiện có với result observer và map postcondition; nếu không, đánh giá hoàn thiện `TryCapture` trong phạm vi bỏ UI, làm rõ trước việc có cần encounter handshake nội bộ. Không hứa bắt 100%, không dùng disappearance làm kết quả và không tự đổi route sau request có outcome chưa rõ.

### Rủi ro cần theo dõi

- API tồn tại nhưng feature/server state không cho sử dụng.
- Promise/callback bị mất hoặc state machine từ chối kết quả đến muộn.
- Map cache bị cập nhật thiếu hoặc bị dùng để suy diễn sai caught/fled.

### Những câu hỏi còn mở

- Tài khoản/game trên Air 1 hiện có `ISoftSfidaService.IsEnabled`, `IsActive`, `CurrentState` thế nào? Cần đọc owner/state thật, chưa có bằng chứng lượt này.
- Luồng SoftSfida chính thức áp dụng result vào loot/state/map ở callback nào, và calling riêng RPC có bỏ qua bước cần thiết không? Cần đọc tiếp `SoftSfidaForegroundService.byp.MoveNext` và `byl.beva` cùng các callee.
- `TryCapture` có được server chấp nhận với wild target chưa qua `SendEncounterRequest` không? Disassembly client chưa đủ trả lời điều kiện server.
- “Không encounter” chỉ cấm UI, hay cấm cả request/throw data nội bộ? Phương án A phù hợp nhất với cách hiểu chặt; phương án B/C cần chốt nếu phải dùng.
- “bg” có bao gồm tắt màn hình/chuyển ứng dụng không? Phân tích hiện mặc định game vẫn chạy ở map; không cam kết nền Android.
- Nếu phải dùng route bắt thường, gặp `BREAKOUT` thì dừng hay retry hữu hạn? Chưa có policy do người dùng chỉ định.

**Đã thực hiện trong lượt phân tích:** đọc source và tài liệu, curated reverse rồi compressed dump, đối chiếu SHA-256 và disassemble các method nêu trên, đọc log đã có. Chưa gọi catch trên thiết bị, chưa chứng minh live end-to-end, không sửa source tính năng và không viết UT.

## 11. Kết quả merge và build/push ngày 2026-09-09

- `origin/main` có hai commit tách runtime bootstrap khỏi diagnostic nặng. Conflict được resolve bằng cách giữ bootstrap nhẹ và policy throttle của upstream, đồng thời giữ `SoftSfidaCaptureRpc`, scene-owner fallback và observer kết quả authoritative của nhánh auto-catch.
- Diagnostic runtime hiện chỉ chạy sau khi controller kết nối vào runtime bridge; bản native vẫn fail-closed nếu chưa có exact build, binding hoặc outcome observer.
- `./gradlew test assembleDebug --rerun-tasks` đã pass; native protocol checks đã pass.
- `scripts/build-magisk.sh` đã build pass cả `arm64-v8a` và `x86_64`, tạo `build/pogo-root-automation-magisk-multiabi.zip`.
- Đã xác nhận emulator Air 1 là `127.0.0.1:5565` và push ZIP thành công. Bước này mới upload ZIP, chưa chạy install/reboot hoặc gọi live catch.

## 12. Hợp nhất automation scan thành một transaction `SCAN_MAP`

### Vấn đề hiện tại

Luồng hiện tại gọi `SNAPSHOT` nhưng native phát lại `INVENTORY`, `FORTS` và `NEARBY` thành các observation độc lập. Kotlin phải ghép state qua cache và chọn một observation làm trigger. Vì `nearby` đang là trigger chính, còn `forts` bị coi là auxiliary, log/filter có thể trông như bị đứng dù native đã đọc được một phần dữ liệu. Cách này cũng không có correlation rõ ràng giữa một lần scan và các action tiếp theo.

### Quyết định thiết kế

Đổi automation loop sang transaction có correlation:

`Kotlin SCAN_MAP(cycle_id)` → `native đọc map + nearby + forts + inventory + player.current_location` → `native REQUEST_CATCH_SPIN(cycle_id, snapshot)` → `Kotlin filter/policy` → `TRY_CATCH` hoặc `TRY_SPIN` → đợi terminal result → timeout interval tiếp theo.

`REQUEST_CATCH_SPIN` được biểu diễn là một observation payload mới để đi qua bridge hiện có, nhưng không còn dùng các observation `NEARBY`/`FORTS`/`INVENTORY` làm trigger cho automation. Các observation cũ vẫn được giữ cho compatibility và diagnostic ngoài automation.

### Trade-off và guard

- Một payload lớn hơn nhưng state là cùng một thời điểm, dễ log và dễ correlate hơn.
- Nếu một binding không có, native vẫn trả response với cờ unavailable; Kotlin phải log và không gửi mutation dựa trên dữ liệu thiếu.
- `cycle_id` phải dương và được kiểm tra khi nhận response; response cũ hoặc trễ không được chạy policy/action cho vòng mới.
- `TRY_CATCH`/`TRY_SPIN` tiếp tục đi qua `AutomationAction`/runner hiện có để giữ identity, capability, lifecycle và serialization guard; tên command mới là semantic mapping, không tạo đường mutation bypass.
- Không tự bật scan observer nền song song; `SCAN_MAP` là pull duy nhất của automation, tránh nhân đôi read với observer.

## Tiêu chí nghiệm thu bổ sung

**Nguồn:** yêu cầu mới trong hội thoại, không có spec riêng; các tiêu chí dưới đây là `inferred — needs BA confirm`.

| ID | Quy tắc / yêu cầu | Kết quả mong đợi | Cách nghiệm thu |
|---|---|---|---|
| SCAN-01 | Service auto catch gửi `SCAN_MAP` | Mỗi vòng active có một log gửi `SCAN_MAP` kèm `cycle_id`; không gửi `SNAPSHOT` cho automation. | Log Kotlin/native theo cùng cycle. |
| SCAN-02 | Native đọc các dữ liệu trong một lần scan | Response chứa nearby, forts, inventory và player location cùng cờ availability. | Log native nêu rõ từng phần; payload decode được ở Kotlin. |
| SCAN-03 | Native trả `REQUEST_CATCH_SPIN` | Kotlin nhận đúng response, log cycle/seq/count/ball/location và dùng response làm filter trigger duy nhất. | Không cần chờ observation `NEARBY` riêng để filter. |
| SCAN-04 | Kotlin gửi action sau filter | Chỉ gửi `TRY_CATCH`/`TRY_SPIN` tương ứng với policy và capability; không dispatch khi cycle stale hoặc snapshot thiếu điều kiện. | Log filter result rồi action dispatch/result theo cùng cycle. |
| SCAN-05 | Interval tuần tự | Chỉ bắt đầu timeout/interval tiếp theo sau khi scan response và action terminal/không-action đã được xử lý; không scan chồng. | Log cycle tăng tuần tự, không có in-flight cycle kép. |
| SCAN-06 | Backward compatibility | Observation cũ vẫn decode được và các module khác không bị phá. | `./gradlew test assembleDebug` và native build. |

## Section 12 — Pokémon có trên map nhưng nearby không đi qua Kotlin

### Phát hiện từ log run mới

Giả định “không có Pokémon” là sai ở tầng sản phẩm: người dùng vẫn nhìn thấy Pokémon trên map. Tuy nhiên log native chứng minh reader chưa hề tạo hoặc gửi nearby payload trong run này:

- Có `187` dòng `runtime observation lifecycle`, Kotlin nhận lifecycle `OVERWORLD`.
- Có `0` dòng `runtime observation nearby`.
- Có `0` dòng `runtime map reader cells`.
- Có `0` dòng native `runtime gameplay command received`, `direct map catch` hoặc `TryCapture`.

Diagnostic đã tìm thấy binding gọi bắt (`direct map catch binding verified=1`) và cài được hook `S2CellManager.OnMapQueryResponse`, nhưng đồng thời ghi `INearbyPokemonService object=<null>`, `IMapSceneViewService object=<null>` và `runtime bootstrap ... map_read=0`. Runtime vì vậy quảng bá `DIRECT_CATCH` nhưng không quảng bá `READ_NEARBY`.

### Chuỗi lỗi/chặn hiện tại

1. `discover_runtime_owners()` chỉ bind `MapEntityService` khi resolve được `INearbyPokemonService` hoặc `IMapSceneViewService`; hai owner đều null ở thời điểm diagnostic.
2. `runtime_observation_thread()` chỉ gọi `request_main_thread_map_snapshot()` khi `map_entity_read_verified || forts_read_verified`. Cả hai đều false, nên hook map query có đánh dấu dirty cũng không thể dẫn tới map read.
3. Không có nearby envelope để `BridgePogoRuntimeSource` cache và Kotlin không có `RawNearbyObservation` để dựng `NearbySnapshot`.
4. `AutomationCoordinator` chỉ tạo `AutomationAction.Catch(DIRECT_MAP)` nếu `snapshot.nearby` có spawn hợp lệ. Vì snapshot nearby null, không có command gửi xuống native.

Đây là lỗi ở native discovery/readiness, không phải Kotlin không chạy hoặc decoder Kotlin làm mất Pokémon. Việc game hiển thị Pokémon chỉ chứng minh UI/game đã có dữ liệu; nó không chứng minh `MapEntityService.Cells` đã được resolver lấy đúng và đọc được từ bridge.

### Vấn đề timing làm lỗi bị giữ nguyên

Diagnostic đánh dấu runtime managed-ready chỉ dựa trên `exact_build_verified && encounter_read_verified`; `map_read` không nằm trong điều kiện ready. Sau khi capability update thành công, `RuntimeLifecycleCoordinator` không gọi lại diagnostic nữa vì session đã được xem là strong-ready. Nếu scene/map services được tạo sau mốc khoảng 3 giây, owner sẽ vẫn null và binding `READ_NEARBY` không tự xuất hiện.

### Hướng sửa tối thiểu được đề xuất

Với config có `autoCatch`, `autoEncounter` hoặc `mapTapWalk`, readiness phải tiếp tục retry discovery cho tới khi có `READ_NEARBY` hoặc ghi rõ unavailable; không bật đường catch chỉ vì `DIRECT_CATCH` đã bind. Có thể giữ observer lifecycle để theo dõi trạng thái, nhưng cần thêm cơ chế re-probe scene/nearby owner trên main thread sau khi map scene sẵn sàng. Chỉ khi log xuất hiện `runtime map reader cells=... wild=N` rồi `runtime observation nearby spawns=N sent=1`, Kotlin mới có dữ liệu để lập lệnh catch.

### Tiêu chí nghiệm thu bổ sung

| ID | Quy tắc / yêu cầu | Kết quả mong đợi | Cách nghiệm thu |
|---|---|---|---|
| BG-11 | Pokémon hiển thị trên game phải được chuyển thành nearby snapshot | Native đọc được cell/wild object và gửi payload có `spawns` hợp lệ sang Kotlin | Log đủ `map reader cells`, `observation nearby`, Kotlin nhận event nearby; đối chiếu spawn ID |
| BG-12 | Owner map có thể khởi tạo muộn | Discovery retry/re-probe sau khi scene/map service sống; không đóng băng `map_read=0` trong cả session | Diagnostic sớm thấy null, diagnostic/re-probe sau thấy owner và capabilities có `READ_NEARBY` |
| BG-13 | Không gửi catch khi chỉ có `DIRECT_CATCH` | Thiếu `READ_NEARBY` thì không gửi `DIRECT_MAP_CATCH`, status phải thể hiện nguyên nhân | Kiểm tra `runtime gameplay command received` không xuất hiện trước khi có nearby payload |

### Kết luận lượt này

Có khả năng Pokémon thực sự đang ở xung quanh, nhưng hiện tại chúng bị kẹt trước bridge: native chưa bind được map entity owner nên không đọc được danh sách, không phải Kotlin nhận rồi bỏ qua. Cần sửa/retry native nearby discovery trước; chưa nên sửa `TryCapture` hoặc ép Kotlin tự tạo target vì sẽ không có `spawnId` hợp lệ.

## Section 13 — Đánh giá thiết kế automation bật tạm thời và poll snapshot

### Kết luận ngắn

Hướng thiết kế là đúng và dễ debug hơn luồng push hiện tại, nhưng cần tách rõ **settings đã lưu** khỏi **automation run state**. `autoCatch`, `autoSpin`, policy, interval và các ngưỡng có thể lưu trong storage; nút master `automation` nên là trạng thái runtime trong memory, mặc định tắt sau process restart/reboot và chỉ bật khi người dùng click.

Luồng nên có một owner duy nhất là `HeadlessAutomationService`/engine: click bật service → connect runtime → diagnostic/readiness → bật module → gửi một yêu cầu snapshot theo interval → Kotlin validate/filter → gửi tối đa một mutation → chờ terminal result hoặc timeout → toast/log → settle delay → vòng tiếp theo. Không nên để overlay tự chạy loop riêng, cũng không nên để native observer push nearby song song với poll nearby nếu chưa có cơ chế dedupe/batch.

### 1. Trạng thái `automation` và persistence

#### Hiện trạng source

- `HeadlessAutomationConfig.enabled` mặc định là `false` nhưng hiện được đọc/ghi trong `SharedPreferences` ([AutomationConfig.kt:20](../../../../app/src/main/java/dev/pogoroot/automation/headless/AutomationConfig.kt:20), [AutomationConfig.kt:63](../../../../app/src/main/java/dev/pogoroot/automation/headless/AutomationConfig.kt:63)).
- Settings screen cũng ghi `enabled` vào storage ([AutomationCategoryFragment.kt:99](../../../../app/src/main/java/dev/pogoroot/automation/overlay/AutomationCategoryFragment.kt:99)).
- Shortcut overlay chỉ gọi `automationConfigRepository.update { ... enabled = ... }`, không gọi `HeadlessAutomationService.enable/disable` ([JoystickOverlayService.kt:287](../../../../app/src/main/java/dev/pogoroot/automation/overlay/JoystickOverlayService.kt:287)).
- `MainActivity` và boot receiver có thể khởi động foreground service, nhưng service sẽ chạy idle nếu config `enabled=false`; việc đổi preference từ overlay không bảo đảm tạo service nếu service đã chết ([HeadlessAutomationService.kt:87](../../../../app/src/main/java/dev/pogoroot/automation/headless/HeadlessAutomationService.kt:87), [AutomationBootReceiver.kt:7](../../../../app/src/main/java/dev/pogoroot/automation/headless/AutomationBootReceiver.kt:7)).

Vì vậy yêu cầu “biến automation luôn off, không lưu storage; click mới phát service bg” **chưa khớp source hiện tại**.

#### Mô hình nên dùng

Tách hai lớp:

| Lớp | Có lưu storage? | Nội dung |
|---|---:|---|
| `AutomationSettings` | Có | `autoCatch`, `autoSpin`, policy filter, `loopIntervalMs`, settle delay, toast preference, allowlist build |
| `AutomationRunState` | Không | `active`, runtime session, phase connect/ready/polling/waiting-result/stopped, pending cycle/command, last result |

Nút master nên có semantics sau:

1. Bật: gọi `HeadlessAutomationService.enable(context, ...)`, service vào foreground, đặt run state `STARTING`, rồi engine bắt đầu connect/start module.
2. Tắt: gọi `HeadlessAutomationService.disable(context)`, đặt run state `STOPPING`, hủy vòng poll, dừng/đợi action đang chạy theo fail-closed policy, gửi STOP runtime và về `IDLE`.
3. Boot/process restart: service có thể được khởi động để giữ API, nhưng run state mặc định `OFF`; không tự resume automation chỉ vì settings cũ còn `autoCatch=true`.
4. UI phải render run state runtime, không render `config.enabled` nếu field này đã trở thành non-persistent. Nếu overlay không đọc được state trực tiếp, cần status channel/read-only API hoặc registry in-memory có reset an toàn khi process chết.

Có thể giữ tên `enabled` ở API trong giai đoạn chuyển tiếp, nhưng semantics phải là `active` runtime. Không nên vừa có `config.enabled` persisted vừa có `engine.active` volatile, vì hai nguồn sự thật sẽ làm UI báo ON trong khi service không chạy hoặc reboot tự chạy ngoài ý muốn.

### 2. Luồng click đến native

Luồng đề xuất:

```text
Overlay click ON
  → HeadlessAutomationService.enable()
  → foreground service / engine active
  → connect bridge
  → START native host
  → managed diagnostic + readiness
  → resolve/re-probe nearby + fort owners
  → enable catch_spin khi capability phù hợp
  → poll snapshot mỗi 1–2 giây
  → Kotlin validate + filter
  → submit Catch(DIRECT_MAP) hoặc Spin
  → native execute trên Unity main thread
  → native result / timeout
  → Kotlin accept result, log, toast
  → settle delay
  → snapshot cycle kế tiếp
```

Bước `connect/start module catch-spin` là đúng về mặt ownership, nhưng module chỉ được enable sau khi exact build, owner và capability đã qua guard. `DIRECT_CATCH` một mình không đủ để bắt; catch map cần nearby spawn hợp lệ. Spin cần fort reader/capability hợp lệ.

### 3. Snapshot mỗi interval

Ý tưởng “mỗi interval gửi `get nearby` gồm Pokémon, forts, player info và Poké Ball count” phù hợp để kiểm chứng end-to-end. Tuy nhiên không nên coi đó là bốn event độc lập không có batch ID. Source hiện có các payload riêng `NEARBY`, `FORTS`, `INVENTORY`; nearby đã có player latitude/longitude, còn Poké Ball count nằm trong inventory item list ([runtime_observation_protocol.h:96](../../../../zygisk/jni/shared/bridge_kotlin/runtime_observation_protocol.h:96), [runtime_observation_protocol.h:114](../../../../zygisk/jni/shared/bridge_kotlin/runtime_observation_protocol.h:114), [runtime_observation_protocol.h:123](../../../../zygisk/jni/shared/bridge_kotlin/runtime_observation_protocol.h:123)).

Khuyến nghị tạo một logical `WorldSnapshotRequest/WorldSnapshotResponse` cho mỗi cycle, dù bên trong native vẫn dùng các reader hiện có:

```text
WorldSnapshot {
  cycleId
  runtimeSessionId
  observedAt
  lifecycle
  nearby { complete, spawns[] }
  forts[]
  playerLocation { latitude, longitude }
  ballCounts { pokeball, greatball, ultraball, ... }
}
```

Nếu chưa muốn đổi protocol thành payload gộp, mỗi payload riêng phải có cùng `snapshotId`, `observedAt` và `complete` state; Kotlin chỉ đưa snapshot vào filter khi đã đủ phần bắt buộc. Không được ghép Pokémon lúc `t=1` với forts/inventory cũ ở `t=0` rồi coi là một state nguyên tử.

Request phải được marshal vào Unity main thread. Kotlin/worker chỉ gửi command; native không được đọc managed collection từ thread bridge. Khi map owner chưa resolve được, response phải là `UNAVAILABLE` kèm lý do (`nearby_owner_missing`, `map_entity_binding_missing`, `main_thread_timeout`), không trả `spawns=[]` như thể map thật sự rỗng.

Poll snapshot không tự giải quyết lỗi hiện tại: nếu `INearbyPokemonService`/`IMapSceneViewService` vẫn null thì request phải log lỗi rõ ràng. Cần re-probe owner sau khi map scene sống, rồi mới phát `READ_NEARBY`.

### 4. Interval và timeout

Nên dùng một loop duy nhất trên executor của engine. Interval mặc định `2_000 ms` an toàn hơn `1_000 ms` cho bước đầu vì map query/native collection walk có thể mất thời gian; cho phép cấu hình trong khoảng `1_000..2_000 ms` nếu cần. Không dùng thêm timer ở overlay hoặc một worker thứ hai.

Các loại thời gian cần tách:

| Timer | Bắt đầu khi nào? | Khi hết hạn |
|---|---|---|
| Snapshot request timeout | Khi native nhận `GET_WORLD_SNAPSHOT` | Đánh dấu cycle lỗi/unavailable; không dùng snapshot cũ như mới |
| Action timeout | Khi native nhận Catch/Spin | `INDETERMINATE`, dừng mutation/re-sync; không retry mù |
| Settle delay | Sau terminal action result | Chờ game/map ổn định trước cycle kế tiếp |
| Poll interval | Sau khi cycle kết thúc hoặc bị bỏ qua an toàn | Bắt đầu cycle mới nếu không còn action pending |

“Sau khi nhận kết quả mới đặt timeout” cần chỉnh lại: timeout phải chạy **trong lúc chờ kết quả**; sau kết quả terminal thì dùng `settle delay`, không dùng timeout để kết luận lại. Nếu không nhận kết quả trước deadline, không được toast `caught`/`fled`; phải báo `unknown/indeterminate` và chặn catch tiếp cho tới khi resync.

Mỗi thời điểm chỉ cho phép một action mutation. Trong lúc Catch/Spin pending, interval có thể bỏ qua poll hoặc chỉ poll read-only có chủ đích; không gửi action thứ hai. Sau result, runner hiện có đã có cơ chế active mutation, timeout và settle gate; nên tái sử dụng thay vì tạo state machine thứ hai bên service.

### 5. Kotlin validate và filter

Thứ tự xử lý trong Kotlin nên là:

1. Kiểm tra session, sequence, timestamp, payload version và `snapshot.complete`.
2. Kiểm tra lifecycle (`OVERWORLD` cho direct map catch/spin), location hợp lệ và freshness.
3. Kiểm tra capability (`READ_NEARBY`, `DIRECT_CATCH` hoặc `READ_FORTS`, `SPIN`).
4. Lọc spawn hết hạn, ID rỗng/trùng, tọa độ lỗi, target đã pending/đã xử lý.
5. Áp dụng policy: catch-all, shiny/IV nếu dữ liệu thật sự có, ưu tiên target, ball threshold.
6. Chọn action và gửi kèm `cycleId`, `snapshotId`, `basedOnObservationSeq`, `commandId`.

Kết quả filter nên biểu diễn được số lượng và lý do, ví dụ `input=5 valid=3 expired=1 duplicate=1 selected=spawnId`. Không log toàn bộ payload/location ở mức INFO mỗi giây; INFO chỉ ghi count/ID/action, DEBUG mới ghi chi tiết khi cần để tránh logcat bị ngập.

Lưu ý: nearby hiện chỉ có species/ID/location, không có IV/shiny đầy đủ. Không áp dụng policy IV/shiny vào direct-map spawn rồi giả định dữ liệu đã có; nếu thiếu metadata thì ghi `filter_skip=metadata_unavailable` hoặc dùng catch-all theo policy đã chọn.

### 6. Native action và kết quả

Kotlin không gọi trực tiếp `MapPokemon.TryCapture`; Kotlin chỉ submit `AutomationAction.Catch(DIRECT_MAP)`, native mới invoke method. Native cần log theo cùng correlation:

```text
Kotlin action submit cycle=... command=... action=DIRECT_MAP_CATCH target=...
Native command received cycle=... command=... target=...
Native TryCapture invoked command=... object=... method=...
Native result command=... outcome=CAUGHT|FLED|ERROR|INDETERMINATE
Kotlin result accepted command=... outcome=...
Toast emitted command=... outcome=...
```

Binding `TryCapture` hiện đã verified, nhưng log trước đó cho thấy `direct map catch outcome observer verified=0`. Do đó invocation/Promise non-null chưa phải terminal result. Chỉ show toast “đã bắt” hoặc “chạy mất” sau khi result observer có outcome authoritative và đã correlate đúng session/command/target. Nếu chưa có observer, toast tối đa là “đã gửi, đang chờ kết quả”, không được báo thành công.

### 7. Logging checklist

| Stage | Log bắt buộc | Correlation |
|---|---|---|
| Click | `automation requested on/off` và nguồn click | `runId` |
| Service | `service started`, `engine active/idle` | `runId` |
| Connect | bridge connected/disconnected, session | `runtimeSessionId` |
| Readiness | diagnostic, exact build, owner/capability | `runtimeSessionId` |
| Module | catch-spin requested/enabled/failed | `runtimeSessionId`, module |
| Snapshot request | request gửi/nhận/timeout | `cycleId`, `snapshotId` |
| Decode | version, counts, validation failure | `snapshotId` |
| Filter | input/accepted/rejected/selected | `cycleId`, `snapshotId` |
| Action | submit, native receive, invoke | `commandId`, target |
| Result | accepted/late/duplicate/timeout | `commandId` |
| Toast | type/message emitted | `commandId` |
| Cycle | skip reason, settle, next due | `cycleId` |

Các lỗi hiện đang bị che như `adapter.readNearby().getOrNull()` phải được log một lần theo cycle với lý do cụ thể. `getOrNull()` có thể vẫn dùng để fail-closed, nhưng không được là nơi duy nhất xử lý lỗi vì nó biến “reader hỏng” thành “không có Pokémon”.

### 8. Những điểm chưa ổn nếu triển khai nguyên văn

| Ý tưởng | Đánh giá | Điều chỉnh |
|---|---|---|
| `automation` luôn off, không lưu | **Ổn và nên làm** | Tách khỏi persisted settings; click gọi service command trực tiếp |
| Click phát background service | **Ổn nhưng hiện chưa đủ** | Overlay phải gọi `enable/disable`, không chỉ update repository |
| Poll mỗi 1–2 giây | **Ổn** | Một owner/loop duy nhất; mặc định 2 giây |
| Một request lấy Pokémon, forts, player, balls | **Ổn về API** | Gộp logical snapshot hoặc thêm batch correlation |
| Kotlin filter theo policy | **Ổn** | Validate/freshness/capability trước policy; log rejected reason |
| Filter trigger Catch/Spin | **Ổn** | Một mutation tại một thời điểm, guard lifecycle/capability |
| Nhận kết quả rồi toast | **Ổn nếu authoritative** | Không coi method return/non-null là caught; dedupe toast |
| Kết thúc rồi đặt timeout | **Chưa ổn về semantics** | Timeout lúc pending; sau result dùng settle delay |
| Native observer cũ và poll mới cùng chạy | **Rủi ro** | Chọn một nguồn nearby hoặc thêm snapshot ID/dedupe rõ ràng |

### Tiêu chí nghiệm thu bổ sung cho thiết kế này

| ID | Quy tắc | Kết quả mong đợi |
|---|---|---|
| AUTO-01 | Master automation không persisted | Sau process restart/reboot, automation OFF; policy settings vẫn giữ |
| AUTO-02 | Click ON/OFF điều khiển service | ON tạo/đánh thức foreground service và engine; OFF dừng loop + native runtime an toàn |
| AUTO-03 | Một loop duy nhất | Không có hai interval hoặc hai action mutation chạy chồng |
| AUTO-04 | Snapshot atomic/correlated | Nearby, forts, location, ball count cùng cycle hoặc được đánh dấu thiếu; không trộn state cũ |
| AUTO-05 | Nearby unavailable khác nearby empty | Owner/reader lỗi phát `UNAVAILABLE` và log lý do; list rỗng hợp lệ chỉ khi reader thành công |
| AUTO-06 | Filter observable | Log được input count, từng nhóm reject, selected target và policy branch |
| AUTO-07 | Action correlation | Mọi command/result/toast liên kết bằng runtime session + cycle/snapshot + command ID |
| AUTO-08 | Timeout đúng nghĩa | Pending quá hạn thành `INDETERMINATE`; không tự retry catch/spin khi outcome chưa rõ |
| AUTO-09 | Toast đúng kết quả | Chỉ toast caught/fled/error sau result hợp lệ; duplicate/late result không toast lần hai |
| AUTO-10 | Nearby readiness | Khi map owner khởi tạo muộn, re-probe thành công và log được `map reader ... wild=N` rồi `nearby ... sent=1` |

### Đề xuất phạm vi triển khai sau khi chốt thiết kế

1. Tách run state volatile khỏi `AutomationConfigRepository`, cập nhật overlay/API/status theo state mới.
2. Bổ sung native request/response snapshot có main-thread marshal và error reason; trước mắt có thể giữ các reader/payload riêng nhưng phải có cycle correlation.
3. Sửa native scene/nearby owner re-probe và readiness gate; không bật auto-catch khi chưa có `READ_NEARBY`.
4. Bổ sung log ở Kotlin/native theo checklist, đặc biệt log lỗi `readNearby()` thay vì nuốt bằng `getOrNull()`.
5. Chỉ sau khi nearby payload chạy ổn mới kiểm tra action Catch/Spin và outcome/toast. Không dùng việc nhìn thấy Pokémon trên UI làm bằng chứng native reader đã đọc được chúng.

### Kết luận

Thiết kế đề xuất tốt hơn cho việc quan sát end-to-end và phù hợp với mong muốn automation chỉ chạy khi người dùng click. Điểm bắt buộc là `automation` phải là run state tạm thời, còn policy mới là storage; snapshot phải có correlation/atomicity; timeout phải bao quanh trạng thái pending; và nearby unavailable phải được phân biệt với danh sách rỗng. Sau các chỉnh sửa đó, đây là hướng triển khai hợp lý.
