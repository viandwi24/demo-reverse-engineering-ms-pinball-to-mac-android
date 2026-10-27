// Matematika vektor, raycast, dan proyeksi kamera.
// Port dari blok 0x01008ae9-0x01008f03 dan 0x0101cb8e-0x0101ccf3 Pinball.exe.
//
// Presisi: exe asli menyetel x87 ke presisi 53-bit (_controlfp(0x10000, 0x30000) @0x010211b9),
// jadi semua nilai sementara di register FPU setara `double`. Nilai yang disimpan ke memori
// (field struct atau variabel lokal float) dibulatkan ke `float`. Kode di sini meniru pola itu.
#pragma once

#include <cstdint>

struct vector2 {
    float x, y;
};

struct vector3 {
    float x, y, z;
};

// Layout sama dengan struct ray di stack FUN_01014956.
struct ray_type {
    vector3 origin;
    vector3 direction;
    float max_distance;
    float min_distance;  // 0.002 (0x3b03126f)
    float time_now;
    float time_delta;
    int field_flag;
};

struct circle_type {
    vector3 center;
    float radius_sq;
};

struct line_type {
    vector3 perpendicular;  // normal (dir.y, -dir.x)
    vector3 direction;
    float pre_comp1;  // offset bidang: -(n.x*x0) - n.y*y0
    float origin;     // batas min (x, atau y bila garis vertikal)
    float end;        // batas max
    vector2 ray_intersect;
};

// _ftol MSVC: truncation menuju nol lewat int64, lalu dipakai 32 bit bawahnya.
inline int32_t ftol(double v) { return static_cast<int32_t>(static_cast<int64_t>(v)); }

// msvcrt rand(): LCG 214013/2531011, seed awal 1 (exe tidak pernah memanggil srand).
int msvcrt_rand();
// Skala rand() → [0,1]: konstanta double 1/32767 di exe (0x01001a28).
constexpr double kRandScale = 3.051850947599719e-05;
void msvcrt_srand(uint32_t seed);

namespace maths {

double normalize_2d(vector2& v);                                       // 01008ae9 (return panjang)
double ray_intersect_circle(const ray_type& ray, const circle_type& c);  // 01008b34
void line_init(line_type& line, float x0, float y0, float x1, float y1);  // 01008beb
double ray_intersect_line(const ray_type& ray, line_type& line);         // 01008c8f
void cross(const vector3& a, const vector3& b, vector3& out);            // 01008db8
double magnitude(const vector3& v);                                    // 01008dff
void vector_add(vector2& a, const vector2& b);                         // 0101cb8e
void rotate_around(vector2& p, float sin_a, float cos_a, const vector2& center);  // 0101cbb1
double distance(const vector2& a, const vector2& b);                   // 0101cbf5
double distance_sq(float ax, float ay, float bx, float by);            // 0101cc23
double dot(const vector2& a, const vector2& b);                        // 0101cc4b
void sincos(float angle, float& s, float& c);                          // 0101cc6b
void rotate_vector_buggy(vector2& v, float angle);                     // 0101ccf3

}  // namespace maths

// Kamera/proyeksi 3D → layar (global 0x01028138-0x0102819c).
namespace proj {

void init(const float matrix3x4[12], float focal, float center_x, float center_y);  // 01008e4b
void set_center(float x, float y);                                                    // 01008eba
double z_distance(const vector3& v);                                                  // 01008ed8
void xform_to_2d(const vector3& v, int out[2]);                                       // 01008f03

}  // namespace proj
