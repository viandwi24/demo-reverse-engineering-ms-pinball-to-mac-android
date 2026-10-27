#include "objects_b.h"

#include <cmath>

#include "ball.h"
#include "table.h"
#include "timer.h"

void (*g_pb_replay_level)(int) = nullptr;

namespace {

// FUN_01015935: lempar bola keluar (kickout/sink) dengan arah & kecepatan acak.
void throw_ball(TBall* ball, const vector3& dir, float angle_mult, float speed_mult1, float speed_mult2) {
    ball->collision_comp = nullptr;
    ball->direction = dir;
    double r = msvcrt_rand();
    float angle = static_cast<float>((1.0 - (r * kRandScale + r * kRandScale)) * angle_mult);
    vector2 d{ball->direction.x, ball->direction.y};
    maths::rotate_vector_buggy(d, angle);  // 0101ccf3 pada float* +0x36
    ball->direction.x = d.x;
    ball->direction.y = d.y;
    r = msvcrt_rand();
    double t = 1.0 - (r * kRandScale + r * kRandScale);
    ball->speed = static_cast<float>(static_cast<double>(speed_mult1) * speed_mult2 * t + speed_mult1);
}

// Pola umum awal Collision: bola ditaruh di titik tumbukan, sisa jarak dikurangi, edge dicatat.
void advance_ball(TBall* ball, const vector2* next_position, float distance, TEdgeSegment* edge) {
    ball->position.x = next_position->x;
    ball->position.y = next_position->y;
    ball->ray_max_distance -= distance;
    ball->not_already_hit(edge);
}

// Pola (f_a + f_b) - (f_b + f_b) * rand * c, dihitung di x87 lalu disimpan ke float (01016f21, 01016df3).
float random_delay(float base, float spread) {
    double s = static_cast<double>(base) + spread;
    double r = msvcrt_rand();
    double sp = static_cast<double>(spread) + spread;
    return static_cast<float>(s - r * kRandScale * sp);
}

// Bagian FieldEffect kickout/hole yang menarik bola ke tengah lingkaran (0101685b, 010169c0).
int circle_pull(const circle_type& c, float mult, TBall* ball, vector2* dst) {
    double dx = static_cast<double>(c.center.x) - ball->position.x;
    double dy = static_cast<double>(c.center.y) - ball->position.y;
    vector2 d{static_cast<float>(dx), static_cast<float>(dy)};
    if (!(dx * dx + dy * dy <= c.radius_sq)) return 0;
    maths::normalize_2d(d);
    dst->x = static_cast<float>(static_cast<double>(d.x) * mult -
                                static_cast<double>(ball->direction.x) * ball->speed);
    dst->y = static_cast<float>(static_cast<double>(d.y) * mult -
                                static_cast<double>(ball->direction.y) * ball->speed);
    return 1;
}

// Lingkaran field + edge lingkaran untuk kickout/hole (bagian bersama ctor 01019884 / 01019a1c).
void build_capture_circle(TCollisionComponent* owner, const visual_struct& v, float radius_mult,
                          circle_type& circle, field_effect_type& field) {
    const float* f = v.float_arr;
    circle.center.x = f[0];
    circle.center.y = f[1];
    // Asli: radius_mult (atribut 0x132) * r di register, simpan float, cek == 0 pada nilai register.
    double rr = static_cast<double>(radius_mult) * f[2];
    float radius = static_cast<float>(rr);
    if (rr == 0.0) radius = 0.001f;  // 0x3a83126f
    auto* c = new TCircle(owner, &owner->active_flag, v.collision_group, vector3{f[0], f[1], f[2]}, radius);
    c->place_in_grid();
    owner->edge_list.push_back(c);
    circle.radius_sq = f[2] * f[2];
    field.active_flag = &owner->active_flag;
    field.collision_group = v.collision_group;
    field.collision_comp = owner;
    edges::add_circle_to_grid(circle, nullptr, &field);
}

}  // namespace

// ---- TRollover --------------------------------------------------------------

TRollover::TRollover(TPinballTable* table, int group, bool create_wall)
    : TCollisionComponent(table, group, create_wall) {}

TRollover::TRollover(TPinballTable* table, int group) : TRollover(table, group, false) {
    rollover_flag = 0;
    if (!list_bitmap.empty()) render::sprite_set_bitmap(render_sprite_, list_bitmap[0]);
    build_walls(group);
}

