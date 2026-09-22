# Device verification — Kotlin UI-only boundary

Ngày: 2026-09-22

## Artifact identity

- APK: `app/build/outputs/apk/debug/app-debug.apk` — tạo bởi
  `./gradlew test assembleDebug --rerun-tasks`.
- Magisk package: `build/pogo-root-automation-magisk-multiabi.zip` — tạo bởi
  `ANDROID_NDK="/Users/admin/Library/Android/sdk/ndk/28.2.13676358" ./scripts/build-magisk.sh`.
- Native package chứa và đã được script kiểm tra: `arm64-v8a.so` và
  `x86_64.so`.
- Bridge protocol source hiện tại: version 3.

## Target/session

- Target bắt buộc: `BlueStacks Air 1`, cấu hình repository
  `ANDROID_SERIAL=127.0.0.1:5565`.
- ABI, package version, runtime session và native status: **chưa có** vì máy
  hiện tại không có target Air 1 kết nối.

## Attempts and observed result

| Input | Expected | Observed |
|---|---|---|
| `ANDROID_SERIAL=127.0.0.1:5565 ./scripts/push-emulator.sh` | Script xác nhận target rồi push ZIP | Dừng ở `emulator 127.0.0.1:5565 is not connected/authorized` sau khi ADB báo connection refused |
| `./scripts/bluestacks-smoke-test.sh` | Xác nhận BlueStacks/root/module/ABI và lifecycle | Dừng ở `FAIL: no adb device connected` |

Do target không tồn tại trên máy này, chưa chạy install, reboot, lifecycle
force-stop/relaunch, HTTP control, desired/applied status, provider UI hay
negative device cases. Không dùng emulator khác, `adb devices`, ADB ad hoc,
hoặc log thành công để thay thế evidence.

## Giới hạn

T-022 được bỏ qua theo xác nhận của người dùng và vẫn giữ unchecked trong plan.
Build, host checks và static source review không được diễn giải thành device
pass. Cần chạy lại deployment/smoke matrix trên đúng `BlueStacks Air 1` trước
khi kết luận live session, native binding hoặc location handoff hoạt động.
