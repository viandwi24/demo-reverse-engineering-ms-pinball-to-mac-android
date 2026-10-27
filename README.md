# Space Cadet Pinball — port Mac & Android

Port native **3D Pinball for Windows – Space Cadet** (versi Windows XP) ke macOS dan Android.
Kodenya ditulis ulang dari hasil reverse engineering `Pinball.exe` (REA + Ghidra), dengan target
perilaku yang sama dengan aslinya. Riwayat lengkap dan tahapannya ada di [JOURNEY.md](JOURNEY.md).

> [!IMPORTANT]
> **Disclaimer: hanya untuk tujuan edukasi dan riset.**
> - Proyek ini dibuat untuk mempelajari reverse engineering dan porting. Ini bukan produk resmi dan tidak
>   berafiliasi dengan, disponsori, atau didukung oleh Microsoft, Maxis, Cinematronics, atau Electronic Arts.
> - "3D Pinball", "Space Cadet", "Windows", dan merek lain adalah milik pemiliknya masing-masing.
>   Nama-nama itu dipakai hanya untuk menjelaskan program yang dipelajari.
> - Repo ini **tidak memuat aset atau file asli game sama sekali**: tidak ada `Pinball.exe`, grafis, suara, musik,
>   teks, font, maupun hasil decompile mentah. Repo ini juga tidak menyediakan atau menautkan tempat mengunduhnya.
> - Untuk menjalankannya, Anda harus memakai salinan game **milik Anda sendiri yang diperoleh secara sah**
>   (misalnya dari instalasi Windows XP berlisensi). Data itu dibaca saat runtime dari folder lokal Anda.
> - Perangkat lunak disediakan "apa adanya" tanpa jaminan. Pengguna bertanggung jawab memastikan pemakaiannya
>   sesuai dengan hukum dan lisensi yang berlaku di wilayahnya.
> - Pemilik hak yang keberatan dengan isi repo ini dapat membuka issue, dan isi tersebut akan segera ditinjau atau dihapus.

## Persiapan
Data game tidak disertakan. Salin file dari salinan game milik Anda sendiri ke `3dPinballWindows/`.
Daftar file yang dibutuhkan ada di [3dPinballWindows/README.md](3dPinballWindows/README.md).

## Mac
```sh
brew install sdl2-compat cmake
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j8
./build/pinball            # dijalankan dari root repo (membaca ./3dPinballWindows)
```
Tombol: Z / tanda / = flipper, spasi = plunger, X . ↑ = dorong meja, F2 game baru, F3 pause,
F5–F8 jumlah pemain, F9 suara, F10 musik, Esc keluar.

## Android
Butuh Android SDK + NDK 29 + JDK 21 (tanpa Gradle). Jalankan emulator atau sambungkan perangkat, lalu:
```sh
android/build.sh --install
```
Sentuh: sepertiga kiri/kanan = flipper, tengah bawah = plunger (tahan lalu lepas), tengah atas = game baru.

## Struktur
- `src/engine/`: engine portabel (fisika, render, objek meja, aturan game di `control/`)
- `src/platform/`, `src/main.cpp`: lapisan Mac (SDL, AudioToolbox)
- `android/`: lapisan Android (NativeActivity, AAudio) + skrip build
- `src/exe_resources.*`: membaca string table dan font pesan dari `Pinball.exe` milik pengguna saat runtime
- `re/`: catatan reverse engineering; `tools/`: skrip bantu (DAT, konstanta exe, Ghidra)
- `first_test/`: contoh kecil cara kerja REA + Ghidra pada program C sendiri (decompile → rekonstruksi → uji)
