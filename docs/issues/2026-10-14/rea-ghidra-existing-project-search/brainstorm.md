# Brainstorm: Search trên Ghidra project đã phân tích bằng REA-compatible tooling

**Loại:** architecture
**Ngày phân tích đầu tiên:** 2026-10-14
**Phạm vi game/build/ABI:** Không phụ thuộc build Pokémon GO cụ thể; công cụ native-analysis tổng quát

## Bước 1 — Yêu cầu và phạm vi

### Vấn đề cần giải quyết

REA hiện có Ghidra provider headless nhưng phân tích binary trong project tạm. Yêu cầu cần đánh giá khả năng tạo một bộ script tương tự REA, hoặc fork REA, để tìm kiếm trực tiếp trên Ghidra project đã được phân tích và tái sử dụng symbol, comment, type, bookmark và kết quả auto-analysis sẵn có.

### Hành vi mong muốn

- Mở đúng Ghidra project và `Program` người dùng chỉ định, không tự chọn project gần đó.
- Thực hiện được các truy vấn đọc phổ biến: search function/symbol/string, xref, caller/callee, decompile và đọc type/comment.
- Không import lại binary hoặc chạy lại toàn bộ analysis nếu project đã có kết quả phù hợp.
- Mặc định read-only; mọi thay đổi metadata phải là chế độ riêng, có target rõ và có thể kiểm tra.
- Giữ được định danh project/program, binary hash, language/compiler spec, image base và trạng thái analysis trong output để tránh trộn bằng chứng giữa build.

### Phạm vi lượt phân tích

- So sánh ba hướng: fork REA provider, viết MCP/CLI bridge riêng dùng Ghidra API, và export project sang dạng trung gian rồi dùng REA hiện tại.
- Xác định integration point tối thiểu với Ghidra headless hoặc Ghidra GUI.
- Đề xuất lộ trình triển khai và tiêu chí chấp nhận; chưa triển khai code trong lượt này.

### Ràng buộc

- Không làm thay đổi binary gốc.
- Không phụ thuộc vào legacy Il2CppDumper hoặc `scripts/reverse-pogo-apk.sh`.
- Không biến project Ghidra hiện có thành bằng chứng cho build khác nếu identity không khớp.
- Tuân thủ giới hạn không thêm/sửa test của repository nếu sau này triển khai tại repository này; vẫn có thể dùng fixture/project thử nghiệm ngoài production source khi được người dùng cho phép.
- File source mới nếu có phải không vượt quá 500 dòng.

### Thuộc tính chất lượng ưu tiên

1. Bảo toàn và xác minh identity của `Program`.
2. Tái sử dụng đầy đủ metadata đã phân tích.
3. Read-only/fail-closed mặc định.
4. Khả năng tương thích với API/query shape của REA.
5. Dễ bảo trì khi Ghidra hoặc REA đổi phiên bản.

### Giả định và câu hỏi còn thiếu

- Chưa có đường dẫn tới project Ghidra thật để thử nghiệm.
- Chưa xác minh project là `.gpr` + `.rep`, shared project/server, hay exported `.gzf`.
- Chưa rõ yêu cầu cần chạy khi Ghidra GUI đang mở hay chỉ cần headless trên project đóng.
- Chưa rõ có cần ghi rename/comment/type ngược vào project hay chỉ search read-only.

## Bước 2 — Bằng chứng từ reverse

### Nguồn và phát hiện