void TRollover::build_walls(int group) {
    visual_struct v;
    loader::query_visual(group, 0, v);
    install_wall(loader::query_float_attribute(group, 0, 600), this, &active_flag, v.collision_group, 0.0f, 600);
    install_wall(loader::query_float_attribute(group, 0, 0x25b), this, &rollover_flag, v.collision_group, 0.0f, 0x25b);
}

int TRollover::Message(int code, float) {
    if (code == 0x400) {
        active_flag = 1;
        rollover_flag = 0;
        if (!list_bitmap.empty()) render::sprite_set_bitmap(render_sprite_, list_bitmap[0]);
    }
    return 0;
}

void TRollover::Collision(TBall* ball, vector2* next_position, vector2*, float distance, TEdgeSegment* edge) {
    advance_ball(ball, next_position, distance, edge);
    if (pinball_table->tilt_lock) return;
    if (rollover_flag == 0) {
        loader::play_sound(soft_hit_sound);
        control_handler(0x3f, this);
    } else {
        timer::set(0.1f, this, TimerExpired);
        active_flag = 0;
    }
    rollover_flag = rollover_flag == 0;
    if (!list_bitmap.empty())
        render::sprite_set_bitmap(render_sprite_, rollover_flag ? nullptr : list_bitmap[0]);
}

int TRollover::get_scoring(int index) { return index < 2 ? scoring[index] : 0; }

void TRollover::put_scoring(int index, int score) {
    if (index < 2) scoring[index] = score;
}

void TRollover::TimerExpired(int, void* caller) { static_cast<TRollover*>(caller)->active_flag = 1; }

// ---- TLightRollover ---------------------------------------------------------

TLightRollover::TLightRollover(TPinballTable* table, int group) : TRollover(table, group, false) {
    rollover_flag = 0;
    timer = 0;
    if (!list_bitmap.empty()) render::sprite_set_bitmap(render_sprite_, nullptr);
    build_walls(group);
    flag_timeout = *loader::query_float_attribute(group, 0, 0x197);
}

int TLightRollover::Message(int code, float) {
    if (code == 0x400) {
        active_flag = 1;
        rollover_flag = 0;
        if (timer) timer::kill(timer);
        timer = 0;
        if (!list_bitmap.empty()) render::sprite_set_bitmap(render_sprite_, nullptr);
    }
    return 0;
}

void TLightRollover::Collision(TBall* ball, vector2* next_position, vector2*, float distance, TEdgeSegment* edge) {
    advance_ball(ball, next_position, distance, edge);
    if (pinball_table->tilt_lock) return;
    if (rollover_flag == 0) {
        loader::play_sound(soft_hit_sound);
        control_handler(0x3f, this);
        rollover_flag = rollover_flag == 0;
        if (!list_bitmap.empty()) render::sprite_set_bitmap(render_sprite_, list_bitmap[0]);
    } else {
        timer::set(0.1f, this, TimerExpired);
        active_flag = 0;
        rollover_flag = rollover_flag == 0;
        if (timer == 0) timer = timer::set(flag_timeout, this, delay_expired);
    }
}

void TLightRollover::delay_expired(int, void* caller) {
    auto* r = static_cast<TLightRollover*>(caller);
    render::sprite_set_bitmap(r->render_sprite_, nullptr);
    r->timer = 0;
}

// ---- TTripwire --------------------------------------------------------------

TTripwire::TTripwire(TPinballTable* table, int group) : TRollover(table, group, true) {}

void TTripwire::Collision(TBall* ball, vector2* next_position, vector2*, float distance, TEdgeSegment* edge) {
    advance_ball(ball, next_position, distance, edge);
    if (pinball_table->tilt_lock) return;
    loader::play_sound(soft_hit_sound);
    control_handler(0x3f, this);
}

// ---- TKickout ---------------------------------------------------------------

