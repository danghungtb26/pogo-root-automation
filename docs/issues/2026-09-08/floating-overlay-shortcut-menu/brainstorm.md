# Brainstorm: Thiết kế lại overlay thành float icon, shortcut menu và settings submenu

**Type:** ux
**Date:** 2026-09-08

---

## Analysis

### 1. Ai là người dùng?

Người vận hành PoGo Root Automation trên thiết bị Android/emulator root, thường mở Pokémon GO cùng lúc với floating overlay. Họ cần thao tác nhanh trong khi đang nhìn bản đồ, nên overlay không được che phần lớn màn hình hoặc bắt họ quay lại `MainActivity` cho các thao tác nhỏ.

Người dùng có thể đã quen với joystick hiện tại nhưng chưa chắc biết hết các setting. Vì vậy menu phải vừa nhanh cho thao tác lặp lại, vừa có nhãn rõ ràng cho các chức năng ít dùng. Ngữ cảnh có thể là màn hình nhỏ, xoay ngang/dọc, hoặc đang thao tác nhanh nên vùng chạm và trạng thái ON/OFF phải rất rõ.

### 2. Mục tiêu của người dùng là gì?

- Giữ một nút nổi nhỏ trên màn hình và kéo nó tới vị trí thuận tay, không che game.
- Chạm nút nổi để mở các shortcut dạng icon.
- Bật/tắt nhanh các automation feature như `autoCatch`, `autoSpin`, `autoEncounter`, `autoDiscard`, `autoTransfer` và master `enabled`.
- Mở `Settings`, xem danh sách nhóm cấu hình, sau đó chạm từng nhóm để vào submenu chi tiết thay vì đọc toàn bộ form dài ngay từ đầu.
- Vẫn dùng được joystick, teleport, speed và cooldown hiện đang có.

### 3. Pain point hiện tại là gì?

Implementation hiện tại trong `JoystickOverlayService` tạo một `LinearLayout` lớn khoảng 230dp, luôn hiển thị status, tọa độ, automation summary, cooldown, setting, joystick, speed, teleport và close trong cùng một panel. Chỉ phần header kéo được; toàn bộ panel chiếm chỗ liên tục và khó mở đúng chức năng cần dùng.

Settings UI hiện gom toàn bộ Item discard, Pokémon transfer, Encounter berry và Feedback vào một `ScrollView`. Các toggle quan trọng `autoCatch`, `autoSpin`, `autoEncounter` đang có trong `AutomationConfigRepository` nhưng chưa nằm trong settings này. Kết quả là:

- **Discoverability:** chưa có điểm vào rõ ràng cho quick toggle; user phải vào `MainActivity` hoặc API cho một số toggle.
- **Screen obstruction:** joystick panel cố định kích thước lớn dù user chỉ muốn bật/tắt hoặc xem trạng thái.
- **Cognitive load:** settings dài, không có cấp điều hướng theo nhóm.
- **Interaction ambiguity:** header vừa là tiêu đề vừa là vùng kéo; nếu sau này biến thành icon, cần phân biệt tap và drag để không mở menu ngoài ý muốn.

Dependency map liên quan:

```text
MainActivity / HeadlessAutomationService
            └── AutomationConfigRepository (SharedPreferences)
                         ↑ đọc/ghi
JoystickOverlayService ──┘
        ├── WindowManager overlay window
        ├── JoystickLocationController
        └── AutomationSettingsActivity
            ├── AutomationSettingsListFragment
            └── AutomationCategoryFragment
```

Quick toggle và settings phải dùng cùng `AutomationConfigRepository`; nếu tạo một state riêng cho overlay, status dễ lệch với headless service/API.

### 4. Những user flow nào bị ảnh hưởng?

#### Flow A — Mở menu shortcut