| Nguồn | Quan sát | Giới hạn |
|---|---|---|
| REA `binary_session` ngày 2026-10-14 | Ghidra provider khả dụng với Ghidra `12.1.3`, JDK `21.0.12.1`, headless launcher và native decompiler đều pass kiểm tra. | Chưa mở target/project thật nên chưa xác minh lock, project schema hoặc một `Program` cụ thể. |
| `rea-agents@6.4.0/package.json` | Package dùng giấy phép MIT và trỏ repository `https://github.com/morluto/rea`; fork hợp lệ về mặt giấy phép nếu giữ copyright/license notice. | Đây là package phát hành cục bộ, không chứng minh upstream mới hơn không thay đổi. |
| `dist/ghidra/GhidraProvider.js` | Provider hiện tại tự mô tả là `isolated ephemeral headless import` và chỉ nhận target `kind === executable`. | Không có nhánh nhận Ghidra local/shared project. |
| `dist/ghidra/GhidraLauncher.js` | Invocation bị cố định thành `rea-project -import <target> -readOnly -deleteProject ... -postScript ReaGhidraBridge.java`. | Đây là điểm cần tách abstraction để thêm `-process` mode. |
| `dist/ghidra/GhidraAnalysisProfile.js` | Profile ghi `import_mode: ephemeral-source-immutable`; digest của profile gắn với semantics hiện tại. | Nếu thêm existing-project mode phải có profile/digest khác, không được giả vờ tương đương import mode. |
| `bridge/ghidra/ReaGhidraBridge.java` | Bridge đã thao tác trên `currentProgram` và cung cấp search procedure/string, names, xrefs, callers/callees, pseudocode, assembly, type và annotation. Nó kiểm `currentProgram.getExecutableSHA256()` với target digest và đọc language/compiler/image base. | Logic query có thể tái sử dụng, nhưng bootstrap/session descriptor hiện giả định target binary đã được REA admit và import. |
| Ghidra `support/analyzeHeadlessREADME.md` | Ghidra chính thức hỗ trợ project có sẵn bằng `<project_location> <project_name>[/folder] -process [project_file]`; `-readOnly` làm mọi thay đổi bị discard. Ví dụ có `-process importedBinA.exe -noanalysis`. | Cần xử lý project lock, shared repository, nhiều file trùng tên và xác định project file bằng path chính xác. |

### Kết luận bằng chứng

- **Đã xác nhận:** Ghidra có primitive cần thiết để xử lý `Program` đã tồn tại; không cần viết lại decompiler/search engine.
- **Đã xác nhận:** phần lớn `ReaGhidraBridge.java` có thể tái sử dụng vì nó truy vấn `currentProgram`, không phụ thuộc trực tiếp vào loader sau khi session đã chạy.
- **Đã xác nhận:** REA 6.4.0 chưa expose existing-project target; thay đổi chính nằm ở target admission, launcher, identity/profile và lifecycle, không nằm ở từng search operation.
- **Suy luận có cơ sở:** fork REA để thêm một Ghidra project provider/mode sẽ ít code trùng lặp hơn một MCP mới, nhưng coupling và chi phí rebase cao hơn.
- **Chưa xác minh:** project thực tế là local `.gpr/.rep`, `.gzf`, hay Ghidra Server; trạng thái GUI/project lock; `Program.getExecutableSHA256()` có còn populated và đúng với binary gốc hay không.

## Bước 3 — Function/method, param và hành vi

### `GhidraProvider.inspectTargetSupport(target)` — Đã xác nhận

- Nguồn: `rea-agents/dist/ghidra/GhidraProvider.js:77`; REA 6.4.0.
- Signature quan sát: `inspectTargetSupport(target)`.
- Vai trò: admission gate hiện chỉ chấp nhận `target.kind === "executable"`; mọi project/database target bị từ chối trước launcher.

| Param | Kiểu / modifier | Ý nghĩa / nguồn giá trị | Ràng buộc | Bằng chứng |
|---|---|---|---|---|
| `target` | REA target object | Artifact đã được session nhận diện | Hiện phải là executable và architecture được hỗ trợ | `GhidraProvider.js:77-126` |

- Kết quả: support status cùng diagnostics.
- Cần thay đổi: thêm target kind riêng cho Ghidra project/program, không nới `analysis-database` chung chung vì dễ nhận nhầm Hopper/JEB database.

### `GhidraHeadlessLauncher.launch(session, options = {})` — Đã xác nhận

- Nguồn: `rea-agents/dist/ghidra/GhidraLauncher.js:37`.
- Signature quan sát: `async launch(session, options = {})`.
- Vai trò: tạo runtime riêng, descriptor có token/digest, build argv, launch process và quản lý cleanup.

| Param | Kiểu / modifier | Ý nghĩa / nguồn giá trị | Ràng buộc | Bằng chứng |
|---|---|---|---|---|
| `session` | object | Runtime root, endpoint, token, run ID, target SHA-256, provider version, profile digest | Tất cả phải thuộc cùng session | `GhidraLauncher.js:37-81` |
| `options.signal` | `AbortSignal?` | Cancellation | Hủy phải cleanup owned process nhưng không được xóa project người dùng | `GhidraLauncher.js:37-44` |

- Kết quả: owned launch/process hoặc typed error.
- Cần thay đổi: launcher phải nhận một discriminated mode (`ephemeral-import` hoặc `existing-project-read-only`) thay vì luôn suy ra project tạm.

### `ghidraHeadlessArguments(options)` — Đã xác nhận

