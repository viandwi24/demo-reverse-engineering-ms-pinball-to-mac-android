// Program uji kecil untuk melihat bagaimana Ghidra/REA mengembalikan kode C dari binary.
// Sengaja meniru pola yang ada di Pinball: struct, float, switch "Message", dan loop.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    float x, y;     // posisi
    float dx, dy;   // arah
    float speed;
    int score;
} Ball;

// Normalisasi vektor, mengembalikan panjang awalnya (mirip FUN_01008ae9 di Pinball).
float normalize(float* v) {
    float len = sqrtf(v[0] * v[0] + v[1] * v[1]);
    if (len != 0.0f) {
        v[0] /= len;
        v[1] /= len;
    }
    return len;
}

// Pantulkan bola terhadap dinding dengan normal (nx, ny) dan elastisitas.
void bounce(Ball* b, float nx, float ny, float elasticity) {
    float dot = b->dx * nx + b->dy * ny;
    if (dot < 0.0f) {
        b->dx -= 2.0f * dot * nx;
        b->dy -= 2.0f * dot * ny;
        b->speed *= elasticity;
    }
}

// "Message" bergaya Pinball: kode angka menentukan aksi.
int message(Ball* b, int code, int value) {
    switch (code) {
    case 1:
        b->score += value;
        break;
    case 2:
        b->score = 0;
        break;
    case 0x3f6:
        b->score += value * 10;
        break;
    default:
        return -1;
    }
    return b->score;
}

int main(int argc, char** argv) {
    Ball b = {0.0f, 0.0f, 3.0f, -4.0f, 10.0f, 0};
    float v[2] = {b.dx, b.dy};
    float len = normalize(v);
    b.dx = v[0];
    b.dy = v[1];
    bounce(&b, 0.0f, 1.0f, 0.8f);
    int bonus = argc > 1 ? atoi(argv[1]) : 500;
    for (int i = 0; i < 3; ++i) message(&b, 1, bonus);
    message(&b, 0x3f6, 7);
    printf("len=%.1f dir=(%.2f, %.2f) speed=%.1f score=%d\n", len, b.dx, b.dy, b.speed, b.score);
    return 0;
}