1. Overlay khởi động ở trạng thái thu gọn: chỉ còn float icon.
2. User chạm icon trong vùng không vượt quá touch slop → menu shortcut mở cạnh icon.
3. User chạm một icon toggle → config được cập nhật ngay, icon đổi trạng thái ON/OFF, menu vẫn mở để thực hiện tiếp thao tác khác.
4. User chạm action như `Joystick`/`Teleport` → mở joystick pad tối giản hoặc dialog tương ứng.
5. User chạm ra ngoài menu hoặc icon lần nữa → menu đóng về float icon.

#### Flow B — Kéo float icon

1. User nhấn và di chuyển icon vượt touch slop.
2. Chỉ float icon di chuyển; không mở menu và không kích hoạt shortcut.
3. Tọa độ được giới hạn trong vùng màn hình dùng được và lưu lại cho lần mở sau.
4. Sau xoay màn hình hoặc thay đổi kích thước cửa sổ, icon được clamp lại nếu vị trí cũ nằm ngoài màn hình.

#### Flow C — Settings dạng list → submenu

1. User mở shortcut menu và chạm `Settings`.
2. Màn hình Settings cấp 1 hiển thị các row có `title`, summary hiện tại và chevron:
   - Automation
   - Item discard
   - Pokémon transfer
   - Encounter berry
   - Feedback
3. User chạm một row để vào đúng submenu của nhóm đó; không render toàn bộ field của mọi nhóm cùng lúc.
4. User chỉnh toggle/giá trị trong submenu.
5. `Back` quay về list Settings; `Save`/`Done` của submenu lưu đúng nhóm. Nếu có thay đổi chưa lưu, Back cần cảnh báo hoặc giữ lại theo quyết định UX cuối cùng.
6. Back lần nữa hoặc Cancel đóng Settings và quay về shortcut menu, không làm mất trạng thái menu chính.

#### Flow D — Dùng chức năng location

`Joystick`, `Teleport` và `Speed` phải vẫn có shortcut rõ ràng. `Cooldown` là một view ngoài lề độc lập, không cần shortcut cấp 1. Chạm `Joystick` chỉ mở một joystick pad tối giản để di chuyển, không mở card chứa status/tọa độ/speed/cooldown. `Teleport` và các action cần user nhập dữ liệu tiếp tục mở `Dialog` tương ứng thay vì chiếm một view cố định. Như vậy joystick vẫn dùng được mà overlay không quay lại panel lớn.

### 5. Empty state và error state là gì?

- **Chưa có overlay permission:** giữ luồng hiện tại ở `MainActivity`, giải thích cần cấp Display over other apps; không hiển thị icon giả.
- **Service chưa sẵn sàng:** float icon vẫn có thể hiển thị, nhưng action phụ thuộc service phải disabled hoặc báo trạng thái `Starting…`; không giả vờ đã bật location.
- **Config không đọc được:** dùng default an toàn của `AutomationConfigRepository`, hiện trạng thái `Unknown`/`Unavailable` nếu không xác định được; không tự bật automation.
- **Toggle ghi thất bại hoặc bị ghi đè bởi API:** hiển thị feedback ngắn, đọc lại repository và render trạng thái thực tế; không để icon giữ trạng thái optimistic vô thời hạn.
- **Settings input không hợp lệ:** báo lỗi inline ở đúng field, giữ submenu mở và không lưu một phần dữ liệu sai.
- **Dialog Teleport lỗi:** giữ nguyên dialog và lỗi hiện tại; đổi layout overlay không được nuốt lỗi parse tọa độ hoặc lỗi mock location.
- **Không có shortcut phụ:** không phải trạng thái hợp lệ ở v1 vì danh sách shortcut là tĩnh; nếu capability bị unavailable thì icon cần disabled có lý do.

### 6. Edge case cho người dùng

