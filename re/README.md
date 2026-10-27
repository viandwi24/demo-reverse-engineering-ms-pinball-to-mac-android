# Artefak reverse engineering

Yang disimpan di sini hanya catatan dan data kecil yang dipakai kode:
- `NOTES.md`: peta modul, format DAT, struct/offset, jebakan decompile
- `funcs.txt`: daftar fungsi (alamat, ukuran, signature); sumber `src/engine/control/control_fns.h`
- `control_table.json`: tabel kontrol (88 entri) dan 142 tag komponen; sumber `src/engine/control/tags.*` dan `control.cpp`

Pseudocode dan disassembly tidak disimpan karena bisa dibuat ulang dari `Pinball.exe`:

```sh
# pseudocode per fungsi (REA + Ghidra, lewat Claude Code): decompile_all → re/decomp/
# disassembly per fungsi (Ghidra headless):
export JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home
mkdir -p re/ghidra_proj
~/scripts/ghidra_12.1.4_PUBLIC/support/analyzeHeadless re/ghidra_proj pinball \
  -import 3dPinballWindows/Pinball.exe -overwrite -scriptPath tools/ghidra \
  -postScript ExportAsm.java "$PWD/re/asm"
```