TKickout::TKickout(TPinballTable* table, int group, int some_flag) : TCollisionComponent(table, group, false) {
    not_some_flag = some_flag == 0;
    if (not_some_flag) active_flag = 0;
    timer_time = 1.5f;
    timer_time2 = 0.05f;
    message_field = 0;
    timer = 0;
    kickout_flag = 0;
    float radius_mult = *loader::query_float_attribute(group, 0, 0x132);
    field_mult = *loader::query_float_attribute(group, 0, 0x131);
    visual_struct v;
    loader::query_visual(group, 0, v);
    soft_hit_sound = v.sound_index4;
    hard_hit_sound = v.kicker.hard_hit_sound;
    build_capture_circle(this, v, radius_mult, field_circle, field);
    // Asli: field (+0x9a) didaftarkan ke grid paling akhir; urutannya tidak memengaruhi hasil.
    collision_ball_set_z = loader::query_float_attribute(group, 0, 0x198)[2];
    throw_speed_mult2 = static_cast<float>(static_cast<double>(v.kicker.unknown193) * 0.01);
    ball_throw_dir = v.kicker.vector;
    throw_angle_mult = v.kicker.unknown195;
    throw_speed_mult1 = v.kicker.boost;
}

int TKickout::Message(int code, float value) {
    if (code == 0x37) {
        if (kickout_flag) {
            if (value < 0.0f) value = timer_time;
            timer = timer::set(value, this, TimerExpired);
        }
        return 0;
    }
    if (code != 0x3f3) {
        if (code != 0x400) return 0;
        if (kickout_flag) {
            if (timer) timer::kill(timer);
            TimerExpired(0, this);
        }
    }
    if (not_some_flag) active_flag = 0;
    return 0;
}

void TKickout::Collision(TBall* b, vector2*, vector2*, float, TEdgeSegment*) {
    if (kickout_flag) return;
    ball = b;
    threshold = 1e9f;
    kickout_flag = 1;
    b->collision_comp = this;
    b->position.x = field_circle.center.x;
    b->position.y = field_circle.center.y;
    orig_ball_z = b->position.z;
    b->position.z = collision_ball_set_z;
    if (pinball_table->tilt_lock) {
        Message(0x37, 0.1f);
    } else {
        loader::play_sound(soft_hit_sound);
        control_handler(0x3f, this);
    }
}

int TKickout::FieldEffect(TBall* b, vector2* dst) {
    if (kickout_flag) return 0;
    return circle_pull(field_circle, field_mult, b, dst);
}

int TKickout::get_scoring(int index) { return index < 5 ? scoring[index] : 0; }

void TKickout::put_scoring(int index, int score) {
    if (index < 5) scoring[index] = score;
}

void TKickout::TimerExpired(int, void* caller) {
    auto* k = static_cast<TKickout*>(caller);
    if (!k->kickout_flag) return;
    k->kickout_flag = 0;
    k->timer = timer::set(k->timer_time2, k, ResetTimerExpired);
    if (TBall* b = k->ball) {
        k->ball = nullptr;
        b->position.z = k->orig_ball_z;
        throw_ball(b, k->ball_throw_dir, k->throw_angle_mult, k->throw_speed_mult1, k->throw_speed_mult2);
        k->active_flag = 0;
        loader::play_sound(k->hard_hit_sound);
    }
}

void TKickout::ResetTimerExpired(int, void* caller) {
    auto* k = static_cast<TKickout*>(caller);
    if (k->not_some_flag == 0) k->active_flag = 1;
    k->timer = 0;
}

// ---- THole ------------------------------------------------------------------

THole::THole(TPinballTable* table, int group) : TCollisionComponent(table, group, false) {
    unknown5e = 0.05f;
    message_field = 0;
    timer = 0;
    ball_captured_flag = 0;
    const float* f = loader::query_float_attribute(group, 0, 0x197);
    unknown5a = f ? *f : 0.25f;
    f = loader::query_float_attribute(group, 0, 0x2bd);
    gravity_mult = f ? *f : 0.5f;
    float radius_mult = *loader::query_float_attribute(group, 0, 0x132);
    field_mult = *loader::query_float_attribute(group, 0, 0x131);
    visual_struct v;
    loader::query_visual(group, 0, v);
    // Asli: atribut 0x198/0x518 dibaca sebelum add_circle_to_grid; urutan tidak memengaruhi hasil.
    z_floor = loader::query_float_attribute(group, 0, 0x198)[2];
    field_flag = ftol(std::floor(static_cast<double>(*loader::query_float_attribute(group, 0, 0x518))));
    build_capture_circle(this, v, radius_mult, field_circle, field);
}

