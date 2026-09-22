# Bridge contract: desired state và native UI status

**Contract revision:** 1.0. **Bridge protocol:** 3. **Ngày chốt:** 2026-09-22.

Đây là schema cho T-004/T-005. Hai phía phải cập nhật cùng một contract; không
được coi một phía đã tương thích nếu phía còn lại chỉ hiểu protocol 2. Protocol
3 là một wire boundary mới: peer không khớp version phải đóng kết nối/fail
closed, không fallback sang coordinator Kotlin hoặc ba CONFIG_SET cũ.

## 1. Framing và giá trị đang giữ

Mọi frame dùng thứ tự byte big-endian:

    u32 payload_length
    u16 protocol_version = 3
    u16 message_type
    u64 frame_message_seq
    payload[payload_length]

Giới hạn giữ nguyên từ bridge hiện tại:

| Giá trị | Giới hạn/ý nghĩa |
|---|---|
| NORMAL_MESSAGE_BYTES | 1 MiB, ngưỡng khuyến nghị cho payload thông thường |
| HARD_MESSAGE_BYTES | 4 MiB, hard limit cho frame/payload; vượt quá thì reject/đóng channel |
| string | u32 byte length + UTF-8, tối đa 64 KiB theo BridgePayloadCodecSupport/native command reader |
| message_seq | u64, khác 0 cho message có correlation; monotonic trong một runtime session |
| bool | đúng một byte 0 hoặc 1; giá trị khác fail closed |
| integer | u32/u64 big-endian; không dùng signed wire integer cho ID/count |
| f64 | IEEE-754 binary64 big-endian; reject NaN/±infinity và giới hạn domain |

Wire message IDs không đổi: COMMAND=4, COMMAND_RESULT=5,
RUNTIME_STATUS=10. Desired state dùng COMMAND với marker riêng; status
dùng RUNTIME_STATUS. Không thêm ID hoặc tái sử dụng ID cũ cho task này.
Các message RUNTIME_READY, OBSERVATION, BINDING_LOST, ERROR vẫn dùng
schema hiện tại nhưng chạy dưới protocol 3 và tiếp tục giữ session/identity/
sequence/freshness guards.

### Common payload prefix

Payload typed mới bắt đầu bằng:

    u32 payload_version = 1

Sau prefix, decoder phải nhận đúng marker/schema của typed payload và phải
đọc hết payload (no trailing bytes). Marker đã chọn cho contract mới:

| Payload | Marker hex | ASCII | Schema |
|---|---:|---|---:|
| desired state request | 0x52445354 | RDST | 1 |
| UI status snapshot | 0x52555354 | RUST | 1 |

Các marker này khác RTCT, CSCF, TRCF, DSCF, RTMD hiện có. Parser
legacy phải không nhận nhầm marker mới; parser mới dispatch desired-state
trước gameplay command parser.

## 2. RuntimeDesiredStateRequest

Đây là **full snapshot** của các setting native hiện có. Không chứa Pokémon,
fort, queue mutation, pointer game, readiness hoặc action command. Tất cả field
state phải cùng một config_revision.

### Field order

Sau common prefix, field được ghi đúng thứ tự sau:

| # | Field | Wire type/unit | Rule |
|---:|---|---|---|
| 1 | runtime_session_id | string | session do native tạo; không rỗng; logic tối đa 256 byte |
| 2 | request_id | string | correlation do client tạo; không rỗng; tối đa 256 byte |
| 3 | marker | u32 | RDST |
| 4 | schema_version | u32 | 1 |
| 5 | config_revision | u64 | khác 0; sequence modular được mô tả bên dưới |
| 6 | enabled | bool | master desired state; false không bật gameplay module |
| 7 | catch_spin_armed | bool | user arm gate của catch/spin/fort navigation |
| 8 | auto_catch | bool | native catch policy |
| 9 | auto_spin | bool | native spin policy |
| 10 | auto_encounter | bool | native encounter policy |
| 11 | catch_all | bool | native direct-map catch filter hiện hỗ trợ |
| 12 | auto_walk_to_fort | bool | native chọn fort; Kotlin chỉ thực hiện navigation |
| 13 | spin_settle_delay_ms | u64, milliseconds | 0..60000 |
| 14 | catch_settle_delay_ms | u64, milliseconds | 0..60000 |
| 15 | auto_discard | bool | native discard module |
| 16 | discard_limit_count | u32 | 0..64 |
| 17 | discard_limits | repeated (u32 item_id, u32 max_count) | item_id > 0, max_count >= 0, ID tăng dần, không trùng |
| 18 | auto_transfer | bool | native transfer module |
| 19 | minimum_iv_percent_to_keep | f64, percent | finite, 0.0..100.0 |
| 20 | keep_unknown_iv | bool | hiện default native-compatible true |
| 21 | keep_shiny | bool | transfer keep policy |
| 22 | keep_hundo | bool | transfer keep policy |
| 23 | keep_special_background | bool | transfer keep policy |
| 24 | keep_favorite | bool | transfer keep policy |
| 25 | keep_legendary | bool | hiện default native-compatible true |
| 26 | keep_mythical | bool | hiện default native-compatible true |
| 27 | expires_at_elapsed_ns | u64, monotonic nanoseconds | phải lớn hơn now; tối đa now + 30s |
| 28 | pid | u32 | target process identity; > 0 |
| 29 | process_name | string | không rỗng; logic tối đa 256 byte |
| 30 | package_name | string | không rỗng; logic tối đa 256 byte |
| 31 | build_fingerprint | string | không rỗng; logic tối đa 4 KiB; phải khớp native ready identity |

