#include "objects.h"

#include <cmath>

#include "ball.h"
#include "table.h"
#include "timer.h"

namespace {

// Geometri flipper global (dihitung ulang setiap kali dipakai), 0x0102565c-0x0102571c.
struct FlipperGeometry {
    float sin_a, cos_a;     // 0102565c, 010256b0
    circle_type circlebase; // 01025660
    circle_type circle_t1;  // 01025670
    line_type line_a;       // 01025680
    line_type line_b;       // 010256c0
    vector3 a1, a2, b1, b2, t1;  // 01025714, 010256b4, 010256f0, 010256fc, 01025708
} g_flip;

}  // namespace

// ---- TFlipperEdge -----------------------------------------------------------

TFlipperEdge::TFlipperEdge(TCollisionComponent* owner, char* active_flag, unsigned collision_group, TPinballTable* table,
                           const float* origin, const float* vec_t1, const float* vec_t2, float extend_t,
                           float retract_t, float coll_mult, float elast, float smooth)
    : TEdgeSegment(owner, active_flag, collision_group) {
    elasticity = elast;
    smoothness = smooth;
    extend_time = extend_t;
    retract_time = retract_t;
    collision_mult = coll_mult;

    vector3 t1{vec_t1[0], vec_t1[1], vec_t1[2]};
    vector3 t2{vec_t2[0], vec_t2[1], vec_t2[2]};
    circle_t1_radius = vec_t1[2] + table->collision_comp_offset;
    circlebase_radius = origin[2] + table->collision_comp_offset;
    t1_src = t1;
    t2_src = t2;
    float t1_m = static_cast<float>(circle_t1_radius * 1.01);
    float base_m = static_cast<float>(circlebase_radius * 1.01);
    circle_t1_radius_m_sq = t1_m * t1_m;
    circlebase_radius_m_sq = base_m * base_m;
    rot_origin.x = origin[0];
    rot_origin.y = origin[1];
    circle_t1_radius_sq = circle_t1_radius * circle_t1_radius;
    circlebase_radius_sq = circlebase_radius * circlebase_radius;

    vector2 dir1{t1.x - origin[0], t1.y - origin[1]};
    maths::normalize_2d(dir1);
    vector2 dir2{t2.x - origin[0], t2.y - origin[1]};
    maths::normalize_2d(dir2);
    angle_max = static_cast<float>(std::acos(maths::dot(dir1, dir2)));
    vector3 c;
    maths::cross(vector3{dir1.x, dir1.y, 0.0f}, vector3{dir2.x, dir2.y, 0.0f}, c);
    if (c.z < 0.0f) angle_max = -angle_max;
    flipper_flag = 0;
    angle_dst = 0.0f;

    vector2 d{t1.x - origin[0], t1.y - origin[1]};
    maths::normalize_2d(d);
    double ndy = -static_cast<double>(d.y);
    a2_src.x = static_cast<float>(ndy * circlebase_radius + origin[0]);
    a2_src.y = static_cast<float>(static_cast<double>(d.x) * circlebase_radius + origin[1]);
    a1_src.x = static_cast<float>(ndy * circle_t1_radius + t1.x);
    a1_src.y = static_cast<float>(static_cast<double>(d.x) * circle_t1_radius + t1.y);
    double px = d.y, py = -static_cast<double>(d.x);
    b1_src.x = static_cast<float>(px * circlebase_radius + origin[0]);
    b1_src.y = static_cast<float>(py * circlebase_radius + origin[1]);
    b2_src.x = static_cast<float>(px * circle_t1_radius + t1.x);
    b2_src.y = static_cast<float>(py * circle_t1_radius + t1.y);
    if (angle_max < 0.0f) {
        std::swap(a1_src, b1_src);
        std::swap(a2_src, b2_src);
    }

    double dx = static_cast<double>(t1.x) - rot_origin.x, dy = static_cast<double>(t1.y) - rot_origin.y;
    double div = std::sqrt(dx * dx + dy * dy) + table->collision_comp_offset + t1.z;
    distance_div_sq = static_cast<float>(div * div);

    double min_time = extend_time < retract_time ? extend_time : retract_time;
    vector2 v1{t1.x, t1.y}, v2{t2.x, t2.y};
    double dist = maths::distance(v1, v2) / circle_t1_radius;
    collision_time_advance = static_cast<float>(min_time / (dist + dist));

    place_in_grid();
}

