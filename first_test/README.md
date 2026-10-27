# first_test: uji coba REA + Ghidra pada program C sendiri

Tujuannya melihat langsung apa yang terjadi saat kode C dikompilasi lalu di-decompile. Di sini sumbernya
kita tahu, sehingga hasil Ghidra bisa dicocokkan dengan kode aslinya. Cara kerja yang sama dipakai untuk
`Pinball.exe`, yang source-nya tidak kita punya.

| File | Isi |
|---|---|
| `main.c` | program asli: struct `Ball`, `normalize()`, `bounce()`, `message()` bergaya Pinball |
| `rebuilt.c` | program yang ditulis ulang **hanya dari hasil decompile** varian stripped |
| `build.sh` | build 3 varian + rebuilt, lalu bandingkan output |

```sh
first_test/build.sh
# arg=''     asli: len=5.0 dir=(0.60, 0.80) speed=8.0 score=1570 | rebuilt: ... score=1570 | SAMA
# arg='1000' asli: ... score=3070 | rebuilt: ... score=3070 | SAMA
# arg='7'    asli: ... score=91   | rebuilt: ... score=91   | SAMA
```

Tiga varian binary:
- `first_test`: `-O0 -g`, masih punya nama fungsi dan debug info.
- `first_test_stripped`: `-O0`, simbol dibuang. Kondisinya sama dengan Pinball.exe yang tidak punya PDB.
- `first_test_O2`: dioptimasi dan di-strip.

---

## 1. Langkah dengan REA (di Claude Code)

```
open_binary(path=".../first_test_stripped", provider_id="ghidra")   → Ghidra mengimpor & menganalisis
list_procedures()                                                     → daftar fungsi
batch_decompile(["0x1000004b0", "0x100000530", "0x1000005d4", "entry"])
close_binary()
```

`list_procedures` pada varian stripped:

| Alamat | Nama dari Ghidra | Sebenarnya (di main.c) |
|---|---|---|
| `0x1000004b0` | `FUN_1000004b0` | `normalize` |
| `0x100000530` | `FUN_100000530` | `bounce` |
| `0x1000005d4` | `FUN_1000005d4` | `message` |
| `0x100000684` | `entry` | `main` |
| (eksternal) | `_atoi`, `_printf`, `___stack_chk_fail` | fungsi pustaka (nama tetap ada karena di-import) |

Nama fungsi buatan kita hilang setelah `strip`. Ghidra menamainya `FUN_<alamat>`, persis seperti 649 fungsi
di Pinball. Nama fungsi pustaka tetap terlihat karena dibutuhkan saat runtime untuk linking.

## 2. Hasil decompile vs kode asli

### normalize: hampir identik
```c
// main.c                                   // Ghidra (FUN_1000004b0)
float normalize(float* v) {                 float FUN_1000004b0(float *param_1) {
    float len = sqrtf(v[0]*v[0]               fVar1 = SQRT(param_1[1] * param_1[1]
                    + v[1]*v[1]);                          + *param_1 * *param_1);
    if (len != 0.0f) {                         if (fVar1 != 0.0) {
        v[0] /= len;                             *param_1 = *param_1 / fVar1;
        v[1] /= len;                             param_1[1] = param_1[1] / fVar1;
    }                                          }
    return len;                                return fVar1;
}                                           }
```
Logikanya utuh. Yang hilang hanya nama variabel. Urutan suku penjumlahan terbalik karena compiler memakai
instruksi `fmadd` (lihat assembly di bawah).

### bounce: struct jadi offset, urutan parameter berubah
```c
// Ghidra (FUN_100000530)
void FUN_100000530(float param_1, float param_2, float param_3, long param_4) {
  fVar1 = *(float *)(param_4 + 0xc) * param_2 + param_1 * *(float *)(param_4 + 8);
  if (fVar1 < 0.0) {
    *(float *)(param_4 + 8)    = *(float *)(param_4 + 8)   - param_1 * fVar1 * 2.0;
    *(float *)(param_4 + 0xc)  = *(float *)(param_4 + 0xc) - param_2 * fVar1 * 2.0;
    *(float *)(param_4 + 0x10) = *(float *)(param_4 + 0x10) * param_3;
  }
}
```
- **Struct hilang:** `b->dx` menjadi `*(float*)(param_4 + 8)`. Dari offset yang diakses (+8, +0xc, +0x10, +0x14),
  layout struct bisa disusun ulang: `{?, ?, dx, dy, speed, score}`.
- **Urutan parameter berubah:** di arm64, pointer dikirim lewat register `x0` dan float lewat `s0..s2`.
  Ghidra menampilkan semua float dulu, baru pointer. Fungsi aslinya `bounce(Ball* b, nx, ny, elasticity)`.
  Pada x86 32-bit seperti Pinball, semua argumen lewat stack sehingga urutannya terjaga.

