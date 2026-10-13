# REA/Ghidra timeout — Pokémon GO 0.433.0

## Target và môi trường

- Target: `reverse/artifacts/pogo-0.433.0/native/libil2cpp.so`
- SHA-256: `6b1e11619362ddd53f9dbcd7be1d4296bdec5dda27b9f742c16b8403359c25a0`
- REA: `rea-agents` 6.4.0
- Provider: Ghidra 12.1.3 headless
- Java: OpenJDK 21.0.12.1
- Host: macOS x86_64

`rea doctor --provider ghidra --target <target>` xác nhận Ghidra, Java, native
decompiler và target đều available. Đây không phải lỗi cài đặt provider.

## Probe đã chạy

```bash
GHIDRA_INSTALL_DIR=/usr/local/opt/ghidra/libexec \
JAVA_HOME=/usr/local/Cellar/openjdk@21/21.0.12.1/libexec/openjdk.jdk/Contents/Home \
REA_GHIDRA_STARTUP_TIMEOUT_MS=1800000 \
/path/to/rea search \
  reverse/artifacts/pogo-0.433.0/native/libil2cpp.so \
  'MapEntityCell|WildMapPokemon|PokemonBagImpl|ItemBagImpl|RecycleItem|ReleasePokemon' \
  --kind strings \
  --mode regex \
  --provider ghidra \
  --snapshot build/rea/pogo-0.433.0-libil2cpp-analysis.json \
  --format json \
  --full-output
```

Trong lúc chạy, Ghidra Java dùng CPU liên tục và khoảng 1.25 GiB RSS; không có
dấu hiệu process bị treo. REA kết thúc sau deadline 30 phút:

```json
{
  "error": "Analysis failed",
  "code": "provider_timeout",
  "category": "timeout",
  "message": "Analysis took too long. Try a smaller request, then run `rea doctor` if it continues.",
  "details": {
    "operation": "search_strings",
    "timeout_ms": 1800000
  },
  "duration_ms": 1802663
}
```

## Kết quả và giới hạn

- Không có Evidence ID hoặc kết quả search được tạo.
- `build/rea/pogo-0.433.0-libil2cpp-analysis.json` không được tạo.
- REA cleanup đã dừng Java/Ghidra và xóa project tạm; raw `ghidra.log` và
  `script.log` không còn sau cleanup.
- REA 6.4.0 chạy provider Ghidra bằng project read-only, ephemeral và
  `-deleteProject`. Cờ CLI `--snapshot` cache Evidence thành công, không lưu
  database Ghidra.
- Không lặp lại probe 30 phút với cùng input. Máy tiếp theo cần nhiều tài nguyên
  hơn hoặc deadline dài hơn; nếu dùng project Ghidra persistent để điều tra thủ
  công, kết quả đó vẫn phải được nâng thành REA Evidence trước khi mở capability
  production.

## Bước tiếp theo trên máy khác

1. Rehydrate artifact theo `reverse/artifacts/README.md` và kiểm hash.
2. Import `reverse/rea/rea-evidence-2026-10-13.json` trước khi tạo analysis mới.
3. Chạy `rea doctor --provider ghidra --target
   reverse/artifacts/pogo-0.433.0/native/libil2cpp.so`.
4. Chỉ chạy probe Ghidra lại khi đã thay đổi điều kiện thất bại: máy mạnh hơn,
   heap/cpu phù hợp hoặc deadline lớn hơn 30 phút.
5. Export Evidence deterministic về bundle canonical khi có kết quả.
