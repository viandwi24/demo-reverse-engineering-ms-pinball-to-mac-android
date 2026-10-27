# Journey: Reverse Engineering 3D Pinball Space Cadet → Mac & Android

Catatan perjalanan port **3D Pinball for Windows – Space Cadet** (versi Windows XP) menjadi aplikasi native
macOS dan Android. Kodenya ditulis ulang dari hasil reverse engineering `Pinball.exe` milik sendiri,
dengan target perilaku yang sama persis dengan aslinya. Semua pekerjaan dilakukan **offline**.

Tanggal: 26 Oktober 2026. Waktu dalam WIB.

---

## 1. Lingkungan & tools

| Kebutuhan | Yang dipakai | Catatan |
|---|---|---|
| Mesin | macOS (Apple Silicon, arm64) | |
| Asisten | Claude Code, dijalankan dengan `CLAUDE_CONFIG_DIR=~/.claude-team` (alias `claude-team`) | config terpisah dari `~/.claude` |
| Reverse engineering | **REA** (`rea-agents` 6.8.0) sebagai MCP server di Claude Code | https://github.com/morluto/rea |
| Disassembler/decompiler | **Ghidra 12.1.4** (gratis, dipakai REA; pengganti Hopper yang berbayar) | REA hanya menerima Ghidra 12.1.x |
| Java untuk Ghidra | **OpenJDK 21** (`brew install openjdk@21`) | Ghidra 12.1 butuh JDK ≥ 21; JDK 17 ditolak |
| Build tools Mac | Xcode Command Line Tools (clang, make, bison 2.3), CMake | |
| Window/audio Mac | SDL2 (`sdl2-compat` Homebrew), AudioToolbox (MIDI bawaan macOS) | |
| Android | Android SDK (platform 35/36, build-tools 36, emulator, image android-35 arm64), NDK 29 | tanpa Gradle |
| Bahan | Folder Pinball dari Windows XP: `Pinball.exe`, `PINBALL.DAT`, `SOUND*.WAV`, `PINBALL.MID`, `FONT.DAT`, dll. | salinan milik sendiri |

### 1.1 Memasang REA untuk Claude Code dengan config dir khusus

Masalah pertama: installer REA menulis ke `~/.claude`, sedangkan sesi ini memakai `~/.claude-team`.
REA sebenarnya menghormati `CLAUDE_CONFIG_DIR`, tapi variabel itu hanya ada di dalam alias, tidak di shell.
Solusinya: jalankan setup dengan variabel yang sama.

```sh
CLAUDE_CONFIG_DIR=~/.claude-team bunx rea-agents@latest setup --client claude_code
```

Hasilnya, registrasi MCP masuk ke `~/.claude-team/.claude.json` dan skill ke
`~/.claude-team/skills/reverse-engineer-anything`. REA di `~/.claude` lalu di-uninstall.

### 1.2 Ghidra di macOS arm64 untuk REA

1. Unduh **Ghidra 12.1.4** (bukan 12.2 terbaru) dari rilis resmi NSA di GitHub, lalu cocokkan SHA-256:
   `ddac49f903da9d5bac833e5cc79395098b9c33cfd3279be5f31bd00387d2d4db`. Unzip ke `~/scripts/ghidra_12.1.4_PUBLIC`.
2. Pasang JDK 21: `brew install openjdk@21`.
3. Paket resmi Ghidra tidak menyertakan decompiler native untuk Apple Silicon, jadi di-build sendiri:
   ```sh
   cd ~/scripts/ghidra_12.1.4_PUBLIC/Ghidra/Features/Decompiler/src/decompile/cpp
   make ghidra_opt ARCH_TYPE="-arch arm64" OSDIR=mac_arm_64
   mkdir -p ../../../os/mac_arm_64 && install -m 755 ghidra_opt ../../../os/mac_arm_64/decompile
   ```