double TFlipperEdge::GetCurrentAngle(float time) {
    if (flipper_flag == 0) return angle_dst;
    double t = ((static_cast<double>(angle_dst) - angle_src) / angle_max) * flipper_move_time;
    if (t < 0.0) t = -t;
    t = t >= 1e-07 ? (static_cast<double>(time) - input_time) / t : 1.0;
    if (t < 0.0) t = 0.0;
    if (1.0 < t) t = 1.0;
    if (flipper_flag == 2) t = 1.0 - t;
    return t * angle_max;
}

void TFlipperEdge::set_angle(float time) {
    maths::sincos(static_cast<float>(GetCurrentAngle(time)), g_flip.sin_a, g_flip.cos_a);
    g_flip.a1 = a1_src;
    g_flip.a2 = a2_src;
    g_flip.b1 = b1_src;
    g_flip.b2 = b2_src;
    g_flip.t1 = t1_src;
    vector2 o{rot_origin.x, rot_origin.y};
    for (vector3* p : {&g_flip.a1, &g_flip.a2, &g_flip.t1, &g_flip.b1, &g_flip.b2}) {
        vector2 v{p->x, p->y};
        maths::rotate_around(v, g_flip.sin_a, g_flip.cos_a, o);
        p->x = v.x;
        p->y = v.y;
    }
}

void TFlipperEdge::build_geometry() {
    maths::line_init(g_flip.line_a, g_flip.a1.x, g_flip.a1.y, g_flip.a2.x, g_flip.a2.y);
    maths::line_init(g_flip.line_b, g_flip.b1.x, g_flip.b1.y, g_flip.b2.x, g_flip.b2.y);
    g_flip.circlebase.radius_sq = circlebase_radius_sq;
    g_flip.circlebase.center.x = rot_origin.x;
    g_flip.circlebase.center.y = rot_origin.y;
    g_flip.circle_t1.radius_sq = circle_t1_radius_sq;
    g_flip.circle_t1.center.x = g_flip.t1.x;
    g_flip.circle_t1.center.y = g_flip.t1.y;
}

int TFlipperEdge::is_ball_inside(float x, float y) {
    const auto& f = g_flip;
    auto lt0 = [](double v) { return v < 0.0; };
    bool outside_quad =
        lt0((static_cast<double>(f.a2.x) - f.a1.x) * (static_cast<double>(y) - f.a1.y) -
            (static_cast<double>(f.a2.y) - f.a1.y) * (static_cast<double>(x) - f.a1.x)) ||
        lt0((static_cast<double>(y) - f.a2.y) * (static_cast<double>(f.b1.x) - f.a2.x) -
            (static_cast<double>(x) - f.a2.x) * (static_cast<double>(f.b1.y) - f.a2.y)) ||
        lt0((static_cast<double>(y) - f.b1.y) * (static_cast<double>(f.b2.x) - f.b1.x) -
            (static_cast<double>(x) - f.b1.x) * (static_cast<double>(f.b2.y) - f.b1.y)) ||
        lt0((static_cast<double>(y) - f.b2.y) * (static_cast<double>(f.a1.x) - f.b2.x) -
            (static_cast<double>(x) - f.b2.x) * (static_cast<double>(f.a1.y) - f.b2.y));
    if (outside_quad) {
        float dx = rot_origin.x - x, dy = rot_origin.y - y;
        float d_sq = dy * dy + dx * dx;
        double t1_sq = (static_cast<double>(f.t1.y) - y) * (static_cast<double>(f.t1.y) - y) +
                       (static_cast<double>(f.t1.x) - x) * (static_cast<double>(f.t1.x) - x);
        if (d_sq > circlebase_radius_sq && circle_t1_radius_sq <= t1_sq) return 0;
    }

    double side;
    if (angle_max < 0.0f) {
        const vector3* p = flipper_flag == 1 ? &f.b1 : flipper_flag == 2 ? &f.a2 : &f.t1;
        side = (static_cast<double>(y) - p->y) * (static_cast<double>(rot_origin.x) - p->x) -
               (static_cast<double>(x) - p->x) * (static_cast<double>(rot_origin.y) - p->y);
        return 0.0 <= side ? 4 : 5;
    }
    if (flipper_flag == 1) {
        side = (static_cast<double>(y) - f.b2.y) * (static_cast<double>(rot_origin.x) - f.b2.x) -
               (static_cast<double>(x) - f.b2.x) * (static_cast<double>(rot_origin.y) - f.b2.y);
    } else if (flipper_flag == 2) {
        side = (static_cast<double>(rot_origin.x) - f.a1.x) * (static_cast<double>(y) - f.a1.y) -
               (static_cast<double>(rot_origin.y) - f.a1.y) * (static_cast<double>(x) - f.a1.x);
    } else {
        side = (static_cast<double>(y) - f.t1.y) * (static_cast<double>(rot_origin.x) - f.t1.x) -
               (static_cast<double>(x) - f.t1.x) * (static_cast<double>(rot_origin.y) - f.t1.y);
    }
    return side < 0.0 ? 4 : 5;
}