mapTapWalkEnabled, favorite/joystick/location target, loopIntervalMs,
showActionToasts, throw quality/curve/encounter snapshot/berry settings và
structured allowlist không nằm trong native desired snapshot ở revision này.
Chúng lần lượt là app/location/UI-owned hoặc persist-only/unsupported theo
inventory; không được tự thêm field và gán semantics mới.

### Revision, duplicate và validation

- Native chỉ nhận request thuộc đúng session, process, package và fingerprint
  của runtime hiện tại, đồng thời kiểm tra expiry trước khi lưu.
- config_revision=0 invalid. Revision so sánh theo số thứ tự modular u64:
  a mới hơn b khi a != b và (a - b) mod 2^64 nằm trong 1..2^63-1.
  Vì vậy bước MAX_UINT64 → 1 hợp lệ khi là bước tăng tuần tự; khoảng cách
  mơ hồ hoặc nhảy ngược bị reject. Kotlin preference hiện wrap từ
  Long.MAX_VALUE về 1; implementation phải serialize/so sánh nhất quán và
  không dùng số âm như wire revision.
- Cùng revision + cùng toàn bộ snapshot là duplicate idempotent; trả receipt
  nhận request, không apply lần hai. Cùng revision + khác bất kỳ field nào bị
  reject revision_conflict.
- Revision cũ hơn bị reject stale_revision; expiry, identity, size, enum,
  count, finite-number hoặc trailing bytes sai đều reject fail closed.
- Native giữ latest complete snapshot trong RAM theo session. Nhận trước
  managed-ready chỉ là received, không gọi IL2CPP, không bật module và không
  được báo ready.
- Full snapshot validate hết trước khi publish dưới một revision gate. Observer
  chỉ đọc snapshot immutable/đã commit; không đọc ba config đang được ghi lần
  lượt.
- Explicit STOP thắng mọi apply đang chờ: native tăng lifecycle generation,
  disable module và bỏ pending apply/result cũ. Một desired snapshot revision
  mới hơn sau STOP có thể được nhận lại; callback revision cũ không được
  re-enable module.

## 3. RuntimeUiStatus

Status là snapshot do native phát trên RUNTIME_STATUS=10; app không suy
readiness từ nearby/encounter/connection hoặc từ desired receipt. Status transport
được xử lý bounded/latest-only khi reconnect; không replay gameplay command hay
action outcome cũ.

### Field order

Sau common prefix, field được ghi đúng thứ tự sau:

| # | Field | Wire type/unit | Rule |
|---:|---|---|---|
| 1 | marker | u32 | RUST |
| 2 | schema_version | u32 | 1 |
| 3 | runtime_session_id | string | native-owned session; không rỗng |
| 4 | pid | u32 | target PID; > 0 |
| 5 | process_name | string | non-empty |
| 6 | package_name | string | non-empty |
| 7 | build_fingerprint | string | non-empty nếu identity đã biết; unverified|... chỉ biểu thị probe |
| 8 | native_lifecycle | u32 enum | UNKNOWN=0, ATTACHED_IDLE=1, STARTING=2, DIAGNOSTIC_PENDING=3, BINDING_READY=4, APPLYING=5, READY=6, STOPPING=7, ERROR=8 |
| 9 | strong_identity_verified | bool | chỉ true sau exact package/build/ABI/binding guards |
| 10 | capability_count | u32 | 0..256 |
| 11 | capabilities | repeated string | sorted, unique; unknown capability không bật feature |
| 12 | desired_revision_present | bool | presence bit |
| 13 | desired_revision | u64 | chỉ có khi bit trước true; revision native đã nhận |
| 14 | applied_revision_present | bool | presence bit |
| 15 | applied_revision | u64 | chỉ có khi bit trước true; full snapshot đã commit/apply |
| 16 | ready | bool | true chỉ khi identity/capability/map/config/module guards đạt |
| 17 | module_count | u32 | 0..8 |
| 18 | modules | repeated module record | field order bên dưới |
| 19 | error_code_present | bool | presence bit |
| 20 | error_code | string | chỉ có khi present; tối đa 256 byte |
| 21 | error_message_present | bool | presence bit |
| 22 | error_message | string | chỉ có khi present; tối đa 4 KiB |
| 23 | observed_at_epoch_ms | u64, Unix milliseconds | diagnostics/display only |
| 24 | observed_at_elapsed_ns | u64, monotonic nanoseconds | freshness/order evidence |

Mỗi module record:

    u32 module_wire
    string module_name
    u32 module_state       // UNKNOWN=0, REGISTERED=1, CONFIGURED=2,
                           // READY=3, DISABLED=4, ERROR=5
    bool revision_present
    u64 revision           // present iff revision_present
    bool error_code_present
    string error_code      // present iff error_code_present

