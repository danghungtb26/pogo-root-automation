# Brainstorm: Phân loại wild spawn và khoảng cách tới player

**Type:** architecture
**Date:** 2026-09-22

> Đã kiểm chứng lại ngày 2026-09-22 bằng source hiện tại, curated reverse classes, full dump và JAR protobuf. Kết quả chi tiết ở mục 10. Các kết luận về schema/code đã được xác nhận; semantics runtime và nguyên nhân triệu chứng trên thiết bị chưa được xác nhận.

---

## Analysis

### 1. Vấn đề cần giải quyết

Phân biệt Pokémon chỉ xuất hiện trong danh sách `NearbyPokemon` với Pokémon đã có map entity và tọa độ đủ tin cậy để làm target. Đồng thời cần biết nguồn spawn để không gộp nhầm wild tự nhiên, incense, lure/fort, route và các loại đặc biệt vào một record duy nhất.

Triệu chứng “có trong list nhưng không hiện trên map” đặt ra khả năng hai khái niệm đang bị trộn; cần xác định nguồn list trước khi kết luận:

- `sighting`: server báo có species/encounter gần một anchor, có thể chưa có object trên map hiện tại;
- `map spawn`: client đã có entity với vị trí, lifecycle và khả năng hiển thị/interaction.

### 2. Bằng chứng từ reverse output

`ClientMapCellProto` có các kênh riêng: `WildPokemon`, `CatchablePokemon`, `NearbyPokemon`, `Tappables` và `Fort` — xem `reverse/pogo-0.427.0/dump.cs.gz:679672`. `IMapEntityService` cũng khai báo riêng `UpdateNearbyPokemon`, `UpdateWildPokemon` và `UpdateWildTappable` — xem `dump.cs.gz:511034`. Dòng `510806` thuộc `IMapEntityCell`, không phải `IMapEntityService`. Số dòng dump trong tài liệu là số dòng sau giải nén; method body `{ }` chỉ là stub, không chứng minh control flow.

Các loại chính:

| Nhóm đề xuất | Reverse evidence | Vị trí | Nhận định |
|---|---|---|---|
| `WILD` | `WildPokemonProto` → `WildMapPokemon` | Có `Latitude`, `Longitude` | Đúng với wild map object thông thường; có `SpawnPointId`, `TimeTillHiddenMs`. Xem `dump.cs.gz:830599`, `classes/WildMapPokemon.cs:1`. |
| `INCENSE` / player-attracted | `GetIncensePokemonProto` → `GetIncensePokemonOutProto` → `IncenseMapPokemon` | Có `Lat`, `Lng` | Request mang `PlayerLatDegrees`, `PlayerLngDegrees`; output có `DisappearTimeMs`. Đây là bằng chứng mạnh cho spawn gắn với player, nhưng bán kính/rule chính xác vẫn cần runtime calibration. Xem `dump.cs.gz:626419`, `626548`, `522460`. |
| `FORT_SPAWN` / fort-attracted | `PokemonFortProto.ActivePokemon`, `ActiveFortPokemon[].PokemonProto` | Có vị trí trong `MapPokemonProto` | `FortPokemonProto.Types.SpawnType` có `Lure`, `PowerUp`, `NaturalArt` (obsolete), `DayNight`. Chỉ `ActiveFortPokemon[]` có wrapper chứa subtype này; riêng `ActivePokemon` không đủ để gán `LURE`. Có `FortId` từ parent và `ExpirationTimeMs` từ child. Xem `dump.cs.gz:829158`, `829345`, `829388`, `830719`. |

Kết luận: ba nhóm bạn đề xuất là hướng phân loại tốt cho phase đầu, nhưng tên nhóm thứ ba nên là `FORT_ATTRACTED` hoặc `FORT_SPAWN`, không nên hard-code chỉ `LURE`, vì cùng một fort có các `SpawnType` khác.

Các nhóm không nên mất thông tin khi decode:

- `ROUTE`: `RouteMapPokemon`, `AttractedPokemonClientProto`, `AttractedPokemonContext.AttractedPokemonRoute` (`dump.cs.gz:696582`, `746357`, `522858`);
- `DAILY_ENCOUNTER`: `DailyEncounterMapPokemon`;
- `DAILY_BONUS_SPAWN`: `DailyBonusSpawnMapPokemon`;
- raid/quest/photobomb/incident và các object đặc biệt: không nên tự coi là ordinary wild nếu chưa có rule riêng.

Đặc biệt, `MapPokemonSpawnSource` chỉ có `WILD`, `ROUTE`, `LURE`, `DAILY_ENCOUNTER`, `DAILY_BONUS_SPAWN` — `dump.cs.gz:515965`. Đây có vẻ là source/ring của `MapPokemon`, không phải taxonomy đầy đủ của mọi proto spawn; không nên dùng enum này làm bằng chứng duy nhất cho incense hoặc mọi loại fort spawn.

