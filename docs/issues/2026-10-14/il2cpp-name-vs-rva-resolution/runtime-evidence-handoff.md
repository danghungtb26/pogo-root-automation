# Runtime evidence handoff — BlueStacks Air 1

Ngày: 2026-10-14

Target duy nhất: `BlueStacks Air 1`, Pokémon GO `0.433.0`, versionCode
`2026100702`, package `com.nianticlabs.pokemongo`, arm64-v8a.

Trạng thái artifact hiện tại là **CODE_COMPLETE / UNVERIFIED_DEVICE**. Direct
catch, transfer và discard vẫn fail closed; không có action nào dưới đây được
coi là `LIVE_READY` chỉ vì install, socket, START/CONFIG_SET hoặc method token
pass.

Checklist runtime (chỉ user tick sau observation thực):

- [ ] A — exact identity/owner/rebind read-only pass.
- [ ] B — từng controlled action đã được user chấp nhận và chạy riêng.
- [ ] C — Promise/result và authoritative postcondition pass cho từng feature.

Handoff này thay phần binding/runtime cũ trong
[BlueStacks Air 1 user handoff](../../2026-10-13/pgs-style-zygisk-0433/bluestacks-user-handoff.md);
installer lifecycle chung của tài liệu cũ vẫn áp dụng.

## Dữ liệu cần trả về

Với mỗi bước, ghi: thời gian, session id hiển thị, trạng thái module/capability,
error/reason chính xác, generation trước/sau nếu UI/diagnostic có, thao tác vừa
thực hiện và postcondition quan sát được. Không cần gửi gameplay payload, account
identifier, Pokémon id hoặc item inventory đầy đủ.

Nếu flow pass, chỉ trả bảng PASS/FAIL/UNKNOWN và các field trên; không đọc log.
Nếu flow fail, mới thu log theo mục cuối.

## A — Read-only identity, owner và rebind

Điều kiện: dùng đúng `BlueStacks Air 1`; Magisk/Zygisk có sẵn; cài
`app/build/outputs/apk/debug/app-debug.apk` qua UI. Không bật Auto Catch,
Auto Transfer hoặc Auto Discard.

| Bước | Thao tác UI | Expected | Stop condition | Evidence cần trả |
|---|---|---|---|---|
| A1 | Mở controller, chấp nhận root/install nếu được hỏi, chỉ xác nhận reboot trong dialog | Installer đi qua trạng thái rõ ràng; không prompt loop | Sai instance, root loop, reboot ngoài xác nhận | installer state/error, displayed module version/hash |
| A2 | Sau reboot, mở controller rồi Pokémon GO 0.433.0 tới overworld | Runtime session `ACTIVE`; exact identity chỉ pass khi package/version/build ID/metadata đều khớp | Crash, package/version khác, identity mismatch | session id, exact-build status/reason, capability list |
| A3 | Không mở bag/encounter; chờ bootstrap owner | Năm method có token/signature reason; ItemBag/PokemonBag owner nếu sẵn có thuộc exact concrete class; main-thread bridge ready | Mutation bất ngờ, ambiguous/token/type/class mismatch | reason/token cho 5 method, owner class, owner generation, main-thread ready |
| A4 | Thoát hẳn Pokémon GO rồi mở lại tới overworld | Session/generation mới; descriptor/owner cũ invalidated; pending cũ không replay | Session id/generation không đổi, pending/action replay | old/new session, old/new generation, stale-descriptor invalidation status |
| A5 | Bật UI intent cho transfer/discard/direct catch nhưng không cấp calibration build | Status trả `runtime_postcondition_unverified_device`/unavailable; không mutation | Capability mutation xuất hiện hoặc gameplay thay đổi | module status/error và capability list |

Chỉ khi A1–A5 đều PASS mới xem xét phần B. `UNKNOWN` ở identity, owner lifetime,
main-thread hoặc generation được xử lý như FAIL và giữ capability đóng.

