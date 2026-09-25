# Brainstorm: Tự động đánh battle và dùng thuốc sau battle

**Type:** feature/architecture
**Date:** 2026-09-22

---

## Phạm vi và giả định

Yêu cầu người dùng được hiểu là:

- tự đánh battle bằng API/runtime nội bộ của Pokémon GO, không chạm màn hình;
- tự dùng đòn nhanh và đòn charge/special khi đủ điều kiện;
- khi battle kết thúc, tự revive Pokémon đã fainted rồi heal Pokémon chưa đầy HP;
- giữ fail-closed khi không xác định chắc chắn version, battle mode, object owner,
  turn, kết quả server hoặc kết quả dùng item.

Chưa có formal spec riêng trong `docs/specs/` hoặc `docs/newspec/`; acceptance
criteria bên dưới là **inferred — needs BA confirm** từ yêu cầu người dùng và
reverse output. “Battle” hiện còn mơ hồ: cần chốt là Gym, Raid, Team GO Rocket,
PvP hay một battle mode mới, vì dump có ít nhất hai battle stack khác nhau.

## 1. Vấn đề cần giải quyết

Runtime hiện tự động hóa map/encounter/item một phần nhưng chưa có capability,
observation hay action nào cho battle. `GameLifecycleState` chỉ có
`DISCONNECTED`, `STARTING`, `LOADING`, `OVERWORLD`, `ENCOUNTER`, `ERROR`; chưa có
`BATTLE`. `PokemonStorageSnapshot` cũng chỉ có identity/IV/metadata, chưa có
current HP, max HP hoặc `isFainted`.

Nếu dùng cách chạm màn hình để đánh, ta sẽ phải tự suy ra vị trí nút, timing và
minigame từ UI. Cách đó không phù hợp với boundary của repo: không có screenshot
inference hoặc `input tap`/`input swipe` fallback. Câu hỏi đúng là liệu client đã
có đường lệnh battle và item chính thức để gọi trực tiếp hay chưa.

## 2. Bằng chứng từ reverse output `pogo-0.427.0`

### 2.1 Battle có đường gửi action nội bộ

Có hai đường cần phân biệt:

| Stack | Reverse evidence | Ý nghĩa |
|---|---|---|
| VNext | `Niantic.Holoholo.VNext.BattleDirector.TrySubmitPlayerAction(BattleEventProto action, bool predictionEnabled = True)` tại `dump.cs.gz:1804930-1805300`, RVA `0x797E030` | Đường mạnh nhất cho việc submit action theo state/turn của client. `BattleDirectorPlayerActionStatus` trả `ACCEPTED`, `REJECTED_BY_SERVER`, `REJECTED_BY_INPUT_BLOCK`, `REJECTED_BY_INVALID_ACTION`, `BUFFERED_BY_INPUT_BLOCK`... |
| VNext transport | `BattleCommunicationService.SendPlayerAction(BattleEventProto action, bool submitTurnNumber = False)` tại `dump.cs.gz:1804602-1804814`, RVA `0x7978C74` | Có retry và promise; nên gọi qua `BattleDirector` thay vì tự gửi RPC nếu owner hợp lệ. |
| Legacy | `Niantic.Holoholo.Battle.BattleServerConnectionService.SubmitBattleAction(BattleActionProto.Types.ActionType action, ...)` tại `dump.cs.gz:332000-332500` | Có action `Attack`, `SpecialAttack`, `SpecialAttack2`, `Dodge`, `UseItem`; đây là stack khác, phải calibrate riêng. |

VNext có đủ các type để biểu diễn action:

- `BattleEventProto.Types.EventType`: `Attack`, `Dodge`, `Shield`, `SwapPokemon`,
  `Item`, `AbilityTrigger`, `BattleEnd`, `BreadMove` và các event liên quan;
- `BattleEventProto.Types.Attack`: `AttackType`, `Move`, `Type`, `TargetId`,
  `SourcePokemonId`, `Score`;
- `BattleTurnAction.SetAttack(...)`, `SetDodge(...)`, `SetShield(...)`,
  `SetItemUse(...)`, `SetSwap...` và các setter khác tại vùng `BattleTurnAction`;
- `MoveType`: `Fast`, `Charge`, `Charge2`, `Shield`, `Ability` và các move type
  khác;
- `BattleStateProto` có `Turn`, `CurrentActorId`, `State`, `UiMode`, `Pokemon`,
  `Events`, `BattleEndTurn`, `AlliedPokemonRemaining` tại
  `dump.cs.gz:607917-608080`.