- Tap và drag trên cùng một icon: dùng touch slop; chỉ xem là click nếu quãng đường di chuyển nhỏ.
- Kéo tới mép màn hình: clamp để icon và menu không nằm ngoài vùng nhìn thấy; không dùng vị trí tuyệt đối có thể mất sau rotation.
- Mở menu sát cạnh phải/trái: menu tự chọn hướng mở vào phía còn đủ chỗ; nếu không đủ, dùng panel full-width nhỏ hoặc re-anchor.
- Xoay màn hình, đổi độ phân giải/emulator hoặc display inset: tính lại bounds từ kích thước màn hình hiện tại.
- Màn hình nhỏ hoặc font lớn: shortcut grid được reflow/scroll, label không bị cắt đến mức không hiểu được.
- Chạm nhanh liên tục vào toggle: update phải idempotent, không tạo nhiều dialog/service instance; summary phải phản ánh giá trị cuối trong repository.
- API/headless service thay đổi config đồng thời với overlay: lần render tiếp theo đọc nguồn dùng chung; không giữ bản copy stale quá lâu.
- Bật `autoDiscard` hoặc `autoTransfer` từ shortcut có thể dẫn tới hành động ảnh hưởng dữ liệu game. Nên có cảnh báo xác nhận khi bật lần đầu hoặc khi chưa từng xác nhận trong Settings.
- Đóng overlay khi Settings/Teleport đang mở: phải đóng child UI sạch sẽ, không leak `WindowManager` view và không làm service crash.
- Process/service restart: vị trí float icon được khôi phục; menu mở dở không cần khôi phục, mặc định thu gọn để tránh che game bất ngờ.

### 7. Có nhất quán với phần còn lại của app không?

Có thể tái sử dụng các thuật ngữ và state hiện tại: `Automation`, `Auto catch`, `Auto spin`, `Auto encounter`, `Auto discard`, `Auto transfer`, `Berry`, `Feedback`, `Current position`, `Last active`. `MainActivity` hiện đã có enable/disable và status; overlay nên đọc cùng repository để không tạo hai định nghĩa trạng thái.

Project hiện dùng native Android views tạo bằng code, không có resource layout hoặc AndroidX UI dependency. Với scope layout này, giữ native `View`/`ListView`/`ScrollView` sẽ ít làm thay đổi build và dependency hơn. Tuy nhiên, hiện label trong app chủ yếu là tiếng Anh; cần chốt một ngôn ngữ hiển thị thống nhất trước khi implement, thay vì trộn tiếng Việt và tiếng Anh trong cùng menu.

### 8. Accessibility cần lưu ý gì?

- Float icon và mọi shortcut có vùng chạm tối thiểu 44×44dp, khuyến nghị 48dp.
- Icon phải có `contentDescription`; label ngắn nên được hiển thị hoặc đọc được bằng TalkBack, không dựa chỉ vào hình/icon.
- ON/OFF phải thể hiện bằng text/state và màu/tint, không chỉ bằng màu.
- Contrast của icon, text và nền overlay phải đủ rõ trên cả bản đồ sáng/tối.
- Settings row cần thứ tự focus tự nhiên, summary không làm mất label chính; submenu có title và hành động Back rõ ràng.
- Hỗ trợ font lớn: panel có thể scroll, không đặt chiều cao cố định cho text hoặc nút.
- Kéo là tương tác chính khó với screen reader; cần ít nhất giữ menu/action truy cập được, và nên có cách `Move overlay` bằng setting hoặc long-press action nếu sản phẩm cần accessibility đầy đủ.

### 9. Thiết kế lại lý tưởng là gì?

Khuyến nghị mô hình ba trạng thái UI, dùng một overlay root để tránh nhiều window chồng nhau:

1. **Collapsed:** một float icon tròn khoảng 52–56dp, có badge nhỏ thể hiện automation đang ON/OFF hoặc error. Icon này là vùng kéo và vùng mở menu.
2. **Shortcut menu:** panel compact neo theo float icon, gồm grid icon + label ngắn. Nhóm shortcut v1 nên ưu tiên:
   - Automation master
   - Auto catch
   - Auto spin
   - Auto encounter
   - Auto discard
   - Auto transfer
   - Joystick
   - Teleport
   - Settings
   - Close overlay

   `Speed` có thể là shortcut cycle hoặc mở dialog riêng; không đưa vào joystick pad. `Cooldown` không nằm trong menu/pad mà hiển thị ở view ngoài lề độc lập theo Section 11.

