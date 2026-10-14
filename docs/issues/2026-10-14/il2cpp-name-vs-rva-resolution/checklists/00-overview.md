# Kế hoạch — Verified IL2CPP method resolver và RVA guard

Ngày lập: 2026-10-14

Issue: `il2cpp-name-vs-rva-resolution`

Target: `com.nianticlabs.pokemongo` 0.433.0, versionCode `2026100702`,
arm64-v8a, Unity `6000.3.22f1`, IL2CPP metadata v39

Nguồn: [Brainstorm name/type so với RVA](../brainstorm.md)

Plan liên quan: [PGS-style Zygisk 0.433.0](../../../2026-10-13/pgs-style-zygisk-0433/checklists/00-overview.md)

## Kết quả mong muốn

Runtime tiếp tục resolve `MethodInfo*` từ managed metadata và gọi bằng
`il2cpp_runtime_invoke`. Method mutation chỉ được chấp nhận khi exact
build identity, declaring class, full signature, uniqueness và metadata token
đều khớp. RVA là build-scoped evidence/guard có điều kiện; không có
fallback gọi trực tiếp `module_base + RVA`.

Plan này gỡ blocker method/build identity cho các task `T-303`, `T-304`,
`T-305` của plan 2026-10-13. Nó không tự hoàn tất typed world-state,
Auto Catch, Auto Transfer hay Auto Discard; owner lifetime, Promise/result và
authoritative postcondition vẫn phải được xác minh riêng trước khi
capability trở thành `LIVE_READY`.

## Bằng chứng đã pin

- `libil2cpp.so` SHA-256:
  `6b1e11619362ddd53f9dbcd7be1d4296bdec5dda27b9f742c16b8403359c25a0`.
- `global-metadata.dat` SHA-256:
  `f5961a8548256da287b4f79466a169c302b90ec59be7f6461e8e58b482c0fb0d`.
- Hash manifest hiện pass 10/10.
- REA bundle sau focused merge có 33 record, 2 unknown identity và 4 unknown
  revision; 13 record khớp exact `libil2cpp.so` target.
- `TryCapture`: token `0x06013ab5`, slot 44, RVA `0x9e1caa4`;
  `ev_51dac3509790854d9db72de1ce51c072e1df8f4a500c1531f1fce5d460e1a947`
  chỉ chứng minh RVA nằm trong body Ghidra gộp `FUN_09e1c6cc`.
- `ReleasePokemon`: token `0x06002a43`, slot 19, RVA `0xa04cf68`;
  `ev_7f6095cfb8ea32b501234264006fa046a80a0fadfcbceb4d2a27597f4a30c92c`
  chỉ chứng minh code containment.
- `RecycleItem`: token `0x06002971`, slot 23, RVA `0xa006bec`;
  `ev_d768040014436d743c0dc46938613e51e0b6671cd710736d295bbc01c4c0ecb4`
  chỉ chứng minh code containment.
- `GetPokemon` token `0x06002a39`, RVA `0xa04bd58`; `GetItemCount` token
  `0x0600295b`, RVA `0x9ffe438`; hai entry này chưa có exact-entry dossier.

Authority của ba dossier là `shipped-artifact`, confidence `observed`, Ghidra
12.1.3, nhưng coverage không đủ chứng minh exact boundary/thunk/native ABI.
Mapping metadata-to-RVA là static-derived provenance, không phải runtime proof.

## Acceptance criteria

