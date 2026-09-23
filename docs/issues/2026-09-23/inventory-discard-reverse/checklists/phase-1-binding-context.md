# Phase 1 — Binding, context và cache

Plan: [Tổng quan](00-overview.md).

Điều kiện bắt đầu: contract tương ứng từ phase 0 đạt; riêng T-004 độc lập. Điều kiện hoàn tất: binding/context/readiness được guard đúng contract tĩnh, chưa mở production trước device verification.

Sau mỗi task, cập nhật ngay details dưới checkbox với file tạo/sửa/xóa/di chuyển, thay đổi method/param/hành vi và kiểm chứng thực tế trước khi chuyển task. Chỉ tick `[x]` khi Verify đạt; chưa đạt giữ `[ ]` và ghi phần còn thiếu.

- [x] **T-004** **[zygisk]** *(native)* — Tách trách nhiệm trong `RT/probe/runtime_probe_discovery.inc` (hiện 778 dòng) và `RT/mainthread/runtime_main_thread_bridge.inc` (582 dòng) trước khi bổ sung binding/dispatch. **Đề xuất — chưa tồn tại:** helper `.inc` riêng cho discovery service/binding và nhóm handler main-thread, đặt cùng thư mục; chọn khối cohesive đủ để cả file gốc và file mới <=500 dòng, không chỉ chuyển vài dòng discard rồi để file gốc vẫn vượt giới hạn. Cập nhật include/declarations trong `zygisk/jni/main.cpp` và `shared/core/runtime_native_declarations.inc` khi cần, giữ thứ tự một translation unit và behavior. **AC:** hỗ trợ AC-02, AC-03, AC-12 và quy tắc cấu trúc AGENTS. **Phụ thuộc:** Không. **Verify:** đối chiếu diff move-only, symbol không mất/trùng và include order hợp lệ; `wc -l` các file đã chạm <=500; build native kiểm ở T-017, chưa gọi extraction đã build nếu chưa chạy.

  <details>
  <summary>Đã hoàn thành — T-004</summary>

  - **Tạo:** `runtime_probe_service_owners.inc` giữ nguyên service-owner discovery loop; `runtime_main_thread_bridge_lifecycle.inc` giữ lifecycle/object/map request helpers.
  - **Sửa:** `runtime_probe_discovery.inc` và `runtime_main_thread_bridge.inc` chỉ còn orchestration/dispatch; main include order vẫn là một translation unit và lifecycle helper được include ngay sau bridge dispatch.
  - **Kiểm chứng:** `wc -l` cho discovery 386, service helper 391, bridge 359, lifecycle 220; không có symbol duplicate theo `rg`; host focused checks pass. Full native build vẫn là T-017.
  - **Sai khác:** Không đổi behavior có chủ ý; build artifact chưa được tạo trong task này.

  </details>

- [x] **T-005** **[zygisk]** *(native)* — Làm chặt resolver chính trong `RT/probe/runtime_probe_discovery.inc` hoặc helper đã tách và fallback `invoke_inventory_list_sorted()` trong `DISCARD/item_inventory_lookup.inc`: kiểm class/namespace, instance/static, return và **5 kiểu tham số** của T-001; giữ mảng invoke 5 phần tử, bool address đúng ABI. Dùng chung contract để fallback không bỏ type guard. Chỉ dùng null delegate được T-001 chứng minh; delegate cần tạo/giữ dùng `inventory_filter_binding.inc`/`inventory_filter_factory.inc` và managed roots đúng. Giữ build/ABI checks, phân biệt static resolved với runtime verified trong `RuntimeBinding` (`shared/core/runtime_native_prelude.inc`); default production admission đóng trong giai đoạn triển khai. **AC:** AC-02, AC-03, AC-16. **Phụ thuộc:** T-001, T-004. **Verify:** review cả hai resolver và invoke không có overload/arg thứ sáu; signature mismatch không đi tới invoke; chạy existing filter factory test ở phase 4 khi helper thay đổi, ghi rõ test này chỉ phủ delegate factory; kiểm số dòng file đã sửa.

  <details>
  <summary>Đã hoàn thành — T-005</summary>

  - **Sửa:** `inventory_binding_contract.inc` kiểm tra exact managed arity 5, instance method, `List<ItemData>` return và cả 5 generic parameter markers; `item_inventory_lookup.inc` dùng cùng guard trước `runtime_invoke`; discovery dùng cùng resolver.
  - **Giữ:** `arguments[5]` gồm đúng `filterDelegate`, `bool`, và ba delegate cuối; không thêm arg thứ sáu. `inventory_filter_factory.inc` vẫn là `Delegate.CreateDelegate` arity 5 riêng, không bị sửa nhầm.
  - **Kiểm chứng:** `runtime_discard_filter_factory_test.cpp`, `runtime_discard_preparation_test.cpp` và `runtime_readiness_retry_test.cpp` đều pass; `git diff --check` pass. Filter factory test chỉ phủ delegate factory, không chứng minh game method binding/runtime calibration.
  - **Sai khác:** production mutation vẫn đóng; T-004 line-split còn là dependency hình thức của plan.

  </details>