double TFlipperEdge::flipper_calc(ray_type& ray, ray_type* out) {
    float best = 1e9f;
    int which = -1;
    double d = maths::ray_intersect_line(ray, g_flip.line_a);
    if (d < 1e9) {
        best = static_cast<float>(d);
        which = 0;
    }
    d = maths::ray_intersect_circle(ray, g_flip.circlebase);
    if (d < best) {
        best = static_cast<float>(d);
        which = 2;
    }
    d = maths::ray_intersect_circle(ray, g_flip.circle_t1);
    if (d < best) {
        best = static_cast<float>(d);
        which = 3;
    }
    d = maths::ray_intersect_line(ray, g_flip.line_b);
    if (d < best) {
        best = static_cast<float>(d);
        which = 1;
    }
    if (!out || !(best < 1e9f)) return best;
    if (which == -1) return 1e9;
    if (which == 0 || which == 1) {
        const line_type& l = which == 0 ? g_flip.line_a : g_flip.line_b;
        out->direction = l.perpendicular;
        out->origin = {l.ray_intersect.x, l.ray_intersect.y, out->origin.z};
    } else {
        const circle_type& c = which == 2 ? g_flip.circlebase : g_flip.circle_t1;
        out->origin.x = static_cast<float>(static_cast<double>(best) * ray.direction.x + ray.origin.x);
        out->origin.y = static_cast<float>(static_cast<double>(best) * ray.direction.y + ray.origin.y);
        vector2 n{out->origin.x - c.center.x, out->origin.y - c.center.y};
        maths::normalize_2d(n);
        out->direction.x = n.x;
        out->direction.y = n.y;
    }
    return best;
}