## B — Controlled action, cần user chấp nhận riêng

Artifact hiện tại cố ý để `kDirectCatchExecutionEnabled`,
`kTransferExecutionEnabled` và `kDiscardExecutionEnabled` bằng `false`; vì vậy
không chạy mutation với artifact này. Mỗi feature cần một calibration build
riêng, chỉ mở đúng một executor sau khi người dùng chấp nhận target/action cụ
thể. Không mở đồng thời ba feature và không auto-retry.

| Feature | Precondition | Một action được phép | Expected immediate result | Stop condition |
|---|---|---|---|---|
| Direct catch | A pass; một overworld target do user chọn; transfer/discard off | Một `TryCapture` | Exact owner/generation correlation; retained Promise; exact `CatchPokemonOutProto` hoặc `INDETERMINATE` | Wrong class/layout, timeout, crash, target/session đổi, action thứ hai |
| Transfer | A pass; user chỉ định đúng một Pokémon test và xác nhận mất dữ liệu | Một `ReleasePokemon` | Exact `PokemonBagImpl`; exact result or `INDETERMINATE`; không candy/UI side effect trước postcondition | Unknown storage state, timeout, wrong result, owner rebind |
| Discard | A pass; user chỉ định item và amount tối thiểu | Một `RecycleItem` | Exact `ItemBagImpl`/ItemData/expiring set; exact result or `INDETERMINATE`; retry barrier giữ đóng | Count/owner/config đổi, timeout, wrong result, reconcile không rõ |

Không coi `Invoked`, Promise completed hoặc result enum `SUCCESS` là hoàn tất.
Nếu action trả `INDETERMINATE`, dừng feature đó cho tới restart/review; không thử
lại để “xem có được không”.

## C — Promise/result và authoritative postcondition

| Feature | Result cần thấy | Postcondition bắt buộc trước `LIVE_READY` | Evidence trả về |
|---|---|---|---|
| Direct catch | Exact `CatchPokemonOutProto`, known terminal status, correlation cùng request/session/generation | Map refresh chứng minh đúng target bị remove/retained theo status; khi caught, storage refresh chứa captured id trước downstream transfer | Promise concrete class, terminal status, map refresh generation, storage observation generation |
| Transfer | Exact `ReleasePokemonOutProto.Status` | Authoritative Pokémon storage refresh chứng minh Pokémon test không còn; owner/generation không đổi giữa request và refresh | Promise/result class, status, pre/post storage generation và presence |
| Discard | Exact `RecycleItemOutProto.Result/NewCount` | `UpdateInventory` hoàn tất, exact result class, fresh inventory snapshot cùng owner generation và count khớp | Promise/result class, result/new count, cache timestamp/generation, pre/post item count |

Timeout, disconnect, wrong concrete class, generation mismatch, missing GC target,
ambiguous status hoặc refresh không chứng minh được đều là `INDETERMINATE`.
Handle phải được release, module phải ngừng retry và capability không được promote.

## Failure evidence

Chỉ sau khi manual flow fail:

```bash
./scripts/logcat-full.sh
```

Script này chọn target qua `scripts/lib/emulator.sh`; chỉ đọc file log vừa tạo
cho failure đó. Không chạy raw ADB và không chọn instance khác.

`scripts/collect-binding-diagnostics.sh` hiện còn gọi `adb` trực tiếp và chưa
dùng target selector của repository. Theo policy hiện tại, **không chạy script
này** cho tới khi nó được sửa qua task riêng; không bypass bằng lệnh thủ công.

Khi trả evidence, kèm error/reason, session/generation, bước A/B/C bị dừng và
đường dẫn log failure. Ledger/capability chỉ được cập nhật theo observation thực;
owner lifetime hoặc authoritative postcondition còn `UNKNOWN` thì trạng thái vẫn
là `UNVERIFIED_DEVICE`.