int THole::Message(int code, float) {
    if (code == 0x400 && ball_captured_flag) {
        if (timer) timer::kill(timer);
        timer = 0;
        ball_drop_flag = 1;
    }
    return 0;
}

void THole::Collision(TBall* ball, vector2*, vector2*, float, TEdgeSegment*) {
    if (ball_captured_flag) return;
    ball_drop_flag = 0;
    threshold = 1e9f;
    ball_captured_flag = 1;
    ball->collision_comp = this;
    ball->position.x = field_circle.center.x;
    ball->position.y = field_circle.center.y;
    ball->direction.z = 0.0f;
    timer = timer::set(0.5f, this, TimerExpired);
    if (!pinball_table->tilt_lock) {
        loader::play_sound(hard_hit_sound);
        control_handler(0x39, this);
    }
}

int THole::FieldEffect(TBall* ball, vector2* dst) {
    if (!ball_captured_flag) return circle_pull(field_circle, field_mult, ball, dst);
    if (ball_drop_flag) {
        // asm 010169dd: dz tetap di register (double) untuk penjumlahan z.
        double dz = static_cast<double>(ball->direction.z) -
                    static_cast<double>(pinball_table->gravity_dir_vect[0]) * ball->time_delta * gravity_mult;
        ball->direction.z = static_cast<float>(dz);
        double z = dz + ball->position.z;
        ball->position.z = static_cast<float>(z);
        if (z <= z_floor) {
            ball_captured_flag = 0;
            ball_drop_flag = 0;
            ball->position.z = z_floor;
            ball->direction.z = 0.0f;
            ball->field_flag = field_flag;
            ball->direction.y = 0.0f;
            ball->collision_comp = nullptr;
            ball->direction.x = 0.0f;
            ball->speed = 0.0f;
            loader::play_sound(soft_hit_sound);
            control_handler(0x3a, this);
        }
    }
    return 0;
}

void THole::TimerExpired(int, void* caller) {
    auto* h = static_cast<THole*>(caller);
    h->timer = 0;
    h->ball_drop_flag = 1;
}

// ---- TRamp ------------------------------------------------------------------