double TFlipperEdge::FindCollisionDistance(ray_type& ray) {
    if (move_end_time < ray.time_now) flipper_flag = 0;
    if (edge_collision_flag) {
        edge_collision_flag = 0;
        return 1e9;
    }

    ray_type out{};
    ray_type r2{};
    r2.min_distance = ray.min_distance;
    auto set_contact = [&](const vector2& dir) {
        next_ball_position = out.origin;
        next_ball_position.x = static_cast<float>(next_ball_position.x - static_cast<double>(dir.x) * 1e-05);
        next_ball_position.y = static_cast<float>(next_ball_position.y - static_cast<double>(dir.y) * 1e-05);
    };

    if (flipper_flag == 0) {
        edge_collision_flag2 = 0;
        edge_collision_flag3 = 0;
        set_angle(ray.time_now);
        build_geometry();
        int side = is_ball_inside(ray.origin.x, ray.origin.y);
        if (side == 0) {
            r2.origin = ray.origin;
            r2.direction = ray.direction;
            r2.max_distance = ray.max_distance;
            r2.min_distance = ray.min_distance;
            double d = flipper_calc(r2, &out);
            if (d == 0.0) {
                set_contact(vector2{r2.direction.x, r2.direction.y});
            } else {
                next_ball_position = out.origin;
            }
            collision_direction = out.direction;
            return d;
        }

        vector2 dir;
        if (maths::distance_sq(ray.origin.x, ray.origin.y, rot_origin.x, rot_origin.y) < circlebase_radius_m_sq) {
            dir = {rot_origin.x - ray.origin.x, rot_origin.y - ray.origin.y};
            maths::normalize_2d(dir);
        } else if (maths::distance_sq(ray.origin.x, ray.origin.y, g_flip.t1.x, g_flip.t1.y) < circle_t1_radius_m_sq) {
            dir = {g_flip.t1.x - ray.origin.x, g_flip.t1.y - ray.origin.y};
            maths::normalize_2d(dir);
        } else {
            const line_type& l = side == 4 ? g_flip.line_a : g_flip.line_b;
            dir = {-l.perpendicular.x, -l.perpendicular.y};
        }
        r2.direction = {dir.x, dir.y, 0.0f};
        r2.origin.x = static_cast<float>(ray.origin.x - static_cast<double>(dir.x) * 5.0);
        r2.origin.y = static_cast<float>(ray.origin.y - static_cast<double>(dir.y) * 5.0);
        r2.max_distance = static_cast<float>(ray.max_distance + 10.0);
        if (flipper_calc(r2, &out) >= 1e9) {
            dir = {rot_origin.x - ray.origin.x, rot_origin.y - ray.origin.y};
            maths::normalize_2d(dir);
            r2.direction = {dir.x, dir.y, 0.0f};
            r2.origin.x = static_cast<float>(ray.origin.x - static_cast<double>(dir.x) * 5.0);
            r2.origin.y = static_cast<float>(ray.origin.y - static_cast<double>(dir.y) * 5.0);
            if (flipper_calc(r2, &out) >= 1e9) return 1e9;
        }
        collision_direction = out.direction;
        set_contact(dir);
        return 0.0;
    }

    // Flipper sedang bergerak: periksa sepanjang waktu langkah ini.
    float px = ray.origin.x, py = ray.origin.y;
    float t = ray.time_now;
    float len = collision_time_advance * ray.max_distance;
    float step_x = collision_time_advance * ray.direction.x;
    float step_y = ray.direction.y * collision_time_advance;
    while (t < ray.time_delta + ray.time_now) {
        set_angle(t);
        build_geometry();
        int side = is_ball_inside(px, py);
        if (side != 0) {
            const line_type* face;
            if (flipper_flag == 1 && side != 5) {
                face = &g_flip.line_a;
            } else if (flipper_flag != 2 || side == 4) {
                vector2 dir{rot_origin.x - px, rot_origin.y - py};
                edge_collision_flag2 = 0;
                edge_collision_flag3 = 1;
                maths::normalize_2d(dir);
                r2.direction = {dir.x, dir.y, 0.0f};
                r2.origin.x = static_cast<float>(px - static_cast<double>(dir.x) * 5.0);
                r2.origin.y = static_cast<float>(py - static_cast<double>(dir.y) * 5.0);
                r2.max_distance = static_cast<float>(ray.max_distance + 10.0);
                if (flipper_calc(r2, &out) < 1e9) {
                    collision_direction = out.direction;
                    set_contact(dir);
                    return 0.0;
                }
                next_ball_position.x = px;
                collision_direction.x = -dir.x;
                next_ball_position.y = py;
                collision_direction.y = -dir.y;
                return 0.0;
            } else {
                face = &g_flip.line_b;
            }
            vector2 dir{-face->perpendicular.x, -face->perpendicular.y};
            face_normal = face->perpendicular;
            edge_collision_flag3 = 0;
            edge_collision_flag2 = 1;
            r2.min_distance = 0.002f;
            r2.direction = {dir.x, dir.y, 0.0f};
            r2.origin.x = static_cast<float>(ray.origin.x - static_cast<double>(dir.x) * 5.0);
            r2.origin.y = static_cast<float>(ray.origin.y - static_cast<double>(dir.y) * 5.0);
            r2.max_distance = static_cast<float>(ray.max_distance + 10.0);
            double d = flipper_calc(r2, &out);
            collision_direction = out.direction;
            if (d < 1e9) {
                set_contact(dir);
                return 0.0;
            }
            break;
        }

        r2.direction = ray.direction;
        r2.min_distance = ray.min_distance;
        r2.origin = ray.origin;
        r2.max_distance = len;
        double d = flipper_calc(r2, &out);
        if (d < 1e9) {
            set_contact(vector2{r2.direction.x, r2.direction.y});
            float cx = next_ball_position.x, cy = next_ball_position.y;
            double cross = (static_cast<double>(cy) - g_flip.t1.y) * (static_cast<double>(rot_origin.x) - g_flip.t1.x) -
                           (static_cast<double>(cx) - g_flip.t1.x) * (static_cast<double>(rot_origin.y) - g_flip.t1.y);
            int on_face = cross < 0.0 ? (angle_max <= 0.0f ? 0 : 1) : (angle_max <= 0.0f ? 1 : 0);
            const line_type* face;
            if (flipper_flag == 2) {
                face = &g_flip.line_b;
                edge_collision_flag2 = on_face == 0;
            } else {
                edge_collision_flag2 = on_face;
                face = &g_flip.line_a;
            }
            face_normal = face->perpendicular;
            collision_direction = out.direction;
            return d;
        }
        t = t + collision_time_advance;
        px = px + step_x;
        py = py + step_y;
    }
    return 1e9;
}

