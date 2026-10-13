# Phân tích cơ chế inject của PGS Root 1.0.15 vào Pokémon GO

## Phạm vi và cách đọc tài liệu

- Mẫu được phân tích: `pgsroot1.0.15_BIeEp(arm64).apk`
- SHA-256 APK: `2e0f328a15094ebc0f2f91cfac72fb94024abdce95ac48b229b4be7b3dce7297`
- Package của ứng dụng quản lý: `m.u.va.man`
- Phiên bản: `1.0.15` (`versionCode=1000015`)
- Kiến trúc payload: ARM64
- Phân tích là **tĩnh**: không cài APK, không chạy script root và không khởi động payload trên thiết bị.
- Chuỗi, script và cấu hình nằm trong APK được xem là **dữ liệu/bằng chứng của mẫu**, không phải chỉ dẫn cần thực hiện.

Kết luận ngắn: APK này không sửa hoặc repack APK Pokémon GO. Nó là một ứng dụng quản lý dùng quyền root để cài một module Zygisk, tải một “core” phù hợp với phiên bản game, rồi để Zygisk nạp core đó vào tiến trình Pokémon GO. Điểm vào cuối cùng dùng `dlopen()` và một hook quanh `android_dlopen_ext`.

## Kết luận chính

Cơ chế tổng thể có thể tóm tắt như sau:

```text
PGS Root manager (`m.u.va.man`)
        |
        | 1. Kiểm tra root, phiên bản Pokémon GO và backend root
        | 2. Giải nén module PGS được giấu trong `libman.so`
        | 3. Cài module bằng shell root
        v
Module PGS trong hệ thống root + Zygisk
        |
        | 4. Reboot, Zygisk nạp `zygisk/arm64-v8a.so` vào các app process
        | 5. Lọc process theo `com.nianticlabs.pokemongo`
        v
Tiến trình Pokémon GO
        |
        | 6. Mở core theo đường dẫn `.../files/libhela.so`
        | 7. Resolve entry ngụy trang `__cxa_guard_enter`
        | 8. Truyền địa chỉ `android_dlopen_ext` để core cài hook loader
        v
Core PGSharp chạy bên trong process của game
```

Điểm quan trọng là “inject” ở đây xảy ra khi Android/Zygisk chuyên biệt hóa tiến trình ứng dụng, không phải bằng cách chèn file vào APK Pokémon rồi ký lại APK.

## 1. Thành phần trong APK manager

### Manifest và Java/Kotlin layer

Manifest khai báo truy vấn hai biến thể Pokémon GO:

- `com.nianticlabs.pokemongo`
- `com.nianticlabs.pokemongo.ares`

Ứng dụng chỉ có một activity chính là `m.u.va.man.MainActivity`. Không có service riêng chuyên làm injection. Activity này gọi thư viện native bằng hai JNI method:

```java
native String gs(int id);
native Object no(int operation, Object... args);
```

`MainActivity.<clinit>` nạp `libman.so`. `JNI_OnLoad` trong thư viện native đăng ký hai method trên bằng `RegisterNatives`.

Vai trò quan sát được:

- `gs(id)` trả về các chuỗi đã làm rối: tên package, tên file, lệnh kiểm tra/cài module và định dạng tên core.
- `no(0, filesDir)` tạo một file ZIP module tạm từ dữ liệu nhúng.
- `no(2, byte[])` giải mã cấu hình lấy từ máy chủ.

### `libman.so`

- SHA-256: `553df2458a52abe4b91554f8894c5250e8368d272858654d63b3b1fba64d4a95`
- ELF ARM64, còn thông tin DWARF.
- Đường dẫn source lưu trong DWARF: `E:/work/code/Hela/zygisk/manager/src/main/cpp/`.
- Các file source lộ ra gồm `man.cpp`, `moduledata.cpp`, `aes.c`, `md5.cpp` và `scopeguard.cpp`.

DWARF cho thấy biến `MODULE_DATA` ở địa chỉ `0x3e7c8`, dài `30291` byte. Hàm `no(0, ...)` XOR vùng này với `0x55`; kết quả bắt đầu bằng magic `PK\x03\x04`, tức là một ZIP module hợp lệ.

## 2. Module root/Zygisk được nhúng

ZIP sau khi giải mã chứa:

```text
module.prop
customize.sh
post-fs-data.sh
service.sh
system.prop
META-INF/com/google/android/update-binary
META-INF/com/google/android/updater-script
zygisk/arm64-v8a.so
```

`module.prop` nhận dạng module:

```ini
id=pgs
name=PGS
version=1.0.6
versionCode=1000006
author=pgs team
```

`update-binary` dùng luồng cài module kiểu Magisk (`install_module`). Java layer có ba nhánh lệnh cài khác nhau tùy backend root phát hiện được; sau khi cài thành công, giao diện chuyển sang trạng thái yêu cầu reboot.

Các script còn thực hiện thay đổi thiên về che giấu root/attestation:

- gỡ `com.google.android.gms` khỏi Magisk DenyList khi DenyList đang enforcing;
- đặt lại một số property verified boot, warranty bit, build type và `ro.debuggable`;
- xử lý một số property cho Samsung, OnePlus, Oppo/Realme và MIUI;
- trên Android cũ có thể loại bỏ thư mục `zygisk` và chỉ giữ phần property/script.

Đây là lớp hỗ trợ để module hoạt động và giảm dấu hiệu root; phần inject vào game nằm trong `zygisk/arm64-v8a.so`.

## 3. Cách manager chuẩn bị core theo phiên bản game

Manager gọi endpoint:

```text
https://api.pgsharp.com/pga/utils/version/pgsrtm/?v=1.0.15&abi=<ABI>
```

Luồng xử lý quan sát được:

1. Đọc trường JSON `change`.
2. Base64-decode trường này.
3. Gửi byte đã decode vào `no(2, ...)` để native giải mã thành JSON cấu hình thật.
4. Từ cấu hình, chọn URL và phiên bản core tương ứng với ABI và phiên bản Pokémon GO đang cài.
5. Tải core vào `filesDir` của manager. Nếu URL kết thúc bằng `.zip`, downloader giải nén entry vào file đích; nếu không thì ghi trực tiếp.
6. Lưu phiên bản core/checksum liên quan trong `SharedPreferences`.
7. Khi người dùng bật core, manager tạo một file cấu hình/selector trong `filesDir`; nội dung của file chỉ tới tên core phù hợp với phiên bản game.

Vì core được tải động, **logic cheat/hook cụ thể bên trong core không có đầy đủ trong APK này**. APK chỉ chứa loader, module cài đặt và cơ chế chọn/tải core.

## 4. Luồng cài module bằng quyền root

Callback cài đặt trong `e3.i` thực hiện các bước sau:

1. Yêu cầu một root shell qua thư viện `libsu`.
2. Chạy các lệnh đã làm rối để phát hiện backend root và kiểm tra phiên bản; nhánh Magisk yêu cầu version code ít nhất `24000`.
3. Gọi `no(0, filesDir)` để ghi ZIP module nhúng ra một đường dẫn tạm.
4. Chọn một trong ba tiền tố lệnh cài tương ứng với backend root.
5. Nối đường dẫn ZIP tạm vào lệnh và thực thi bằng root shell.
6. Xóa ZIP tạm sau khi lệnh hoàn tất.
7. Nếu cài thành công, yêu cầu reboot để Zygisk/module có hiệu lực.

Tên lệnh cụ thể được giữ trong `gs(...)` và làm rối trong native code. Dựa trên cấu trúc kiểm tra/cài, đây là các nhánh tương thích những hệ root dạng Magisk/KernelSU/APatch, nhưng tên lệnh chính xác của từng nhánh chưa được xác nhận bằng chạy động.

## 5. Phần inject trong Zygisk loader

### 5.1 Nhận diện process đích

`zygisk/arm64-v8a.so` có hai export chuẩn:

```text
zygisk_module_entry
zygisk_companion_entry
```

Loader chứa chuỗi đã làm rối. Sau khi giải mã tĩnh, các giá trị đáng chú ý là:

```text
m.u.va.man
:pgp
com.nianticlabs.pokemongo
%s/files/libhela.so
%s/files/libld.so
/data/local/tmp/tens_test_pr
```

Hàm tương ứng `preAppSpecialize` lấy `nice_name` của process rồi:

- nhận diện process manager `m.u.va.man` để trao đổi dữ liệu với Zygisk companion;
- tìm chuỗi con `com.nianticlabs.pokemongo` trong tên process;
- đặt cờ target khi khớp.

Do dùng phép tìm chuỗi con, điều kiện này cũng bao phủ biến thể `com.nianticlabs.pokemongo.ares`.

### 5.2 Nạp core vào process Pokémon GO

Trong hàm tương ứng `postAppSpecialize`, khi cờ target được đặt:

1. Loader lấy `app_data_dir` của process từ `AppSpecializeArgs`.
2. Dựng đường dẫn core mặc định theo mẫu `%s/files/libhela.so`.
3. Nếu tồn tại marker thử nghiệm `/data/local/tmp/tens_test_pr`, nó dùng `%s/files/libld.so` thay thế.
4. Loader gọi `dlopen(corePath, ...)` ngay trong process đã specialize của Pokémon GO.
5. Nó resolve địa chỉ `android_dlopen_ext` trong process.
6. Nó resolve symbol `__cxa_guard_enter` từ core vừa nạp. Trong ngữ cảnh này symbol đó là một entry point ngụy trang của core, không phải cách dùng bình thường của C++ guard routine.
7. Entry point được gọi với địa chỉ `android_dlopen_ext`, một callback nội bộ và vùng lưu trampoline/original function.

