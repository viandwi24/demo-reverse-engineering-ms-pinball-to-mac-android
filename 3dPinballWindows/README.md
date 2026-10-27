# Data game (tidak disertakan)

Folder ini sengaja kosong. File asli **3D Pinball for Windows – Space Cadet** adalah hak cipta pemiliknya,
jadi tidak disertakan di repo ini dan repo ini juga tidak menautkan tempat mengunduhnya.

Salin file dari salinan game **milik Anda sendiri yang diperoleh secara sah** (versi Windows XP, biasanya ada di
`C:\Program Files\Windows NT\Pinball\`) ke folder ini, sehingga isinya menjadi:

```
3dPinballWindows/
  PINBALL.DAT      # wajib: grafis, objek, dan collision meja
  Pinball.exe      # wajib: string table dan font pesan dibaca dari resource-nya saat runtime
  PINBALL.MID      # musik (opsional; tanpa file ini game jalan tanpa musik)
  SOUND*.WAV       # efek suara (opsional; tanpa file ini game jalan tanpa suara)
  FONT.DAT, PINBALL2.MID, table.bmp, wavemix.inf   # ikut terbawa, tidak dipakai port ini
```

Isi folder ini (kecuali README ini) diabaikan oleh `.gitignore`, jadi tidak akan ikut ter-commit.

Versi yang dipakai saat porting (untuk memastikan Anda memakai build yang sama):
- `Pinball.exe` SHA-1 `2a5b525e0f631bb6107639e2a69df15986fb0d05`
- `PINBALL.DAT` SHA-1 `c1a616edb0985cf8aadfb2a1029dadae16cdfcc5`