- [x] **T-006** **[zygisk]** *(native)* — Sửa `DISCARD/item_data_factory.inc`, `item_inventory_lookup.inc`, `recycle_item_action.inc` và callsite `hash_set_binding.inc` để preparation trả context nhất quán. Lấy row thật từ list, xác nhận class/id/count/recyclable, `itemExpirationData` non-null đúng layout và set từ **cùng service/lần dựng model** qua `get_ExpiringItemsCopy()`. T-001 quyết định xử lý row trùng item; không lấy row đầu nếu không chứng minh đúng stack/context. Root owner/list/row/set/delegate trong khoảng thời gian cần thiết, invalidate theo owner generation; set getter không tự copy. Bỏ fallback unrelated supplier/empty set trên đường recycle nếu chưa chứng minh tương đương; không tự dựng ItemData/ExpirationData để vượt guard. **AC:** AC-06, AC-07, AC-16. **Phụ thuộc:** T-001, T-005. **Verify:** codewalk missing owner, row stale, count mismatch, recyclable=false, expiration null, set khác generation đều chặn trước `RecycleItem`; ledger managed handles có create/free và không dùng pointer đã invalidate; existing preparation test chạy theo T-016, không coi mock phủ field mới.

  <details>
  <summary>Đã hoàn thành — T-006</summary>

  - **Sửa:** `item_inventory_lookup.inc` quét toàn bộ list và từ chối duplicate
    item row; `item_data_factory.inc` chỉ trả row thật; `recycle_item_action.inc`
    kiểm class/id/count/recyclable và expiration non-null trên Android.
  - **Bỏ:** supplier unrelated và empty `HashSet` fallback. Set phải đến từ
    `get_ExpiringItemsCopy()` của service owner trong cùng main-thread call.
  - **Lifetime:** list/row/set chỉ tồn tại trong synchronous main-thread
    preparation; không lưu pointer qua tick. Filter delegate có process GC root;
    RecycleItem/UpdateInventory Promises dùng GC-handle create/free ở mọi
    terminal path. Owner generation chặn completion cũ.
  - **Verify:** preparation test PASS; native build PASS; device field/layout
    confirmation vẫn là T-018 evidence, không được suy từ mock.

  </details>

- [x] **T-007** **[zygisk]** *(native)* — Thêm readiness/refresh binding vào runtime và `DISCARD/inventory_reader.inc`, dùng cache owner của bag (`cwch`) đã xác minh; **đề xuất — chưa tồn tại:** helper `DISCARD/inventory_readiness.inc` chứa baseline/generation checks và `DISCARD/inventory_refresh.inc` chứa coordinator refresh. `GetItemCount(Item)` D:68101/RVA `0x8163D78` chỉ được coi zero hợp lệ sau baseline đạt. Nếu cần refresh, gọi `InventoryCache.UpdateInventory()` không tham số, giữ concrete Promise/owner/session, một request refresh đang chạy, cadence/deadline bounded theo T-002; không gọi private callbacks hoặc mutate timestamp/cache/prediction. Lưu token/generation để response refresh cũ không mở barrier mới. **AC:** AC-04, AC-11, AC-13. **Phụ thuộc:** T-002, T-004, T-005. **Verify:** truth table baseline missing/stale/valid-zero và delta/no-delta khớp T-002; snapshot local mới lấy không tự có server freshness; refresh fail/unknown giữ blocked, không RPC mỗi tick; tách file để <=500 dòng, chưa mở readiness bằng stub/always-true.

  <details>
  <summary>Đã hoàn thành — T-007</summary>

  - **Tạo:** `inventory_readiness.inc` giữ cache timestamp baseline, owner
    generation và observation generation; zero count chỉ hợp lệ sau baseline.
  - **Sửa:** `runtime_probe_service_owners.inc` xác minh `cwch`,
    `GetLatestTimestamp(): Int64` và `UpdateInventory():
    IPromise<GetHoloholoInventoryOutProto>`; `reconcile.inc` giữ một refresh
    Promise và deadline 15 giây. Không gọi private callback hay ghi cache.
  - **Verify:** focused readiness test, native build và codewalk pass; cache
    owner/Promise/no-delta concrete behavior vẫn cần T-018/T-020 device evidence.

  </details>