void TFlipperEdge::EdgeCollision(TBall* ball, float) {
    edge_collision_flag = 1;
    vector2 pos{next_ball_position.x, next_ball_position.y};
    vector2 dir{collision_direction.x, collision_direction.y};
    double boost = 0.0;
    if (flipper_flag != 0) {
        double dx = static_cast<double>(next_ball_position.x) - rot_origin.x;
        double dy = static_cast<double>(next_ball_position.y) - rot_origin.y;
        float d_sq = static_cast<float>(dx * dx + dy * dy);
        if (edge_collision_flag2 == 0) {
            if (edge_collision_flag3 != 0) {
                float e = g_flip.circlebase.radius_sq * 1.01 < d_sq
                              ? static_cast<float>((1.0 - std::sqrt(d_sq / distance_div_sq)) * elasticity)
                              : elasticity;
                basic_collision(ball, &pos, &dir, e, smoothness, 1e9f, 0.0f);
                return;
            }
        } else if (g_flip.circlebase.radius_sq * 1.01 < d_sq) {
            vector2 face{face_normal.x, face_normal.y};
            double dot = maths::dot(face, dir);
            boost = dot >= 0.0 ? dot * ((std::fabs(angle_max) / flipper_move_time) * std::sqrt(d_sq / distance_div_sq)) : 0.0;
            boost = boost * collision_mult;
        }
    }
    float threshold = boost <= 0.0 ? 1e9f : -1.0f;
    basic_collision(ball, &pos, &dir, elasticity, smoothness, threshold, static_cast<float>(boost));
}

void TFlipperEdge::port_draw() {
    set_angle(input_time);
    build_geometry();
}

void TFlipperEdge::place_in_grid() {
    float x_lo = rot_origin.x - circlebase_radius, y_lo = rot_origin.y - circlebase_radius;
    float x_hi = circlebase_radius + rot_origin.x, y_hi = rot_origin.y + circlebase_radius;
    float v;
    if ((v = t1_src.x - circle_t1_radius) < x_lo) x_lo = v;
    if ((v = t1_src.y - circle_t1_radius) < y_lo) y_lo = v;
    if (x_hi < (v = circle_t1_radius + t1_src.x)) x_hi = v;
    if (y_hi < (v = t1_src.y + circle_t1_radius)) y_hi = v;
    if ((v = t2_src.x - circle_t1_radius) < x_lo) x_lo = v;
    if ((v = t2_src.y - circle_t1_radius) < y_lo) y_lo = v;
    if (x_hi < (v = t2_src.x + circle_t1_radius)) x_hi = v;
    if (y_hi < (v = circle_t1_radius + t2_src.y)) y_hi = v;
    edges::add_box_to_grid(y_lo, x_lo, y_hi, x_hi, this, nullptr);
}

void TFlipperEdge::SetMotion(int code, float value) {
    if (code == 1) {
        angle_src = static_cast<float>(GetCurrentAngle(value));
        angle_dst = angle_max;
        flipper_move_time = extend_time;
    } else if (code == 2) {
        angle_src = static_cast<float>(GetCurrentAngle(value));
        angle_dst = 0.0f;
        flipper_move_time = retract_time;
    } else if (code == 0x400) {
        flipper_flag = 0;
        angle_dst = 0.0f;
        return;
    }
    if (flipper_flag == 0) input_time = value;
    flipper_flag = code;
    move_end_time = flipper_move_time + input_time;
}

// ---- TFlipper ---------------------------------------------------------------