### 3. Vấn đề trong native/runtime hiện tại

#### P0 — `NearbyPokemonProto` không phải map target

`NearbyPokemonProto` chỉ có `PokedexNumber`, `DistanceMeters`, `EncounterId`, `FortId` và display data; không có `Latitude`/`Longitude` — `dump.cs.gz:680389`. Vì vậy sighting này không thể tự biến thành một target có tọa độ chính xác. Fort position chỉ là anchor tham chiếu, không phải vị trí Pokémon.

Nếu gộp hai loại object, hậu quả sẽ là:

- distance tới fort bị hiểu nhầm là distance tới Pokémon;
- Pokémon ở xa hoặc chưa được instantiate bị coi như đang có trên map;
- mất phân biệt giữa sighting và entity có thể interaction;
- có nguy cơ gửi target sai cho action layer.

Đây là ranh giới semantic quan trọng nhất cần giữ khi mở rộng source classification.

#### P1 — Runtime bridge mới đọc một nhánh map object

`zygisk/jni/shared/runtime/map/runtime_map_reader.inc:140` hiện đọc trực tiếp dictionary `MapEntityCell.eghw` chứa `IWildMapPokemon`, kiểm tra exact class `WildMapPokemon`, rồi đọc `WildPokemonProto` qua helper. Nó không gọi getter `AllCellWildPokemon`. Nó chưa có nhánh đọc riêng `NearbyPokemon`, tappables, `PokemonFortProto.ActivePokemon`/`ActiveFortPokemon`, `IncenseMapPokemon`, `RouteMapPokemon` hay daily classes. Vì vậy payload runtime hiện không thể đại diện cho ba nhóm đầy đủ.

#### P1 — Native observation payload thiếu semantic metadata

Payload observation hiện chỉ đủ để truyền identity/species/position cho các wild entity mà reader đã đọc. Nó chưa mang source/subtype, `fortId`, `spawnPointId`, `reportedDistanceMeters`, `VisibleOnMap` hoặc confidence về vị trí. Do đó không thể phân biệt chắc chắn `WILD`, `INCENSE`, `FORT_LURE` và `NEARBY_SIGHTING` ở phía consumer.

#### P1 — `isComplete` dễ bị hiểu sai

Runtime observer đang quét các entity đã instantiate trong các cell hiện tại, không phải toàn bộ server response. `isComplete = true` không nên được hiểu là “đã thấy tất cả Pokémon quanh player”; nên có scope như `LOCAL_MAP_ENTITIES`/`SERVER_MAP_RESPONSE` và trạng thái truncated/stale nếu cần.

#### P2 — Chưa giữ expiry và source metadata

Reverse output đã có expiry ở nhiều nguồn (`TimeTillHiddenMs`, `ExpirationTimeMs`, `DisappearTimeMs`), nhưng runtime observation chưa giữ expiry theo từng source. Nếu muốn ưu tiên target sắp biến mất hoặc xử lý stale, cần giữ expiry cùng source và confidence.

#### P2 — Tên `WildMapPokemon` chưa đủ để kết luận “wild tự nhiên”

Ngoài proto `WildPokemon`, map layer còn có `Tappables` và `DynamicTappablesService`; `WildMapPokemon` có cả `TappableEncounterProto`/`IDynamicTappable` và `VisibleOnMap` — `classes/WildMapPokemon.cs:1-36`. Cần capture upstream channel, `SourceType`, tappables/anchor và subtype trước khi gán `WILD` một cách tuyệt đối.

### 4. Có tính được khoảng cách tới player không?

Có. Hiện đã có `GeoMath.distanceMeters()` dùng Haversine và `ScanPlanner` tính distance từ `NearbySnapshot.playerPosition` tới `spawn.position`; đây không phải chức năng cần viết từ đầu. Native cũng đã đọc player position có guard `get_HasValidLocation()` và truyền optional player position trong nearby payload. Hai mức dữ liệu cần phân biệt:

1. **Target có tọa độ thật:** lấy player position ở cùng observation/frame rồi tính Haversine giữa player và target.
2. **Sighting không có tọa độ target:** giữ `NearbyPokemonProto.DistanceMeters` như `reportedDistanceMeters`. Không nên gọi đây là khoảng cách chính xác tới Pokémon cho đến khi runtime sample xác minh distance đó đo tới fort/anchor hay tới target ẩn. `FortId` cho thấy nó có thể gắn với nearby anchor.

