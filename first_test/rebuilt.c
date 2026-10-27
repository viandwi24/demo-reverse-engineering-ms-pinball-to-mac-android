// Rekonstruksi dari hasil decompile first_test_stripped (Ghidra lewat REA), TANPA melihat main.c.
// Nama fungsi dan struct diberi berdasarkan perilaku; offset dari pseudocode.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// Struct dari offset yang diakses: +0/+4 (tidak dibaca di fungsi), +8, +0xc, +0x10, +0x14
typedef struct {
    float f0, f4;       // +0x0, +0x4: hanya diinisialisasi 0 di entry (posisi?)
    float dir_x;        // +0x8
    float dir_y;        // +0xc
    float speed;        // +0x10: dikali param_3 (faktor 0.8 → elastisitas)
    int score;          // +0x14
} Obj;

// FUN_1000004b0: sqrt(x*x + y*y), bagi kedua komponen bila != 0, kembalikan panjang
static float vec_normalize(float* v) {
    float len = sqrtf(v[1] * v[1] + v[0] * v[0]);
    if (len != 0.0f) {
        v[0] = v[0] / len;
        v[1] = v[1] / len;
    }
    return len;
}

// FUN_100000530(float, float, float, long): di arm64 argumen float lewat s0..s2 dan pointer lewat x0,
// Ghidra menampilkannya berurutan per jenis register. Urutan asli kemungkinan (obj, nx, ny, factor).
static void reflect(Obj* o, float nx, float ny, float factor) {
    float d = o->dir_y * ny + nx * o->dir_x;
    if (d < 0.0f) {
        o->dir_x = o->dir_x - nx * d * 2.0f;
        o->dir_y = o->dir_y - ny * d * 2.0f;
        o->speed = o->speed * factor;
    }
}

// FUN_1000005d4: switch kode pesan → ubah +0x14, kembalikan nilainya atau -1
static int handle_message(Obj* o, int code, int value) {
    if (code == 1)
        o->score += value;
    else if (code == 2)
        o->score = 0;
    else if (code == 0x3f6)
        o->score += value * 10;
    else
        return -1;
    return o->score;
}

// entry: konstanta hex float → 3.0 (0x40400000), -4.0 (0xc0800000), 10.0 (0x41200000), 1.0, 0.8 (0x3f4ccccd)
int main(int argc, char** argv) {
    Obj o = {0.0f, 0.0f, 3.0f, -4.0f, 10.0f, 0};
    float v[2] = {3.0f, -4.0f};
    float len = vec_normalize(v);
    o.dir_x = v[0];
    o.dir_y = v[1];
    reflect(&o, 0.0f, 1.0f, 0.8f);
    int bonus = argc < 2 ? 500 : atoi(argv[1]);
    for (int i = 0; i < 3; ++i) handle_message(&o, 1, bonus);
    handle_message(&o, 0x3f6, 7);
    // Argumen printf hilang di decompile (vararg arm64 lewat stack); diisi dari format string + variabel yang tersedia.
    printf("len=%.1f dir=(%.2f, %.2f) speed=%.1f score=%d\n", len, o.dir_x, o.dir_y, o.speed, o.score);
    return 0;
}