TFlipper::TFlipper(TPinballTable* table, int group) : TCollisionComponent(table, group, false) {
    visual_struct v;
    loader::query_visual(group, 0, v);
    hard_hit_sound = v.sound_index2;
    soft_hit_sound = v.sound_index3;
    elasticity = v.elasticity;
    timer = 0;
    smoothness = v.smoothness;

    const float* coll_mult = loader::query_float_attribute(group, 0, 0x323);
    const float* retract = loader::query_float_attribute(group, 0, 0x325);
    const float* extend = loader::query_float_attribute(group, 0, 0x324);
    const float* t2 = loader::query_float_attribute(group, 0, 0x322);
    const float* t1 = loader::query_float_attribute(group, 0, 0x321);
    const float* origin = loader::query_float_attribute(group, 0, 800);
    flipper_edge = new TFlipperEdge(this, &active_flag, v.collision_group, table, origin, t1, t2, *extend, *retract,
                                    *coll_mult, elasticity, smoothness);

    float frames = static_cast<float>(static_cast<int>(list_bitmap.size()) - 1);
    extend_animation_frame_time = flipper_edge->extend_time / frames;
    retract_animation_frame_time = flipper_edge->retract_time / frames;
    bmp_index = 0;
    input_time = 0;
}

int TFlipper::Message(int code, float value) {
    if (code == 1) {
        control_handler(1, this);
        timer_time = extend_animation_frame_time;
        if (hard_hit_sound) loader::play_sound(hard_hit_sound);
    } else if (code == 2) {
        timer_time = retract_animation_frame_time;
        if (soft_hit_sound) loader::play_sound(soft_hit_sound);
    } else if (code < 0x3f1) {
        return 0;
    } else if (code > 0x3f3 && code != 0x3fe) {
        if (code != 0x3fc && code != 0x400) return 0;
        if (message_field == 0) return 0;
        if (timer) timer::kill(timer);
        bmp_index = -1;
        message_field = 2;
        TimerExpired(timer, this);
        flipper_edge->SetMotion(code, value);
        return 0;
    } else {
        code = 2;
        timer_time = retract_animation_frame_time;
    }

    if (timer) {
        timer::kill(timer);
        timer = 0;
    }
    float delay;
    if (message_field == 0) {
        delay = timer_time;
    } else {
        delay = value - flipper_edge->input_time;
        delay = static_cast<float>(delay - std::floor(static_cast<double>(delay / timer_time)) * timer_time);
        if (delay < 0.0f) delay = 0.0f;
    }
    message_field = code;
    input_time = value;
    timer = timer::set(delay, this, TimerExpired);
    flipper_edge->SetMotion(code, value);
    return 0;
}

void TFlipper::port_draw() { flipper_edge->port_draw(); }

void TFlipper::TimerExpired(int, void* caller) {
    auto* f = static_cast<TFlipper*>(caller);
    bool done = false;
    int count = ftol(std::floor((pb::g_time_now - static_cast<double>(f->input_time)) / f->timer_time + 0.5));
    if (count == 0) count = 1;
    if (f->message_field == 1) {
        f->bmp_index += count;
        int last = static_cast<int>(f->list_bitmap.size()) - 1;
        if (last <= f->bmp_index) {
            f->bmp_index = last;
            done = true;
        }
    }
    if (f->message_field == 2) {
        f->bmp_index -= count;
        if (f->bmp_index < 1) {
            f->bmp_index = 0;
            done = true;
        }
    }
    if (done) {
        f->message_field = 0;
        f->timer = 0;
    } else {
        f->timer = timer::set(f->timer_time, f, TimerExpired);
    }
    f->SetSpriteState(f->bmp_index);
}

// ---- TPlunger ---------------------------------------------------------------

TPlunger::TPlunger(TPinballTable* table, int group) : TCollisionComponent(table, group, true) {
    visual_struct v;
    loader::query_visual(group, 0, v);
    boost = 0.0f;
    ball_feed_timer_ = 0;
    pullback_timer_ = 0;
    sound_index_p1 = v.sound_index2;
    sound_index_p2 = v.sound_index3;
    hard_hit_sound = v.kicker.hard_hit_sound;
    threshold = 1e9f;
    max_pullback = 100;
    pullback_increment = ftol(100.0 / (static_cast<double>(list_bitmap.size()) * 8.0));
    elasticity = 0.5f;
    smoothness = 0.5f;
    pullback_delay = 0.025f;
    const float* pos = loader::query_float_attribute(group, 0, 0x259);
    table->plunger_position[0] = pos[0];
    table->plunger_position[1] = pos[1];
}