Reverse có field `SqrDistanceToAvatar` trong wrapper nhận `WildPokemonProto`/`Tappable` và `IMapAvatar` — `dump.cs.gz:509938-509960`; `IPlayerProximityService` khai báo `DistanceM`, `WhenInRange`, `WhenOutOfRange` — `dump.cs.gz:524177-524199`. Đây là bằng chứng về cấu trúc proximity; dump chưa chứng minh công thức/đơn vị của `SqrDistanceToAvatar`. Khoảng cách địa lý từ lat/lng có thể tính mà không cần pixel hay vị trí trên màn hình.

Tuy nhiên, distance không đồng nghĩa với “đang hiện trên map”. Cell đã load, entity lifecycle, stale state, map scope, source-specific range và `VisibleOnMap` là các yếu tố cần kiểm tra. `MapPlacePokemonSpawner` có `EncounterRangeM` và `SetUpRangeCallbacks` — `dump.cs.gz:515156-515231`; exact predicate của `VisibleOnMap` chưa được khôi phục từ dump, nên cần runtime read-only verification.

Nên phân biệt các field sau:

- `distanceToPlayerMeters`: tự tính từ player → target khi target position exact;
- `reportedDistanceMeters`: distance server trả trong `NearbyPokemonProto`;
- `targetPosition`: nullable, không fabricate;
- `anchorPosition`/`fortId`: vị trí dùng để giải thích sighting, không thay thế target position;
- `mapPresence`/`visibleOnMap`: trạng thái entity riêng với distance;
- `positionConfidence`: `EXACT`, `ANCHOR_ONLY`, `UNKNOWN`.

### 5. Các phương án kiến trúc

#### A. Mở rộng tối thiểu record spawn hiện tại

Thêm `source`, nullable `position`, optional `reportedDistanceMeters`, `fortId` và visibility vào record hiện tại.

Ưu điểm: ít thay đổi consumer. Nhược điểm: record sẽ tiếp tục trộn map spawn và sighting; mọi consumer phải nhớ kiểm tra nhiều nullable field, dễ tạo lại bug tọa độ giả.

#### B. Một typed observation model — phương án đề xuất

Tạo model có source rõ ràng, ví dụ:

```text
MapSpawnObservation(
  encounterId,
  speciesId,
  source,
  targetPosition?,
  anchorPosition?,
  fortId?,
  spawnPointId?,
  distanceToPlayerMeters?,
  reportedDistanceMeters?,
  mapPresence,
  expiresAt?,
  confidence
)
```

Ví dụ trên chỉ là phác thảo field, chưa phải typed union thực sự. Cần tách `observationKind` (`MAP_ENTITY`/`SIGHTING`/`PROTO_TARGET`) khỏi `spawnSource` (`WILD`, `INCENSE`, `FORT_SPAWN`, `ROUTE`, `DAILY_ENCOUNTER`, `DAILY_BONUS_SPAWN`, `UNKNOWN`) và `sourceSubtype`. `NEARBY_SIGHTING` là loại observation, không phải nguồn sinh Pokémon. Một raw proto có lat/lng cũng chưa chứng minh map entity tồn tại. Chỉ map observation có target position được xác thực và trạng thái hợp lệ mới được đưa vào action planner.

Ưu điểm: giữ semantic integrity, fail-closed và có thể mở rộng. Nhược điểm: cần thay đổi protocol, raw decoder, core model và các consumer.

#### C. Tách hẳn hai channel

`MapSpawnsObservation` chỉ chứa entity có target position; `NearbySightingsObservation` chỉ chứa species/encounter/distance/fort anchor. Correlation về sau dùng `EncounterId` khi sighting trở thành map entity.

Ưu điểm: ranh giới rõ nhất, không thể vô tình catch sighting. Nhược điểm: UI/list cần ghép hai nguồn nếu muốn hiển thị một danh sách chung.

Khuyến nghị dùng **B ở domain boundary và C ở raw/runtime boundary**: decode riêng map spawn và sighting, sau đó expose typed union/domain model; không ép sighting vào record exact-position hiện tại.

### 6. Data flow đề xuất

```text
Map RPC / IL2CPP runtime
        |
        +--> WildPokemon / WildMapPokemon -------- exact target position
        +--> CatchablePokemon --------------------- target position; source chưa biết
        +--> Fort.ActivePokemon/ActiveFortPokemon -- fort + target position; subtype nếu có
        +--> GetIncensePokemonOutProto ------------ incense target position
        +--> AttractedPokemonClientProto ---------- context; route đã có static evidence
        +--> NearbyPokemon ------------------------- reported distance + fort reference nếu có
        |
source-specific decoder (không fabricate position)
        |
typed observation + freshness + confidence
        |
player position cùng observation --> Haversine distance nếu target exact
        |
Map target planner: exact position + valid lifecycle + in-scope map entity
Nearby UI: có thể hiển thị sighting nhưng không coi là catch target
```

