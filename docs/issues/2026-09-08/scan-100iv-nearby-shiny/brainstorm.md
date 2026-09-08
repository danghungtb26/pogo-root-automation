# Brainstorm: Scan 100 IV và NearbyScan shiny

**Type:** feature (có yếu tố architecture/runtime)
**Date:** 2026-09-08

---

## Analysis

### Bối cảnh và giả định hiện tại

Repo đã có nền tảng structured runtime phù hợp để xây feature này, nhưng chưa có scan session hay live binding đầy đủ:

- `NearbySnapshot`/`NearbySpawn` đang có `spawnId`, species, tọa độ, `firstSeenAtEpochMs`, expiry và `playerPosition`.
- `EncounterSnapshot` đã có `iv`, `shiny`, CP, tọa độ; `isHundo` chỉ đúng khi cả Attack/Defense/Stamina đều bằng `15`, còn `isShundo` là hundo + shiny.
- `AutomationCoordinator` hiện chỉ chọn một nearby spawn sắp hết hạn để tạo `OpenEncounter`; chưa có filter distance/time/species.
- `AutomationRunner` đã serialize mutation: mỗi lần chỉ có một action active, sau đó cần observation mới trước khi lập kế hoạch tiếp.
- `GameCapability` đã có `READ_NEARBY`, `ENCOUNTER`, `OPEN_ENCOUNTER`, nhưng chưa có capability riêng cho đọc đủ IV/shiny hoặc rời encounter.
- `PogoRuntimeSource` và mapper có chỗ chứa IV/shiny, nhưng `docs/M2_NEARBY_BINDING.md` xác nhận live extraction từ Pokémon GO vẫn còn thiếu và runtime hiện fail closed khi binding chưa được xác minh.

Hai thuật ngữ cần chốt rõ trong thiết kế:

1. **“Scan 100 IV”** trong phân tích này được hiểu là quét tối đa `100` candidate nearby rồi xác định candidate nào có IV đúng `15/15/15`. Con số `100` là giới hạn số candidate/probe trong một session, không phải cam kết game luôn trả về 100 Pokémon.
2. **“Shiny nearby”** được hiểu là mở encounter từng candidate để đọc `EncounterSnapshot.shiny`. Nearby list không được tự gắn `shiny = true` nếu runtime chưa cung cấp bằng chứng trực tiếp; `null` phải giữ là `UNKNOWN`.

### 1. Tính năng này giải quyết vấn đề gì?

#### F1 — Scan tối đa 100 candidate để tìm hundo

Người dùng muốn lọc danh sách Pokémon gần vị trí hiện tại theo khoảng cách, thời gian và loài, sau đó kiểm tra nhanh candidate nào có IV 100% mà không phải tự mở từng con. Giá trị chính là giảm thao tác thủ công và biến việc “tìm hundo trong vùng hiện tại” thành một session có tiến độ, kết quả và lý do rõ ràng.

Có hai cách hiểu kỹ thuật:

- **Passive scan:** runtime đã đọc được IV ngay từ nearby/spawn object. Đây là UX tốt nhất nhưng repo hiện không có field hay capability chứng minh điều này.
- **Active encounter probe:** lấy nearby candidate, mở encounter, đọc IV, ghi kết quả rồi rời encounter nếu không đạt. Đây là phương án khả thi hơn với model hiện tại, nhưng chậm hơn, tạo nhiều encounter và cần capability đóng/rời encounter.

Khuyến nghị coi active encounter probe là MVP; passive scan chỉ là tối ưu sau khi version-specific runtime chứng minh được dữ liệu IV trước encounter là thật và ổn định.

#### F2 — NearbyScan để xác định shiny

Người dùng muốn biết candidate nearby nào là shiny đủ sớm để có thể giữ encounter, cảnh báo hoặc trigger catch. Tính năng này không nên mô tả là “nhìn nearby là biết shiny” nếu source không có cờ shiny tin cậy. Luồng đúng là:

`nearby snapshot → lọc candidate → OpenEncounter → đọc shiny → alert/giữ encounter/catch theo policy`.

Nếu `shiny == false`, candidate không match; nếu `shiny == null`, kết quả là chưa xác định và không được coi là non-shiny. Điều này quan trọng để tránh false negative khi runtime chưa đọc xong state.

### 2. Ai được hưởng lợi?

Đối tượng là người vận hành app trên thiết bị/emulator root đã có runtime bridge và build fingerprint được xác minh. Đây là nhóm dùng automation có cấu trúc, không phải feature cho toàn bộ người dùng khi runtime chỉ ở trạng thái probe/read-only.

Hiện tại người dùng phải:

- tự nhìn nearby;
- tự chọn từng Pokémon;
- tự mở encounter để xem IV hoặc shiny;
- tự quay lại overworld và ghi nhớ candidate nào đã kiểm tra.