3. **Settings navigator:** `Settings` mở một `Activity` có `ListView` cấp 1. Mỗi row chỉ có title, summary và chevron; chạm row sẽ push `AutomationCategoryFragment` tương ứng. Boolean đơn giản có thể lưu ngay; các numeric limit/IV và berry selection giữ nút `Save`/`Done` theo từng submenu để tránh lưu dở dang.

4. **Joystick pad và dialogs:** `Joystick` chỉ mở joystick pad để di chuyển. `Teleport` mở dialog nhập tọa độ hiện tại; các action cần input cũng dùng dialog. Dismiss/Back đóng pad hoặc dialog và quay về shortcut menu, không tắt service. Cooldown badge vẫn độc lập và không bị ẩn theo menu.

Các quy tắc tương tác cần giữ:

- Chỉ collapsed float icon được kéo; panel mở không biến toàn bộ nội dung thành drag handle.
- Tap/drag được phân biệt bằng touch slop.
- Menu neo theo icon nhưng tự lật hướng khi sát mép.
- Tọa độ overlay được lưu trong `built_in_joystick` và được clamp khi restore.
- Toggle nhanh dùng `AutomationConfigRepository.update`; không tạo state song song.
- `autoDiscard`/`autoTransfer` cần confirm khi bật nếu chưa có consent tương ứng.

### 10. Cách validate cải thiện UX

Validation tối thiểu trên thiết bị Android và BlueStacks ở portrait/landscape:

- Từ Pokémon GO, mở menu và bật/tắt từng feature trong không quá 2 thao tác sau khi đã biết vị trí icon.
- Kéo icon đến bốn góc và giữa màn hình; đóng/mở service và xoay màn hình để kiểm tra không mất icon.
- Mở Settings, đi qua từng row vào submenu, Back về list rồi Save; kiểm tra chỉ nhóm được chỉnh bị thay đổi.
- Bật `autoCatch`/`autoSpin` và kiểm tra headless engine/API thấy cùng giá trị; test riêng `autoDiscard`/`autoTransfer` với cảnh báo.
- Mở joystick/teleport từ menu và xác nhận location flow, cooldown, status và error hiện tại không đổi.
- Dùng TalkBack, font lớn, màn hình nhỏ và nền game sáng để kiểm tra label, focus, contrast và touch target.

Chỉ số UX nên quan sát nếu có test người dùng: thời gian từ khi thấy icon tới thao tác thành công, số lần chạm nhầm khi kéo, tỷ lệ tìm được Settings/shortcut đầu tiên, số lần phải quay lại `MainActivity`, và số lỗi bật nhầm automation nguy hiểm.

---

## Acceptance Criteria (from spec)

> Source: không tìm thấy spec UI tương ứng trong `docs/newspec/**` hoặc `docs/specs/**`; các tiêu chí dưới đây là **inferred — needs BA confirm**, dựa trên yêu cầu user và implementation hiện tại.