Identity nên ưu tiên `EncounterId`; nếu cùng encounter xuất hiện ở nearby và map channel, merge/update chỉ khi observation mới có target position và freshness hợp lệ. Không overwrite exact target bằng fort anchor.

### 7. Verification plan

Offline:

- Fixture `WildPokemonProto`: kiểm tra `WILD`, exact lat/lng, `SpawnPointId`, expiry và distance.
- Fixture `NearbyPokemonProto`: kiểm tra giữ `DistanceMeters`/`FortId`, `targetPosition == null`, không tạo tọa độ fort thành target.
- Fixture `PokemonFortProto` có `ActivePokemon` và `ActiveFortPokemon` với `Lure`, `PowerUp`, `DayNight`: kiểm tra source/subtype và expiry. JAR hiện tại thiếu enum tên `DAY_NIGHT`; dùng numeric value `3` qua `setSpawnTypeValue(3)` và kiểm tra giữ nguyên raw value; xem mục 10.3.
- Fixture `GetIncensePokemonOutProto`: kiểm tra `INCENSE`, exact lat/lng, `DisappearTimeMs`.
- Fixture route/daily: đảm bảo không rơi âm thầm vào `WILD`.
- Kiểm tra invalid/stale/incomplete observation không đi vào action planner.

Runtime read-only trên đúng build/ABI:

- Đối chiếu `MapEntityService.Cells` với `AllCellWildPokemon`, `NearbyPokemon` và tappables.
- Kiểm tra `MapPokemon.Location`, `SourceType`, `DespawnTime`, `WildMapPokemon.VisibleOnMap` ở các khoảng cách khác nhau.
- Đối chiếu player position, fort position, `NearbyPokemonProto.DistanceMeters` và Haversine để xác định anchor của distance.
- Kiểm tra vòng đời khi Pokémon có trong nearby list nhưng chưa/không còn có `MapPokemon` entity.

### 8. Acceptance Criteria

> Không tìm thấy formal spec riêng cho yêu cầu này; các tiêu chí dưới đây là **inferred — needs BA confirm** từ ticket/user và reverse output.

| ID | Tiêu chí | Kết quả mong đợi |
|---|---|---|
| WS-1 | Wild map spawn | Có `WILD`, exact target position, identity ổn định, expiry nếu proto có. |
| WS-2 | Incense spawn | Nhận diện `INCENSE` từ nguồn đã xác nhận, không mặc định mọi `AttractedPokemonClientProto` là incense; giữ vị trí và disappearance time. |
| WS-3 | Fort spawn | Có `fortId`, target position riêng và subtype nếu nguồn cung cấp; giữ numeric subtype chưa nhận diện, không suy `ActivePokemon` thành `LURE`. |
| WS-4 | Nearby sighting | Không fabricate target position từ fort; giữ `reportedDistanceMeters`, `fortId`, encounter và `positionConfidence = UNKNOWN/ANCHOR_ONLY`. |
| WS-5 | Distance exact | Khi player và target position cùng snapshot hợp lệ, tính `distanceToPlayerMeters` bằng Haversine. |
| WS-6 | Visibility vs distance | Không coi `distance <= threshold` là bằng chứng duy nhất rằng Pokémon đang hiện/catchable trên map. |
| WS-7 | Planner safety | Chỉ target có exact position, freshness/lifecycle hợp lệ và map presence phù hợp mới được gửi sang action planner. |
| WS-8 | Các loại bổ sung | Route/daily/đặc biệt được tag riêng hoặc bị loại có chủ đích; không silent fallback thành `WILD`. |
| WS-9 | Protocol compatibility | Payload mới hỗ trợ optional position/source/distance/expiry mà không làm hỏng runtime payload cũ. |
| WS-10 | Verification | Unit fixtures pass, runtime binding read-only xác nhận field offsets/lifecycle, `git diff --check` sạch. |

### 9. Open questions cần runtime xác minh

- `NearbyPokemonProto.DistanceMeters` là distance tới fort, tới điểm spawn ẩn hay một anchor đã được server quy ước?
- `WildMapPokemon.VisibleOnMap` được set ở layer nào và có nghĩa là render-visible, selectable hay chỉ còn trong map entity collection?
- `CatchablePokemon` trong raw RPC có phải là toàn bộ fort spawn hiện hành không, hay còn source được tạo sau đó bởi `MapPlacePokemonSpawner`?
- Incense thường/daily/event dùng những owner và lifecycle nào? Static evidence hiện có gắn `AttractedPokemonClientProto` với route; chưa có bằng chứng nó là nhánh incense.
- Phạm vi scan có cần route/daily/fort power-up ngay phase đầu không, hay chỉ ba nhóm `WILD`/`INCENSE`/`FORT_LURE`?