Scan session sẽ cung cấp danh sách candidate, tiến độ `n/100`, trạng thái `pending/probing/match/non-match/unknown/expired`, và kết quả cuối. Những người chỉ muốn chơi thủ công vẫn có thể dùng feature như read-only candidate list nếu không bật active probe.

### 3. Các use case cốt lõi

#### Must-have cho F1

- Tạo session với `maxCandidates = 100`.
- Lọc theo bán kính tính từ `NearbySnapshot.playerPosition` đến `NearbySpawn.position`.
- Lọc theo species ID; UI có thể cho chọn theo tên nhưng engine phải dùng ID ổn định.
- Lọc theo thời gian còn lại trước expiry. MVP nên hiểu `time` là `minimumRemainingSeconds`; candidate không đủ thời gian sẽ bị bỏ qua hoặc đánh dấu `too_late`.
- Xử lý candidate theo queue tuần tự, không gửi 100 lệnh cùng lúc.
- Mở encounter, chờ observation đúng `spawnId`/`encounterId`, rồi match khi IV là chính xác `15/15/15`.
- Hiển thị kết quả hundo và dừng/pause session theo policy khi đã tìm thấy, hoặc tiếp tục tìm tất cả candidate.

#### Must-have cho F2

- Dùng chung filter distance/time/species và queue với F1.
- Mở từng encounter để đọc `shiny`.
- Khi `shiny == true`, phát alert và cho phép giữ encounter để user/catch policy xử lý.
- Khi không shiny hoặc không đọc được, ghi trạng thái phân biệt; không ghi đè `UNKNOWN` thành `false`.
- Dedupe theo `spawnId` trong cùng session để không mở lại cùng candidate do nearby snapshot lặp.

#### Nice-to-have, chưa nên đưa vào MVP

- Quét passive IV/shiny trước encounter.
- Auto-teleport hoặc tự đi tới candidate. Đây là scope location automation riêng và làm tăng rủi ro/độ phức tạp.
- Đồng bộ kết quả lên server ngoài app.
- Tự động catch toàn bộ match mà không có lựa chọn `alert only`, `hold encounter` hoặc `auto-catch`.
- Ranking thông minh theo rarity, CP dự đoán hoặc giá trị sưu tầm.

### 4. Các edge case

#### Dữ liệu nearby và filter

- `playerPosition == null`: không tính được distance; candidate phải là `UNSCANNABLE`/bị loại, không giả định khoảng cách bằng 0.
- Tọa độ malformed: mapper hiện đã reject; scan planner cũng không nên nhận object ngoài domain mà chưa validate.
- `expiresAtEpochMs == null`: không biết thời gian còn lại. Mặc định loại khỏi active probe nếu filter time bật; cho phép option `includeUnknownExpiry` nếu user muốn, nhưng phải hiện cảnh báo.
- `expiresAtEpochMs <= observedAtEpochMs`: candidate hết hạn, không enqueue.
- `expiryConfidence == ESTIMATED`: vẫn dùng được để ưu tiên/lọc nhưng phải gắn marker estimated; không trình bày như deadline exact.
- Species filter rỗng: nghĩa là tất cả loài, không phải không có candidate.
- Species ID trùng hoặc danh sách rất lớn: normalize thành set, sort để persist ổn định.
- Khoảng cách ở đúng boundary: dùng quy ước rõ ràng `distanceMeters <= maxDistanceMeters`; test cả 0m và điểm đúng bán kính.
- “Time” có thể được hiểu là spawn age, minimum remaining time, khoảng giờ chạy session hoặc scan interval. Khuyến nghị tách thành các field riêng; không dùng một field mơ hồ.

#### Session và queue

- Nearby snapshot mới xuất hiện cùng `spawnId` nhưng đổi tọa độ/expiry: cập nhật candidate nếu chưa probe; không reset kết quả đã xác nhận nếu identity vẫn cùng spawn.
- Candidate biến mất giữa lúc chờ: đánh dấu `STALE_OR_EXPIRED`, không retry mù.
- Có hơn 100 candidate: dừng ở 100 theo ranking đã chốt; hiển thị `truncated` và tổng số bị bỏ qua.
- Có ít hơn 100 candidate: hoàn thành session với số thực tế, không coi là lỗi.
- User bấm Start/Pause/Stop liên tục: command/session phải idempotent theo `scanSessionId`; chỉ một active probe.
- Runtime disconnect hoặc build mất allowlist: dừng mutation, giữ kết quả read-only, hủy active probe ở trạng thái indeterminate và yêu cầu resync trước khi tiếp tục.
- Action `OpenEncounter` timeout/indeterminate: không mở lại ngay vì command có thể đã chạy; chờ observation mới hoặc user resume.
- User thao tác thủ công vào encounter trong lúc scan: scanner phải nhường quyền, đồng bộ lifecycle rồi mới tiếp tục; không gửi `OpenEncounter` chồng lên encounter hiện tại.

#### IV và shiny