4. Tambahkan ke registrasi MCP `rea` (di `~/.claude-team/.claude.json`, bagian `env`):
   `GHIDRA_INSTALL_DIR=~/scripts/ghidra_12.1.4_PUBLIC` dan
   `JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home`.
5. Cek dengan `rea doctor --json`: semua cek `ghidra-*` harus healthy. Uji coba: decompile program C kecil
   lewat REA dengan `provider_id: ghidra`.

Catatan: `rea doctor` sempat gagal karena shell memuat Node v24 dari fnm, padahal terminal user memakai Node v26 dari nvm.
Perbedaan PATH antar shell ini perlu diperhatikan.

---

## 2. Analisis awal: apa yang di-reverse

Langkah pertama dengan REA: `inspect_pe_imports`, `open_binary` (Ghidra), `binary_overview`, `decompile_all`.

| Temuan | Detail |
|---|---|
| Binary | PE32 x86, image base `0x1000000`, 852 prosedur (649 internal + 203 impor), PDB tidak tersedia (nama fungsi hilang) |
| Ketergantungan Windows | GDI (DIB 8-bit + palet), WaveMix di atas `waveOut*`, MIDI lewat MCI, USER32, registry |
| Presisi | `_controlfp(0x10000, 0x30000)` → x87 53-bit: intermediate dihitung `double`, disimpan sebagai `float` |
| Acak | `rand()` msvcrt tanpa `srand` → LCG 214013/2531011 bisa ditiru persis |
| Waktu | Langkah fisika memakai dt dari `timeGetTime` (maks 100 ms), dengan sub-step |
| Aset | `PINBALL.DAT` = container `PARTOUT(4.0)RESOURCE` (541 grup), WAV 8-bit 11 kHz, MIDI format 1 |

Artefak yang dihasilkan selama pengerjaan (di `re/`):
- `re/decomp/functions/*.c`: pseudocode semua fungsi (REA `decompile_all`)
- `re/asm/*.asm`: disassembly semua fungsi (Ghidra headless + `tools/ghidra/ExportAsm.java`)
- `re/extra/`: fungsi yang hanya dipanggil lewat vtable dan terlewat Ghidra (`tools/ghidra/MakeFuncs.java`)
- `re/funcs.txt`, `re/control_table.json`, `re/NOTES.md`: peta modul, struct, dan offset

Setelah proyek dirapikan, yang disimpan di repo hanya `NOTES.md`, `funcs.txt`, dan `control_table.json`.
Pseudocode, disassembly, dan proyek Ghidra dihapus karena bisa dibuat ulang dari `Pinball.exe` (cara di `re/README.md`).
- `tools/peconst.py`: membaca konstanta dan string dari exe; `tools/datdump.py`: membaca struktur DAT

### Jebakan decompile yang ditemukan (selalu cek `.asm`)
- Ghidra menghilangkan nilai return x87 (mis. `normalize` sebenarnya mengembalikan panjang vektor).
- Argumen `ftol()` hilang (mis. `timer_set` = `now - ftol(t * -1000)`).
- `floor(x) - 1.0` sebelum `ftol` terbaca tanpa `- 1.0` (kode bentuk dinding di DAT tersimpan +1).
- Float yang sebenarnya integer (mis. `5.60519e-45` = int 4).
- Konstanta skala `rand()` ternyata `double` 1/32767, bukan `float`.

---

## 3. Tahapan & waktu

| # | Tahap | Perkiraan awal | Realisasi |
|---|---|---|---|
| 0 | Setup REA + Ghidra, analisis awal, decompile 649 fungsi | — | ±3 menit (decompile 13 detik) |
| 1 | Parser `PINBALL.DAT` + render meja di Mac | 2–4 jam | 5 menit (16:52–16:57) |
| 2 | Objek meja + fisika | 8–15 jam | 38 menit (17:06–17:44) |
| 3–5 | Aturan game, suara/musik, UI dasar | 8–16 jam | ±14 menit (17:52–18:06) |
| 6 | Debug + pencocokan | 3–5 jam | sebagian (uji otomatis); belum dicocokkan dengan rekaman asli |
| 7 | Android (APK + emulator), oleh sub-agent | 4–7 jam | ±8 menit (18:23–18:31) |