---

## Synthesis

### Key Insight

Ranh giới sighting và map entity là đúng. Lỗi gán tọa độ fort đã được xác nhận trong `PogoProtoDecoder.decodeMapObjects()`, nhưng native structured nearby không đi qua decoder này. Vì vậy chưa thể quy triệu chứng trên thiết bị cho lỗi đó. Native reader hiện chỉ thu một nhánh wild, thiếu source/visibility/expiry; khoảng cách Haversine đã có ở core.

### Recommended Approach

Tách raw decoder/runtime thành map entities, proto targets và nearby sightings; giữ observation kind độc lập với source/subtype. Tái sử dụng Haversine hiện có, giữ `DistanceMeters` như giá trị server báo, xử lý enum mới ngoài schema JAR và version hóa payload. Consumer phải kiểm tra freshness/lifecycle cùng bằng chứng map presence; phạm vi này bao gồm cả native catch selector, không chỉ Kotlin planner.

### Risks to Watch

- Dùng `MapPokemonSpawnSource` như taxonomy đầy đủ sẽ làm mất incense và các source đặc biệt.
- `distanceToPlayerMeters` không chứng minh object đang render hoặc catchable.
- Nếu giữ model `position` bắt buộc và merge mọi channel vào một list, bug tọa độ giả sẽ tái diễn dưới dạng khác.

### Open Questions

- Cần runtime sample để xác định semantics chính xác của `NearbyPokemonProto.DistanceMeters` và `VisibleOnMap`.
- Cần chốt scope phase đầu: chỉ ba nhóm chính hay bao gồm route/daily.

---

## 10. Kiểm chứng lại ngày 2026-09-22

### 10.1. Phạm vi và mức độ bằng chứng

Đã đọc curated classes trước, sau đó tra các class chưa có extract trong `reverse/pogo-0.427.0/dump.cs.gz`; đối chiếu Kotlin/C++ hiện tại và dùng `javap` kiểm tra API thực của `app/libs/POGOProtos-2.60.8.jar`. Không dùng tài liệu brainstorm khác làm bằng chứng thay thế source. Không tìm thấy `docs/newspec/` hoặc `docs/specs/` trong checkout này; acceptance criteria vẫn là suy luận từ yêu cầu.

Lần này chỉ kiểm chứng tĩnh. Không chạy probe trên BlueStacks Air 1, không xác nhận field values/lifecycle trong tiến trình đang chạy. Dump được repo pin cho game `0.427.0`, version code `2026082702`, arm64-v8a; điều đó không tự chứng minh thiết bị đang chạy đúng artifact. Các script diagnostic đã đọc không cung cấp phép đối chiếu `NearbyPokemonProto.DistanceMeters`/`VisibleOnMap` cần cho câu hỏi này.

| Nhận định | Kết quả | Bằng chứng/giới hạn |
|---|---|---|
| `NearbyPokemonProto` không có target lat/lng | **Đúng** | Dump `680389` và API Java đều có distance/encounter/fort, không có latitude/longitude. |
| Decoder đang gán fort position cho sighting | **Đúng trong nhánh protobuf** | `PogoProtoDecoder.kt:62-76`; không phải đường structured nearby của native. |
| Đây chắc chắn là nguyên nhân Pokémon có trong list nhưng không hiện map | **Chưa chứng minh** | Cần xác định list nào, payload version và encounter ID của trường hợp thực tế. |
| Native đọc `AllCellWildPokemon` | **Đúng về nhóm dữ liệu, sai về accessor** | Reader đọc dictionary `eghw` trực tiếp, lọc exact class `WildMapPokemon`. |
| Runtime đã có đủ wild/incense/fort spawn | **Không** | Reader không có các nhánh incense/fort Pokémon/route/daily; đọc fort để spin không đồng nghĩa đọc Pokémon của fort. |
| Runtime thiếu source/visibility/expiry | **Đúng** | Record native chỉ có ID/species/lat/lng; snapshot có completeness và optional player position. |
| Có thể tính distance từ player | **Đúng, một phần đã triển khai** | `GeoMath` + `ScanPlanner`, player location reader và nearby codec đã có. |
| `AttractedPokemonContext.Route` là tên enum | **Sai** | C# là `AttractedPokemonRoute = 1`; Java là `ATTRACTED_POKEMON_ROUTE`. |
| `AttractedPokemonClientProto` là incense | **Không có bằng chứng** | `RouteMapPokemon.Initialize(AttractedPokemonClientProto)` đã có; incense nhận `GetIncensePokemonOutProto`. |
| `CatchablePokemon` đồng nghĩa fort/lure spawn | **Chưa chứng minh** | Đây là list `MapPokemonProto`, không có field source/fort ID trong từng record. |
| `MapPokemonSpawnSource` đủ để phân loại mọi nguồn | **Không** | Enum có năm giá trị đã nêu; `SetMapPokemonRing` nhận enum này. `SourceType` là kiểu `Item`, không phải enum đó. |
| Fort subtype chỉ có lure/power-up/day-night | **Thiếu** | Dump còn `NaturalArt = 2` có `[Obsolete]`; JAR hiện tại còn thiếu cả tên `NATURAL_ART` và `DAY_NIGHT`. |
| Tọa độ có thật chứng minh entity đang render/catchable | **Không** | Dữ liệu proto, tồn tại object, render visibility và khả năng action là các điều kiện khác nhau. |