- IV thiếu một trong ba thành phần: `UNKNOWN`, không phải non-hundo.
- IV chưa được runtime expose: `UNKNOWN` và kết thúc probe với reason rõ ràng.
- IV ngoài `0..15`: mapper reject, không match.
- `shiny == null`: `UNKNOWN`; không dùng `false` mặc định.
- Encounter trả về species/ID khác candidate: không dùng kết quả đó cho candidate cũ; đánh dấu identity mismatch và resync.
- Cùng candidate vừa match shiny vừa hundo: result là `SHUNDO`, alert một lần và giữ cùng một encounter/result record.
- Candidate hundo không shiny: chỉ match F1; candidate shiny không hundo: chỉ match F2.

#### Offline và lifecycle

- Không có runtime bridge: có thể hiển thị filter preview từ nearby cache nếu cache còn fresh, nhưng không cho active probe.
- App/service restart: kết quả đã lưu có thể giữ để xem lại, nhưng active scan không tự resume command dang dở.
- Game lifecycle không phải `OVERWORLD` khi bắt đầu probe: chờ/stop tùy policy; không gửi `OpenEncounter` khi đang ở encounter khác.
- User đã catch hoặc đóng encounter thủ công: observation mới là nguồn sự thật, candidate cũ không được catch/probe lại tự động nếu identity không còn hợp lệ.

### 5. Các ràng buộc

#### Kỹ thuật

- Structured-only boundary: scanner chỉ dùng `NearbySnapshot`/`EncounterSnapshot` và bridge; không dựa vào screenshot, OCR, pixel tap hoặc input driver.
- `AutomationRunner` hiện cố ý chỉ submit tối đa một mutation và yêu cầu fresh observation. Scan phải xây trên state machine này, không tạo một vòng lặp bypass runner.
- IV/shiny hiện nằm ở encounter model, không nằm ở `NearbySpawn`; vì vậy active probe cần `OPEN_ENCOUNTER`, `ENCOUNTER` và capability đọc dữ liệu tương ứng.
- Muốn trả về overworld sau candidate non-match cần capability/action mới như `LEAVE_ENCOUNTER` hoặc `CLOSE_ENCOUNTER`; không được giả sử `OpenEncounter` tự đóng.
- Runtime binding là version-specific. Mỗi build/ABI/translation layer phải được allowlist exact; không reuse mapping chỉ vì version name giống nhau.
- Bridge payload hiện có giới hạn cứng 4 MiB và nearby codec cho phép nhiều entry, nhưng giới hạn transport không chứng minh game source thực sự cung cấp 100 candidate. Session phải chịu được số lượng nhỏ hơn.
- Core phải giữ logic filter, ranking, match và state transition độc lập Android để test JVM; overlay/API chỉ điều khiển session và render status.

#### UX/design

- Cần phân biệt rõ `nearby detected`, `probing`, `hundo`, `shiny`, `shundo`, `unknown`, `expired`, `blocked`.
- Không hiển thị shiny/hundo như sự thật khi dữ liệu chưa được encounter xác nhận.
- UI phải có nút `Start`, `Pause`, `Stop`, progress `n/100`, active target, số match và lý do skip/error.
- User phải chọn hành vi khi match: `Alert only`, `Hold encounter`, hoặc `Auto-catch` theo policy hiện có. Mặc định nên là `Alert only`/hold để tránh mutation ngoài ý muốn.
- Filter distance/time/species là cấu hình của scan session; không nên âm thầm sửa global `autoEncounter`.

#### Tuân thủ và vận hành

Automation, active encounter probing và location manipulation có thể vi phạm điều khoản hoặc tạo rủi ro tài khoản của game. Repo hiện đã chủ động không triển khai anti-detection, root hiding hay Play Integrity bypass; feature mới phải giữ nguyên nguyên tắc này, có fail-closed khi runtime không được xác minh và không hứa “an toàn tài khoản”.

### 6. Các phương án đã cân nhắc

#### Phương án A — Passive scan từ nearby/spawn metadata

Runtime trả IV/shiny ngay trong `RawNearbySpawn`, scanner chỉ filter và hiển thị.

- Ưu điểm: nhanh, ít encounter, UX tốt, dễ scan đủ 100 candidate.
- Nhược điểm: repo chưa có dữ liệu/capability này; phải reverse-engineer đúng build và chứng minh field không phải giá trị dự đoán/sai lệch. Nếu runtime chỉ biết shiny sau encounter thì phương án này không khả thi.
- Kết luận: giữ làm future optimization, không dùng làm MVP assumption.

#### Phương án B — Active encounter probe tuần tự

Nearby chỉ dùng để tìm candidate; mỗi candidate được mở encounter, đọc IV/shiny, rồi giữ hoặc rời encounter.

- Ưu điểm: khớp model hiện tại (`EncounterSnapshot` đã có IV/shiny), kết quả có identity và timestamp rõ ràng, tận dụng `AutomationRunner`.
- Nhược điểm: chậm, có thể phát sinh nhiều mutation/encounter, cần close/leave capability, phải xử lý timeout và race lifecycle.
- Kết luận: khuyến nghị cho MVP, với giới hạn session, pause/stop và single-flight.

