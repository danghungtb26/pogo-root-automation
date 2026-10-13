# Phân tích cách PGTools gắn vào tiến trình Pokémon GO

## Kết luận chính

PGTools 6.0.6 khởi động Pokémon GO, chờ ứng dụng mở xong, lấy PID của tiến trình bằng `pidof`, sau đó chạy một chương trình injector riêng để gắn vào PID đó và nạp một script JavaScript.

Luồng lệnh được dựng trong phương thức đã bị làm rối tên `ka.i.g()`; log nội bộ gọi chức năng này là `ScriptManager.startPogoAndInject`:

```sh
chmod 755 <thu-muc-files-cua-pgtools>/<injector> &&
am start -S -W <package-pokemon-go>/<launch-activity> &&
<thu-muc-files-cua-pgtools>/<injector> \
  -p $(pidof <package-pokemon-go>) \
  -s <thu-muc-files-cua-pgtools>/<script> \
  -e -R v8 -P '{}'
```

## Trình tự gắn vào tiến trình

1. PGTools đọc hai giá trị từ `SharedPreferences`:
   - `injectorFileName`: tên file injector.
   - `scriptFileName`: tên script cần nạp.

2. Ứng dụng lấy đường dẫn thư mục riêng bằng `Context.getFilesDir()` và cấp quyền thực thi cho injector:

   ```sh
   chmod 755 <filesDir>/<injectorFileName>
   ```

3. Package mục tiêu được đọc từ cấu hình `pogoPackageName`. Giá trị mặc định là:

   ```text
   com.nianticlabs.pokemongo
   ```

4. PGTools dùng `PackageManager.getLaunchIntentForPackage()` để tìm activity khởi chạy của Pokémon GO. Activity không được ghi cứng trong mã.

5. Pokémon GO được dừng và khởi động lại bằng:

   ```sh
   am start -S -W <package>/<activity>
   ```

   Trong đó:

   - `-S` dừng tiến trình hiện có trước khi mở lại.
   - `-W` chờ quá trình khởi động activity hoàn tất.
   - Các lệnh nối với nhau bằng `&&`, vì vậy injector chỉ được chạy nếu bước trước thành công.

6. PID của Pokémon GO được lấy tại thời điểm chạy bằng:

   ```sh
   pidof <package-pokemon-go>
   ```

7. Injector được chạy với PID vừa tìm thấy và đường dẫn tới script:

   ```sh
   <injector> -p <PID> -s <script> -e -R v8 -P '{}'
   ```

## Ý nghĩa có khả năng cao của các tham số injector

Các tham số này khớp với giao diện dòng lệnh của injector thuộc hệ sinh thái Frida:

- `-p <PID>`: gắn vào một tiến trình đang chạy.
- `-s <file>`: nạp script từ file.
- `-e`: giữ script tồn tại sau khi injector kết thúc, thường được gọi là “eternalize”.
- `-R v8`: dùng JavaScript runtime V8.
- `-P '{}'`: truyền đối tượng tham số JSON rỗng cho script.

Đây là suy luận dựa trên hình dạng tham số, chưa phải bằng chứng trực tiếp về mã bên trong injector.

## Quyền root và lớp thực thi shell

Lớp shell đi kèm ứng dụng thử mở shell theo thứ tự:

1. `su --mount-master`
2. `su`
3. `sh` nếu không lấy được shell root

Việc gắn vào tiến trình của một ứng dụng Android khác thực tế cần đặc quyền root hoặc một cơ chế đặc quyền tương đương. Nhánh `sh` thông thường khó có thể thực hiện thành công thao tác gắn chéo UID.

## Chế độ debug ẩn

Khi cấu hình `hidden_debug` được bật, PGTools thay đổi lệnh thành dạng chạy nền:

```sh
nohup sh -c '<lenh-injector>' >> <cacheDir>/script-log.txt 2>&1 &
```

Ở nhánh này, tham số `-e` bị loại bỏ và đầu ra được ghi vào `script-log.txt` để phục vụ chẩn đoán.

## Dấu hiệu liên quan đến Frida

Mã dọn dẹp của PGTools xóa các file tạm sau:

```sh
rm -rf /data/local/tmp/frida-* /data/local/tmp/katana-*
```

Kết hợp với các tham số `-p`, `-s`, `-e`, `-R v8` và `-P`, điều này cho thấy injector nhiều khả năng là một bản đổi tên hoặc chỉnh sửa từ công cụ kiểu Frida. Tuy nhiên, đây vẫn là suy luận tĩnh.

## Hành vi quản lý và khởi động lại

- Thành phần Android thực hiện công việc nền là `net.pgtools.auto.AutoService`.
- PGTools lưu bộ đếm `pogoStartupCount` sau mỗi lần khởi động và inject.
- Ứng dụng có cơ chế lên lịch khởi động lại dịch vụ và Pokémon GO.
- Khi dừng dịch vụ, PGTools có thể dừng Pokémon GO bằng `am force-stop`.
- Một số luồng dọn dẹp còn xóa cache của package Pokémon GO trước khi khởi động lại.

## Phần chưa thể xác định từ APK này

APK không chứa injector thực tế hoặc script được inject dưới dạng payload native rõ ràng. Tên hai file được nhận từ cấu hình runtime và chúng nằm trong thư mục riêng của PGTools. Thư viện native đáng chú ý duy nhất đóng gói trực tiếp trong APK là `libdatastore_shared_counter.so`, không phải injector nói trên.

Vì vậy, APK này chỉ cho thấy lớp điều phối việc gắn tiến trình. Chưa thể xác nhận injector bên ngoài sử dụng kỹ thuật thấp tầng nào, chẳng hạn:

- `ptrace`
- `process_vm_writev`
- nạp thư viện từ xa
- cơ chế dựa trên Zygisk
- một kỹ thuật injection tùy chỉnh khác

Muốn xác định chính xác, cần lấy file injector đã được PGTools tải xuống hoặc tạo ra trên thiết bị, sau đó phân tích file native đó bằng REA.

## Phạm vi và bằng chứng

File được phân tích:

```text
pgtools-6.0.6-079d8b42e6c6cfaae64033341f861ac6.apk
```

SHA-256:

```text
b30b5c45a6425bdc74a13254dac5e22e8e58f827d098562b12fa1cd41660f78f
```

REA Evidence:

- `ev_dc2cdfaefe0ff0e3fd96b0c81afc1952ccb2942fae12f36dd5fd04fa265dd330`
- `ev_b97b6c115ab16f0a278f5ebb93079fbf232980b4191fd9ac6160f70adc66c761`

REA xác nhận danh tính và cấu trúc gói APK bằng phân tích tĩnh. Provider JADX của REA chưa được cấu hình trong môi trường, nên đường đi trong DEX được đối chiếu thêm bằng công cụ disassembly của Android SDK. APK không được thực thi trong quá trình phân tích.