### 10.2. Phạm vi chính xác của lỗi tọa độ và distance

Hai đường xử lý hiện có:

```text
NEARBY / OBSERVATION_PAYLOAD_VERSION
  -> PogoProtoDecoder.decodeMapObjects()
  -> WildPokemon giữ tọa độ proto; NearbyPokemon lấy tọa độ fort
  -> RawNearbyObservation

NEARBY / RUNTIME_NEARBY_PAYLOAD_VERSION (= 2)
  -> RuntimeNearbyPayloadCodec.decode()
  -> record native đọc từ WildMapPokemon / WildPokemonProto
  -> RawNearbyObservation

REQUEST_CATCH_SPIN
  -> RuntimeCatchSpinPayloadCodec
  -> RuntimeNearbyPayloadCodec cho nearby blob
```

Nguồn: `game-adapter/pogo/src/main/kotlin/dev/pogoroot/automation/pogo/BridgePogoRuntime.kt:232-274`, `RuntimeCatchSpinPayloadCodec.kt:37-44`. Native push observer và native catch/spin snapshot dùng cùng map reader; xem `zygisk/jni/shared/runtime/observation/runtime_observation.inc:216-240` và `zygisk/jni/modules/catch_spin/coordinator.inc:60-86`.

Trong decoder protobuf, sighting chỉ được thêm khi encounter chưa có trong map kết quả và tìm được fort **trong cùng cell**. Không có fort thì bị bỏ qua. Wild record được ưu tiên khi trùng encounter ID. Vì vậy lỗi đã chứng minh là “sighting có fort bị biến thành spawn có tọa độ anchor”; không phải “mọi nearby record đều bị gán tọa độ fort”. `DistanceMeters` bị bỏ mất trong decoder này.

Trong native reader, helper ưu tiên `WildMapPokemon.egws` → `EncounterOutProto.Pokemon`, rồi fallback `egwr`; phải có `WildPokemonProto` hợp lệ. Nhánh chỉ có tappable data không tự được hỗ trợ chỉ vì object mang tên `WildMapPokemon`. Nguồn: `zygisk/jni/shared/runtime/observation/runtime_observation.inc:17-73`.

Distance hiện có:

- `core/src/main/kotlin/dev/pogoroot/automation/core/location/GeoMath.kt:40`: Haversine.
- `core/src/main/kotlin/dev/pogoroot/automation/core/scan/ScanCriteria.kt:77`: tính distance, loại candidate thiếu player position khi có `maxDistanceMeters`.
- `zygisk/jni/shared/runtime/map/runtime_map_forts.inc:43`: chỉ đọc player location khi binding và `HasValidLocation` hợp lệ.
- `zygisk/jni/shared/runtime/map/runtime_map_reader.inc:392`: đọc player trước khi duyệt các cell trong cùng lần scan trên main thread.

“Cùng lần scan” chưa chứng minh player location và proto spawn có cùng timestamp gốc: proto/encounter có thể là dữ liệu cache. Cần giữ tuổi dữ liệu hoặc freshness theo nguồn khi yêu cầu distance đủ tin cậy cho action. `EXACT` trong đề xuất chỉ nên có nghĩa là tọa độ của target do nguồn đã xác nhận cung cấp; không hàm ý sai số vật lý bằng 0 hay server chắc chắn cho bắt.

Ngoài ra, native `select_catch_target()` hiện chọn ID nhỏ nhất chưa request sau kiểm tra available/complete/coordinate, **không tính khoảng cách tới Pokémon** và không đọc visibility/expiry. Nguồn: `zygisk/jni/modules/catch_spin/requested_catch_ids.inc:3-15`. Haversine ở `ScanPlanner` không chứng minh native auto-catch đã có distance gate. Khi triển khai WS-7 phải rà cả consumer native này.

### 10.3. Schema và taxonomy cần điều chỉnh

`game-adapter/pogo/build.gradle.kts:10-17` sử dụng `POGOProtos-2.60.8.jar`. Kiểm tra trực tiếp bằng `javap` cho thấy:

| Nguồn | Fort spawn subtype |
|---|---|
| Dump game 0.427.0, dòng `829345` | `Lure = 0`, `PowerUp = 1`, `NaturalArt = 2` (obsolete), `DayNight = 3` |
| `POGOProtos.Rpc.FortPokemonProto$SpawnType` trong JAR | `LURE`, `POWER_UP`, `UNRECOGNIZED`; không có tên enum cho 2/3 |

Java proto có `getSpawnTypeValue()` và builder có `setSpawnTypeValue(int)`. Vì vậy có thể tạo fixture numeric `2`/`3` và giữ raw value ngay cả khi enum getter trả về loại chưa nhận diện. Mapping tên cho numeric value phải nằm trong adapter có ràng buộc phiên bản; không mặc định unknown thành `LURE` hoặc `WILD`. Fixture test cũng cần JAR ở test runtime classpath; dependency hiện là `compileOnly`, chưa có cấu hình test riêng ở module này.

Ba trục đề xuất sau kiểm chứng:

- **Loại observation:** sighting / proto có tọa độ target / map entity đã xác nhận. Nguồn raw `WildPokemon`, `CatchablePokemon`, `NearbyPokemon`, native object nên được giữ để truy vết.
- **Nguồn spawn:** wild / incense / fort / route / daily / unknown; thêm subtype theo nguồn khi có bằng chứng. Riêng `ActivePokemon` thiếu wrapper subtype; `CatchablePokemon` thiếu cả fort ID trực tiếp.
- **Trạng thái:** target/anchor position, freshness, map presence, visibility và expiry confidence; không suy một field từ field khác khi thiếu dữ liệu.

Nếu dùng typed union, cần biến thể riêng với invariant rõ ràng, ví dụ `NearbySighting`, `ProtoSpawnCandidate`, `VerifiedMapSpawn`. Một data class chung có nhiều nullable field như phác thảo B không tự tạo được bảo đảm kiểu dữ liệu. Đây là điều chỉnh thiết kế đề xuất, chưa phải cấu trúc đang có trong repo.

### 10.4. Completeness, expiry và tương thích payload

`isComplete` hiện yếu hơn “đã đọc đủ local map entities”:

- Native khởi tạo `true`, chỉ chuyển `false` ở kiểm tra giới hạn 512 spawn trước mỗi cell (`runtime_map_reader.inc:340`, `425-429`). Object sai class hoặc helper đọc thất bại bị `continue` mà không hạ completeness (`169-195`).
- `read_map_wild_pokemon()` không chặn số lượng ngay trong vòng lặp từng spawn. Nếu một cell làm tổng vượt 512, validator có thể reject toàn snapshot; không nên mô tả đây là cơ chế truncation đầy đủ.
- Protobuf decoder không đọc `ClientMapCellProto.IsTruncatedList`, mặc dù field/getter có ở cả dump và JAR; `RawNearbyObservation.isComplete` giữ mặc định `true`.

Do đó cần định nghĩa scope và biểu diễn skipped/partial/truncated một cách kiểm chứng được; không chỉ đổi nhãn scope rồi coi boolean hiện tại là chính xác. `isComplete` cũng không thay thế freshness.

Về expiry: dump xác nhận `TimeTillHiddenMs`, `ExpirationTimeMs`, `DisappearTimeMs` và `MapPokemon.DespawnTime` tồn tại. Nó chưa xác nhận epoch/timebase, sentinel values hay “hidden” có trùng “despawn/catch unavailable” không. Không được cộng `observedAtEpochMs + TimeTillHiddenMs` rồi gán confidence `EXACT` chỉ từ tên field, nhất là khi đọc proto đã cache. Hiện cả `PogoProtoDecoder` và `RuntimeNearbyPayloadCodec` đặt expiry `null/UNKNOWN`.

Về WS-9: `RuntimeNearbyPayloadCodec.kt:59` yêu cầu hết payload sau phần player position. Append source/expiry tùy ý vào v2 sẽ bị decoder hiện tại từ chối. Cần schema/version mới hoặc envelope có extension đã thỏa thuận, cập nhật sender/broker/receiver và kiểm tra cả nearby blob lồng trong catch/spin. Khả năng decode payload thiếu optional tail hiện tại không đồng nghĩa reader cũ chấp nhận mọi field mới.

### 10.5. Acceptance Criteria bổ sung sau kiểm chứng

> Nguồn: yêu cầu kiểm chứng hiện tại và các bằng chứng source ở trên. Đây vẫn là **inferred — needs BA confirm**, dùng cho triển khai tương lai; không phải báo cáo các tiêu chí đã pass.