#### Phương án C — External scanner/API hoặc nguồn dữ liệu bên ngoài

Lấy IV/shiny từ dịch vụ khác rồi hiển thị candidate.

- Ưu điểm: có thể có nhiều dữ liệu hơn mà không cần đọc client ngay.
- Nhược điểm: stale/mismatch vị trí, thêm network/account/privacy, không khớp structured-only runtime, không có guarantee candidate thuộc đúng account/session; có thể mở rộng scope sang bot/server ngoài repo.
- Kết luận: không chọn cho repo này.

### 7. Tương tác với feature hiện có

#### Với `autoEncounter` và `autoCatch`

Đây là xung đột quan trọng nhất. Nếu scanner đang probe mà `autoEncounter` vẫn chạy, `AutomationCoordinator` có thể chọn spawn khác; nếu `autoCatch` vẫn chạy, encounter scanner đang dùng có thể bị catch trước khi đọc đủ state.

Khuyến nghị:

- `ScanSession` có ownership/mode độc quyền đối với encounter queue.
- Khi scanner `RUNNING`, tạm suspend `autoEncounter` và policy catch thường, hoặc đưa tất cả vào một coordinator chung có priority rõ ràng.
- Khi session `PAUSED/STOPPED/COMPLETED`, restore policy cũ.
- Không submit song song một `OpenEncounter`, `Catch` hoặc `LeaveEncounter`.

#### Với nearby reducer và snapshot

`NearbySnapshotReducer` đã làm dedupe/expiry/add-update-remove. Scanner nên nhận diff hoặc snapshot normalized từ reducer, dùng `spawnId` làm key, và invalidate candidate khi expired/removed. Không tạo cache duplicate với rule expiry khác.

#### Với encounter/catch hiện có

`CatchPlanner` đã ưu tiên shiny, hundo và shundo. Scan result nên tái sử dụng `CatchReason.SHINY`, `CatchReason.HUNDO` hoặc `CatchReason.SHUNDO` thay vì thêm logic catch thứ hai. Tuy nhiên scanner không được tự gọi catch nếu user chọn alert-only.

#### Với overlay và control API

`/v1/status` hiện chỉ có runtime/policy status, chưa có danh sách scan. Nên thêm scan status riêng, ví dụ `GET /v1/scan/status`, và command start/stop/pause riêng thay vì nhồi toàn bộ session state vào `HeadlessAutomationConfig`. Overlay có thể dùng cùng repository/status bridge để hiển thị progress.

### 8. Dependencies và boundary cần bổ sung

#### Core/domain

- `ScanCriteria`: `maxCandidates`, `maxDistanceMeters`, `minimumRemainingSeconds`, `speciesIds`, `includeUnknownExpiry`, `mode` (`HUNDO`, `SHINY`, có thể `BOTH` sau).
- `NearbyCandidate`: spawn + distance + remaining + eligibility/reason.
- `ScanSession`/`ScanSessionStatus`: `sessionId`, lifecycle, queue, active candidate, counts, results, last error.
- `ScanPlanner`: filter, sort, cap 100, dedupe.
- `ScanProbeStateMachine`: transition từ nearby candidate sang open encounter, encounter observation, result, leave/resync.
- `ScanMatch`: `HUNDO`, `SHINY`, `SHUNDO`, `UNKNOWN`, `NON_MATCH`.

Không nên đưa `ScanSession` vào `AutomationPolicy` nếu session cần pause/stop, progress và kết quả chi tiết; policy là cấu hình dài hạn còn session là state ngắn hạn.

#### Adapter/runtime

Tối thiểu cần capability/contract tương ứng:

- `READ_NEARBY` — đã có.
- `OPEN_ENCOUNTER` — đã có.
- `ENCOUNTER` — đã có về mặt tên, nhưng runtime phải thực sự map IV/shiny.
- `READ_ENCOUNTER_IV` và `READ_ENCOUNTER_SHINY` — khuyến nghị thêm để không coi field nullable là dữ liệu đã sẵn sàng.
- `LEAVE_ENCOUNTER`/`CLOSE_ENCOUNTER` — cần cho candidate non-match nếu scanner tự động tiếp tục.
- Nếu sau này passive scan chứng minh được, thêm riêng `READ_NEARBY_IV`/`READ_NEARBY_SHINY`, không lẫn với encounter capability.

#### Bridge và app control

- Action mới cho leave/close cần được encode/decode, kiểm tra capability, lifecycle và identity.
- Scan status/event cần gắn `runtimeSessionId`, `scanSessionId`, `spawnId`, `encounterId`, `commandId`, `messageSeq` để chống event cũ hoặc duplicate.
- `AutomationControlServer` có thể thêm `/v1/scan/start`, `/v1/scan/pause`, `/v1/scan/stop`, `/v1/scan/status`; vẫn chỉ bind `127.0.0.1`.
- Session config có thể persist trong `SharedPreferences` dưới dạng species ID CSV và scalar filters; active queue/results nên ở repository có lifecycle rõ ràng, không tự resume mutation sau process restart.

