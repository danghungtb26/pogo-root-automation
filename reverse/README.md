# Reverse evidence index

REA là workflow reverse canonical của repo. Script Il2CppDumper cũ đã bị loại
bỏ; không tạo lại script tương đương.

## Nội dung

- `artifacts/`: artifact gốc và artifact do REA trích xuất để phân tích lại.
  Binary lớn được giữ cục bộ và ignore bởi Git; `SHA256SUMS` cùng README được
  commit để kiểm identity.
- `rea/rea-evidence-2026-10-13.json`: bundle Evidence deterministic, hiện có
  17 record và 2 residual unknown.
- `reports/`: báo cáo phân tích tiếng Việt đã có. Đây là historical analyst
  reference; claim cần dùng cho binding phải được nâng thành REA Evidence.
- `pogo-0.427.0/`: material legacy, chỉ dùng tạo hypothesis, không dùng chứng
  minh binding 0.433.0.

## Quy trình dùng lại

1. Kiểm `shasum -a 256 -c reverse/artifacts/SHA256SUMS` từ root repo.
2. Import bundle trong `reverse/rea/` vào REA và query retained Evidence trước.
3. Chọn đúng artifact đã pin trong `reverse/artifacts/`.
4. Với APK/DEX, dùng các thao tác Android của REA; với ELF dùng provider native
   và phân tích function/xref có mục tiêu.
5. Ghi Evidence ID, artifact hash, authority, coverage và limitation vào ledger.
6. Export lại bundle deterministic vào `reverse/rea/` khi có evidence mới.

Không coi token metadata là RVA, pseudocode là source, tên method là call graph,
hay báo cáo cũ là bằng chứng runtime. Provider thiếu thì giữ unknown/capability
đóng.