TRamp::TRamp(TPinballTable* table, int group) : TCollisionComponent(table, group, false) {
    message_field = 0;
    unknown_c4f = true;
    visual_struct v;
    loader::query_visual(group, 0, v);
    collision_group = v.collision_group;
    const float* f = loader::query_float_attribute(group, 0, 0x2bd);
    field_mult = f ? *f : 0.2f;
    f = loader::query_float_attribute(group, 0, 0x519);
    ramp_flag1 = f ? ftol(std::floor(static_cast<double>(*f))) : 0;

    f = loader::query_float_attribute(group, 0, 0x514);
    line_count = ftol(std::floor(static_cast<double>(*f)));
    planes_.resize(line_count > 0 ? line_count : 0);
    for (int i = 0; i < line_count; ++i) {
        const float* p = f + 1 + i * 13;
        ramp_plane_type& pl = planes_[i];
        pl.normal = {p[0], p[1], p[2]};
        pl.point0 = {p[3], p[4]};
        pl.point1 = {p[5], p[6]};
        pl.point2 = {p[7], p[8]};
        pl.gravity_angle1 = p[9];
        pl.gravity_angle2 = p[10];
        pl.field_force = {p[11], p[12]};
    }
    plane = planes_.data();

    f = loader::query_float_attribute(group, 0, 0x517);
    int bit = ftol(std::floor(static_cast<double>(f[0])));
    vector2 l1_end{f[2], f[3]};
    vector2 l1_start{f[4], f[5]};
    line1 = new TLine(this, &active_flag, 1u << (bit & 0x1f), l1_start, l1_end);
    edge_list.push_back(line1);
    line1->wall_value = 0;
    line1->place_in_grid();

    f = loader::query_float_attribute(group, 0, 0x515);
    wall1_collision_group = ftol(std::floor(static_cast<double>(f[0])));
    int wall1_enabled = ftol(std::floor(static_cast<double>(f[1])));
    wall1_collision_group = static_cast<int>(1u << (wall1_collision_group & 0x1f));
    wall1_ball_offset = f[7];
    vector2* w1_a = nullptr;  // param_4 (EBP+8)
    vector2* w1_b = nullptr;  // param_5 (EBP-0x24)
    ramp_plane_closest(plane, line_count, f + 3, &w1_a, &w1_b);
    line2 = new TLine(this, &active_flag, collision_group, *w1_b, *w1_a);
    edge_list.push_back(line2);
    line2->wall_value = 0;
    line2->place_in_grid();

    f = loader::query_float_attribute(group, 0, 0x516);
    wall2_collision_group = ftol(std::floor(static_cast<double>(f[0])));
    int wall2_enabled = ftol(std::floor(static_cast<double>(f[1])));
    wall2_collision_group = static_cast<int>(1u << (wall2_collision_group & 0x1f));
    wall2_ball_offset = f[7];
    vector2* w2_a = nullptr;  // EBP-0x20
    vector2* w2_b = nullptr;  // EBP-0x14
    ramp_plane_closest(plane, line_count, f + 3, &w2_a, &w2_b);
    line3 = new TLine(this, &active_flag, collision_group, *w2_b, *w2_a);
    edge_list.push_back(line3);
    line3->wall_value = 0;
    line3->place_in_grid();

    // Grup collision untuk sisi bidang (from, to); 0 = tidak dibuat.
    auto edge_group = [&](const vector2* from, const vector2* to) -> int {
        if (from == w1_a && to == w1_b) return wall1_enabled ? wall1_collision_group : 0;
        if (from == w2_a && to == w2_b) return wall2_enabled ? wall2_collision_group : 0;
        return v.collision_group;
    };
    auto add_edge = [&](int i, vector2* from, vector2* to) {
        int grp = edge_group(from, to);
        if (!grp) return;
        auto* l = new TLine(this, &active_flag, grp, *from, *to);
        edge_list.push_back(l);
        l->wall_value = i + 1;  // asli: pointer bidang
        l->place_in_grid();
    };

    float x_min = 1e9f, y_min = 1e9f;    // EBP-0x10, EBP-0xc
    float x_max = -1e9f, y_max = -1e9f;  // EBP-0x8, EBP-0x4
    for (int i = 0; i < line_count; ++i) {
        ramp_plane_type& pl = plane[i];
        const vector2& p0 = pl.point0;
        const vector2& p1 = pl.point1;
        const vector2& p2 = pl.point2;
        // asm 01019e99..0101a084: makro min/max; perbandingan y-min, x-max, y-max memakai x_min
        // (bug asli, dipertahankan).
        float m = p2.x < p0.x ? p2.x : p0.x;
        float min_x = m <= p1.x ? m : p1.x;
        if (!(x_min < min_x)) x_min = min_x;
        m = p2.y < p0.y ? p2.y : p0.y;
        float min_y = m <= p1.y ? m : p1.y;
        y_min = x_min >= min_y ? min_y : x_min;
        m = p2.x <= p0.x ? p0.x : p2.x;
        float max_x = m >= p1.x ? m : p1.x;
        x_max = x_min <= max_x ? max_x : x_min;
        m = p2.y <= p0.y ? p0.y : p2.y;
        float max_y = m >= p1.y ? m : p1.y;
        y_max = x_min <= max_y ? max_y : x_min;

        add_edge(i, &pl.point0, &pl.point1);
        add_edge(i, &pl.point1, &pl.point2);
        add_edge(i, &pl.point2, &pl.point0);

        double g = table->gravity_dir_vect[0];
        pl.field_force.x = static_cast<float>(std::cos(static_cast<double>(pl.gravity_angle2)) *
                                              std::sin(static_cast<double>(pl.gravity_angle1)) * g);
        pl.field_force.y = static_cast<float>(std::sin(static_cast<double>(pl.gravity_angle2)) *
                                              std::sin(static_cast<double>(pl.gravity_angle1)) * g);
    }

    field.active_flag = &active_flag;
    field.collision_comp = this;
    field.collision_group = v.collision_group;
    edges::add_box_to_grid(y_min, x_min, y_max, x_max, nullptr, &field);
}