- Nguồn: `rea-agents/dist/ghidra/GhidraLauncher.js:221-295`.
- Signature quan sát: `ghidraHeadlessArguments(options)`.
- Hành vi hiện tại: luôn sinh `<temp>/project rea-project -import <targetPath> ... -readOnly -deleteProject`.
- Cần thay đổi: tách builder theo mode. Existing-project mode phải sinh `<projectLocation> <projectName>/<folderPath> -process <programName> -noanalysis -readOnly`; tuyệt đối không thêm `-deleteProject`.

### `ReaGhidraBridge.run()` — Đã xác nhận

- Nguồn: `rea-agents/bridge/ghidra/ReaGhidraBridge.java:164-219`.
- Signature: `public void run() throws Exception`; nhận đúng một script arg là descriptor path.
- Hành vi: yêu cầu `currentProgram`, kiểm Ghidra version và `currentProgram.getExecutableSHA256()` khớp `descriptor.targetSha256`, sau đó mở decompiler và phục vụ query.
- Kết quả / hoàn tất: sống đến lệnh shutdown hoặc disconnect; dispose decompiler/extensions trong `finally`.
- Cần thay đổi nhỏ: thông báo lỗi “requires an imported program” thành “requires an active program”; bổ sung project/program identity vào handshake. Query implementation không cần đổi để có search cơ bản.

### `GhidraProjectTarget` — Đề xuất, chưa tồn tại

```ts
type GhidraProjectTarget = {
  kind: "ghidra-project-program";
  projectLocation: string;
  projectName: string;
  folderPath?: string;
  programName: string;
  expectedExecutableSha256: string;
  access: "read-only";
};
```

| Field | Nguồn giá trị | Ràng buộc đề xuất |
|---|---|---|
| `projectLocation` | Người gọi chọn | Absolute canonical path; phải chứa đúng local project; không nhận glob |
| `projectName` | Người gọi chọn hoặc parse chính xác từ `.gpr` | Tên literal, không gắn folder tùy ý |
| `folderPath` | Người gọi chọn | Project path canonical; không `..` |
| `programName` | Người gọi chọn sau inventory | Literal project file; không wildcard ở API v1 |
| `expectedExecutableSha256` | Binary gốc hoặc giá trị project đã xác minh | Bắt buộc và phải khớp `Program.getExecutableSHA256()` |
| `access` | Cố định v1 | Chỉ `read-only`; write-back là capability riêng về sau |

- Output identity đề xuất: project canonical path/hashable identity, project name, folder/program path, executable SHA-256, Ghidra version, language ID, compiler spec ID, image base, analysis option digest và read-only state.
- Chưa xác minh: API Ghidra nào cung cấp stable project ID; nếu không có, dùng canonical path + `.gpr` metadata digest + program path và ghi limitation thay vì tự nhận là globally stable.

## Bước 4 — Luồng hành vi và điểm tích hợp

### Luồng read-only đề xuất

1. Người gọi truyền `projectLocation`, `projectName`, `folderPath`, `programName` và `expectedExecutableSha256`.
2. Admission layer canonicalize path, từ chối glob/traversal, xác nhận `.gpr` và project storage cùng tồn tại; không tự scan rồi chọn match đầu tiên.
3. Launcher tạo **chỉ** runtime/endpoint/log tạm thuộc provider, nhưng trỏ `analyzeHeadless` vào project người dùng.
4. Ghidra chạy `-process <programName> -noanalysis -readOnly -postScript ReaGhidraBridge.java <descriptor>`.
5. Bridge nhận `currentProgram`, kiểm Ghidra version, executable SHA-256, project/program selector, language/compiler/image base và capability mode.
6. Chỉ sau khi handshake khớp, MCP/CLI expose các query hiện có như `search_procedures`, `search_strings`, `list_names`, `xrefs`, `analyze_function`.
7. Khi close/cancel, provider đóng socket/process và xóa runtime tạm; **không xóa, rename, save hoặc compact project**.

### Điểm tích hợp và ownership

| Thành phần | Owner | Input | Output / trách nhiệm |
|---|---|---|---|
| Target/admission | REA application/session layer | Explicit project/program selector | Canonical target identity hoặc typed refusal |
| Launcher argv | Ghidra provider | Target + committed profile | `analyzeHeadless -process` argv, không shell interpolation |
| Program bridge | `ReaGhidraBridge.java` | `currentProgram` + authenticated descriptor | Query result và handshake identity |
| Evidence projection | REA provider/application | Provider result + profile | Evidence ghi rõ authority, coverage, limitation và project mode |
| Repo integration | `reverse/` workflow | Evidence bundle + artifact hashes | Tái sử dụng query result; không thay thế `reverse/artifacts/SHA256SUMS` |