desired_revision là latest snapshot native đã nhận/validate; applied_revision
là snapshot đã commit cho module owner. ready=false hoặc thiếu/unknown module
state không được app biến thành action success. registered, configured và
ready phải hiển thị khác nhau.

## 4. Receipt, status và outcome semantics

| Tín hiệu | Ý nghĩa | Không được suy ra |
|---|---|---|
| COMMAND_RESULT phase ACCEPTED, command_id=request_id, message desired_state_received | Native đã auth/validate và lưu full desired snapshot trong RAM | Không chứng minh apply, module ready, map ready hay gameplay action |
| RuntimeUiStatus.desired_revision | Snapshot đã được native nhận | Không chứng minh đã publish vào module |
| RuntimeUiStatus.applied_revision | Full snapshot cùng revision đã commit dưới revision gate | Không chứng minh catch/spin/discard/transfer đã xảy ra |
| RuntimeUiStatus.ready + module READY | Native guards/config/module readiness đạt | Không phải action outcome hoặc server acknowledgement |
| AUTOMATION_EVENT / COMMAND_RESULT của gameplay | Outcome/event do native observer phát, giữ correlation/freshness riêng | Không được Kotlin re-dispatch cùng action |
| RUNTIME_STATUS sau reconnect | Latest native snapshot cho session hiện tại | Không replay command/action cũ |

Receipt bị REJECTED khi request stale/conflict/expired/identity mismatch/
unsupported protocol. Mixed peer không nhận receipt; frame mismatch đóng channel.

## 5. Lifecycle và reconnect

| Tình huống | Contract behavior |
|---|---|
| Nhận desired trước managed-ready | Lưu RAM, status received/DIAGNOSTIC_PENDING; không IL2CPP/mutation |
| Cùng session reconnect | Native giữ latest desired/applied state; gửi status mới nhất; Kotlin resend latest desired idempotent; không replay gameplay |
| Session mới do game relaunch | Native state/pending/generation/seq mới; app bỏ readiness/lease/navigation cũ và gửi durable desired snapshot sau ready |
| Explicit STOP | Native disable/reset module/pending state, status STOPPING → ATTACHED_IDLE; stale callback không override |
| App kill/socket mất | Không tự tạo auto-resume policy mới; broker không giữ unbounded queue. Runtime behavior hiện tại được bảo toàn theo inventory, nhưng client reconnect chỉ resync snapshot/status |
| Protocol/schema mismatch | Reject/close, status lỗi nếu còn channel; không fallback legacy Kotlin owner |

Transport state (connected, socket error, request timeout) là state app-local,
không ghi vào RuntimeUiStatus như native readiness. Root runtime.status chỉ
diagnostic; không phải desired-state queue hoặc persistence.

## 6. Compatibility matrix

| Controller | Native | Kết quả |
|---|---|---|
| v3 + desired schema 1 | v3 + desired/status schema 1 | Đường mới được phép chạy sau auth/session/identity/capability guards |
| v2 legacy | v3 | Frame protocol mismatch; fail closed, không fallback |
| v3 | v2 legacy | Native frame reader từ chối protocol; controller không coi timeout là success |
| v3 + schema/marker khác | v3 | Typed payload reject; không parser legacy/không mutation |
| v3 + unknown capability/setting | v3 | Giữ unknown/unsupported; ready=false hoặc field không gửi; không silent default thành feature |
| v3 duplicate request | v3 | Exact duplicate idempotent receipt; same revision khác snapshot reject |

Legacy START/STOP/DIAGNOSTIC và ba config payload vẫn tồn tại trong source
cho compatibility/test ở giai đoạn cutover, nhưng production client mới không
gọi chúng. Khi chúng còn được parser, desired marker phải được dispatch trước
gameplay và các parser khác vẫn phải reject field/trailing bytes sai.

`BridgeEvent.RuntimeStatus` cũ chỉ còn để giữ source compatibility; encoder
protocol 3 từ chối nó. `BridgeEvent.RuntimeUiStatus` và marker RUST là đường
status duy nhất được encode/decode, nên một caller chưa chuyển không thể âm
thầm phát payload status schema cũ.

## 7. Hằng số/guard cần đối chiếu khi implement

- Kotlin BridgeProtocol.VERSION, BridgePayloadCodecSupport.PAYLOAD_VERSION,
  frame codec, decoder/encoder và new Runtime*PayloadCodec phải dùng cùng
  protocol/schema/marker/field order.
- Native kBridgeProtocolVersion, command routing, runtime_control.inc và
  status sender phải dùng cùng version/marker/order; read_bridge_frame đã
  fail closed khi protocol khác.
- Giữ kBridgeHardMessageBytes=4 MiB, string/count limits và exact identity
  checks. Không dùng ACK START/CONFIG_SET hoặc desired receipt làm applied/
  action outcome.
- Không thêm raw game observation decoder vào app; status/event/map-target/
  navigation là boundary DTO. READ_MAP_TARGET, navigation lease và stale
  session guards tiếp tục có hiệu lực.