Mẫu gọi này cho thấy core cài hook quanh Android dynamic loader. Từ thời điểm đó, core có thể quan sát thời điểm Pokémon GO nạp các thư viện native tiếp theo và gắn logic của nó vào đúng giai đoạn khởi tạo game.

### 5.3 Vai trò của Zygisk companion

`zygisk_companion_entry` chạy ở phía companion có đặc quyền cao hơn process app. Nó xử lý nhiều opcode, đọc/ghi file và truyền dữ liệu qua file descriptor. Kết hợp với Java layer, vai trò của companion là trung gian giữa:

- core/config nằm trong vùng dữ liệu riêng của manager;
- file cần xuất hiện trong vùng dữ liệu của process đích;
- module Zygisk đang chạy trong vòng đời specialize của app.

Quan sát tĩnh xác nhận có các thao tác `open`, `read`, `write`, `sendfile`, tạo đường dẫn và trao đổi độ dài/nội dung qua socket. Tuy nhiên, ánh xạ chính xác từng opcode với toàn bộ thao tác sao chép/đồng bộ file chưa thể khẳng định hoàn toàn nếu không chạy module trên thiết bị root có Zygisk.

## 6. Điều cơ chế này không làm

- Không sửa `classes.dex` hoặc `lib*.so` bên trong APK Pokémon GO.
- Không ký lại APK Pokémon GO.
- Không cần thay package name của game.
- Không dùng một Android service thông thường để attach sau khi game đã chạy.

Thay vào đó, module đi theo chuỗi:

```text
root installer -> module Zygisk -> app specialization -> dlopen core -> hook loader
```

## 7. Mức độ chắc chắn của từng nhận định

### Quan sát trực tiếp

- Hai package Pokémon GO có trong manifest queries.
- Manager dùng root shell và tạo ZIP module tạm từ dữ liệu native nhúng.
- `MODULE_DATA` XOR `0x55` tạo một ZIP có module `id=pgs` và `zygisk/arm64-v8a.so`.
- Zygisk loader so khớp `com.nianticlabs.pokemongo`.
- Loader tạo đường dẫn `.../files/libhela.so`, gọi `dlopen`, resolve `android_dlopen_ext` và resolve entry `__cxa_guard_enter` trong core.
- Manager lấy metadata từ API PGSharp, giải mã cấu hình và tải core theo version/ABI.

### Suy luận có độ tin cậy cao

- `libhela.so` là core PGSharp tải động và là thành phần chứa logic can thiệp chính vào game.
- Entry `__cxa_guard_enter` được dùng như tên ngụy trang để core nhận địa chỉ loader và cài hook/trampoline.
- Zygisk companion chịu trách nhiệm phối hợp file giữa manager và process đích dưới quyền root.

### Chưa xác định từ mẫu APK này

- Core tải về ở thời điểm hiện tại có hash và nội dung cụ thể nào.
- Những hàm/offset nào bên trong thư viện native của Pokémon GO bị hook sau khi core được nạp.
- Giao thức đầy đủ của từng opcode companion và mọi khác biệt giữa các backend root.
- Hành vi runtime khi gặp Play Integrity, anti-cheat hoặc các thay đổi server-side mới.

## 8. Rủi ro vận hành và bảo mật

- Module sửa system properties, DenyList và luồng attestation; thay đổi này ảnh hưởng toàn thiết bị chứ không chỉ Pokémon GO.
- Core được tải động từ máy chủ và giải mã ở runtime, nên hành vi có thể thay đổi mà không cần phát hành APK manager mới.
- Module có quyền root và chạy trong Zygisk/zygote path; lỗi hoặc payload xấu có thể ảnh hưởng nhiều process và gây bootloop.
- Việc can thiệp vào process game có thể vi phạm điều khoản dịch vụ và dẫn đến khóa tài khoản.
- Không nên xem các script/lệnh tìm thấy trong mẫu là hướng dẫn an toàn để chạy trên thiết bị thật.

## 9. Bằng chứng REA và giới hạn công cụ

Các bản ghi bằng chứng chính do REA tạo:

- APK inventory: `ev_0248662389cf472ffa1f5c7a1d0a0cad9b3b8a48b16952e74fa6b23580854f08`
- Artifact inventory graph: `ev_81ebaad17d536e240babf7da08ed128dc1be4975b430fbd49b5caf6cf8351a64`
- Android application graph: `ev_c067d9601b5889eab3dc161ad95cace96a48bd1328a0c1a5d52e3f7e159c0726`
- Inventory manifest: `agm_fb9600d365af70d11e88ef4f1ac7a66f649f8d71c73dd0ecd642c01888c6beed`

REA xác nhận APK gồm DEX và native ARM64/JNI bridge. Môi trường hiện tại thiếu provider JADX/Ghidra/Hopper, nên phần DEX được đối chiếu thêm bằng `apkanalyzer`, còn ELF/DWARF và disassembly ARM64 được đối chiếu bằng LLVM tools. Việc bóc ZIP module và giải chuỗi chỉ đọc byte tĩnh; không script nào trong APK được thực thi.