| ID | Tiêu chí |
|---|---|
| `AC-VBR-01` | Runtime tự xác minh package, versionCode, IL2CPP build ID và metadata SHA-256; controller fingerprint không được coi là proof. |
| `AC-VBR-02` | Resolver yêu cầu exact declaring class/image, instance/static, method name, full param types, return type, uniqueness và metadata token; mọi mismatch có structured reason. |
| `AC-VBR-03` | Descriptor đã verify được gọi qua `il2cpp_runtime_invoke`; không có direct-RVA call hay fallback khi token/RVA mâu thuẫn. |
| `AC-VBR-04` | Build profile 0.433.0 khai báo exact contract cho `TryCapture`, `GetPokemon`, `ReleasePokemon`, `GetItemCount`, `RecycleItem`; RVA guard chỉ required khi exact entry/thunk evidence đạt. |
| `AC-VBR-05` | `*_verified` tách method identity, owner/session lifetime và outcome/postcondition; pointer non-null không tự mở mutation capability. |
| `AC-VBR-06` | Session/owner generation reset làm invalid descriptor/owner/pending cũ; Unity/IL2CPP work tiếp tục qua main-thread bridge. |
| `AC-VBR-07` | Promise/result/server postcondition thiếu evidence giữ capability `UNVERIFIED_DEVICE`/`UNAVAILABLE`; dispatch không bằng completed. |
| `AC-VBR-08` | REA bundle/snapshot/ledger ghi artifact hash, Evidence ID, authority, coverage, limitation và residual unknown; không dùng dump 0.427.0 làm proof. |
| `AC-VBR-09` | Existing native checks, Magisk build/package, full Gradle build, line-count audit và `git diff --check` đạt; không tạo/sửa test source. |

## Mapping AC → task

| AC | Task |
|---|---|
| `AC-VBR-01` | `VBR-101`–`VBR-103` |
| `AC-VBR-02` | `VBR-002`, `VBR-201`–`VBR-203` |
| `AC-VBR-03` | `VBR-202`, `VBR-301`–`VBR-303` |
| `AC-VBR-04` | `VBR-001`–`VBR-003`, `VBR-203`, `VBR-303` |
| `AC-VBR-05` | `VBR-301`, `VBR-302`, `VBR-304`, `VBR-402` |
| `AC-VBR-06` | `VBR-304`, `VBR-401` |
| `AC-VBR-07` | `VBR-004`, `VBR-402`, `VBR-403` |
| `AC-VBR-08` | `VBR-001`–`VBR-004`, `VBR-502` |
| `AC-VBR-09` | `VBR-501`, `VBR-502` |

## Phạm vi

- Static evidence và exact build profile cho PoGo 0.433.0 arm64.
- Independent runtime build verification.
- IL2CPP API surface cho token/type-name ownership và verified resolver dùng chung.
- Migration năm critical method sang verified descriptor contract.
- Optional RVA-pointer integrity guard sau khi exact boundary/thunk được chứng minh.
- Owner/session, Promise/result và postcondition diagnostics/gates cần để
  bàn giao lại plan PGS-style.
- Existing checks, build/package và tài liệu evidence/handoff.

## Ngoài phạm vi

- Gọi native trực tiếp bằng RVA, viết prototype suy đoán hoặc fallback
  khi metadata/token mismatch.
- Hoàn thiện full-source scanner, Virtual GO Plus, policy Auto Catch/
  Transfer/Discard hoặc Android UI ngoài status/capability contract cần thiết.
- Tạo hoặc sửa test source; chỉ chạy existing tests.
- Chạy gameplay mutation/device trong lúc lập plan. Device verification là
  handoff riêng và chỉ dùng script còn lại của repo.
- Raw ADB, script thay thế/tạm hoặc khôi phục reverse script cũ.

## Dependency và câu hỏi còn mở

1. REA/Ghidra hiện gộp boundary của ba mutation method. `VBR-003` chặn
   việc bật RVA equality guard, nhưng không chặn token/full-signature resolver.
2. ABI/ownership của `il2cpp_type_get_name` và export
   `il2cpp_method_get_token` phải được pin trong `VBR-002` trước khi
   `VBR-201` đổi API typedef/lifetime.
3. Nguồn runtime độc lập cho versionCode và bytes metadata chưa được
   chọn. `VBR-101` phải chốt source/authority và fail-closed semantics
   trước implementation.
4. `PokeballThrow`, `ItemData`, generic Promise/result và authoritative
   postcondition chưa đủ evidence. `VBR-004`/`VBR-402` giữ action capability
   đóng thay vì suy ra từ RVA.
5. Owner lifetime và Promise completion chỉ có thể chốt trong process thực.
   `VBR-401`–`VBR-403` tách read-only diagnostics khỏi controlled mutation
   handoff; không claim `LIVE_READY` khi chưa có kết quả.