| ID | Rule / Requirement | Formula / Expected | Acceptance note |
|----|--------------------|--------------------|-----------------|
| AC-1 | Float icon thu gọn | Khi overlay chạy, trạng thái mặc định chỉ hiển thị một float icon khoảng 52–56dp | Đáp ứng “có 1 float icon”, giảm diện tích che game. |
| AC-2 | Kéo thả toàn màn hình | `ACTION_MOVE` vượt touch slop → cập nhật vị trí; vị trí cuối được clamp trong display bounds và lưu lại | Đáp ứng “có thể kéo thả khắp màn hình”; tap không bị coi là drag. |
| AC-3 | Mở shortcut menu | Tap float icon → menu shortcut icon + label mở; tap ngoài hoặc tap lại → đóng | Menu không tạo thêm nhiều overlay window và không mất service. |
| AC-4 | Quick toggle | Icon của `enabled`, `autoCatch`, `autoSpin`, `autoEncounter`, `autoDiscard`, `autoTransfer` đọc/ghi cùng `AutomationConfigRepository` và hiển thị ON/OFF sau khi ghi | Đáp ứng “bật tắt nhanh các tính năng”; state phải đồng nhất với headless/API. |
| AC-5 | Bảo vệ action có tác động dữ liệu | Bật `autoDiscard`/`autoTransfer` từ shortcut lần đầu hoặc khi chưa có consent → hiển thị confirm/warning trước khi ghi `true` | Tránh bật nhầm automation có thể thay đổi inventory/Pokémon. Cần BA confirm chính sách confirm. |
| AC-6 | Giữ chức năng location | Từ shortcut menu vẫn mở được joystick, teleport, speed và close; cooldown được hiển thị ở view độc lập; behavior của `JoystickLocationController` không đổi | Không regression các tính năng hiện có; cooldown không bị phụ thuộc vị trí menu chính. |
| AC-7 | Settings cấp list | Tap `Settings` → hiển thị `ListView` các nhóm, mỗi row có title/summary/chevron; không hiển thị toàn bộ form chi tiết ở cấp này | Đáp ứng “mở menu setting dạng listview”. |
| AC-8 | Settings submenu | Tap từng row → chỉ mở submenu tương ứng; Back quay về list; invalid input báo inline; Save/Done lưu đúng nhóm | Đáp ứng “click vào từng cái mới vào submenu để cấu hình”. |
| AC-9 | Dismiss và lifecycle | Back/Cancel/dismiss ngoài đóng child panel đúng cấp; stop service remove toàn bộ view/dialog an toàn | Không leak/crash khi overlay bị dừng giữa chừng. |
| AC-10 | Accessibility và kích thước | Shortcut/row có touch target ≥44×44dp, `contentDescription`, label/state không phụ thuộc màu, hoạt động với font lớn/TalkBack ở mức cơ bản | Bảo đảm menu icon vẫn dùng được dù user không nhận diện icon hoặc dùng trợ năng. |
| AC-11 | Restore vị trí | Restart service khôi phục vị trí float icon; nếu vị trí cũ ngoài bounds thì clamp về vùng nhìn thấy; menu mặc định thu gọn | Tránh icon biến mất sau restart/rotation. |
| AC-12 | Verification | Build debug và test hiện tại pass; kiểm thử thủ công device/emulator bao phủ tap-vs-drag, 4 mép màn hình, settings navigation và các toggle | Tiêu chí hoàn thành thực tế cho thay đổi UI native hiện chưa có UI test framework. |

- Yêu cầu “1 float icon có thể kéo thả khắp màn hình” → `AC-1`, `AC-2`, `AC-11`.
- Yêu cầu “click thì sổ ra menu các icon shortcut” → `AC-3`, `AC-6`.
- Yêu cầu “bật tắt nhanh các tính năng” → `AC-4`, `AC-5`.
- Yêu cầu “setting mở menu dạng listview, click từng cái vào submenu” → `AC-7`, `AC-8`.
- Yêu cầu cooldown là view ngoài lề, `HH:MM`, kéo độc lập → `CV-1`, `CV-2`, `CV-3`, `CV-4` ở Section 11.
- Bảo toàn hành vi hiện tại và chất lượng UI → `AC-9`, `AC-10`, `AC-12`.

---

## Synthesis

### Key Insight

Đây không chỉ là đổi kích thước panel; cần đổi mô hình tương tác từ “một panel luôn mở” thành “handle thu gọn → shortcut menu → màn hình chi tiết”. Nếu chỉ ẩn panel mà không thiết kế lại entry point cho joystick, teleport và các toggle, tính năng hiện có sẽ bị mất hoặc trở nên khó tìm.

