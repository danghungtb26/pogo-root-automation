# Phase 7 — Auto Discard

Plan: [Tổng quan](00-overview.md)

## Điều kiện bắt đầu

- Inventory world-state implementation `T-405` build đạt và action arbiter có
  host/static evidence; live behavior còn `UNVERIFIED_DEVICE`.
- `T-304` đã xác nhận `GetItemCount`/`RecycleItem`; capability discard vẫn đóng
  nếu `ItemData` hoặc `ExpiringItemsCopy` còn unresolved.

## Task

- [ ] **T-701** **[reverse]** *(native/reverse)* — Hoàn tất offline contract còn thiếu cho discard: `ItemData` field layout/constructor, enum → object conversion, source/owner của `ExpiringItemsCopy`, exact `RecycleItem` overload, Promise/result và full inventory refresh signal. Cập nhật evidence ledger và instrument owner/generation diagnostics; chưa recycle. **AC:** Mọi argument có class/type/layout source rõ; owner/lifetime runtime chưa chứng minh được gắn `UNVERIFIED_DEVICE`, không dùng factory/field bản cũ theo suy đoán. **Phụ thuộc:** `T-304`, `T-405`. **Verify:** Metadata/callsite cross-check + native build/static resolver review; lệnh `collect-binding-diagnostics.sh` chỉ ghi trong handoff user, agent không chạy.

  <details>
  <summary>Bị chặn — T-701</summary>

  BLOCKED: `ItemData`, `ExpiringItemsCopy`, `RecycleItem` overload/result/refresh contract T-304 chưa có Evidence 0.433.0. Discard capability giữ đóng; không dùng factory/layout cũ để recycle.

  </details>

- [ ] **T-702** **[zygisk]** *(native/android)* — Port `inventory_binding_contract.inc`, `item_data_factory.inc`, `hash_set_binding.inc`, `inventory_filter_factory.inc`, `recycle_item_action.inc` theo evidence 0.433.0; ngay trước mutation gọi `GetItemCount`, tính `discard = max(0, current - keepCount)`, build `ItemData`/`ExpiringItemsCopy` đúng owner và submit qua arbiter. Đồng bộ default/catalog/config cho item IDs cần giữ (gồm `1201`, `1202`, `708`, `706` nếu evidence game xác nhận), không xóa item lạ. **AC:** `keepCount` là ngưỡng giữ lại, count stale/unknown hoặc item không allowlist thì không recycle; config persist/replay theo revision. **Phụ thuộc:** `T-406`, `T-701`. **Verify:** Native/Gradle checks; host/dry-run trace với nhiều count/keepCount xác nhận phép tính và type/quantity guards; không gọi `RecycleItem` trên device.

  <details>
  <summary>Bị chặn — T-702</summary>

  BLOCKED: `ItemData`, `ExpiringItemsCopy`, `RecycleItem` overload/result/refresh contract T-304 chưa có Evidence 0.433.0. Discard capability giữ đóng; không dùng factory/layout cũ để recycle.

  </details>

- [ ] **T-703** **[zygisk]** *(native)* — Hoàn thiện `modules/discard/promise_observer.inc`, `reconcile.inc`, `inventory_readiness.inc`: theo dõi Promise bằng GC handle, parse terminal result, mark inventory dirty, yêu cầu full refresh và xác minh count mới; timeout là `INDETERMINATE`, không retry quantity cũ. Phát event/status có item id, requested count, terminal count, reason và `UNVERIFIED_DEVICE`; artifact test chỉ cho phép mutation sau exact fingerprint + manual arm, không quảng bá `LIVE_READY`. **AC:** Code chỉ báo discard success khi server result + inventory postcondition khớp; partial/failure/timeout không bị báo completed. **Phụ thuộc:** `T-702`. **Verify:** Native build/check; host-controlled outcome/refresh state paths và static review mọi terminal path nhả arbiter/GC handle, không chạy device.

  <details>
  <summary>Bị chặn — T-703</summary>

  BLOCKED: `ItemData`, `ExpiringItemsCopy`, `RecycleItem` overload/result/refresh contract T-304 chưa có Evidence 0.433.0. Discard capability giữ đóng; không dùng factory/layout cũ để recycle.

  </details>

### T-704 — Chuyển khỏi phạm vi agent

Theo yêu cầu người dùng ngày 2026-10-13, E2E discard trên BlueStacks không còn
là task agent. Trạng thái trước khi đổi: chưa thực hiện. Test item allowlist với
quantity nhỏ, count trước/sau, Promise, refresh, stale/concurrent/reconnect và
config persistence được chuyển sang handoff `T-804`; status giữ
`UNVERIFIED_DEVICE` tới khi user trả kết quả.

## Điều kiện hoàn tất

- Discard dùng exact runtime objects/owner 0.433.0 và re-read count trước mutation.
- `keepCount`, allowlist và stale/unknown guards fail closed.
- Code yêu cầu Promise + full inventory refresh trước terminal success; live
  behavior giữ `UNVERIFIED_DEVICE` tới khi user chạy handoff.

Sau mỗi task, agent phải cập nhật checkbox và khối `<details>` của chính task đó
trước khi chuyển sang task kế tiếp.
