# Pinball.exe — catatan reverse engineering

Target: `3dPinballWindows/Pinball.exe` (Windows XP build, PE32 x86)
- SHA-256 `2bbc8234685fe2f6324040af6ea20123cf00c4a56882ce0d9074f0beefac67bc`
- Image base `0x1000000`, `.text` 136 KB, 852 procedures (649 internal, 203 imports)
- PDB `pinball.pdb` GUID `97fd241c-ea10-4fcc-8960-a06f3babe841` age 1 — tidak tersedia, nama fungsi internal stripped (`FUN_xxxxxxxx`)
- Decompile lengkap (Ghidra 12.1.4 via REA): `re/decomp/functions/*.c`, index `re/decomp/decompile-index.json`

## Aset (semua format standar)
- `PINBALL.DAT` — container `PARTOUT(4.0)RESOURCE`, tabel "Space Cadet Table" (grafis, objek, collision, state)
- `SOUND*.WAV` — PCM 8-bit mono 11025 Hz
- `PINBALL.MID`, `PINBALL2.MID` — Standard MIDI format 1
- `table.bmp` — 642x525 8-bit paletted
- `FONT.DAT`, `wavemix.inf` (konfigurasi mixer WaveMix)

## Format PINBALL.DAT (FUN_010064e2)
- Header 0xb7 byte: signature 21 byte, nama app @0x15, deskripsi @0x47, file_size u32 @0xab,
  group_count u16 @0xaf, body_size u32 @0xb1, unknown_size u16 @0xb5 (lalu unknown_size byte dilewati)
- Tiap grup: u8 jumlah entri; tiap entri: u8 tipe, ukuran dari tabel `DAT_010235d0`
  (`{2,-1,2,-1,...,0 @13,...}`; -1 = baca u32)
- Tipe: 0 short, 1 bitmap8, 3 nama grup, 5 palet (256 x BGRx), 9 string, 10 short[], 11 float[], 12 z-map
- Bitmap8: header 14 byte `res u8, w,h,x,y i16, size u32, flags u8` (flags&1 = stride rata 4), piksel bottom-up
- Z-map: header 14 byte `w,h,stride i16 + 8 byte`, lalu u16 bottom-up; ada z-map kosong (dimensi 0)

## Layout layar (FUN_01015424, FUN_01013705, FUN_0101a5ab)
- Buffer 600x416 dari grup `table_size`, di-clear indeks 0
- `background` (panel skor 203x394) di-blit ke (386,12)
- Sprite `table` di (0,0); sprite lain di `pos_bitmap - (137,2)` (origin meja, offset `+0xea/+0xee`)

## Dependensi platform (yang harus diganti di Android)
- Grafis: GDI (DIB 8-bit + palette, BitBlt/StretchDIBits)
- Audio: WaveMix di atas waveOut*, MIDI via mciSendCommand
- Input/UI: USER32 window, menu, dialog, keyboard
- Setting/highscore: registry (ADVAPI32)
- CRT: msvcrt (rand, floor, _CIacos, _ftol, sprintf, ...)

## Temuan penting untuk akurasi
- `FUN_010211b9`: `_controlfp(0x10000, 0x30000)` → x87 precision control 53-bit (double).
  Port: hitung intermediate dalam `double`, bulatkan ke `float` saat disimpan.
- `rand()` msvcrt tanpa `srand` di import → harus ditiru persis (LCG 214013/2531011, `>>16 & 0x7fff`).
- Main loop `FUN_0100833a` (WinMain): langkah simulasi `FUN_01014bf9(dt_ms)` dengan dt dari `timeGetTime`
  → fisika bergantung waktu frame nyata; di Windows asli pun tidak deterministik frame-per-frame.
- String internal menunjukkan modul: `loader_*` (parser DAT), `partout`, lampu `lite3xx`, `soundwaveNN`,
  `mission_text_box`, `info_text_box`, key mapper, high score.

## Tahap 2 — peta engine (dikumpulkan 26 Okt 2026)
Tool: `re/asm/<addr>.asm` (ekspor Ghidra headless, `tools/ghidra/ExportAsm.java`), `re/funcs.txt` (alamat, ukuran, signature).
Ghidra kadang menghilangkan return x87 (mis. `FUN_01008ae9` sebenarnya return panjang vektor) dan argumen `ftol` —
selalu cek `.asm` untuk fungsi float.

