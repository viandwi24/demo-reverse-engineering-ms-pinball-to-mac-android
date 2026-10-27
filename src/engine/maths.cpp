#include "maths.h"

#include <cmath>

namespace {
uint32_t g_rand_state = 1;
}

int msvcrt_rand() {
    g_rand_state = g_rand_state * 214013u + 2531011u;
    return static_cast<int>((g_rand_state >> 16) & 0x7fff);
}

void msvcrt_srand(uint32_t seed) { g_rand_state = seed; }

namespace maths {

double normalize_2d(vector2& v) {
    double len = std::sqrt(static_cast<double>(v.x) * v.x + static_cast<double>(v.y) * v.y);
    if (len != 0.0) {
        double inv = 1.0 / len;
        v.x = static_cast<float>(inv * v.x);
        v.y = static_cast<float>(inv * v.y);
    }
    return len;
}

double ray_intersect_circle(const ray_type& ray, const circle_type& c) {
    float dx = c.center.x - ray.origin.x;
    double dy_full = static_cast<double>(c.center.y) - ray.origin.y;
    float dy = static_cast<float>(dy_full);
    double b = static_cast<double>(dx) * ray.direction.x + dy_full * ray.direction.y;
    if (b >= 0.0) {
        double dist_sq = static_cast<double>(dx) * dx + static_cast<double>(dy) * dy;
        if (dist_sq < c.radius_sq)  // bola di dalam lingkaran: jarak negatif
            return b - std::sqrt(b * b + (c.radius_sq - dist_sq));
        double disc = b * b + (c.radius_sq - dist_sq);
        if (disc >= 0.0) {
            double t = b - std::sqrt(disc);
            if (t >= 0.0 && t <= ray.max_distance) return t;
        }
    }
    return 1e9;
}

void line_init(line_type& line, float x0, float y0, float x1, float y1) {
    line.direction.x = x1 - x0;
    line.direction.y = y1 - y0;
    vector2 dir{line.direction.x, line.direction.y};
    normalize_2d(dir);
    line.direction.x = dir.x;
    line.direction.y = dir.y;
    line.perpendicular.x = line.direction.y;
    line.perpendicular.y = -line.direction.x;
    line.pre_comp1 = static_cast<float>(-(static_cast<double>(line.direction.y) * x0) -
                                        (-static_cast<double>(line.direction.x) * y0));
    bool ascending;
    if (line.direction.x >= 1e-9 || line.direction.x <= -1e-9) {
        ascending = x0 < x1;
        y0 = x0;
        y1 = x1;
    } else {
        line.direction.x = 0.0f;
        ascending = y0 < y1;
    }
    if (ascending) {
        line.origin = y0;
        line.end = y1;
    } else {
        line.origin = y1;
        line.end = y0;
    }
}

double ray_intersect_line(const ray_type& ray, line_type& line) {
    double perp_dot = static_cast<double>(ray.direction.x) * line.perpendicular.x +
                      static_cast<double>(ray.direction.y) * line.perpendicular.y;
    if (perp_dot < 0.0) {
        double t = -((static_cast<double>(line.perpendicular.y) * ray.origin.y +
                      static_cast<double>(line.perpendicular.x) * ray.origin.x + line.pre_comp1) /
                     perp_dot);
        if (-static_cast<double>(ray.min_distance) <= t && t <= ray.max_distance) {
            line.ray_intersect.x = static_cast<float>(t * ray.direction.x + ray.origin.x);
            double hit_y = t * ray.direction.y + ray.origin.y;
            line.ray_intersect.y = static_cast<float>(hit_y);
            if (line.direction.x == 0.0f) {
                if (hit_y >= line.origin && hit_y <= line.end) return t;
            } else if (line.origin <= line.ray_intersect.x && line.ray_intersect.x <= line.end) {
                return t;
            }
        }
    }
    return 1e9;
}

void cross(const vector3& a, const vector3& b, vector3& out) {
    out.x = static_cast<float>(static_cast<double>(b.z) * a.y - static_cast<double>(b.y) * a.z);
    out.y = static_cast<float>(-(static_cast<double>(a.x) * b.z - static_cast<double>(b.x) * a.z));
    out.z = static_cast<float>(static_cast<double>(a.x) * b.y - static_cast<double>(b.x) * a.y);
}

double magnitude(const vector3& v) {
    double sq = static_cast<double>(v.z) * v.z + static_cast<double>(v.y) * v.y + static_cast<double>(v.x) * v.x;
    return sq == 0.0 ? 0.0 : std::sqrt(sq);
}

void vector_add(vector2& a, const vector2& b) {
    a.x += b.x;
    a.y = b.y + a.y;
}

void rotate_around(vector2& p, float sin_a, float cos_a, const vector2& center) {
    float x = p.x;
    p.x = static_cast<float>(((static_cast<double>(x) - center.x) * cos_a -
                              (static_cast<double>(p.y) - center.y) * sin_a) + center.x);
    p.y = static_cast<float>((static_cast<double>(p.y) - center.y) * cos_a +
                             (static_cast<double>(x) - center.x) * sin_a + center.y);
}

double distance(const vector2& a, const vector2& b) {
    double dx = static_cast<double>(a.x) - b.x, dy = static_cast<double>(a.y) - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

double distance_sq(float ax, float ay, float bx, float by) {
    double dx = static_cast<double>(ax) - bx, dy = static_cast<double>(ay) - by;
    return dx * dx + dy * dy;
}

double dot(const vector2& a, const vector2& b) {
    return static_cast<double>(a.x) * b.x + static_cast<double>(a.y) * b.y;
}

void sincos(float angle, float& s, float& c) {
    s = static_cast<float>(std::sin(static_cast<double>(angle)));
    c = static_cast<float>(std::cos(static_cast<double>(angle)));
}

void rotate_vector_buggy(vector2& v, float angle) {
    double s = std::sin(static_cast<double>(angle));
    double c = std::cos(static_cast<double>(angle));
    double new_x = c * v.x - s * v.y;
    v.x = static_cast<float>(new_x);
    // Asli memakai x yang sudah diputar (bukan x lama) untuk menghitung y.
    v.y = static_cast<float>(c * v.y + new_x * s);
}

}  // namespace maths

namespace proj {
namespace {
float g_matrix[16];  // 0x01028160, baris ke-4 = {0,0,0,1}
float g_focal;       // 0x01028140
float g_center_x;    // 0x0102813c
float g_center_y;    // 0x01028138

void transform(const vector3& v, double out[3]) {
    // FUN_01008d4e: hasil disimpan ke float lokal
    float r[3];
    for (int i = 0; i < 3; ++i)
        r[i] = static_cast<float>(static_cast<double>(v.x) * g_matrix[i * 4] + static_cast<double>(v.y) * g_matrix[i * 4 + 1] +
                                  static_cast<double>(v.z) * g_matrix[i * 4 + 2] + g_matrix[i * 4 + 3]);
    for (int i = 0; i < 3; ++i) out[i] = r[i];
}
}  // namespace

void init(const float m[12], float focal, float center_x, float center_y) {
    for (int i = 0; i < 12; ++i) g_matrix[i] = m[i];
    g_matrix[12] = g_matrix[13] = g_matrix[14] = 0.0f;
    g_matrix[15] = 1.0f;
    g_focal = focal;
    g_center_x = center_x;
    g_center_y = center_y;
}

void set_center(float x, float y) {
    g_center_x = x;
    g_center_y = y;
}

double z_distance(const vector3& v) {
    double t[3];
    transform(v, t);
    vector3 tv{static_cast<float>(t[0]), static_cast<float>(t[1]), static_cast<float>(t[2])};
    return maths::magnitude(tv);
}

void xform_to_2d(const vector3& v, int out[2]) {
    double t[3];
    transform(v, t);
    double scale = t[2] == 0.0 ? 999999.875 : static_cast<double>(g_focal) / t[2];
    out[0] = ftol(t[0] * scale + g_center_x);
    out[1] = ftol(t[1] * scale + g_center_y);
}

}  // namespace proj