Điều này cho thấy **có cơ sở kỹ thuật để tự đánh không cần touch**. Tuy nhiên,
reverse chưa chứng minh object `BattleDirector` nào đang active trên đúng device,
mode nào dùng VNext hay Legacy, cũng chưa chứng minh trực tiếp `Score` của charge
move có thể tự đặt tùy ý hay phải đi qua minigame pipeline.

### 2.2 State đủ để lập policy và nhận biết kết thúc

`BattleState` của VNext có `ActiveAllies`, `ActiveOpponents`, `AllActors`,
`PokemonList`, `LocalActor`, `CurrentTurnSyncInfo`, `BattleStatus`, `UIMode` và
`HasReceivedStateForBattle`. `BattlePokemonState` có `CurrentHealth`, `MaxHealth`,
`ProjectedHealth`, `IsFainted`, `LastReceivedHealTurn`, `FaintedTurn`.

`BattleStateProto.Types.State` có các state terminal/error rõ ràng:

- `BattleEnd`;
- `ErrorBattleEnd`;
- `ErrorUnavailableBattle`, `ErrorUnavailableTurn`,
  `ErrorUnavailableItem`, `ErrorUnavailablePokemon`,
  `ErrorUnavailableResource`.

`BattleStateExt.HasBattleEndEvent(BattleStateProto state)` là một tín hiệu có thể
dùng cùng với state/status, nhưng không nên dùng riêng một event UI để kết luận
đã an toàn gọi item. Cần chờ kết quả authoritative và trạng thái game ổn định.

### 2.3 Client có API chính thức để revive/heal

`IItemBag` tại `dump.cs.gz:26741` có:

```text
GetItemCount(Item itemType)
UseItemOnPokemon(Item item, ulong pokemonId)
UseItemsOnPokemon(Item item, int count, ulong pokemonId)
UseMedicineOnPokemons(Item item, List<PokemonProto> pokemons)
```

`ItemBagImpl` (`TypeDefIndex: 1871`) có các RVA:

- `UseItemsOnPokemon`: `0x816520C`;
- `UseItemOnPokemon`: `0x81658AC`;
- `UseMedicineOnPokemons`: `0x816659C`.

Item enum có `Potion=101`, `SuperPotion=102`, `HyperPotion=103`, `MaxPotion=104`,
`Revive=201`, `MaxRevive=202`. `PotionAttributesProto` có `StaPercent` và
`StaAmount`; `ReviveAttributesProto` có `StaPercent`.

Có cả `UseItemBulkHealProto` với danh sách `PokemonId`, và response chứa status,
per-Pokémon result, `RemainingItemCount`. Tuy nhiên, phase đầu nên gọi
`IItemBag.UseItemOnPokemon` tuần tự để dễ reconcile kết quả và tránh bulk request
khi danh sách storage/HP còn chưa được verify.

Nên gọi interface/client-owned service qua main-thread bridge, không forge trực
tiếp `UseItemPotion`/`UseItemRevive` RPC. `ItemBagImpl` có inventory prediction,
callback và reconciliation; bỏ qua lớp này dễ tạo state giả hoặc gửi item khi
inventory đã thay đổi.

## 3. Ai hưởng lợi và use case

Người dùng chính là operator chạy automation headless/rooted trên một build Pokémon
GO đã được verify. Use case bắt buộc cho MVP:

1. Đang ở battle mode được hỗ trợ, runtime đọc battle state mới và xác nhận local
   actor/active Pokémon.
2. Khi đến lượt và input không bị block, tự submit fast attack hợp lệ.
3. Khi đủ energy và policy cho phép, tự submit charge/special move hợp lệ.
4. Khi Pokémon faint, thực hiện swap/tiếp tục hoặc kết thúc theo state thật của
   game; không tự đoán team còn sống.
5. Khi server báo battle đã kết thúc, đọc inventory/storage mới, revive Pokémon
   fainted, sau đó heal Pokémon còn thiếu HP.
6. Mỗi mutation phải chờ kết quả authoritative hoặc chuyển sang
   `INDETERMINATE` và dừng chuỗi.

Nice-to-have, không nên đưa vào MVP đầu: dodge theo telegraph, shield, tự chọn
team, tối ưu move theo type/energy, swap chiến thuật, retry khi timeout, bulk heal,
và hỗ trợ đồng thời nhiều battle mode.

## 4. Luồng đề xuất