#### Runtime binding

Đây là dependency chặn việc chạy thật. Cần chứng minh trên exact build:

1. nearby spawn có identity/position/expiry ổn định;
2. `OpenEncounter(spawnId)` mở đúng candidate;
3. encounter observation trả đủ IV và/hoặc shiny;
4. có cách nhận biết kết thúc/đóng encounter;
5. binding không nhầm giữa native ARM và môi trường translated x86_64.

Nếu một trong các điều kiện chưa đạt, UI chỉ nên báo `runtime capability unavailable` và giữ read-only, không fallback sang screenshot/input.

### 9. Rủi ro và cách giảm thiểu

1. **Nhầm passive metadata với dữ liệu thật.** IV/shiny bị hiển thị sai sẽ nguy hiểm hơn việc báo unknown. Giảm thiểu bằng capability rõ ràng, mapping fail closed và acceptance yêu cầu test raw-to-domain trên build thật.
2. **Race/duplicate encounter.** Nearby update, user action và command result có thể đến khác thứ tự. Giảm thiểu bằng single-flight, `sessionId`/`spawnId`/`encounterId`/`commandId`, sequence validation và không retry command indeterminate.
3. **Session quét quá lớn hoặc quá chậm.** 100 probe không đồng nghĩa 100 kết quả trong vài giây. Giảm thiểu bằng `maxCandidates`, timeout, pause/stop, progress, expiry pre-check và không queue bulk mutation.
4. **Xung đột với auto automation.** Scanner có thể bị `autoEncounter` hoặc `autoCatch` giành encounter. Giảm thiểu bằng ownership/mutex và suspend policy có thông báo.
5. **False negative vì field nullable.** `iv == null` hoặc `shiny == null` không phải non-match. Giảm thiểu bằng trạng thái `UNKNOWN` và metric riêng.
6. **Runtime drift sau game update.** Binding đúng version cũ có thể sai version mới. Giảm thiểu bằng exact fingerprint allowlist, capability gating, device smoke và không dùng heuristic version name.
7. **Rủi ro ToS/tài khoản.** Active probing và automation có thể bị game coi là hành vi không hợp lệ. Repo cần giữ cảnh báo vận hành, không bổ sung anti-detection/bypass và không tự bật feature trên runtime chưa được xác minh.

### 10. Cách kiểm thử và đo lường

#### Unit tests

- `ScanPlanner`: distance boundary, unknown player position, species set, remaining time, estimated/unknown expiry, cap 100, deterministic ordering.
- `ScanMatcher`: exact 15/15/15, partial IV, null shiny, shiny+hundo thành shundo, invalid IV.
- State machine: `PENDING → PROBING → MATCH/NON_MATCH/UNKNOWN/EXPIRED/ERROR` và idempotency Start/Pause/Stop.
- Dedupe: cùng `spawnId` không probe hai lần trong session; command khác session không được nhận.

#### Integration/bridge tests

- Encode/decode action close/leave và scan-related status/event.
- Open encounter chỉ được submit khi lifecycle là overworld và capability đầy đủ.
- Không gửi command thứ hai khi command trước còn active.
- Result/observation cũ, sai runtime identity, sai encounter ID hoặc sai message sequence đều bị bỏ qua/fail closed.

#### Device smoke

- Test trên từng exact build fingerprint/ABI/translation layer được allowlist.
- Nearby có candidate thật; mở đúng candidate; đọc IV/shiny; đóng/rời encounter; resync về overworld.
- Test expired candidate, runtime disconnect, manual close, timeout và game update unsupported.

#### Metrics nên có

`candidates_seen`, `candidates_eligible`, `probes_started`, `hundos_found`, `shinies_found`, `shundos_found`, `unknown_results`, `expired_skips`, `timeouts`, `identity_mismatches`, `session_duration_ms`.

## Acceptance Criteria (from spec)

> Source: không tìm thấy `docs/newspec/**` hoặc `docs/specs/**` cho feature này; yêu cầu từ ticket/user và các contract hiện có trong repo được chuyển thành tiêu chí **inferred — needs BA confirm**.