### Pabrik objek meja — FUN_0101aaf3 (TPinballTable ctor), daftar dari grup `table_objects` (short array id 0x401)
| tipe | ctor | contoh grup | | tipe | ctor | contoh grup |
|---|---|---|---|---|---|---|
| 0x3e8/0x3f2 | 010191ba wall | #280.. | | 0x3f8 | 01019204 oneway | s_onewy* |
| 0x3e9 | 0101a4f3 plunger | plunger | | 0x3f9 | 0101a416 sink | v_sink* |
| 0x3ea | 0100ad21 light | lite* | | 0x3fa | 010194b0 flag | a_flag* |
| 0x3eb/0x3ec | 0101c59d flipper | a_flip1/2 | | 0x3fb | 0101961f solo target | a_targ10.. |
| 0x3ed | 0101944a bumper | a_bump* | | 0x3fc | 010197bb light rollover | a_roll9 |
| 0x3ee | 010195e0 popup target | a_targ1..9 | | 0x3fd | 01019bb8 ramp | ramp, s_ramp9 |
| 0x3ef | 0101a4b4 drain | drain | | 0x3fe | 01019a1c hole | ramp_hole |
| 0x3f3 | 01019677 blocker | v_bloc1 | | 0x3ff | 0101a2b5 demo | demo |
| 0x3f4/0x405 | 01019884 kickout | a_kout* | | 0x400 | 01019816 trip | s_trip* |
| 0x3f5 | 010196e5 gate | v_gate* | | 0x402 | 0100adb1 light group | goal_lights.. |
| 0x3f6 | 0101983d kicker | a_kick* | | 0x404 | 01018ffd component group | |
| 0x3f7 | 01019771 rollover | a_roll* | | 0x406 | 0100ae72 bargraph | fuel_bargraph |
| 0x407/0x408 | 0101aa18/0101aa56 sound | soundwave* | | 0x409 | 01014092 text box | info/mission_text_box |
Ball: ctor 01019309 (0x16a byte), table component: 0101a5ab.

### Hierarki
- TPinballComponent ctor 01017c6d (vtable 0x10024a8): [0]Message [1]? [2]put_scoring [3]get_scoring [4]dtor
- TCollisionComponent ctor 010190b3 (vtable 0x10024ec): +[5]Collision(ball,pos,normal,dist,edge)=01017e85, [6]FieldEffect=01019168
- TEdgeSegment ctor 01015866 (vtable 0x1002444): +4 owner, +8 ptr→flag aktif owner, +0xc processed, +0x11 mask, geometri @+0x15.
  vtable [0]EdgeCollision(ball,dist) [3]FindCollisionDistance(ray)
  - Circle 01015898 (0x1002454): center @0x15, r² @0x21; dist=01008b34; collision 0101cc8d
  - Line 0101c6fd/0101c77e (0x1002864): TLine @0x15 {nx,ny,?,dx,dy,?,offset,min,max, hit.x,hit.y}, endpoint @0x45; dist=01008c8f; collision 0101c7e6; offset 0101c813
  - FlipperEdge 0101c2ba (0x1002854): [0]0101be06 [3]0101b825
- Edge manager (DAT_0102503c) ctor 0100b883: grid 10x15, box {+4 edge list, +0xc field list}; raycast 0100b486 (DDA), cek box 0100b3ae, field 0100b316
- Ray (local di ball step 01014956): {x,y,z, dx,dy,dz, len, minDist=0.002, t, dt, mask}

### Loop fisika
- 01014bf9(dt_ms): clamp 100 ms, dt=ms*0.001 → 01014a68(t, dt): per bola: gravitasi/field (0100b316) lalu ≤10 sub-step 01014956
- 01014956: raycast; tidak kena → maju; kena → edge->EdgeCollision(ball, dist), return fraksi waktu terpakai
- `_DAT_010253d0` = kecepatan maks bola (= table+0x106 ball[0]+0x9a * 200)
- 0101ccf3 rotasi vektor: y memakai x yang SUDAH diputar (bug asli, harus dipertahankan)

### Status tahap 2 (selesai 26 Okt 2026)
- Engine: src/engine/{maths,loader,render,timer,collision,component,ball,table}.cpp
- Objek: objects.cpp (flipper, plunger, drain), objects_a.cpp (oneway, bumper, popup/solo target, blocker, gate,
  kicker, flag), objects_b.cpp (rollover, light rollover, tripwire, kickout, hole, ramp, demo, sink), lights.cpp
- Sudah diaudit vs asm: ray_intersect_circle/line, line_init, normalize, basic_collision, box index, timed_frame, gravitasi
- Belum: control (FUN_01011ec9 dkk., 112 fungsi) → tahap 3; sound, text box, component group; FUN_010153ae (replay)
- Uji stres 5 menit (sanitizer): 21 bola, tidak ada crash/NaN/bola lolos

### Status tahap 3–5 (26 Okt 2026)
- Kontrol: src/engine/control/{control,tags,control_c1,control_c2,control_c3}.cpp — 118 fungsi, tabel 88 entri dari exe
- Meja: TPinballTable::Message (new game, ganti pemain, tilt, game over), AddScore, pb (mode, nudge, keydown VK asli)
- Teks: font bitmap PBMSG_FT (resource exe, string 158 "1 UseBitmapFont"), string table 189 teks (dibaca saat runtime oleh src/exe_resources.cpp)
- Audio: src/platform/audio_sdl.cpp (8 kanal LRU 11025 Hz seperti WaveMix), midi_mac.cpp (pinball.mid, AudioToolbox)
- Uji: 5 seed x ±10 menit (sanitizer), ±30 game penuh tanpa crash
- Belum: dialog isi nama high score (pakai nama default "Player N"), menu bar (pakai tombol F2/F3/F5-F10),
  pencocokan dengan rekaman game asli