### Recommended Approach

Giữ một `WindowManager` overlay root nhưng quản lý các mode `Collapsed`, `Shortcuts` và `Joystick pad`; chỉ handle thu gọn được kéo, có touch-slop và persist/clamp vị trí. Tách settings thành navigator `ListView` cấp nhóm và submenu chi tiết, tái sử dụng `AutomationConfigRepository` hiện tại; quick toggle cũng ghi trực tiếp vào repository để headless/API/overlay luôn cùng nguồn state. Ưu tiên menu compact có icon kèm label, mở teleport/action input bằng dialog, và xác nhận trước khi bật các automation có khả năng discard/transfer.

### Risks to Watch

- Phân biệt tap/drag và tính bounds/insets khi xoay màn hình có thể gây lỗi UX khó thấy nếu chỉ test một emulator.
- Overlay đang dùng native view programmatically và `FLAG_NOT_FOCUSABLE`; Settings được tách thành `Activity` riêng để có focus/keyboard, back stack và layout đầy đủ cho các input numeric.
- Quick toggle có thể bị ghi đồng thời từ `MainActivity`, API hoặc headless service; mọi render phải đọc lại repository, không giữ bản copy riêng.
- `autoDiscard`/`autoTransfer` là action nhạy cảm; nếu bỏ confirm hoặc label không rõ, user có thể bật nhầm.

### Open Questions

- Danh sách và thứ tự shortcut chính thức là gì? `Speed` còn cần quyết định; ~~`Cooldown` là icon cấp 1 hay nằm trong card `Joystick`~~ → Resolved in Section 11: cooldown là view ngoài lề độc lập, không thuộc shortcut/card.
- ~~Khi chạm `Joystick`, user muốn joystick mở trong card nhỏ neo cạnh float icon hay muốn panel joystick chiếm lại layout lớn như hiện tại?~~ → Resolved in Section 12: chỉ mở joystick pad tối giản để di chuyển, không có control card/panel thông tin.
- UI giữ tiếng Anh như app hiện tại hay chuyển sang tiếng Việt? Cần chọn một ngôn ngữ, đặc biệt cho label toggle và Settings row.
- `autoDiscard` và `autoTransfer` có bắt buộc confirm mỗi lần bật, chỉ confirm lần đầu, hay đã được user coi là opt-in trong Settings?
- Submenu numeric dùng `Save/Cancel` theo nhóm hay lưu tức thì? Hành vi Back khi có thay đổi chưa lưu cần được chốt.
- ~~Vị trí mặc định của float icon nên giữ anchor hiện tại (góc dưới trái) hay đặt một vị trí mặc định mới? Có muốn snap vào cạnh màn hình sau khi kéo không?~~ → Resolved in Section 13: mặc định góc dưới trái, không snap.

---

## Section 11 — Clarification: cooldown là view độc lập ngoài lề

Người dùng xác nhận cooldown không nằm trong shortcut menu hoặc location control card. Đây là một floating view/badge riêng ở ngoài lề màn hình, có thể kéo-thả độc lập với float icon chính và chỉ hiển thị thời gian dạng `HH:MM`.

### Điều chỉnh thiết kế

- Main overlay vẫn gồm float icon, shortcut menu và location control.
- Cooldown badge là một view overlay sibling riêng, có `WindowManager.LayoutParams` và touch handler riêng. Kéo badge không di chuyển float icon/menu; kéo float icon cũng không di chuyển badge.
- Badge có vị trí lưu riêng, ví dụ `cooldown_overlay_x`/`cooldown_overlay_y`, và được clamp theo display bounds sau restart, rotation hoặc đổi resolution.
- Khi cooldown đang active, badge chỉ render `HH:MM`, không render label dài, khoảng cách, action name hoặc giây. Nên làm tròn lên theo phút để thời gian còn lại khác 0 không bị hiển thị sớm thành `00:00`:
  `displayMinutes = ceil(remainingMillis / 60_000)` → `HH:MM`.