| ID | Rule / Requirement | Formula / Expected | Acceptance note |
|----|--------------------|--------------------|-----------------|
| SC-1 | Định nghĩa hundo | Match iff `iv.attack == 15 && iv.defense == 15 && iv.stamina == 15` | Requirement F1 “scan 100iv”; `null`/partial IV là `UNKNOWN`, không match. |
| SC-2 | Filter khoảng cách | `distanceMeters(playerPosition, spawn.position) <= maxDistanceMeters` | Dùng `GeoMath.distanceMeters`; thiếu player position thì không active-probe. |
| SC-3 | Filter thời gian | `remainingMs = expiresAtEpochMs - observedAtEpochMs`; eligible khi `remainingMs >= minimumRemainingMs` | MVP hiểu “time” là thời gian còn lại; unknown expiry chỉ được đưa vào khi user bật option rõ ràng. |
| SC-4 | Filter loài | `speciesId ∈ configuredSpeciesIds`, species set rỗng nghĩa là all | Persist/filter bằng ID ổn định, không match theo display name. |
| SC-5 | Giới hạn 100 | `eligibleCandidates.take(maxCandidates)` với `maxCandidates <= 100` | UI hiển thị `processed/selected/total`; không hứa source luôn có 100 candidate. |
| SC-6 | Dedupe và expiry | Mỗi `spawnId` tối đa một probe trong session; expiry/removal trước probe → `EXPIRED`/`STALE` | Không mở lại candidate cũ do snapshot lặp. |
| SC-7 | Single-flight probe | Tại một thời điểm tối đa một `OpenEncounter`/`LeaveEncounter` active | Sau mỗi command cần result/observation hợp lệ; không bulk queue 100 mutation. |
| SC-8 | Match shiny | Match iff `encounter.shiny == true`; `null` → `UNKNOWN` | Requirement F2 “xem con nào shiny”; không suy diễn shiny từ nearby nếu chưa có runtime evidence. |
| SC-9 | Kết quả shundo | `isHundo && shiny == true` → `SHUNDO` | Chỉ alert một lần cho cùng `spawnId`/`encounterId`; có thể giữ encounter theo policy. |
| SC-10 | Hành vi khi match | `Alert only` không mutation; `Hold encounter` không tự leave; `Auto-catch` chỉ qua `CatchPlanner`/capability/allowlist | Mặc định an toàn là alert/hold; user phải opt-in auto-catch. |
| SC-11 | Non-match và tiếp tục queue | Hundo không đạt hoặc shiny `false` → kết quả non-match; scanner leave/close rồi mới probe candidate kế | Cần capability `LEAVE_ENCOUNTER`/`CLOSE_ENCOUNTER`; không giả lập bằng delay/screenshot. |
| SC-12 | Unknown/error | Partial IV, `shiny == null`, binding thiếu field, timeout hoặc identity mismatch → `UNKNOWN`/`ERROR` | Không biến unknown thành false và không retry command indeterminate ngay lập tức. |
| SC-13 | Tương thích automation hiện có | Scanner active phải suspend/coordinate `autoEncounter` và `autoCatch` | Không có hai owner cùng điều khiển encounter. |
| SC-14 | Runtime safety | Mutation chỉ khi `strongIdentityVerified`, exact build fingerprint allowlisted và capability đủ | Runtime chưa ready/binding mất → read-only/fail closed. |
| SC-15 | Persistence/lifecycle | Restart không tự resume command dang dở; Stop/Pause idempotent; session có progress và error reason | Kết quả có thể giữ để xem, nhưng command active phải resync. |
| SC-16 | Verification | JVM unit, bridge protocol, Android compile và device smoke pass trên build allowlist | Bao phủ filter, matcher, state race, timeout, unsupported runtime và manual interruption. |

- Requirement “scan được 100 IV, option khoảng cách/time/loài” → **SC-1 đến SC-7** và **SC-15**; done khi session lọc đúng, giới hạn tối đa 100 và không quét trùng/stale.
- Requirement “nearby scan xem con nào shiny vì ở gần nên trigger encounter” → **SC-7 đến SC-12**; done khi chỉ encounter evidence xác nhận shiny, match được giữ/cảnh báo đúng policy và candidate khác được xử lý tuần tự.
- Requirement không phá structured architecture/runtime safety → **SC-13 đến SC-16**.

## Synthesis

### Key Insight

Hai tính năng không nên được xây như hai vòng lặp độc lập. Chúng là cùng một bài toán `nearby candidate discovery + encounter probe`, chỉ khác predicate kết quả: F1 cần IV `15/15/15`, F2 cần `shiny == true`. Nearby state hiện đủ để lọc candidate nhưng chưa đủ để kết luận IV/shiny; vì vậy MVP phải có session queue tuần tự, encounter identity, trạng thái `UNKNOWN`, và capability leave/close.

### Recommended Approach

Xây một `ScanSession` dùng chung cho HundoScan và ShinyScan, bắt đầu bằng filter distance/species/minimum remaining time và cap 100. Dùng `AutomationRunner` làm single-flight executor: `NearbySnapshot → OpenEncounter → EncounterSnapshot → match → alert/hold/catch hoặc leave → observation mới`. Trước khi triển khai UI đầy đủ, cần hoàn tất/kiểm chứng exact-build runtime binding cho IV/shiny và action leave; nếu chưa có thì chỉ expose read-only nearby/filter preview, không giả lập kết quả.

