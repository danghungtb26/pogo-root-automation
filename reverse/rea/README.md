# REA Evidence

`rea-evidence-2026-10-13.json` là bundle deterministic export từ REA, không phải
source code hoặc decompiler dump. Bundle hiện giữ 18 record: inventory/extraction
cho APKM/base/split Pokémon GO 0.433.0, PGSRoot, embedded UI và PGTools, cùng
manifest/package identity do JADX 0.7.1 quan sát trên base APK. Hai residual
unknown về IL2CPP class/method contract và native call path vẫn mở. SHA-256 bundle:
`8c88de1e593331fdaa4a8e1dd0e1987e8506c0aab89efca37f3f5d61d19debcb`.

Import bundle trước khi tạo analysis mới. Evidence mới phải giữ artifact hash,
authority, coverage, limitations và được export lại vào file canonical này.
