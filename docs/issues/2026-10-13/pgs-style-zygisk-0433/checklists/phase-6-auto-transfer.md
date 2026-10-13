# Phase 6 — Auto Transfer

Plan: [Tổng quan](00-overview.md)

## Điều kiện bắt đầu

- `T-503`/`T-506` hoàn tất code path catch terminal event và captured storage ID
  có provenance; live outcome còn `UNVERIFIED_DEVICE`.
- `T-304` có contract `GetPokemon`/`ReleasePokemon` và Promise/postcondition.
- Action arbiter và storage refresh từ Phase 4 đang hoạt động.

## Task

- [ ] **T-601** **[zygisk]** *(native/android)* — Hoàn thiện binding và policy contract trong `modules/transfer/metadata.inc`, `execute.inc`, `config.inc`, `AutomationConfig.kt`, `RuntimeDesiredStateMapper.kt` và transfer protocol/dispatcher: resolve `IPokemonBag.GetPokemon(id)`/`PokemonBagImpl.GetPokemon(id)` và `ReleasePokemon(pokemon)` bằng tuple/type/arity; đọc metadata cần cho keep policy (favorite, shiny, legendary, mythical, special/unknown). Field chưa đọc được phải giữ Pokémon, không mặc định transfer. **AC:** Policy và config persist/replay đúng, method binding đúng 0.433.0 theo offline evidence, unknown classification fail closed và live status `UNVERIFIED_DEVICE`. **Phụ thuộc:** `T-304`, `T-405`, `T-506`. **Verify:** Native/Gradle existing checks; encode/decode config revision; static binding/type/policy review và build, không lookup/release trên device.

  <details>
  <summary>Chưa thực hiện — T-601</summary>

  Chưa có thay đổi hoặc kết quả kiểm chứng. Agent cập nhật khối này ngay sau khi xử lý task, trước khi sang task tiếp theo.

  </details>

- [ ] **T-602** **[zygisk]** *(native)* — Sửa `modules/transfer/coordinator.inc` để chỉ enqueue từ terminal catch-success event cùng session có captured storage ID, không phụ thuộc catch strategy; đợi direct hoặc Virtual GO Plus map/storage sync nhả arbiter rồi mới gọi `GetPokemon`. Re-evaluate config revision/keep policy ngay trước dispatch và dedupe theo `(session, storageId)` xuyên strategy. Không scan bag định kỳ để tự transfer Pokémon cũ. **AC:** Auto Transfer code gắn đúng Pokémon vừa bắt, không chạy song song catch/spin/discard và không replay qua reconnect. **Phụ thuộc:** `T-406`, `T-503`, `T-506`, `T-601`. **Verify:** Host-controlled action timeline của từng strategy chứng minh catch success → lookup → policy → release; duplicate/late/stale/cross-strategy event bị drop, không chạy device.

  <details>
  <summary>Chưa thực hiện — T-602</summary>

  Chưa có thay đổi hoặc kết quả kiểm chứng. Agent cập nhật khối này ngay sau khi xử lý task, trước khi sang task tiếp theo.

  </details>

- [ ] **T-603** **[zygisk]** *(native)* — Hoàn thiện `release_promise_observer.inc`, `promise_observer.inc` và storage postcondition: giữ Promise bằng GC handle, parse `ReleasePokemonOutProto`/server result theo evidence, mark storage/inventory dirty, yêu cầu `GetPokemon(storageId)` absent hoặc bag revision đổi; timeout trả `INDETERMINATE` và suppress retry cho ID đó tới refresh authoritative. UI/capability ghi `UNVERIFIED_DEVICE`. **AC:** Code không báo transferred khi mới dispatch; failure/kept/unknown/indeterminate là outcome riêng và không double-release. **Phụ thuộc:** `T-601`, `T-602`. **Verify:** Native host checks/build cho state machine, keep/no-dispatch, timeout/dedupe/postcondition gates; không release Pokémon trên device.

  <details>
  <summary>Chưa thực hiện — T-603</summary>

  Chưa có thay đổi hoặc kết quả kiểm chứng. Agent cập nhật khối này ngay sau khi xử lý task, trước khi sang task tiếp theo.

  </details>

### T-604 — Chuyển khỏi phạm vi agent

Theo yêu cầu người dùng ngày 2026-10-13, E2E catch → transfer trên BlueStacks
không còn là task agent. Trạng thái trước khi đổi: chưa thực hiện. Các case keep
policy, captured storage ID, Promise result, storage postcondition, reconnect và
không replay pending ID được chuyển sang checklist handoff do `T-804` tạo.

## Điều kiện hoàn tất

- Transfer chỉ nhận captured storage ID từ catch success cùng session.
- Keep policy fail closed và config persist/replay không mang theo pending pointer/ID.
- Promise + storage postcondition quyết định terminal outcome.
- Code/build/host checks hoàn tất với transfer `UNVERIFIED_DEVICE`; mutation và
  postcondition thật do user chạy sau handoff.

Sau mỗi task, agent phải cập nhật checkbox và khối `<details>` của chính task đó
trước khi chuyển sang task kế tiếp.