void TRamp::ramp_plane_closest(ramp_plane_type* planes, int count, const float* pts, vector2** out_start,
                               vector2** out_end) {
    vector2 a{pts[0], pts[1]};
    vector2 b{pts[2], pts[3]};
    float best = 1e9f;
    for (int i = 0; i < count; ++i) {
        ramp_plane_type& pl = planes[i];
        struct Pair {
            const vector2* da;  // jarak dari a
            const vector2* db;  // jarak dari b
            vector2* s;
            vector2* e;
        };
        // asm 01016b6a..01016c14: jarak pertama disimpan ke float, yang kedua tetap di register.
        const Pair pairs[3] = {{&pl.point0, &pl.point1, &pl.point0, &pl.point1},
                               {&pl.point1, &pl.point2, &pl.point1, &pl.point2},
                               {&pl.point2, &pl.point0, &pl.point2, &pl.point0}};
        for (int k = 0; k < 3; ++k) {
            const Pair& p = pairs[k];
            double sum;
            if (k == 0) {
                float d1 = static_cast<float>(maths::distance(a, *p.da));
                sum = maths::distance(b, *p.db) + d1;
            } else if (k == 1) {
                float d1 = static_cast<float>(maths::distance(b, *p.db));
                sum = maths::distance(a, *p.da) + d1;
            } else {
                float d1 = static_cast<float>(maths::distance(a, *p.da));
                sum = maths::distance(b, *p.db) + d1;
            }
            if (sum < best) {
                best = static_cast<float>(sum);
                *out_start = p.s;
                *out_end = p.e;
            }
        }
    }
}

void TRamp::Collision(TBall* ball, vector2* next_position, vector2*, float distance, TEdgeSegment* edge) {
    ball->not_already_hit(edge);
    ball->ray_max_distance -= distance;
    ball->position.x = next_position->x;
    ball->position.y = next_position->y;
    if (edge->wall_value) {
        const ramp_plane_type& pl = plane[edge->wall_value - 1];
        ball->collision_flag = 1;
        ball->collision_offset = pl.normal;
        ball->ramp_field_force = pl.field_force;
        ball->position.z = static_cast<float>(static_cast<double>(ball->collision_offset.x) * ball->position.x +
                                              static_cast<double>(ball->position.y) * ball->collision_offset.y +
                                              ball->offset + ball->collision_offset.z);
        ball->field_flag = collision_group;
        return;
    }
    if (edge == line1) {
        if (!pinball_table->tilt_lock) {
            loader::play_sound(soft_hit_sound);
            control_handler(0x3f, this);
        }
        return;
    }
    ball->collision_flag = 0;
    if (edge == line2) {
        ball->field_flag = wall1_collision_group;
        if (!ramp_flag1) return;
        ball->position.z = ball->offset + wall1_ball_offset;
    } else {
        ball->field_flag = wall2_collision_group;
        if (!ramp_flag1) return;
        ball->position.z = ball->offset + wall2_ball_offset;
    }
}

int TRamp::FieldEffect(TBall* ball, vector2* dst) {
    dst->x = static_cast<float>(ball->ramp_field_force.x -
                                static_cast<double>(ball->direction.x) * ball->speed * field_mult);
    dst->y = static_cast<float>(ball->ramp_field_force.y -
                                static_cast<double>(ball->direction.y) * ball->speed * field_mult);
    return 1;
}

int TRamp::get_scoring(int index) { return index < 4 ? scoring[index] : 0; }

void TRamp::put_scoring(int index, int score) {
    if (index < 4) scoring[index] = score;
}

// ---- TDemo ------------------------------------------------------------------

TDemo::TDemo(TPinballTable* table, int group) : TCollisionComponent(table, group, false) {
    restart_timer = 0;
    plunger_timer = 0;
    flipper_l_timer = 0;
    flipper_r_timer = 0;
    message_field = 0;
    unknown_c4f = false;
    active_flag = 0;
    flipper_r_active = 0;
    flipper_l_active = 0;
    table->demo = this;
    const float* f = loader::query_float_attribute(group, 0, 0x197);
    if (!f) {
        min_reaction_time1 = 0.2f;
        reaction_spread1 = 0.1f;
        min_reaction_time2 = 0.2f;
        reaction_spread2 = 0.1f;
    } else {
        min_reaction_time1 = f[0];
        reaction_spread1 = f[1];
        min_reaction_time2 = f[2];
        reaction_spread2 = f[3];
    }
    visual_struct v;
    loader::query_visual(group, 0, v);
    edge1 = install_wall(loader::query_float_attribute(group, 0, 0x578), this, &active_flag, v.collision_group, 0.0f, 0x578);
    install_wall(loader::query_float_attribute(group, 0, 0x579), this, &active_flag, v.collision_group, 0.0f, 0x579);
    edge2 = install_wall(loader::query_float_attribute(group, 0, 0x57a), this, &active_flag, v.collision_group, 0.0f, 0x57a);
    install_wall(loader::query_float_attribute(group, 0, 0x57b), this, &active_flag, v.collision_group, 0.0f, 0x57b);
    edge3 = install_wall(loader::query_float_attribute(group, 0, 0x57c), this, &active_flag, v.collision_group,
                         table->collision_comp_offset, 0x57c);
}