Tách cấu hình session khỏi `AutomationPolicy`, thêm status API/overlay có progress và reason, đồng thời pause hoặc cấp ownership độc quyền cho `autoEncounter`/`autoCatch` khi scan đang chạy. Passive IV/shiny metadata chỉ nên là phase tối ưu sau khi có bằng chứng runtime, không phải dependency của MVP.

### Risks to Watch

- Runtime có thể không đọc được IV/shiny đúng thời điểm hoặc sau game update; phải fail closed và phân biệt unknown với false.
- Probe tuần tự 100 candidate có thể chậm, hết expiry hoặc va chạm với thao tác thủ công/auto-catch; cần single-flight, timeout, pause/stop và dedupe.
- Active automation có thể vi phạm ToS hoặc tạo rủi ro tài khoản; không mở rộng sang anti-detection, bypass hoặc nguồn scanner bên ngoài.

### Open Questions

- `time` chính xác là `minimumRemainingSeconds`, khoảng giờ chạy session, spawn age hay interval polling? Khuyến nghị chốt `minimumRemainingSeconds` cho filter và đặt `sessionTimeout`/`pollInterval` thành field riêng.
- “100 IV” là quét tối đa 100 candidate trong một session hay cần lấy 100 kết quả hundo? Khuyến nghị hiểu là tối đa 100 candidate/probe để có bound rõ ràng.
- Khi gặp shiny/hundo, mặc định `alert + giữ encounter`, hay tự catch? Cần BA/user chốt vì đây là mutation khác nhau.
- Runtime build mục tiêu có thật sự expose IV/shiny trước/sau encounter và có method leave/close nào? Cần fixture/device smoke trên exact fingerprint; code hiện tại chưa chứng minh được.
- Có cho phép scan candidate không có expiry không? Khuyến nghị mặc định loại khi có time filter, chỉ include khi user opt-in và UI hiển thị `expiry unknown`.
- Kết quả scan có cần lưu bền qua process restart không, hay chỉ giữ trong session? Khuyến nghị không auto-resume active probe; chỉ lưu history/read-only summary nếu cần.

## Section 11 — UX overlay: hai list icon dọc và màn hình xem tất cả

### Yêu cầu UX mới

Overlay sẽ có hai widget nhỏ độc lập:

1. **Hundo/100 IV list:** hiển thị các candidate đã xác nhận IV `15/15/15`.
2. **Shiny list:** hiển thị các candidate đã xác nhận `shiny == true`.

Mỗi widget xếp dọc, hiển thị tối đa `6` Pokémon bằng sprite/icon; không hiển thị tên, IV, CP hay text dài trên overlay. Shundo nên xuất hiện ở cả hai list để không bị mất khỏi một trong hai ngữ cảnh. Identity của item vẫn là `spawnId`/`encounterId`, nên hai icon giống nhau vẫn có thể là hai candidate khác nhau; không dedupe theo species chỉ vì UI đang icon-only.

Widget được tap để mở màn hình `ScanResultsFragment` xem toàn bộ kết quả. Hành vi tap và drag phải tách bằng touch-slop: tap mở fragment, kéo vượt ngưỡng mới di chuyển widget. Hai widget có vị trí độc lập, có thể kéo-thả trên màn hình và lưu vị trí sau restart.

### Layout và interaction được khuyến nghị

- Dùng hai overlay window nhỏ độc lập, tương tự cooldown overlay hiện tại, thay vì đặt cả hai vào `rootView` của joystick. Cách này cho phép di chuyển từng list mà không kéo theo nút menu/joystick.
- Tái sử dụng `OverlayDragHandler`; handler hiện đã phân biệt `ACTION_UP` không di chuyển với drag thật và gọi `performClick()` cho tap.
- Mỗi widget có kích thước cố định theo density, một cột tối đa 6 icon, khoảng cách nhỏ và nền bo góc trong suốt. Icon chỉ cần đủ nhận diện; không cần label từng dòng.
- Có marker rất nhỏ để phân biệt hai widget, ví dụ biểu tượng hundo/sparkle hoặc vị trí mặc định khác nhau. Marker không chiếm chỗ của tên Pokémon.
- Khi list rỗng, ẩn widget hoặc giữ một placeholder rất nhỏ tùy chốt UX; khuyến nghị ẩn để không che game. Khi có kết quả đầu tiên, widget xuất hiện.
- Thứ tự overlay là match mới nhất trước; full fragment giữ toàn bộ result rows và cho sort/filter chi tiết.
- Tap vào bất kỳ vùng nào của widget mở fragment ở tab tương ứng (`Hundo` hoặc `Shiny`), không bắt user phải tap chính xác từng icon.
- Full fragment hiển thị tên, icon, IV/shiny, thời điểm, khoảng cách, expiry, `spawnId`/trạng thái và action hợp lệ; phần này mới dùng text để giải quyết việc nhận diện chi tiết.

### Icon source và fallback

