# Task: Loại bỏ hoàn toàn screen runtime mode

**Type:** refactor
**Date:** 2026-09-07
**Parent task:** `runtime-api-action-bridge`

## Mục tiêu

Chuyển headless automation thành structured-only runtime. Xóa toàn bộ code chụp
màn hình, nhận diện pixel và điều khiển gameplay bằng `input tap/swipe`; không còn
`AutomationRuntimeMode` hoặc fallback screen.

## Phạm vi

### Xóa

- `app/src/main/java/dev/pogoroot/automation/headless/ScreenAutomation.kt`
- `app/src/main/java/dev/pogoroot/automation/root/RootBinaryShell.kt`
- `AutomationRuntimeMode`, mode routing và `runtimeMode` config/API field
- `GameScreenState`, screen dimensions, frame/sweep counters và status field tương ứng
- `encounterSweep` cùng các delay chỉ phục vụ screenshot/input
- Direct screen manual endpoints `/v1/actions/catch` và `/v1/actions/spin`

### Refactor

- `HeadlessAutomationEngine` chỉ chạy `StructuredAutomationController`.
- Đổi `encounterSweep` thành `autoEncounter` nếu vẫn cần bật `OpenEncounter` từ
  nearby structured state.
- Status dùng `runtimeSessionId`, `runtimeLifecycle`, `observationSeq`, suspension,
  action và error làm nguồn canonical.
- Cập nhật `AutomationControlServer`, `AutomationConfigRepository`,
  `AutomationPolicyBridge`, `HeadlessAutomationService`, `MainActivity`, overlay
  copy, manifest, host script và tài liệu.

### Giữ nguyên

- `RuntimeBridgeClient`, `RuntimeSessionManager`, `BridgePogoRuntimeSource`,
  `PogoGameAdapter`, `AutomationRunner` và bridge protocol.
- `RootShell` vì còn phục vụ runtime bridge, runtime status và mock location.
- Core planners, structured domain models, berry/discard/transfer policy và joystick
  location control.

## Dependency và blocker

Task này phụ thuộc contract structured trong `runtime-api-action-bridge`, nhưng không
cần chờ live mutation executor để xóa screen code. Sau khi cutover, native probe
chưa có live observation/capability thì service phải hiển thị read-only/fail-closed;
không được tự fallback sang screen.

Manual catch/spin không được giữ dưới dạng root input. Nếu cần compatibility, tạo
task riêng cho structured manual API với `encounterId`/`fortId` và bắt buộc đi qua
`AutomationRunner`.

## Thứ tự thực hiện

1. Chốt/migrate config và status contract; bổ sung test policy `autoEncounter`.
2. Simplify engine/service thành structured-only.
3. Xóa screen classes, binary root shell và manual screen routes.
4. Cập nhật scripts, UI copy, manifest và docs.
5. Chạy JVM tests, Android compile, native tests và structured read-only device smoke.

## Acceptance criteria

| ID | Điều kiện hoàn thành |
|---|---|
| SRM-01 | Không còn `AutomationRuntimeMode`, `SCREEN`, `STRUCTURED` hoặc mode routing trong app. |
| SRM-02 | Không còn `ScreenAutomation.kt`, `RootBinaryShell.kt`, `GameScreenAnalyzer`, `RootScreenCapture`, `RootUiDriver`, `screencap`, `input tap` hay `input swipe` trong headless path. |
| SRM-03 | Headless service luôn kết nối/chạy structured controller; runtime chưa ready chỉ tạo trạng thái read-only/fail-closed. |
| SRM-04 | Status/API không còn `runtimeMode`, `screenState`, screen dimensions, frame counters hoặc sweep counters. |
| SRM-05 | `health/status/start/stop/config` vẫn hoạt động; không còn direct manual screen endpoint. |
| SRM-06 | Config cũ có `runtime_mode=SCREEN` không thể kích hoạt behavior screen; `encounterSweep` được ignore/migrate rõ ràng. |
| SRM-07 | `RootShell`, runtime bridge, runtime status và joystick vẫn compile/hoạt động. |
| SRM-08 | Test/build/source guard chứng minh không còn screen implementation và không có root-input fallback. |

## Câu hỏi mở

- Có muốn giữ tên `HeadlessAutomationEngine`, hay đổi thành
  `StructuredAutomationEngine`? Không phải blocker; giữ tên hiện tại ít churn hơn.
- `autoEncounter` có cần xuất hiện trong overlay settings không, hay chỉ host API?