int TDemo::Message(int code, float) {
    if (code == 0x3f6) {
        if (restart_timer) timer::kill(restart_timer);
        restart_timer = 0;
    } else if (code == 0x3fe) {
        if (restart_timer) timer::kill(restart_timer);
        restart_timer = 0;
        if (active_flag) restart_timer = timer::set(5.0f, this, NewGameRestartTimer);
    } else if (code == 0x400) {
        if (flipper_l_timer) timer::kill(flipper_l_timer);
        flipper_l_timer = 0;
        if (flipper_r_timer) timer::kill(flipper_r_timer);
        flipper_r_timer = 0;
        if (flipper_l_active) UnFlipLeft(0, this);
        if (flipper_r_active) UnFlipRight(0, this);
        if (plunger_timer) PlungerRelease(0, this);
    }
    return 0;
}

void TDemo::Collision(TBall* ball, vector2* next_position, vector2*, float distance, TEdgeSegment* edge) {
    int wall = edge->wall_value;
    ball->not_already_hit(edge);
    ball->position.x = next_position->x;
    ball->ray_max_distance -= distance;
    ball->position.y = next_position->y;
    switch (wall) {
    case 0x578:
        if (flipper_l_timer == 0 && flipper_l_active == 0)
            flipper_l_timer = timer::set(random_delay(min_reaction_time1, reaction_spread1), this, FlipLeft);
        break;
    case 0x579:
        FlipLeft(0, this);
        break;
    case 0x57a:
        if (flipper_r_timer == 0 && flipper_r_active == 0)
            flipper_r_timer = timer::set(random_delay(min_reaction_time1, reaction_spread1), this, FlipRight);
        break;
    case 0x57b:
        FlipRight(0, this);
        break;
    case 0x57c:
        if (plunger_timer == 0) {
            pinball_table->Message(0x3ec, ball->time_now);
            double r = msvcrt_rand();
            plunger_timer = timer::set(static_cast<float>(r * kRandScale + 2.0), this, PlungerRelease);
        }
        break;
    default:
        break;
    }
}

// Catatan: asli mengirim DAT_01025634 (waktu akhir frame berjalan). Di luar timed_frame nilainya sama
// dengan pb::g_time_now; timer dijalankan setelah g_time_now diperbarui.
void TDemo::PlungerRelease(int, void* caller) {
    auto* d = static_cast<TDemo*>(caller);
    d->plunger_timer = 0;
    d->pinball_table->Message(0x3ed, pb::g_time_now);
}

void TDemo::UnFlipRight(int, void* caller) {
    auto* d = static_cast<TDemo*>(caller);
    if (d->flipper_r_active) d->pinball_table->Message(0x3eb, pb::g_time_now);
    d->flipper_r_active = 0;
}

void TDemo::UnFlipLeft(int, void* caller) {
    auto* d = static_cast<TDemo*>(caller);
    if (d->flipper_l_active) d->pinball_table->Message(0x3e9, pb::g_time_now);
    d->flipper_l_active = 0;
}

void TDemo::FlipRight(int, void* caller) {
    auto* d = static_cast<TDemo*>(caller);
    if (d->flipper_r_active) return;
    if (d->flipper_r_timer) {
        timer::kill(d->flipper_r_timer);
        d->flipper_r_timer = 0;
    }
    d->pinball_table->Message(0x3ea, pb::g_time_now);
    d->flipper_r_active = 1;
    timer::set(random_delay(d->min_reaction_time2, d->reaction_spread2), d, UnFlipRight);  // id tidak disimpan (asli)
}