### message: switch jadi if/else
```c
undefined4 FUN_1000005d4(long param_1, int param_2, int param_3) {
  if (param_2 == 1)          *(int *)(param_1 + 0x14) += param_3;
  else if (param_2 == 2)     *(int *)(param_1 + 0x14) = 0;
  else { if (param_2 != 0x3f6) return 0xffffffff;
         *(int *)(param_1 + 0x14) += param_3 * 10; }
  return *(undefined4 *)(param_1 + 0x14);
}
```
`switch` dengan sedikit case dikompilasi menjadi rangkaian perbandingan. `-1` tampil sebagai `0xffffffff`
karena tipe return tidak diketahui (`undefined4` = 4 byte tanpa tipe). Pola `Message(code, value)` di Pinball
dibaca dengan cara yang sama.

### main (`entry`): konstanta jadi hex, ada artefak decompiler
```c
  local_20 = 0x40400000;   // 3.0f   ← float disimpan sebagai pola bit
  local_1c = 0xc0800000;   // -4.0f
  local_40 = 0x41200000;   // 10.0f
  FUN_100000530(0, 0x3f800000, 0x3f4ccccd, &local_50);        // bounce(&b, 0.0, 1.0, 0.8)
  for (local_5c = 0; local_5c < 3; local_5c++)
    FUN_1000005d4(local_5c + -3, &local_50, 1, local_6c);     // ← argumen pertama palsu (artefak)
  _printf("len=%.1f dir=(%.2f, %.2f) speed=%.1f score=%d\n"); // ← argumen printf hilang
```
| Yang terlihat | Arti |
|---|---|
| `0x40400000`, `0x3f4ccccd` | float dalam bentuk bit; ubah ke angka: 3.0, 0.8 (`struct.unpack('<f', ...)`) |
| `local_5c + -3` sebagai argumen tambahan | Ghidra salah menebak jumlah argumen; cek fungsi tujuannya (hanya 3 parameter) |
| `_printf("...")` tanpa argumen | di macOS arm64, argumen vararg lewat stack dan tidak terlacak decompiler |
| `___stack_chk_fail` | pengaman stack yang ditambahkan compiler, bukan bagian logika program |

Ghidra **tidak selalu benar**. Kalau ada yang janggal, cek assembly. Untuk Pinball ditemukan jebakan serupa:
nilai return x87 hilang, argumen `ftol` hilang, dan `floor(x) - 1` terbaca tanpa `- 1`.

### Assembly normalize (bukti untuk pseudocode di atas)
```
fmul   s2, s2, s3        ; y*y
fmadd  s0, s0, s1, s2    ; x*x + (y*y)
fsqrt  s0, s0            ; len
fcmp   s0, #0.0 ; b.eq   ; if (len != 0)
fdiv   s0, s0, s1        ; v[0] / len
...
ret                      ; len dikembalikan di s0
```

### Varian -O2: fungsi lenyap
```c
undefined8 entry(int param_1, long param_2) {
  if (1 < param_1) _atoi(*(char **)(param_2 + 8));
  _printf("len=%.1f dir=(%.2f, %.2f) speed=%.1f score=%d\n");
  return 0;
}
```
Dengan optimasi, compiler menghitung `normalize` dan `bounce` saat kompilasi (input-nya konstan) dan
menyisipkan `message` ke dalam `main`. Fungsi-fungsinya tidak ada lagi di binary. Kode dari binary
teroptimasi tidak bisa dipetakan satu-satu ke fungsi aslinya. Pinball.exe untungnya dikompilasi dengan
struktur fungsi yang masih utuh (852 prosedur).

## 3. Dari decompile ke kode sendiri (`rebuilt.c`)

Langkahnya sama dengan yang dipakai untuk Pinball:

1. **Petakan fungsi:** beri nama sesuai perilaku (`FUN_1000004b0` → `vec_normalize`).
2. **Susun ulang struct dari offset:** +8/+0xc dipakai bersama sebagai vektor arah, +0x10 dikali faktor 0.8,
   +0x14 ditambah poin → `struct Obj { f0, f4, dir_x, dir_y, speed, score }`.
3. **Terjemahkan konstanta:** `0x3f4ccccd` → `0.8f`, `0x3f6` tetap sebagai kode pesan.
4. **Perbaiki artefak:** buang argumen palsu, kembalikan urutan parameter, lengkapi argumen `printf` dari format string.
5. **Uji kesetaraan:** jalankan program asli dan rekonstruksi dengan input yang sama, lalu bandingkan output.
   Di sini hasilnya **SAMA** untuk 3 input. Untuk Pinball, uji yang sama dilakukan lewat demo terskrip,
   screenshot piksel, dan simulasi game penuh.