| ID | Tiêu chí | Kết quả có thể kiểm tra |
|---|---|---|
| WS-11 | Giữ enum ngoài schema JAR | Numeric `2`, `3` và một giá trị chưa biết được giữ nguyên; không rơi về `LURE`/`WILD`; fixture dùng raw numeric API khi thiếu tên enum. |
| WS-12 | Tách observation kind và spawn source | Nearby sighting không phải spawn source; raw lat/lng không tự đặt map presence/visibility thành true. |
| WS-13 | Completeness có phạm vi rõ | Fixture raw truncated, object đọc thất bại, vượt giới hạn không được quảng bá là đầy đủ; consumer xử lý partial theo contract. |
| WS-14 | Expiry có timebase và confidence | Relative duration, sentinel hoặc timestamp chưa xác minh không được chuyển thành expiry `EXACT`; dữ liệu cache có test riêng. |
| WS-15 | Khoanh vùng nguyên nhân trên thiết bị | Ghi payload type/version, encounter ID, nguồn tọa độ và trạng thái entity cho cùng trường hợp; chỉ quy lỗi protobuf nếu observation thực sự đi qua nhánh đó. |

Liên hệ với tiêu chí ban đầu: WS-4 được hỗ trợ bởi bằng chứng lỗi code nhưng chưa được sửa; WS-5 đã có phép tính song chưa chứng minh freshness cùng thời điểm; WS-1/2/3/6/7/8/9/10 chưa hoàn tất theo yêu cầu mở rộng. Các fixture liệt kê ở mục 7 là kế hoạch, không phải test có sẵn đã chạy thành công.

### 10.6. Rà lại các câu hỏi kiến trúc

| Câu hỏi | Kết luận sau kiểm chứng |
|---|---|
| Vấn đề cần giải quyết | Giữ đúng semantic target/sighting và nguồn spawn; trước hết xác định đường dữ liệu gây triệu chứng. |
| Ràng buộc | Giữ boundary theo build/ABI, capability/freshness/fail-closed; không fabricate position; device work qua script hiện có, đúng BlueStacks Air 1. |
| Thuộc tính chất lượng | Ưu tiên tính đúng và kiểm thử được, rồi khả năng bảo trì, cuối cùng mở rộng coverage; chưa có phép đo để kết luận chi phí runtime. |
| Phương án | A/B/C vẫn là các lựa chọn ở mục 5; bổ sung biến thể proto target để không đồng nhất raw response với object hiện hữu. |
| Đánh đổi | B+C bảo vệ invariant tốt hơn khi thực sự là typed union, đổi lại phải migrate codec và nhiều consumer; nullable record đơn thuần không có bảo đảm này. |
| Điểm tích hợp | Reverse binding/native reader → protocol → adapter/core; có consumer Kotlin và native catch selector. |
| Luồng dữ liệu | Hai decoder NEARBY và nearby blob trong catch/spin được phân biệt ở mục 10.2; nguồn raw và source taxonomy không trùng nhau. |
| Cách kiểm thử | Fixture từng nguồn + numeric enum lạ + payload compatibility + partial/freshness; probe read-only sau đó mới xác nhận lifecycle/visibility. |
| Rủi ro | Gán anchor làm target; suy render/catchability từ coordinate; mất subtype/coverage do schema hoặc completeness không chính xác. |
| Chuyển đổi | Khoanh nguồn observation, bổ sung contract/fixtures, version hóa payload, thêm nguồn từng bước sau guard. Khi rollback, bỏ capability/nguồn chưa hỗ trợ; không quay lại fabricate tọa độ. |

### 10.7. Kết luận và câu hỏi còn mở

Hướng tách sighting khỏi map entity và không dùng fort coordinate thay target là có cơ sở. Cần sửa kết luận nguyên nhân thành lỗi **của nhánh protobuf đã chứng minh**, đồng thời coi coverage/visibility/lifecycle của native là bài toán riêng. Phần distance nên tái sử dụng code hiện có; phần source phải giữ provenance, subtype và giá trị chưa biết.

Đã giải quyết bằng source: accessor của native reader, tên enum route, sự tồn tại của Haversine/player-position transport, schema thiếu subtype trong JAR và vị trí lỗi gán tọa độ fort. Các câu hỏi `DistanceMeters` đo tới đâu, `VisibleOnMap` có nghĩa chính xác gì, coverage `CatchablePokemon`, lifecycle các biến thể incense và scope phase đầu vẫn mở; không đánh dấu là runtime-verified.

Chỉ cập nhật tài liệu trong lần kiểm chứng này. Không sửa implementation, không chạy unit/build hoặc device test; việc đọc source/dump/JAR không thay thế những bước kiểm thử đó. `git diff --check` và kiểm tra whitespace riêng cho file tài liệu chưa tracked đều sạch.