void TDemo::FlipLeft(int, void* caller) {
    auto* d = static_cast<TDemo*>(caller);
    if (d->flipper_l_active) return;
    if (d->flipper_l_timer) {
        timer::kill(d->flipper_l_timer);
        d->flipper_l_timer = 0;
    }
    d->pinball_table->Message(1000, pb::g_time_now);
    d->flipper_l_active = 1;
    timer::set(random_delay(d->min_reaction_time2, d->reaction_spread2), d, UnFlipLeft);  // id tidak disimpan (asli)
}

void TDemo::NewGameRestartTimer(int, void* caller) {
    auto* d = static_cast<TDemo*>(caller);
    if (g_pb_replay_level) g_pb_replay_level(1);  // FUN_010153ae(1)
    d->pinball_table->Message(0x3f6, static_cast<float>(d->pinball_table->player_count));
    d->restart_timer = 0;
}

// ---- TSink ------------------------------------------------------------------

TSink::TSink(TPinballTable* table, int group) : TCollisionComponent(table, group, true) {
    message_field = 0;
    timer = 0;
    visual_struct v;
    loader::query_visual(group, 0, v);
    throw_speed_mult2 = static_cast<float>(static_cast<double>(v.kicker.unknown193) * 0.01);
    ball_throw_dir = v.kicker.vector;
    throw_angle_mult = v.kicker.unknown195;
    throw_speed_mult1 = v.kicker.boost;
    sound_index4 = v.sound_index2;
    sound_index3 = v.sound_index3;
    const float* f = loader::query_float_attribute(group, 0, 0x259);
    ball_position = {f[0], f[1]};
    timer_time = *loader::query_float_attribute(group, 0, 0x197);
}

int TSink::Message(int code, float value) {
    if (code == 0x38) {
        if (value < 0.0f) value = timer_time;
        timer = timer::set(value, this, TimerExpired);
    } else if (code == 0x3fc) {
        player_message_field_backup[pinball_table->current_player] = message_field;
        message_field = player_message_field_backup[ftol(std::floor(static_cast<double>(value)))];
    } else if (code == 0x400) {
        if (timer) timer::kill(timer);
        timer = 0;
        message_field = 0;
        for (int i = 0; i < pinball_table->player_count; ++i) player_message_field_backup[i] = 0;
    }
    return 0;
}

void TSink::Collision(TBall* ball, vector2* next_position, vector2* direction, float, TEdgeSegment*) {
    timer = 0;
    if (pinball_table->tilt_lock) {
        basic_collision(ball, next_position, direction, elasticity, smoothness, 1e9f, 0.0f);
        return;
    }
    ball->active_flag = 0;
    render::sprite_set_bitmap(ball->render_sprite_, nullptr);
    loader::play_sound(sound_index4);
    control_handler(0x3f, this);
}

int TSink::get_scoring(int index) { return index < 3 ? scoring[index] : 0; }

void TSink::put_scoring(int index, int score) {
    if (index < 3) scoring[index] = score;
}

void TSink::TimerExpired(int, void* caller) {
    auto* s = static_cast<TSink*>(caller);
    TBall* ball = s->pinball_table->ball_list[0];
    ball->collision_comp = nullptr;
    ball->active_flag = 1;
    ball->position.x = s->ball_position.x;
    ball->position.y = s->ball_position.y;
    throw_ball(ball, s->ball_throw_dir, s->throw_angle_mult, s->throw_speed_mult1, s->throw_speed_mult2);
    if (s->sound_index3) loader::play_sound(s->sound_index3);
    s->timer = 0;
}

// ---- Pabrik -----------------------------------------------------------------

TPinballComponent* create_object_b(TPinballTable* table, int type, int group) {
    switch (type) {
    case 0x3f4: return new TKickout(table, group, 1);
    case 0x405: return new TKickout(table, group, 0);
    case 0x3f7: return new TRollover(table, group);
    case 0x3f9: return new TSink(table, group);
    case 0x3fc: return new TLightRollover(table, group);
    case 0x3fd: return new TRamp(table, group);
    case 0x3fe: return new THole(table, group);
    case 0x3ff: return new TDemo(table, group);
    case 0x400: return new TTripwire(table, group);
    default: return nullptr;
    }
}