### Nhánh lỗi và fail-closed

- Project đang mở trong Ghidra GUI: tài liệu Ghidra cảnh báo Headless Analyzer có thể không chạy. V1 phải trả `project_locked_or_open`; không copy thầm `.rep`, không kill GUI.
- Không tìm thấy `programName`, hoặc có nhiều match khi dùng folder không chính xác: từ chối, không fallback sang file khác.
- `Program.getExecutableSHA256()` null/khác expected hash: đóng session, không cho query như target đã xác thực.
- Ghidra version/profile khác commitment: tạo profile mới hoặc từ chối snapshot/Evidence cũ.
- `-readOnly` không được phản ánh đúng trong handshake: không expose session.
- Cancellation/timeout: terminate process do provider sở hữu; project người dùng giữ nguyên. Không dùng cleanup path hiện tại có semantics xóa project tạm cho project ngoài.
- Shared Ghidra Server: để ngoài MVP vì cần authentication, repository permissions và lock semantics riêng.

### Ranh giới với `pogo-root-automation`

- Công cụ này thuộc reverse tooling, không thuộc `core/`, `app/`, `game-adapter/` hoặc `zygisk/` runtime.
- Nếu dùng cho project này, output quan trọng vẫn phải đi qua REA Evidence bundle và hash trong `reverse/`; project Ghidra không tự trở thành nguồn canonical chỉ vì đã được analyst annotate.
- Không tạo lại pipeline Il2CppDumper/reverse APK cũ. Tool chỉ truy vấn một Ghidra `Program` đã được người dùng chọn và xác thực.

## Bước 5 — Hướng xử lý

### Phương án A — Fork REA, thêm existing-project mode

**Thiết kế:** thêm tool/contract rõ nghĩa như `open_ghidra_project_program`, target/profile mới và nhánh launcher `-process`; tái sử dụng `ReaGhidraBridge.java`, client transport, search tools và Evidence projection.

**Ưu điểm**

- Giữ nguyên các tool name và result shape quen thuộc của REA.
- Có sẵn authentication token, cancellation, process ownership, capability reporting và Evidence ledger.
- Phù hợp nhất với `reverse/` workflow canonical của repository.

**Nhược điểm**

- Không phải thay vài dòng argv: target/session model hiện snapshot một executable bất biến, nên cần thay đổi contract và identity cẩn thận.
- Fork dài hạn phải rebase theo upstream; vì package MIT nên hợp lệ nhưng vẫn phải giữ license notice.
- Cần tránh đưa project path mutable vào `subject.digest` như thể nó là binary artifact.

**Đánh giá:** tốt nhất cho bản production và nên chuẩn bị dưới dạng upstream-friendly change/PR thay vì một fork lệch lâu dài.

### Phương án B — MCP/CLI bridge riêng, dùng Ghidra headless `-process`

**Thiết kế:** một service nhỏ Node/TypeScript quản lý session và chạy Java `HeadlessScript`; copy/adapt phần query cần thiết từ bridge REA theo MIT, ban đầu chỉ expose inventory/search/xref/decompile.

**Ưu điểm**

- MVP nhanh, ít đụng model nội bộ REA.
- API có thể thiết kế đúng cho project/program ngay từ đầu.
- Dễ thử trên một project thật trước khi đầu tư tích hợp Evidence đầy đủ.

**Nhược điểm**

- Phải tự làm security/session cleanup, typed errors, budgets, cancellation và identity evidence.
- Dễ dần trở thành một bản REA thứ hai khó bảo trì.
- Muốn dùng trong workflow canonical vẫn phải viết adapter/export Evidence.

**Đánh giá:** phù hợp cho proof-of-concept hẹp; không nên là đích cuối nếu cần toàn bộ semantics của REA.

### Phương án C — Ghidra GUI extension/MCP cho project đang mở

**Thiết kế:** extension chạy trong Ghidra GUI, phục vụ query từ active/open `Program` qua authenticated local socket.

**Ưu điểm**

- Dùng được project đang mở, thấy ngay unsaved/current analyst state.
- Không gặp headless project-open limitation.

**Nhược điểm**