### Tahap 1: Format data & render statis
- Loader DAT diporting dari `FUN_010064e2`. Tipe entri: short, bitmap8, nama grup, palet, string, short[], float[], z-map.
- Header bitmap 14 byte. Z-map punya header 14 byte, dan ada z-map kosong.
- Layout layar 600×416: meja di (0,0), panel skor di (386,12), sprite di `posisi − (137,2)`.
- Diverifikasi dengan mencocokkan piksel sprite lampu ke gambar meja.

### Tahap 2: Engine fisika & objek
- Matematika & raycast (lingkaran, garis), dicocokkan instruksi per instruksi dengan asm.
- Grid collision 10×15 dengan DDA, field effect (gravitasi + `rand()`), pantulan dengan elastisitas, kehalusan, threshold, dan boost.
- Renderer asli: dirty-rect, z-buffer 16-bit, bola digambar di atas lalu latarnya dipulihkan.
- Flipper (edge berputar, raycast sepanjang waktu gerak), plunger, drain, lampu.
- 16 kelas objek lain dikerjakan paralel oleh 2 sub-agent: bumper, target, kicker, kickout, hole, ramp, oneway, gate, rollover, sink, tripwire, flag, blocker, demo.
- Uji: demo terskrip + uji ketahanan 5 menit dengan sanitizer.

### Tahap 3: Aturan game (modul kontrol, 112 fungsi)
- Tabel kontrol 88 entri (komponen → fungsi aturan + nilai skor) dan 142 "tag" komponen diekstrak dari exe.
  Header bertipe dibuat otomatis (`src/engine/control/tags.h`).
- 118 fungsi kontrol dikerjakan paralel oleh 3 sub-agent (`control_c1/c2/c3.cpp`): misi, rank, bonus, jackpot, extra ball, multiball, tilt, kode cheat.
- Dispatcher meja (`TPinballTable::Message`): new game, ganti pemain, game over, tilt, skor per pemain.
- 189 string resource dan font bitmap `PBMSG_FT` diekstrak dari exe (string 158 "1 UseBitmapFont").

### Tahap 4–5: Suara, musik, UI
- Mixer 8 kanal 11025 Hz yang meniru WaveMix: suara baru mengambil kanal yang paling lama dipakai.
- `PINBALL.MID` diputar terus-menerus lewat AudioToolbox (synth DLS bawaan macOS).
- Input memakai kode virtual-key Windows asli. F2 game baru, F3 pause, F5–F8 jumlah pemain, F9/F10 suara/musik.
  Setting dan high score disimpan di Application Support.

### Tahap 6: Verifikasi
- 5 seed acak × ±10 menit (sekitar 30 game penuh) dengan AddressSanitizer + UBSan: tanpa crash.
- Teks misi muncul sesuai alur aslinya ("Launch Ramp To Accept Launch Training Mission", "Wormhole Opened", "Crash Bonus", dll.).

### Tahap 7: Android (selesai 18:31)
- NativeActivity tanpa kode Java dan tanpa Gradle: NDK clang → `libpinball.so`, `aapt2` → APK, `zipalign`, `apksigner`.
- Tampilan lewat `ANativeWindow` (landscape, diperbesar). Efek suara lewat AAudio. Data game dipaketkan sebagai assets.
- Kontrol sentuh: sepertiga kiri/kanan layar = flipper, tengah bawah = plunger.
- Emulator: AVD `PinballTest` (Android 35, Google APIs, arm64-v8a).
- Paket `com.spacecadet.pinball`, APK `android/build/pinball.apk` (3,3 MB, arm64-v8a, minSdk 26, target 35).
- Musik: `PINBALL.MID` lewat MIDI player bawaan Android. Efek suara: mixer 8 kanal di atas AAudio.
- Masalah yang diperbaiki: Android peka huruf besar/kecil pada nama file, sedangkan engine meminta `sound1.wav` padahal filenya `SOUND1.WAV`.
- Tombol keyboard/gamepad sama dengan versi Mac. Sentuh bagian atas tengah layar = game baru.

