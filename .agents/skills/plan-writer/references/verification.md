# Kiểm chứng theo phạm vi thay đổi

Đọc `AGENTS.md`, build config và test thực tế trước khi đưa lệnh vào plan. Các lệnh dưới đây là lựa chọn theo phạm vi, không phải yêu cầu chạy tất cả khi chỉ viết plan.

## Kotlin và Android

| Phạm vi | Lệnh focused |
|---|---|
| Domain/movement/rules | `./gradlew :core:test` |
| Frame/event/payload codec | `./gradlew :bridge:protocol:test` |
| Adapter/capability/fake/protobuf | `./gradlew :game-adapter:api:test :game-adapter:fake:test :game-adapter:pogo:test` |
| App | Xác nhận task test app hiện có trong Gradle, chọn task phù hợp với thay đổi |

Trước khi bàn giao thay đổi code, lập task chạy focused checks liên quan và `./gradlew test assembleDebug` khi môi trường cho phép. Dùng `--rerun-tasks` khi cần tránh kết quả up-to-date/cache che khuất source mới. Không giả định Gradle build APK đã build hoặc kiểm chứng Zygisk C++.

## Native và đóng gói

- Chọn host-side C++ tests liên quan từ source/CI hiện hành. Xác nhận đường dẫn `.cpp` và include trước khi ghi lệnh `c++ -std=c++17 -Wall -Wextra -Werror ...`; không sao chép path đã lỗi thời. Build xong phải chạy binary test, không chỉ compile.
- Khi sửa native runtime, thêm kiểm tra build bằng `scripts/build-magisk.sh`; script build và package cả `arm64-v8a` lẫn `x86_64`.
- Override được hỗ trợ trên Mac hiện tại:

```bash
ANDROID_NDK="$HOME/Library/Android/sdk/ndk/27.1.12297006" ./scripts/build-magisk.sh
```

- APK: `app/build/outputs/apk/debug/app-debug.apk`; ZIP: `build/pogo-root-automation-magisk-multiabi.zip`. Artifact package cần đủ hai ABI theo script.
- Khi sửa shell, thêm syntax check bằng `bash -n` hoặc `sh -n` theo interpreter của file. CI ở `.github/workflows/ci.yml` là nguồn đối chiếu, không dùng lệnh build native thủ công để bỏ qua script vận hành.

## Thiết bị

- Target là **BlueStacks Air 1**, chọn/xác nhận qua script của repo. Không dùng ADB ad hoc, kể cả để liệt kê, reconnect/reset hoặc thu log.
- Theo nhu cầu, đọc cách dùng của script sẵn có trước khi ghi lệnh cụ thể: `push-emulator.sh`, `install-magisk-module.sh`, `device-smoke-test.sh`, `bluestacks-smoke-test.sh`, `binding-probe-test.sh`, `collect-binding-diagnostics.sh`, `headless-control.sh`.
- Cài APK qua `install-magisk-module.sh --apk <path>` khi thuộc phạm vi; chỉ thêm `--reboot` nếu reboot đã được yêu cầu/cho phép. Không coi yêu cầu viết plan là quyền cài/chạy trên thiết bị.
- Thu log đầy đủ bằng `./scripts/logcat-full.sh` với option được hỗ trợ; chỉ đọc log liên quan khi thao tác thất bại.
- Script thiếu chức năng/thất bại: chẩn đoán và ghi thay đổi cần thiết; không tự tạo script thay thế, workflow tạm hoặc lệnh ADB tương đương để vượt qua.
- Test domain/fake/host C++ không chứng minh live binding hoạt động. Task xác minh runtime phải nêu identity/build/ABI, guards, lifecycle và postcondition quan sát được; `READ_MAP_TARGET` vẫn tuân thủ điều kiện calibration/device verification trong `AGENTS.md`.

## Hoàn tất và giới hạn

- Kiểm tra file source không phải Markdown đã thay đổi không vượt 500 dòng; kiểm tra `git diff --check` sạch.
- Chỉ sửa docs/skill không cần build game/app; kiểm tra cấu trúc, tham chiếu và validator phù hợp.
- Môi trường thiếu SDK/NDK/device hoặc lệnh fail thì ghi rõ vào details, giữ task xác minh chưa đạt ở `[ ]`. Không khẳng định toàn plan hoàn tất khi kiểm tra bắt buộc còn thiếu.