- Khi không có cooldown active, khuyến nghị ẩn badge để `00:00` không bị hiểu nhầm là đang chờ. Đây là giả định UX cần BA confirm nếu user muốn badge luôn hiện.
- Cooldown không cần icon shortcut cấp 1; nó là thông tin persistent, luôn độc lập với việc shortcut menu đang đóng hay mở.

### Acceptance criteria bổ sung

| ID | Rule / Requirement | Formula / Expected | Acceptance note |
|----|--------------------|--------------------|-----------------|
| CV-1 | View cooldown độc lập | Cooldown badge là overlay sibling riêng, không nằm trong bounds/layout của float icon hoặc shortcut menu | Đáp ứng “1 cái là cooldown; là 1 view ngoài lề”. |
| CV-2 | Format tối giản | `remainingMillis > 0` → `ceil(remainingMillis / 60_000)` và format `HH:MM` với hai chữ số cho giờ/phút | Không hiển thị `MM:SS`, khoảng cách, action hoặc text status trong badge. |
| CV-3 | Kéo-thả độc lập | Touch/drag badge chỉ cập nhật `cooldownWindowParams.x/y`; touch/drag float icon chỉ cập nhật main window params | Hai view không được kéo theo nhau hoặc mở shortcut do kéo. |
| CV-4 | Persist và lifecycle | Vị trí cooldown được lưu/khôi phục riêng, clamp trong bounds; stop service remove badge an toàn | Không mất badge sau restart và không leak `WindowManager` view. |

### Open Questions sau clarification

- Danh sách và thứ tự shortcut chính thức là gì? `Speed` còn cần quyết định.
- ~~Khi chạm `Joystick`, user muốn joystick mở trong card nhỏ neo cạnh float icon hay muốn panel joystick chiếm lại layout lớn như hiện tại?~~ → Resolved in Section 12: chỉ mở joystick pad tối giản để di chuyển, không có control card/panel thông tin.
- UI giữ tiếng Anh như app hiện tại hay chuyển sang tiếng Việt? Cần chọn một ngôn ngữ, đặc biệt cho label toggle và Settings row.
- `autoDiscard` và `autoTransfer` có bắt buộc confirm mỗi lần bật, chỉ confirm lần đầu, hay đã được user coi là opt-in trong Settings?
- Submenu numeric dùng `Save/Cancel` theo nhóm hay lưu tức thì? Hành vi Back khi có thay đổi chưa lưu cần được chốt.
- ~~Vị trí mặc định của float icon và cooldown badge là gì? Có muốn snap vào cạnh màn hình sau khi kéo không? Cả hai vẫn phải kéo độc lập.~~ → Resolved in Section 13: float icon góc dưới trái, cooldown badge góc trên phải, cả hai không snap và vẫn kéo độc lập.

---

## Section 12 — Clarification: Joystick chỉ là pad di chuyển

Người dùng xác nhận `Joystick` không phải một control card chứa status, tọa độ, speed hoặc cooldown. Khi chạm shortcut `Joystick`, chỉ cần hiển thị joystick pad để kéo hướng di chuyển; các chức năng khác giữ entry point riêng bằng dialog hoặc view riêng theo yêu cầu.

### Flow được chốt

- **Joystick:** shortcut → hiện joystick pad tối giản; người dùng kéo pad để gọi `controller.setJoystick(...)`. Không đưa summary, speed, cooldown hay nút teleport vào cùng view.
- **Teleport:** shortcut → mở `AlertDialog` nhập latitude/longitude như flow hiện tại; không tạo ô nhập cố định trên overlay.
- **Các action cần input:** dùng `Dialog` tương ứng thay vì mở một panel lớn. `Speed` chưa chốt là cycle shortcut hay dialog nhập giá trị, nhưng không nên làm phình joystick pad.
- **Cooldown:** tiếp tục là badge/view ngoài lề độc lập theo Section 11, không nằm trong joystick pad hoặc shortcut menu.
- **Thoát joystick pad:** Back, dismiss hoặc chạm lại shortcut/float icon đóng pad và quay về trạng thái menu/thu gọn; không dừng service.