---

## 4. Struktur proyek

```
README.md                ringkasan, cara build, tombol
3dPinballWindows/        data asli (tidak disertakan; README berisi daftar file yang dibutuhkan)
re/                      catatan RE: NOTES.md, funcs.txt, control_table.json, README.md
tools/                   datdump.py, peconst.py, ghidra/*.java
src/dat_file.*           loader PINBALL.DAT
src/engine/              engine portabel (tanpa SDL)
  maths, loader, render, timer, collision, component, ball, table (pb)
  objects*.cpp, lights.cpp, misc_objects.cpp, score.cpp
  control/               aturan game (tabel, tag, 118 fungsi kontrol)
src/exe_resources.*     baca string table + font dari Pinball.exe pengguna
src/platform/            audio Mac (SDL mixer, AudioToolbox MIDI)
src/main.cpp             app Mac (SDL)
android/                 lapisan Android + skrip build APK
```

## 5. Cara build & jalan

Siapkan data game terlebih dahulu: salin file dari salinan game milik Anda sendiri ke `3dPinballWindows/`
(daftar file ada di `3dPinballWindows/README.md`).

```sh
# Mac
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j8
./build/pinball                          # main
mkdir -p /tmp/demo && ./build/pinball 3dPinballWindows --demo /tmp/demo   # skenario uji + screenshot
```

```sh
# Android (emulator harus sudah jalan)
android/build.sh --install               # build APK, instal, lalu buka app
```

## 6. Yang belum / catatan akurasi
- `SOUND62.WAV` tidak ada di folder data, sehingga satu efek suara itu tidak berbunyi (di Mac maupun Android).
- Di Android, tarikan plunger pertama setelah game dimulai hanya membawa bola ke jalurnya; bola baru meluncur pada tarikan kedua. Belum dicek apakah game asli juga begitu.
- Belum dicocokkan dengan rekaman game asli. Ini butuh rekaman dari Windows XP atau VM.
- Dialog isi nama high score belum dibuat (dipakai nama default "Player N"). Menu bar diganti tombol F.
- Satu tebakan: grup bumper (`0x100cabf`) membaca memori di luar objek di exe asli. Hasilnya bergantung pada heap XP, jadi diasumsikan selalu benar.
- `fsin`/`fcos` x87 diganti `sin`/`cos` libm; selisihnya paling banyak 1 bit terakhir.
- Fisika memakai dt waktu nyata (sama seperti aslinya), jadi lintasan bola tidak identik frame-per-frame, bahkan di Windows sendiri.

## 7. Perapian codebase
- Dihapus: data game dan `Pinball.exe` (diganti `3dPinballWindows/README.md` berisi daftar file dan SHA-1 versi
  yang dipakai), hasil build Mac/Android, keystore debug (dibuat ulang otomatis oleh `android/build.sh`),
  artefak RE besar (`re/out`, `re/decomp`, `re/asm`, `re/extra`, proyek Ghidra), dan `pbmsg_ft.bin`.
- Ukuran proyek turun dari ±125 MB menjadi ±770 KB. `.gitignore` menjaga file-file itu tetap di luar git.
- `tools/peconst.py` tidak lagi memakai path absolut: otomatis mencari `3dPinballWindows/Pinball.exe` atau membaca `$PINBALL_EXE`.
- Perapian untuk publikasi: string table dan font pesan tidak lagi disimpan di repo (`src/data/*.inc` dihapus).
  Keduanya dibaca saat runtime dari resource `Pinball.exe` milik pengguna (`src/exe_resources.cpp`), dan hasilnya
  sudah dicek identik byte-per-byte dengan versi lama. Semua tautan unduhan data game dihapus, lalu disclaimer
  edukasi ditambahkan di `README.md`.