6. Plan PGS-style cũ vẫn là owner của world state/action implementation.
   `VBR-502` chỉ cập nhật dependency/evidence thực tế, không tick task cũ
   nếu Verify của chúng chưa đạt.

## Thứ tự phase

1. [Phase 0 — Evidence và exact contracts](phase-0-evidence-contracts.md)
2. [Phase 1 — Strong runtime build identity](phase-1-strong-build-identity.md)
3. [Phase 2 — Verified method resolver](phase-2-verified-method-resolver.md)
4. [Phase 3 — Binding migration và capability gates](phase-3-binding-migration.md)
5. [Phase 4 — Runtime evidence và postcondition handoff](phase-4-runtime-evidence.md)
6. [Phase 5 — Tích hợp, build và bàn giao](phase-5-verification-handoff.md)

## Rủi ro và cách xử lý

| Rủi ro | Xử lý |
|---|---|
| Token/full signature khớp nhưng profile sai build | `AC-VBR-01` bắt buộc strong runtime identity trước mutation. |
| Ghidra coarse boundary làm pointer không khớp RVA | Giữ RVA guard `unknown`; không hạ gate token/signature và không direct-call. |
| `type_get_name` leak hoặc free sai | Pin exact export ABI/ownership trước khi đổi typedef; RAII chỉ áp dụng khi evidence xác nhận ownership. |
| Resolver trả match đầu tiên | Collect tất cả match, yêu cầu uniqueness; enumeration budget vượt giới hạn là lỗi riêng. |
| Static identity bị nhầm thành action success | Identity, owner, dispatch, Promise và postcondition là các gate/trạng thái tách biệt. |
| Pointer/owner cũ qua reconnect | Invalidate theo session/owner generation; không persist live pointer. |
| Source native vượt 500 dòng | Tạo unit resolver/profile gọn và tách `.h`/`.inc`; audit trước bàn giao. |
| Device diagnostic thất bại | Chỉ dùng `collect-binding-diagnostics.sh`/`logcat-full.sh`; đọc log khi thất bại, không dùng raw ADB. |

## Quy tắc thực thi và ghi chú bắt buộc

Sau mỗi task, kiểm tra diff và kết quả Verify, cập nhật ngay checkbox
cùng khối `<details>` nằm dưới nó, rồi mới chuyển task. Chỉ đánh
dấu `[x]` khi đạt Verify. Trong
`<details><summary>Đã hoàn thành — T-ID</summary>...</details>`, ghi file đã
tạo/sửa/xóa/di chuyển, function/method/param hoặc hành vi đã thêm/đổi/
bỏ, và lệnh kiểm chứng cùng kết quả thực tế. Mục không phát sinh
ghi “Không”. Nếu còn lỗi, chưa kiểm chứng hoặc bị chặn, giữ `[ ]`,
ghi trạng thái và phần đã làm/điểm còn thiếu. Không dồn ghi chú
tới cuối phase, không đánh dấu hoàn tất dựa trên dự kiến.

Quy tắc bổ sung:

- Kiểm tra dirty worktree trước mỗi task; không reset/checkout thay đổi
  không thuộc task.
- Không tạo/sửa test source; được chạy existing tests.
- File source không phải Markdown phải ≤500 dòng.
- REA là workflow reverse duy nhất; không khôi phục script/dump cũ.
- Device/emulator chỉ dùng script còn lại và target **BlueStacks Air 1**;
  viết plan không tự cấp quyền chạy device/mutation.

## Definition of Done

- Mọi task trong phạm vi đạt Verify và có details thực tế.
- Runtime build identity và verified descriptor resolver fail closed đúng AC.
- Năm critical method đã dùng contract build-scoped; không direct-RVA call.
- RVA equality guard chỉ bật cho entry/thunk có qualifying Evidence; phần còn
  unknown được ghi rõ và không bị biến thành success.
- Existing native/Gradle/build/package checks đạt, `git diff --check` sạch
  và line-count policy đạt.
- REA bundle/ledger và plan PGS-style cũ chỉ được cập nhật theo kết
  quả thực; runtime-dependent phần chưa test giữ `UNVERIFIED_DEVICE`.