int TPlunger::Message(int code, float) {
    switch (code) {
    case 0x3ec:
        if (pullback_timer_ == 0) {
            boost = 0.0f;
            threshold = 1e9f;
            loader::play_sound(hard_hit_sound);
            PullbackTimer(0, this);
        }
        break;
    case 0x3f7: {
        auto* ball = static_cast<TBall*>(pinball_table->component_list[0]);
        ball->Message(0x400, 0.0f);
        ball->position.x = pinball_table->plunger_position[0];
        ball->position.y = pinball_table->plunger_position[1];
        ball->active_flag = 1;
        pinball_table->ball_in_drain_flag = 0;
        control_handler(code, this);
        break;
    }
    case 0x3f8:
        if (ball_feed_timer_) timer::kill(ball_feed_timer_);
        ball_feed_timer_ = timer::set(0.97f, this, BallFeedTimer);
        loader::play_sound(sound_index_p1);
        control_handler(code, this);
        break;
    case 0x3f9:
        threshold = 0.0f;
        boost = static_cast<float>(max_pullback);
        timer::set(0.2f, this, ReleasedTimer);
        break;
    case 0x400:
        if (ball_feed_timer_) timer::kill(ball_feed_timer_);
        ball_feed_timer_ = 0;
        [[fallthrough]];
    case 0x3ed:
    case 0x3f1:
    case 0x3f2:
        threshold = 0.0f;
        if (pullback_timer_) timer::kill(pullback_timer_);
        pullback_timer_ = 0;
        if (code == 0x3ed) loader::play_sound(sound_index_p2);
        SetSpriteState(0);
        timer::set(pullback_delay, this, ReleasedTimer);
        break;
    default:
        break;
    }
    return 0;
}

void TPlunger::Collision(TBall* ball, vector2* next_position, vector2* direction, float, TEdgeSegment*) {
    if (pinball_table->tilt_lock) Message(0x3f9, 0.0f);
    int r = msvcrt_rand();
    float kick = static_cast<float>(static_cast<double>(r) * kRandScale * boost * 0.1 + boost);
    basic_collision(ball, next_position, direction, elasticity, smoothness, threshold, kick);
}

void TPlunger::PullbackTimer(int, void* caller) {
    auto* p = static_cast<TPlunger*>(caller);
    float b = static_cast<float>(p->pullback_increment) + p->boost;
    p->boost = b;
    if (b <= static_cast<float>(p->max_pullback)) {
        p->pullback_timer_ = timer::set(p->pullback_delay, p, PullbackTimer);
    } else {
        p->pullback_timer_ = 0;
        p->boost = static_cast<float>(p->max_pullback);
    }
    int frame = ftol(std::floor(static_cast<float>(static_cast<int>(p->list_bitmap.size()) - 1) *
                                static_cast<double>(p->boost / static_cast<float>(p->max_pullback))));
    p->SetSpriteState(frame);
}

void TPlunger::BallFeedTimer(int, void* caller) {
    auto* p = static_cast<TPlunger*>(caller);
    p->pullback_timer_ = 0;
    p->Message(0x3f7, 0.0f);
}

void TPlunger::ReleasedTimer(int, void* caller) {
    auto* p = static_cast<TPlunger*>(caller);
    p->threshold = 1e9f;
    p->boost = 0.0f;
}

// ---- TDrain -----------------------------------------------------------------

TDrain::TDrain(TPinballTable* table, int group) : TCollisionComponent(table, group, true) {
    timer = 0;
    timer_time = *loader::query_float_attribute(group, 0, 0x197);
}

int TDrain::Message(int code, float) {
    if (code == 0x400) {
        if (timer) {
            timer::kill(timer);
            timer = 0;
        }
        pinball_table->ball_in_drain_flag = 0;
    }
    return 0;
}

void TDrain::Collision(TBall* ball, vector2*, vector2*, float, TEdgeSegment*) {
    ball->Message(0x400, 0.0f);
    pinball_table->ball_in_drain_flag = 1;
    timer = timer::set(timer_time, this, TimerCallback);
    control_handler(0x3f, this);
}

void TDrain::TimerCallback(int, void* caller) { control_handler(0x3c, static_cast<TDrain*>(caller)); }