## 8. Cara kerja REA & Ghidra (dengan contoh `first_test/`)

### Siapa mengerjakan apa
| Komponen | Peran |
|---|---|
| **Ghidra** | Mesin analisis. Memuat binary (PE/Mach-O/ELF), memecah kode mesin jadi instruksi (disassembly), menemukan batas fungsi dan pemanggilannya, lalu mengubah instruksi menjadi pseudocode mirip C (decompile) lewat representasi antara P-code. |
| **REA** | Jembatan antara Ghidra dan Claude Code (MCP server). Menjalankan Ghidra secara headless dan menyediakan perintah seperti `open_binary`, `list_procedures`, `procedure_pseudo_code`, `procedure_assembly`, `decompile_all`, `inspect_pe_imports`, `xrefs`. Hasilnya dikembalikan dalam bentuk data terstruktur yang disertai bukti (alamat, hash, batasan analisis). |
| **Claude Code** | Membaca hasil REA, menamai fungsi, menyusun ulang struct, membandingkan dengan assembly, lalu menulis ulang kode C++ dan mengujinya. |

Alurnya: `binary → Ghidra (disassembly → fungsi → P-code → pseudocode) → REA (MCP) → Claude Code → kode baru → uji`.

### Uji coba dengan program sendiri
`first_test/main.c` berisi fungsi kecil bergaya Pinball (`normalize`, `bounce`, `message` dengan kode `0x3f6`).
Program itu dikompilasi dalam tiga varian (dengan simbol, di-strip, dan `-O2`), lalu varian strip di-decompile lewat REA.
Detail lengkap dan perbandingan baris demi baris ada di [`first_test/README.md`](first_test/README.md).

Temuan utama:
- **Nama hilang, logika tetap.** Fungsi buatan sendiri menjadi `FUN_<alamat>`, sedangkan nama impor (`_printf`, `_atoi`) tetap ada.
  `normalize` ter-decompile hampir identik dengan aslinya.
- **Struct menjadi offset.** `b->dx` tampil sebagai `*(float*)(param + 8)`. Layout struct disusun ulang dari offset yang dipakai.
  Inilah cara struct `TBall`, `TPinballTable`, dll. direkonstruksi untuk Pinball (lihat komentar `// +0x2a` di kode).
- **Konstanta float tampil sebagai pola bit.** `0x3f4ccccd` = 0.8, `0x40400000` = 3.0.
- **Ada artefak decompiler.** Argumen palsu, urutan parameter float/pointer yang tertukar (konvensi arm64), dan argumen `printf` yang hilang.
  Assembly dipakai untuk memastikan. Di Pinball (x86 + FPU x87) jebakannya lain: nilai return x87 hilang dan argumen `ftol` hilang.
- **Optimasi menghapus fungsi.** Pada `-O2`, `normalize` dan `bounce` sudah dihitung saat kompilasi sehingga tidak ada lagi di binary.

### Dari decompile ke aplikasi sendiri
`first_test/rebuilt.c` ditulis **hanya** dari hasil decompile:
nama fungsi diberi sesuai perilaku, struct disusun dari offset, konstanta diterjemahkan, dan artefak dibuang.
Output-nya dibandingkan dengan program asli untuk 3 input, dan hasilnya **identik** (`first_test/build.sh`).

Port Pinball memakai pola yang sama, hanya dalam skala lebih besar: 649 fungsi diberi nama dan ditulis ulang
per modul. Tiap modul diuji dengan demo terskrip, pencocokan piksel sprite, dan simulasi game penuh.