```text
Runtime identity + exact build/ABI verified
        |
        v
Read-only battle probe
        |
        +--> VNext BattleDirector/BattleState
        |       |
        |       +--> current turn + active actor + HP/energy + move data
        |       +--> TrySubmitPlayerAction(BattleEventProto)
        |
        +--> Legacy BattleServerConnectionService
                |
                +--> SubmitBattleAction(BattleActionProto)

BattleEnd + authoritative result + stable post-battle lifecycle
        |
        v
Read Pokemon state + item counts
        |
        +--> Revive fainted: MaxRevive/Revive, từng Pokémon, chờ result
        |
        +--> Heal non-full: MaxPotion/.../Potion, từng Pokémon, chờ result
        |
        v
Resync inventory/storage, emit completed/partial/indeterminate
```

Khuyến nghị target **VNext trước**, nhưng chỉ sau khi read-only probe chứng minh
mode thực tế dùng `BattleDirector`. Nếu mode người dùng cần lại là Legacy, phải
đặt một binding riêng; không dùng nhầm VNext action payload.

## 5. Thiết kế kỹ thuật đề xuất

### 5.1 Domain và adapter boundary

Thêm capability riêng, không tái dùng `ENCOUNTER` hoặc `OVERWORLD`, ví dụ:

- `READ_BATTLE_STATE`;
- `SUBMIT_BATTLE_ACTION`;
- `USE_BATTLE_MEDICINE`.

Thêm lifecycle `BATTLE` và model typed cho battle snapshot, tối thiểu gồm battle
identity/session, mode, turn/serial, local actor, active Pokémon, opponent,
current/max/projected HP, energy, fainted, available moves, input-blocked,
freshness và terminal/error state. `PokemonStorageSnapshot` hiện không đủ để suy
ra HP sau battle; cần một observation/storage model mới hoặc mở rộng có chủ ý.

Action layer cần tách rõ:

- `SubmitBattleFastMove`;
- `SubmitBattleChargeMove`;
- `SwapBattlePokemon`/`UseBattleShield` nếu phase sau;
- `RevivePokemon`;
- `HealPokemon`.

Không để `BattleEventProto` hoặc `BattleActionProto` lọt vào `core/`; chúng thuộc
version-specific adapter/native binding.

### 5.2 Native binding và main-thread rules

Binding phải resolve owner/service sau khi game đã load ổn định, giống boundary hiện
tại của `ItemBagImpl`:

- package/version/ABI/build fingerprint exact;
- `strongIdentityVerified` và capability allowlist;
- Zenject owner/service type exact;
- method signature/field layout/enum value guards;
- main-thread invocation qua `runtime_main_thread_bridge.inc`;
- promise/callback hoặc state transition dùng để xác định outcome.

Không enumerate managed object hoặc gọi `TrySubmitPlayerAction`/`UseItemOnPokemon`
từ worker thread. `docs/DATAFLOW_OVERVIEW.md` và `docs/RUNTIME_BRIDGE.md` đã ghi rõ
Unity object access phải marshal qua main thread; repo từng có crash khi gọi managed
diagnostic quá sớm trong `il2cpp_init`.

### 5.3 Battle policy

MVP policy nên deterministic:

1. Chỉ action khi snapshot còn mới, đúng battle session, đúng local actor và đúng
   turn/serial.
2. Nếu input bị block, buffer tối đa theo contract của `BattleDirector`; không tự
   gửi lặp ngoài client policy.
3. Fast move dùng move ID/source Pokémon/target/turn mà state vừa cung cấp.
4. Charge move chỉ gửi khi move có đủ energy và action type khớp; `Score` không được
   fabricate cho đến khi live calibration chứng minh semantics của minigame.
5. Một action pending tại một thời điểm; server reject/invalid/unknown thì dừng
   battle automation và phát lỗi.
6. Khi active Pokémon faint, chỉ swap khi local state báo swap hợp lệ; không tự
   suy ra ID từ list cũ.

### 5.4 Hồi phục sau battle

Thứ tự đề xuất:

1. Xác nhận terminal state (`BattleEnd`/kết quả mode tương ứng), battle session đã
   đóng và game không còn ở `BATTLE` active.
2. Đọc lại Pokémon state authoritative; lấy danh sách `isFainted` và HP hiện tại.
3. Chọn `MaxRevive` trước `Revive` nếu có, nếu không thì dùng `Revive`. Mỗi request
   một Pokémon, chờ promise/observation update rồi mới gửi request tiếp.