- Lifecycle/UI/threading và quyền mutation phức tạp hơn đáng kể.
- Cursor/active-program state dễ tạo query không tái lập nếu API không bắt target rõ.
- Cần đóng gói extension theo Ghidra version và thiết kế consent/authentication chặt.

**Đánh giá:** chỉ làm phase sau nếu yêu cầu bắt buộc search khi GUI đang mở.

### Phương án D — Export project/program rồi dùng REA hiện tại

**Thiết kế:** export `Program`/metadata sang artifact trung gian rồi import vào REA.

**Ưu điểm:** không sửa nhiều REA runtime.

**Nhược điểm:** REA 6.4.0 hiện vẫn chỉ admit executable cho Ghidra provider; export/import có thể mất metadata hoặc thay identity, nên cuối cùng vẫn cần provider mới. Không đáp ứng tốt mục tiêu “search trực tiếp project đã analyze”.

**Đánh giá:** không khuyến nghị.

### Hướng đề xuất

Chọn **Phương án A theo hai phase**:

1. Làm proof-of-concept ngoài production path bằng chính bridge REA và `analyzeHeadless -process -noanalysis -readOnly` trên một bản project thử/backup do người dùng chỉ định. Mục tiêu chỉ xác minh Ghidra mở được đúng `Program`, hash và metadata xuất hiện đúng.
2. Sau khi probe pass, fork REA và thêm contract `open_ghidra_project_program` upstream-friendly. Giữ API v1 read-only, local project, project phải đóng trong GUI, một program explicit mỗi session.

Không nên bắt đầu bằng việc clone toàn bộ REA thành “bộ script tương tự”. Phần có giá trị nhất đã có sẵn trong bridge; viết lại sẽ tăng rủi ro khác biệt search/Evidence mà không tạo thêm capability.

### Phạm vi triển khai tối thiểu đề xuất

- Contract/schema và CLI/MCP tool mở project/program.
- Canonical validation cho local project, folder và program selector.
- Analysis profile mới: `existing-project-read-only-v1`.
- Launcher builder `-process -noanalysis -readOnly`; không `-deleteProject`, không seeds/analyzer mutation ở v1.
- Descriptor/handshake có project/program identity và expected executable hash.
- Tái sử dụng toàn bộ read-only query operations hiện có.
- Typed errors cho project open/locked, program missing/ambiguous, hash mismatch và unsupported shared project.
- Đóng session không lưu project; output Evidence ghi rõ đây là analysis database state, không phải static execution evidence.

## Bước 6 — Tiêu chí chấp nhận và xác minh

| ID | Nguồn yêu cầu | Điều kiện đầu vào | Hành vi / postcondition | Method liên quan | Cách kiểm chứng |
|---|---|---|---|---|---|
| AC-01 | Yêu cầu người dùng | Local project đã đóng, selector chỉ đúng một `Program` | Mở đúng project/program đã analyze mà không import lại binary | `open_ghidra_project_program`, launcher `-process` | So sánh program path, hash, language/compiler/image base và analyst-defined symbol/comment/type với Ghidra GUI |
| AC-02 | Đề xuất từ yêu cầu | Session read-only | Search procedure/string/name, xref, caller/callee và decompile dùng metadata hiện có | Existing REA query operations | Chọn trước các fixture fact đã rename/comment/type; query trả đúng cùng address/value |
| AC-03 | Fail-closed | Expected SHA-256 sai hoặc project không có executable SHA-256 | Session bị từ chối trước khi expose query | `ReaGhidraBridge.run()` identity gate | Probe với digest sai và null-digest fixture; nhận typed identity error |
| AC-04 | Target chính xác | Program thiếu hoặc selector ambiguous | Không tự chọn program khác | Admission + launcher selector | Probe missing/duplicate-name cases; không có active session |
| AC-05 | Không phá project | Close, cancel, timeout hoặc bridge crash | Không xóa project, không save metadata/analyzer changes; chỉ runtime tạm bị dọn | Launcher/client cleanup | Dùng project copy kiểm chứng domain state trước/sau và xác nhận project vẫn mở bình thường trong GUI |
| AC-06 | Ghidra limitation | Project đang mở/locked | Trả lỗi rõ, không kill GUI và không copy/bypass lock | Launcher error projection | Giữ project mở trong GUI rồi thử open; GUI/process vẫn nguyên trạng |
| AC-07 | Tương thích REA | Existing executable workflow | `open_binary` ephemeral import và Evidence semantics cũ không thay đổi | Provider router/profile | Chạy suite hiện có của fork REA và real-Ghidra verification theo hướng dẫn upstream |
| AC-08 | Canonical reverse workflow | Query thành công | Evidence phân biệt analysis-database state với binary artifact và ghi project/program identity + limitation | Evidence projection | Export/import bundle rồi kiểm record giữ đủ identity, coverage và profile digest |