### Acceptance criteria bổ sung

| ID | Rule / Requirement | Formula / Expected | Acceptance note |
|----|--------------------|--------------------|-----------------|
| LOC-1 | Joystick pad tối giản | Chạm shortcut `Joystick` → chỉ render `JoystickView` và vùng cần thiết để kéo di chuyển | Không có status, tọa độ, speed, cooldown hoặc teleport field trong pad. |
| LOC-2 | Điều khiển movement | Kéo pad → giữ nguyên callback `controller.setJoystick(angle, strength)` và behavior movement hiện tại | Đổi layout không làm thay đổi logic mock location. |
| LOC-3 | Input dùng dialog | Chạm `Teleport` hoặc action cần nhập → mở dialog; validation/error vẫn nằm trong dialog và không tạo persistent input view | Đáp ứng yêu cầu teleport và thao tác nhập liệu không chiếm layout overlay. |
| LOC-4 | Đóng đúng cấp | Back/dismiss đóng pad hoặc dialog, quay về shortcut/collapsed; cooldown badge và main float icon không bị đóng theo | Không dừng service, không mất vị trí các overlay sibling. |

- Yêu cầu “chỉ cần hiện joystick pad để di chuyển” → `LOC-1`, `LOC-2`.
- Yêu cầu “teleport hay gì đó sẽ hiện dialog cho nhập thay vì view” → `LOC-3`, `LOC-4`.

---

## Section 13 — Quyết định vị trí mặc định và snap

Người dùng giao quyền chọn phương án. Chốt thiết kế như sau:

- **Float icon chính:** mặc định ở góc dưới trái, cách mép trái `16dp` và cách mép dưới `24dp`. Vị trí này gần với anchor hiện tại, giảm thay đổi bất ngờ cho user đã quen overlay cũ.
- **Cooldown badge:** mặc định ở góc trên phải, cách mép phải `16dp` và cách mép trên `24dp`, tạo cảm giác là một thông tin ngoài lề độc lập với tool menu.
- **Không snap:** sau khi kéo và thả, mỗi view giữ đúng vị trí người dùng thả trong phạm vi bounds. Không tự hút về cạnh gần nhất; như vậy đúng với yêu cầu có thể di chuyển khắp màn hình.
- Cả hai view vẫn clamp nếu vị trí cũ nằm ngoài vùng nhìn thấy sau rotation, đổi resolution hoặc thay đổi display inset.

### Acceptance criteria bổ sung

| ID | Rule / Requirement | Formula / Expected | Acceptance note |
|----|--------------------|--------------------|-----------------|
| POS-1 | Vị trí mặc định float icon | `x = 16dp` từ trái, `y = 24dp` từ đáy | Giữ gần anchor hiện tại nhưng giảm kích thước footprint nhờ float icon. |
| POS-2 | Vị trí mặc định cooldown | `x = 16dp` từ phải, `y = 24dp` từ đỉnh | Cooldown badge nằm ngoài lề, không dính vào shortcut menu. |
| POS-3 | Không snap sau drag | Khi user release, giữ tọa độ đã thả sau khi clamp bounds; không làm tròn về cạnh | Hai view có thể đặt ở bất kỳ vùng nào trên màn hình. |
| POS-4 | Restore an toàn | Vị trí được persist riêng cho từng view; khi restore dùng `clamp(position, currentDisplayBounds)` | Không làm view biến mất sau rotation/resolution change. |

- Yêu cầu “chọn đại” về vị trí mặc định → `POS-1`, `POS-2`.
- Yêu cầu kéo-thả độc lập và không bị ép về cạnh → `POS-3`, `POS-4`.