4. Sau khi revive xong, chọn potion theo chính sách cấu hình và item count; mặc
   định `MaxPotion` rồi `HyperPotion`, `SuperPotion`, `Potion`.
5. Chỉ heal Pokémon có HP < max HP; không gửi heal cho Pokémon đang fainted hoặc
   ID không còn trong storage.
6. Nếu thiếu item, response lỗi, timeout hoặc inventory update không khớp, dừng
   và trả kết quả `PARTIAL`/`INDETERMINATE`; không retry mù.

`UseMedicineOnPokemons`/bulk heal có thể là tối ưu sau này khi fixture và runtime
calibration xác nhận thứ tự, response và semantics của danh sách. Per-Pokémon call
an toàn hơn cho MVP vì dễ đảm bảo idempotency và reconcile.

## 6. Các phương án và lựa chọn

| Phương án | Ưu điểm | Nhược điểm | Quyết định |
|---|---|---|---|
| Touch/screenshot/input event | Dễ nhìn và prototype | Fragile, phụ thuộc UI/timing, vi phạm boundary fail-closed của repo | Loại |
| Forge RPC `BattleEventProto`/`UseItemPotion` trực tiếp | Ít phụ thuộc UI | Bỏ qua state machine/prediction/client validation; dễ desync hoặc bị reject | Không khuyến nghị |
| Gọi `BattleDirector`/`IItemBag` qua managed owner chính thức | Dùng đúng client state, promise, prediction và result | Cần binding exact, owner discovery và main-thread bridge | **Khuyến nghị** |
| Chỉ tự động heal sau battle, chưa tự đánh | Rủi ro thấp hơn, deliver sớm | Chưa đáp ứng phần auto battle | Có thể làm milestone đầu |
| Hỗ trợ cả VNext và Legacy ngay | Bao phủ nhiều mode | Tăng mạnh binding/verification surface | Chưa nên làm MVP |

## 7. Edge cases và rủi ro

- Battle state stale, thiếu `CurrentActorId`, thiếu move/energy hoặc turn nhảy bất
  thường: không gửi action.
- `REJECTED_BY_INPUT_BLOCK`, `REJECTED_BY_SERVER`, `REJECTED_BY_INVALID_ACTION`,
  null promise/exception: dừng hoặc chuyển indeterminate theo từng status; không
  gửi lại vô hạn.
- Battle kết thúc do timeout, disconnect, surrender, error hoặc app background:
  không tự dùng thuốc nếu chưa phân biệt terminal thật và state tạm.
- Có nhiều battle instance/party member: phải khóa đúng local actor và battle ID.
- Pokémon faint nhưng storage snapshot cũ, ID `ulong`/`String` decode sai hoặc
  chưa nhận update: không gọi item.
- Inventory thay đổi giữa read và call: rely on `IItemBag` result; cập nhật lại
  snapshot sau mỗi mutation.
- Max potion/revive hết, potion partial heal, Pokémon deployed ở gym hoặc item
  bị server cấm: đánh dấu partial, không retry mù.
- App/game thoát trong lúc promise pending: command là indeterminate; session mới
  không được tiếp tục chuỗi cũ.
- Version/ABI/IL2CPP layout thay đổi: capability không được advertise và module
  phải trở về read-only.
- Automation battle có thể vi phạm ToS/anti-cheat hoặc tạo rủi ro tài khoản; đây
  là rủi ro vận hành cần người dùng tự chấp nhận.

## 8. Dependencies và ảnh hưởng repo

Các lớp cần mở rộng, theo thứ tự:

1. `core/`: battle snapshot, medicine policy/result, lifecycle và action models;
2. `bridge/protocol/`: observation/action payload version mới, command result,
   freshness/session fields;
3. `game-adapter/api/`: capabilities và action contracts;
4. `game-adapter/fake/`: deterministic battle state/action/medicine fixtures;
5. `game-adapter/pogo/`: decoder, mapper, capability gate, lifecycle validation;
6. `zygisk/jni/`: version-scoped battle probe, `BattleDirector`/legacy owner,
   `IItemBag` action bridge, result observation và main-thread tasks;
7. app/controller: config, runner/coordinator, logs/status và policy lock.

Không nên sửa `GameCapability.kt` hoặc các file đang dirty chỉ để minh họa trước
khi scope battle mode và binding strategy được chốt. Mỗi trách nhiệm nên tách thành
file nhỏ để giữ giới hạn 500 dòng/source file của repo.