Repo hiện chưa có `PokemonIconResolver` hay bộ drawable sprite Pokémon. Cần thêm boundary `speciesId → icon` ở app/UI; core chỉ trả `speciesId` và result metadata, không phụ thuộc Android drawable.

Thứ tự resolve nên là:

1. asset/drawable local đã được version hóa;
2. cache icon đã tải hoặc đã bundled nếu dự án cho phép;
3. placeholder silhouette cố định khi species chưa có asset.

Không render `#speciesId` trên widget icon-only. Full fragment có thể dùng tên species hoặc `#speciesId` làm fallback để không làm mất khả năng debug.

### State cần publish cho overlay

Scanner/headless cần publish một summary nhỏ, không để overlay đọc trực tiếp `PogoRuntimeSource`:

- `hundoResults: List<ScanResultSummary>`
- `shinyResults: List<ScanResultSummary>`
- `lastUpdatedAtEpochMs`
- `sessionId`/trạng thái runtime để clear stale state
- `speciesId`, `spawnId`, `encounterId`, match type, matched time và optional distance/expiry

Widget chỉ lấy `take(6)` sau khi sort theo `matchedAtEpochMs` giảm dần. Repository vẫn lưu đủ result rows trong session để fragment xem all. Khi session mới bắt đầu hoặc runtime session đổi, phải clear/đánh dấu stale kết quả; không giữ icon cũ như match live của runtime mới.

`ScanResultRepository` là boundary phù hợp giữa headless service và overlay. Nếu cần giữ qua process restart, chỉ persist summary/result rows; không tự resume active probe hoặc command đang dở. `SharedPreferences` có thể đủ cho tối đa 100 result rows nếu payload giữ nhỏ, nhưng nên bọc sau repository để sau này chuyển sang file/database không ảnh hưởng UI.

### Tương tác với fragment hiện có

Nên tạo một `ScanResultsActivity` làm host và mở `ScanResultsFragment` theo tab/section được chọn. Không nên mở fragment trực tiếp từ `Service`; service chỉ start Activity bằng `Intent` với `resultType = HUNDO` hoặc `SHINY`.

Fragment cần có:

- hai section/tab Hundo và Shiny;
- danh sách đầy đủ, không giới hạn 6;
- trạng thái empty/loading/stale/runtime unavailable;
- tap một row để xem chi tiết hoặc quay lại game;
- không tự trigger catch chỉ vì user mở danh sách.

### Acceptance criteria bổ sung cho UX

| ID | Rule / Requirement | Formula / Expected | Acceptance note |
|----|--------------------|--------------------|-----------------|
| UX-1 | Hai list riêng | Có đúng một widget Hundo và một widget Shiny trên overlay | Không trộn match của hai predicate vào một list. |
| UX-2 | Giới hạn icon | Mỗi widget render tối đa `6` item theo `matchedAtEpochMs DESC` | Full fragment vẫn xem được toàn bộ result rows. |
| UX-3 | Icon-only | Mỗi item trên overlay chỉ render sprite/icon Pokémon | Không render tên/IV/CP/text dài; có placeholder nếu thiếu asset. |
| UX-4 | Phân loại match | Hundo list chỉ nhận `HUNDO`/`SHUNDO`; Shiny list chỉ nhận `SHINY`/`SHUNDO` | Một `SHUNDO` được phép xuất hiện ở cả hai list. |
| UX-5 | Tap mở all | Tap widget Hundo/Shiny mở `ScanResultsFragment` ở section tương ứng | Tap không trigger catch hoặc scan mới. |
| UX-6 | Drag độc lập | Kéo widget vượt `touchSlop` sẽ di chuyển riêng widget đó | Không kéo theo joystick, menu hoặc widget còn lại. |
| UX-7 | Tap-vs-drag | Tap không phát sinh movement; drag không mở fragment ngoài ý muốn | Tái sử dụng behavior của `OverlayDragHandler`. |
| UX-8 | Persist vị trí | Vị trí Hundo và Shiny được lưu bằng key riêng và restore sau service restart | Clamp theo kích thước màn hình/orientation. |
| UX-9 | Stale state | Runtime/session đổi hoặc scan reset → icon cũ bị clear/đánh dấu stale | Không hiển thị result của session cũ như dữ liệu live. |
| UX-10 | Full list fidelity | Fragment xem được tất cả result rows, tên/icon, IV/shiny, time, distance và status | Không bị giới hạn bởi `take(6)` của overlay. |

### Điều chỉnh synthesis

Về UX, overlay không phải nơi hiển thị toàn bộ scan state; nó là “quick glance” chỉ để nhận diện nhanh bằng icon. Hai widget nên là hai window draggable độc lập, còn `ScanResultsFragment` là nơi chứa danh sách đầy đủ và thông tin chi tiết. Điều này giữ overlay gọn, tận dụng được `OverlayDragHandler`/`OverlayPositionStore` hiện có và không làm scanner phụ thuộc vào UI rendering.