### Kế hoạch xác minh

1. Người dùng cung cấp exact path của một local Ghidra project và exact project folder/program path; ưu tiên một bản copy có thể bỏ đi.
2. Kiểm phiên bản Ghidra tạo project và Ghidra runtime dùng để mở; khác major/minor thì dừng trước khi migration tự động.
3. Ghi trước một tập fact trong GUI: function rename, comment, type và xref đã biết.
4. Chạy probe read-only bằng `-process <program> -noanalysis -readOnly` với bridge; không bật write tools.
5. Đối chiếu handshake identity và các fact đã ghi trước.
6. Thử các nhánh digest sai, selector sai, project đang mở và cancellation.
7. Chỉ sau khi probe đạt mới sửa fork REA, chạy test/conformance/real-Ghidra verification của upstream fork và kiểm license notices.

Không cần chạy Gradle/Android test cho tài liệu brainstorm này vì không thay đổi source runtime. Khi triển khai tool trong fork REA, dùng test policy của repository REA; nếu thêm integration vào repository này thì vẫn tuân thủ quy tắc hiện tại là không tạo/sửa test files và chỉ chạy test sẵn có.

### Kết luận và câu hỏi còn mở

- **Khả thi:** Ghidra đã cung cấp `-process` cho project có sẵn và REA bridge hiện tại đã có hầu hết query cần thiết.
- **Khuyến nghị:** fork REA theo thay đổi upstream-friendly; không viết lại một REA khác. Bắt đầu bằng proof-of-concept read-only trên project copy.
- **Giới hạn MVP:** local project, GUI đóng, một explicit `Program`, read-only, exact executable SHA-256.
- **Cần người dùng cung cấp trước triển khai:** đường dẫn `.gpr`/project directory, project folder/program path, phiên bản Ghidra đã tạo project và xác nhận có cần chạy khi GUI đang mở hay không.
- **Câu hỏi phase sau:** write-back rename/comment/type, Ghidra Server/shared project và GUI-attached provider.

## Kết quả triển khai 2026-10-14

- Project thực tế là local project
  `reverse/artifacts/pogo-0.433.0/pogo-0.433.0.gpr` + `.rep`, có một Program
  `/libil2cpp.so` khớp SHA-256
  `6b1e11619362ddd53f9dbcd7be1d4296bdec5dda27b9f742c16b8403359c25a0`.
- Ghidra GUI đang giữ exclusive project lock. Overlay không kill GUI hoặc bỏ
  lock; wrapper tạo copy-on-write snapshot của `.gpr/.rep`, rồi chỉ mở snapshot.
- Đã triển khai overlay `rea-agents@6.4.0` tại
  `reverse/rea/ghidra-existing-project/` và wrapper
  `scripts/rea-ghidra-project.sh`. Cả Node client lẫn Java bridge enforce
  read-only; negative smoke test trả `capability_unavailable` cho annotation.
- Positive smoke test chạy qua REA/Ghidra 12.1.3 trong khoảng vài chục giây,
  không re-analyze và không timeout 30 phút như ephemeral import.
- Evidence retained `ev_88f0e196...` (procedure search) và `ev_4e0f9e23...`
  (defined-string search) không tìm thấy tên map/catch/storage/Virtual GO Plus
  trong Program hiện tại. Kết quả này chứng minh coverage của database đang có,
  không chứng minh type vắng mặt khỏi build vì Program chưa được đặt managed
  symbols.
- Mapper `scripts/il2cpp-metadata-map.mjs` sau đó ghép đúng
  `global-metadata.dat` v39 với ELF 0.433.0 và tạo 583 focused rows. REA dossiers
  xác nhận các RVA `TryCapture`, `ReleasePokemon` và `RecycleItem` nằm trong
  analyzed code; snapshot hiện giữ 4 Evidence records.
- Blocker provider và token-to-RVA đã được giải quyết. T-303/T-304/T-307 vẫn
  fail-closed vì exact boundary/signature/ABI, owner/lifetime, call path,
  Promise/result và runtime postcondition chưa đủ authority.