## 9. Acceptance Criteria

> Các tiêu chí dưới đây là **inferred — needs BA confirm** vì chưa có formal spec.

| ID | Tiêu chí | Kết quả mong đợi |
|---|---|---|
| AB-01 | Exact runtime gate | Chỉ advertise battle capability khi package/version/ABI/build fingerprint, owner và method guards đều pass. |
| AB-02 | Battle observation | Có snapshot fresh chứa battle ID/mode, local actor, active Pokémon, turn/serial, HP/energy/move/input-blocked và terminal/error state. |
| AB-03 | Không touch UI | Fast attack được submit bằng client battle API; không screenshot inference, `input tap` hoặc `input swipe`. |
| AB-04 | Fast move | Action chỉ gửi đúng local actor, active Pokémon, target, move, turn/serial; accepted/rejected được phản ánh rõ. |
| AB-05 | Charge/skill | Charge/special/ability chỉ gửi khi state cho phép; không fabricate `Score`/minigame result trước live calibration. |
| AB-06 | Battle terminal | Kết thúc chỉ được xác nhận từ authoritative battle state/result, không từ timeout UI đơn thuần. |
| MED-01 | Revive | Sau terminal battle, từng Pokémon fainted được revive bằng item có sẵn, chờ outcome và resync sau mỗi call. |
| MED-02 | Heal | Sau revive, từng Pokémon HP < max được heal bằng potion có sẵn; không heal Pokémon fainted/không xác định. |
| MED-03 | Medicine safety | Null promise, error, timeout, stale ID, thiếu item hoặc inventory mismatch làm chuỗi dừng/partial/indeterminate, không retry mù. |
| MED-04 | Không nhầm lifecycle | Medicine không chạy khi battle còn active, session sai, runtime mất strong identity hoặc lifecycle chưa ổn định. |
| VER-01 | Offline verification | Fake adapter/decoder có fixture cho fast/charge/reject/battle-end/revive/heal/partial/error. |
| VER-02 | Runtime verification | Read-only probe và một mutation tối thiểu được xác nhận trên đúng target `BlueStacks Air 1`, đúng build/ABI, qua script repo. |
| VER-03 | Regression | Focused tests, `./gradlew test assembleDebug` khi môi trường cho phép và `git diff --check` đều pass. |

## 10. Open questions cần chốt trước khi code

1. “Battle” đầu tiên là Gym, Raid, Team GO Rocket, PvP hay mode khác?
2. “Skill” nghĩa là charge move, special attack, trainer ability, shield hay tất cả?
3. Có cần dodge/swap/shield không, hay MVP chỉ fast + charge attack?
4. Sau battle có heal toàn bộ team hay chỉ Pokémon vừa tham chiến?
5. Chọn potion theo Max trước hay dùng item yếu trước để tiết kiệm Max Potion?
6. Khi battle thua/timeout/disconnect, có được tự heal/revive không?
7. Có cho phép bật auto-battle độc lập với auto-heal không?
8. Cần giữ audit log cho từng action/item và giới hạn số request mỗi battle như thế
   nào?

---

## Synthesis

### Kết luận chính

**Có hướng làm khả thi ở mức reverse feasibility.** Dump có
`BattleDirector`/`BattleCommunicationService` để submit `BattleEventProto`, có
legacy `BattleServerConnectionService` cho action cũ, và có `IItemBag` chính thức
để revive/heal. Nhưng repo hiện chưa có battle binding, lifecycle, observation hay
medicine action; vì vậy chưa nên bật trực tiếp trên live build.

### Khuyến nghị triển khai

Làm theo milestone: (1) read-only battle probe trên đúng một mode/build; (2) fast
attack; (3) charge move sau khi xác minh minigame/score; (4) terminal detection;
(5) per-Pokémon revive rồi heal qua `IItemBag`; (6) sau cùng mới cân nhắc dodge,
shield, swap và bulk heal.

### Mức độ tự tin

- Protocol/API tồn tại: **cao** dựa trên reverse output.
- Chọn đúng battle stack/mode trên thiết bị: **chưa biết**, cần runtime read-only
  verification.
- Tự động charge/skill không chạm UI: **khả thi nhưng cần calibration**, đặc biệt
  `Score`, timing, turn, serial và input-block behavior.
- Tự revive/heal sau battle: **khả thi cao**, nếu storage state và
  `ItemBagImpl` owner được bind, promise/result được observe và gọi đúng main
  thread.
