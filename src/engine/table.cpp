#include "table.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "ball.h"
#include "control/control.h"
#include "control/control_fns.h"
#include "lights.h"
#include "misc_objects.h"
#include "score.h"
#include "objects.h"
#include "objects_a.h"
#include "objects_b.h"
#include "timer.h"

// ---- TTableLayer ------------------------------------------------------------

TTableLayer::TTableLayer(TPinballTable* table) : TCollisionComponent(table, -1, false) {
    int group = loader::query_handle("table");
    visual_struct v;
    loader::query_visual(group, 0, v);

    const float* center = loader::query_float_attribute(group, 0, 700);
    proj::set_center(center[0], center[1]);
    render::set_table_zmap(v.zmap, 0, 0);
    visual_bitmap = v.bitmap;
    rectangle_type bounds{0, 0, v.bitmap->width, v.bitmap->height};
    render_sprite_ = render::create_sprite(VisualType::Static, v.bitmap, v.zmap, 0, 0, &bounds);

    table->sound_index1 = v.sound_index2;
    table->sound_index2 = v.sound_index3;
    table->sound_index3 = v.kicker.hard_hit_sound;

    if (const float* g = loader::query_float_attribute(group, 0, 0x131)) {
        table->gravity_dir_vect[0] = g[0];
        table->gravity_dir_vect[1] = g[1];
        table->gravity_dir_vect[2] = g[2];
    } else {
        table->gravity_dir_vect[0] = 25.0f;
        table->gravity_dir_vect[1] = 0.5f;
        table->gravity_dir_vect[2] = 1.5707964f;  // 0x3fc90fd8
    }
    double cos_b = std::cos(static_cast<double>(table->gravity_dir_vect[2]));
    double sin_a = std::sin(static_cast<double>(table->gravity_dir_vect[1]));
    gravity_dir_x = static_cast<float>(sin_a * cos_b * table->gravity_dir_vect[0]);
    double sin_b = std::sin(static_cast<double>(table->gravity_dir_vect[2]));
    gravity_dir_y = static_cast<float>(sin_a * sin_b * table->gravity_dir_vect[0]);

    const float* friction = loader::query_float_attribute(group, 0, 0x2bd);
    gravity_mult = friction ? *friction : 0.2f;

    table->x_offset = v.bitmap->x;
    table->y_offset = v.bitmap->y;
    table->width = v.bitmap->width;
    table->height = v.bitmap->height;

    threshold = v.kicker.threshold;
    boost = 15.0f;

    // Batas meja dari tiga titik pertama poligon.
    const float* p = v.float_arr;
    auto min3 = [](float a, float b, float c) {
        float m = c <= b ? c : b;
        return a <= m ? a : m;
    };
    auto max3 = [](float a, float b, float c) {
        float m = b <= c ? c : b;
        return m <= a ? a : m;
    };
    x_min = min3(p[0], p[2], p[4]);
    x_max = max3(p[0], p[2], p[4]);
    y_min = min3(p[1], p[3], p[5]);
    y_max = max3(p[1], p[3], p[5]);

    g_edge_manager = new TEdgeManager(x_min, y_min, x_max - x_min, y_max - y_min);

    for (int i = 0; i < v.float_arr_count; ++i, p += 2) {
        auto* line = new TLine(this, &active_flag, v.collision_group, p[2], p[3], p[0], p[1]);
        line->place_in_grid();
        edge_list.push_back(line);
    }

    field.collision_group = -1;
    field.active_flag = &active_flag;
    field.collision_comp = this;
    edges::add_box_to_grid(y_min, x_min, y_max, x_max, nullptr, &field);
}

int TTableLayer::FieldEffect(TBall* ball, vector2* vec_dst) {
    float gx = gravity_dir_x;
    int r = msvcrt_rand();
    vec_dst->x = static_cast<float>(gx - ((0.5 - static_cast<double>(r) * kRandScale) + ball->direction.x) *
                                             ball->speed * gravity_mult);
    vec_dst->y = static_cast<float>(gravity_dir_y - static_cast<double>(ball->direction.y) * ball->speed * gravity_mult);
    return 1;
}

// ---- TPinballTable ----------------------------------------------------------

namespace {
constexpr int kScoreMultipliers[5] = {1, 2, 3, 5, 10};  // DAT_01024758

TTextBox* info_box() { return control::info_text_box; }
TTextBox* mission_box() { return control::mission_text_box; }

int demo_text(TPinballTable* t, int normal, int demo) {
    return t->demo && t->demo->active_flag ? demo : normal;
}
}  // namespace

TPinballTable::TPinballTable() : TPinballComponent(nullptr, -1, false) {
    active_flag = 1;

    auto* ball = new TBall(this);
    ball_list.push_back(ball);
    ball->active_flag = 0;

    layer = new TTableLayer(this);
    light_group = new TLightGroup(this, 0);

    score_struct* s = score::create("score1", nullptr);
    cur_score_struct = s;
    player_scores[0].score_struct = s;
    for (int i = 1; i < 4; ++i) player_scores[i].score_struct = score::dup(s);
    current_player = 0;
    max_ball_count = 3;
    score_ballcount = score::create("ballcount1", nullptr);
    score_player_number1 = score::create("player_number1", nullptr);

    int count = 0;
    const int16_t* objects = loader::query_iattribute(loader::query_handle("table_objects"), 0x401, count);
    for (int i = 0; i < count / 2; ++i) {
        int type = objects[i * 2];
        int group = objects[i * 2 + 1];
        switch (type) {
        case 1000:
        case 0x3f2: new TWall(this, group); break;
        case 0x3e9: plunger = new TPlunger(this, group); break;
        case 0x3ea: light_group->list.push_back(new TLight(this, group)); break;
        case 0x3eb: flipper_l = new TFlipper(this, group); break;
        case 0x3ec: flipper_r = new TFlipper(this, group); break;
        case 0x3ef: drain = new TDrain(this, group); break;
        case 0x402: new TLightGroup(this, group); break;
        case 0x406: new TLightBargraph(this, group); break;
        default:
            if (!create_object_a(this, type, group) && !create_object_b(this, type, group))
                create_misc_object(this, type, group);
            break;
        }
    }
    render::build_occlude_list();
    control::make_links(this);
}

TPinballTable::~TPinballTable() = default;

TPinballComponent* TPinballTable::find_component(const char* name) {
    for (TPinballComponent* c : component_list)
        if (c->group_name && std::strcmp(c->group_name, name) == 0) return c;
    return nullptr;
}

TPinballComponent* TPinballTable::find_component(int group) {
    for (TPinballComponent* c : component_list)
        if (c->group_index == group) return c;
    return nullptr;
}

int TPinballTable::AddScore(int score) {
    if (score_special3_flag) {
        score_special3 += score;
        if (score_special3 > 5000000) score_special3 = 5000000;
    }
    if (score_special2_flag) {
        score_special2 += score;
        if (score_special2 > 5000000) score_special2 = 5000000;
    }
    int added = kScoreMultipliers[score_multiplier] * score + score_added;
    cur_score += added;
    if (cur_score > 1000000000) {
        ++cur_score_e9;
        cur_score -= 1000000000;
    }
    score::set(cur_score_struct, cur_score);
    return added;
}

void TPinballTable::ChangeBallCount(int count) {
    ball_count = count;
    if (count < 1) {
        score::erase(score_ballcount, 1);
    } else {
        score::set(score_ballcount, max_ball_count - count + 1);
        score::update(score_ballcount);
    }
}

void TPinballTable::tilt(float time) {
    if (tilt_lock || ball_in_drain_flag) return;
    info_box()->Clear();
    mission_box()->Clear();
    info_box()->Display(control::get_rc_string(0x23), -1.0f);
    loader::play_sound(sound_index3);
    tilt_timeout_timer = timer::set(30.0f, this, TiltTimeout);
    int n = static_cast<int>(component_list.size());
    Message(0x3f3, -1.0f);
    for (int i = 0; i < n; ++i) component_list[i]->Message(0x3f3, time);
    light_group->Message(8, 0.0f);
    tilt_lock = 1;
    control::fn_0100c0be(0x3f3);
}

void TPinballTable::TiltTimeout(int, void* caller) {  // 01017738
    auto* t = static_cast<TPinballTable*>(caller);
    t->tilt_timeout_timer = 0;
    if (t->tilt_lock) {
        vector2 v{};
        for (TBall* b : t->ball_list) t->drain->Collision(b, &v, &v, 0.0f, nullptr);
    }
}

void TPinballTable::LightShowTimer(int, void* caller) {  // 01017467
    auto* t = static_cast<TPinballTable*>(caller);
    t->light_show_timer = 0;
    t->Message(0x3f5, 0.0f);
}

void TPinballTable::EndGame_timeout(int, void* caller) {  // 0101748b
    auto* t = static_cast<TPinballTable*>(caller);
    t->end_game_timeout_timer = 0;
    pb::end_game();
    for (int i = static_cast<int>(t->component_list.size()) - 1; i >= 0; --i) t->component_list[i]->Message(0x3fe, 0.0f);
    if (t->demo) t->demo->Message(0x3fe, 0.0f);
    control::handler(0x43, mission_box());
    info_box()->Display(control::get_rc_string(0x18), -1.0f);
}

void TPinballTable::replay_timer_callback(int, void* caller) {  // 010175ef
    auto* t = static_cast<TPinballTable*>(caller);
    t->replay_active_flag = 0;
    t->replay_timer = 0;
}

int TPinballTable::Message(int code, float value) {
    switch (code) {
    case 1000:
        if (!tilt_lock) flipper_l->Message(1, value);
        break;
    case 0x3e9:
        if (!tilt_lock) flipper_l->Message(2, value);
        break;
    case 0x3ea:
        if (!tilt_lock) flipper_r->Message(1, value);
        break;
    case 0x3eb:
        if (!tilt_lock) flipper_r->Message(2, value);
        break;
    case 0x3ec:
    case 0x3ed:
        plunger->Message(code, value);
        break;
    case 0x3ee:
    case 0x3ef:
    case 0x3f3:
    case 0x3f7:
    case 0x3f8:
    case 0x3f9:
    case 0x3fb:
    case 0x3fc:
    case 0x3ff:
        break;
    case 0x3f0:
    case 0x3f1:
    case 0x3f2:
        for (int i = static_cast<int>(component_list.size()) - 1; i >= 0; --i) component_list[i]->Message(code, value);
        break;
    case 0x3f4:
        light_group->Message(0xe, 0.0f);
        if (tilt_lock) {
            tilt_lock = 0;
            if (tilt_timeout_timer) timer::kill(tilt_timeout_timer);
            tilt_timeout_timer = 0;
        }
        break;
    case 0x3f5:
        light_group->Message(0x22, 0.0f);
        light_group->Message(0x14, 0.0f);
        plunger->Message(0x3f8, 0.0f);
        info_box()->Display(control::get_rc_string(demo_text(this, 0x1a, 0x1e)), -1.0f);
        if (demo) demo->Message(0x3f6, 0.0f);
        break;
    case 0x3f6:
        if (end_game_timeout_timer) {
            timer::kill(end_game_timeout_timer);
            EndGame_timeout(0, this);
            pb::mode_change(1);
        }
        if (light_show_timer == 0) {
            unknown_p3e = 0;
            Message(0x400, 0.0f);
            TBall* ball = ball_list[0];
            ball->position.y = 0.0f;
            ball->position.x = 0.0f;
            ball->position.z = -0.8f;  // 0xbf4ccccd
            int players = ftol(std::floor(static_cast<double>(value)));
            player_count = players < 1 ? 1 : players > 4 ? 4 : players;
            current_player = 0;
            cur_score_struct = player_scores[0].score_struct;
            cur_score = 0;
            score::set(player_scores[0].score_struct, 0);
            score_multiplier = 0;
            for (int i = 1; i < player_count; ++i) {
                score::set(player_scores[i].score_struct, 0);
                player_scores[i].score = 0;
                player_scores[i].score_e9 = 0;
                player_scores[i].ball_count = max_ball_count;
                player_scores[i].extra_balls = extra_balls;
                player_scores[i].ball_locked_counter = ball_locked_counter;
                player_scores[i].jackpot_score = score_special3;
            }
            ball_count = max_ball_count;
            ChangeBallCount(ball_count);
            score::set(score_player_number1, current_player + 1);
            score::update(score_player_number1);
            for (int i = player_count; i < 4; ++i) score::set(player_scores[i].score_struct, -1);
            score_special3_flag = 0;
            score_special2_flag = 0;
            unknown_p71 = 0;
            info_box()->Clear();
            mission_box()->Clear();
            light_group->Message(0x1c, 0.2f);
            float duration = loader::play_sound(sound_index1);
            light_show_timer = timer::set(duration, this, LightShowTimer);
        } else {
            timer::kill(light_show_timer);
            light_show_timer = 0;
            Message(0x3f5, 0.0f);
        }
        break;
    case 0x3fa:
        if (replay_timer) timer::kill(replay_timer);
        replay_timer = timer::set(static_cast<float>(std::floor(static_cast<double>(value))), this, replay_timer_callback);
        replay_active_flag = 1;
        break;
    case 0x3fd:
        if (player_count < 2) {
            info_box()->Display(control::get_rc_string(demo_text(this, 0x1a, 0x1e)), -1.0f);
        } else {
            int next = (current_player + 1) % player_count;
            if (player_scores[next].ball_count > 0) {
                PlayerData& cur = player_scores[current_player];
                cur.score = cur_score;
                cur.score_e9 = cur_score_e9;
                cur.ball_count = ball_count;
                cur.extra_balls = extra_balls;
                cur.ball_locked_counter = ball_locked_counter;
                cur.jackpot_score = score_special3;
                const PlayerData& n = player_scores[next];
                cur_score = n.score;
                cur_score_e9 = n.score_e9;
                ball_count = n.ball_count;
                extra_balls = n.extra_balls;
                ball_locked_counter = n.ball_locked_counter;
                score_special3 = n.jackpot_score;
                cur_score_struct = n.score_struct;
                score::set(cur_score_struct, cur_score);
                cur_score_struct->dirty_flag = 1;
                ChangeBallCount(ball_count);
                score::set(score_player_number1, next + 1);
                score::update(score_player_number1);
                for (TPinballComponent* c : component_list) c->Message(0x3fc, static_cast<float>(next));
                static const int kText[4][2] = {{0x1a, 0x1e}, {0x1b, 0x1f}, {0x1c, 0x20}, {0x1d, 0x21}};
                info_box()->Display(control::get_rc_string(demo_text(this, kText[next][0], kText[next][1])), -1.0f);
                score_special3_flag = 0;
                score_special2_flag = 0;
                unknown_p71 = 0;
                current_player = next;
            }
        }
        break;
    case 0x3fe:
        loader::play_sound(sound_index2);
        mission_box()->Clear();
        info_box()->Display(control::get_rc_string(0x22), -1.0f);
        end_game_timeout_timer = timer::set(3.0f, this, EndGame_timeout);
        break;
    case 0x400:
        for (int i = static_cast<int>(component_list.size()) - 1; i >= 0; --i) component_list[i]->Message(0x400, 0.0f);
        if (replay_timer) timer::kill(replay_timer);
        replay_timer = 0;
        if (light_show_timer) {
            timer::kill(light_show_timer);
            light_group->Message(0x22, 0.0f);
        }
        light_show_timer = 0;
        score_multiplier = 0;
        score_added = 0;
        score_special1 = 0;
        score_special2 = 10000;
        score_special2_flag = 0;
        score_special3 = 20000;
        score_special3_flag = 0;
        unknown_p71 = 0;
        extra_balls = 0;
        unknown_p75 = 0;
        ball_locked_counter = 0;
        multiball_flag = 0;
        unknown_p78 = 0;
        replay_active_flag = 0;
        replay_timer = 0;
        tilt_lock = 0;
        break;
    default:
        break;
    }
    control::fn_0100c0be(code);
    return 0;
}

// ---- Loop fisika ------------------------------------------------------------

namespace pb {

TPinballTable* g_main_table = nullptr;
float g_time_now = 0.0f;
float g_ball_speed_limit = 0.0f;

void create_table(const DatFile* dat, const std::string& data_dir) {
    loader::init(dat, data_dir);

    // FUN_01015424: kamera dari "camera_info", lalu buffer render.
    int size_group = loader::query_handle("table_size");
    const DatEntry* size = dat->find(size_group, EntryType::ShortArray);
    int16_t dims[2];
    std::memcpy(dims, size->raw.data(), 4);
    const DatEntry* cam = dat->find(loader::query_handle("camera_info"), EntryType::FloatArray);
    const float* c = reinterpret_cast<const float*>(cam->raw.data());
    proj::init(c, c[12], dims[0] * 0.5f, dims[1] * 0.5f);
    render::init(c[13], c[14], dims[0], dims[1]);
    const DatEntry* panel = dat->find("background", EntryType::Bitmap8);
    render::paint_background(*panel->bitmap, panel->bitmap->x, panel->bitmap->y);
    timer::init(150);
    // String 158 "1 UseBitmapFont": versi XP memakai font bitmap PBMSG_FT dari resource exe.
    set_text_box_font(score::load_msg_font());

    g_main_table = new TPinballTable();
    control::g_hook_score_struct_score = [](score_struct* s) { return s->score; };
    control::g_hook_highscore_score = [](int i) { return highscore::g_table[i].score; };
    mode_change(3);  // FUN_01015424: DAT_0102556c == 0 -> mode 3
    g_ball_speed_limit = g_main_table->ball_list[0]->offset * 200.0f;
}

double collide(float time_now, float dt, TBall* ball) {
    if (!ball->active_flag || ball->collision_comp) return dt;
    if (g_ball_speed_limit < ball->speed) ball->speed = g_ball_speed_limit;
    float speed = ball->speed;
    ball->time_delta = dt;
    float ray_len = static_cast<float>(static_cast<double>(dt) * ball->speed);
    ball->ray_max_distance = ray_len;

    ray_type ray;
    ray.origin = ball->position;
    ray.direction = ball->direction;
    ray.max_distance = ray_len;
    ray.min_distance = 0.002f;
    ray.time_now = time_now;
    ray.time_delta = dt;
    ray.field_flag = ball->field_flag;
    ball->time_now = time_now;

    TEdgeSegment* edge = nullptr;
    float distance = static_cast<float>(g_edge_manager->FindCollisionDistance(ray, ball, &edge));
    ball->edge_collision_count = 0;
    if (distance >= 1e9f) {
        float len = static_cast<float>(static_cast<double>(dt) * ball->speed);
        ball->ray_max_distance = len;
        vector2 move{static_cast<float>(static_cast<double>(len) * ball->direction.x),
                     static_cast<float>(static_cast<double>(len) * ball->direction.y)};
        vector2 pos{ball->position.x, ball->position.y};
        maths::vector_add(pos, move);
        ball->position.x = pos.x;
        ball->position.y = pos.y;
    } else {
        edge->EdgeCollision(ball, distance);
        if (speed > 1e-9f) return std::fabs(static_cast<double>(distance) / speed);
    }
    return dt;
}

void timed_frame(float time_now, float dt, bool draw_balls) {
    TPinballTable* table = g_main_table;
    for (TBall* ball : table->ball_list) {
        if (!ball->active_flag) continue;
        if (!ball->collision_comp) {
            if (table->active_flag) {
                vector2 accel{0.0f, 0.0f};
                g_edge_manager->FieldEffects(ball, accel);
                accel.x = accel.x * dt;
                accel.y = accel.y * dt;
                ball->direction.x = ball->speed * ball->direction.x;
                ball->direction.y = ball->speed * ball->direction.y;
                vector2 d{ball->direction.x, ball->direction.y};
                maths::vector_add(d, accel);
                ball->speed = static_cast<float>(maths::normalize_2d(d));
                ball->direction.x = d.x;
                ball->direction.y = d.y;
                ball->inv_dir.x = ball->direction.x == 0.0f ? 1e9f : 1.0f / ball->direction.x;
                ball->inv_dir.y = ball->direction.y == 0.0f ? 1e9f : 1.0f / ball->direction.y;
            }
            float t = time_now;
            float left = dt;
            float remaining = dt;
            for (int i = 10; remaining > 1e-6f && i; --i) {
                double used = collide(t, left, ball);
                remaining = static_cast<float>(left - used);
                t = static_cast<float>(used + t);
                left = remaining;
            }
        } else {
            ball->time_delta = dt;
            vector2 unused;
            ball->collision_comp->FieldEffect(ball, &unused);
        }
    }
    if (draw_balls)
        for (TBall* ball : table->ball_list)
            if (ball->active_flag) ball->Repaint();
}

namespace {
int g_mode = 0;              // DAT_01025568
int g_mode_timer = 0;        // DAT_01025574
float g_nudge_count = 0.0f;  // DAT_0102564c
int g_nudge_left = 0, g_nudge_right = 0, g_nudge_bottom = 0;  // DAT_0102563c/40/44
int g_nudge_timer = 0;       // DAT_01025648
int g_key_left = 90, g_key_right = 191, g_key_plunger = 32;              // DAT_01028238/3c/40
int g_key_bump_left = 88, g_key_bump_right = 190, g_key_bump_bottom = 38;  // DAT_01028244/48/4c
int g_render_offset_x = 0, g_render_offset_y = 0;  // DAT_010253ac/b0

// FUN_01014743: mode 3/4 = jeda tanpa fisika.
bool mode_blocks_frame(int dt_ms) {
    if (g_mode > 0) {
        if (g_mode < 3) return false;
        if (g_mode == 3 || g_mode == 4) {
            g_mode_timer -= dt_ms;
            if (-1 < g_mode_timer && -1 < dt_ms) return true;
            mode_change(g_mode == 3 ? 4 : 1);
        }
    }
    return true;
}

// FUN_01014dcf: dorong meja (nudge). Ubah arah bola dan geser tampilan.
void nudge(float x, float y) {
    vector2 push{static_cast<float>(x * 0.5), static_cast<float>(y * 0.5)};
    for (TBall* ball : g_main_table->ball_list) {
        if (!ball->active_flag || ball->collision_comp) continue;
        ball->direction.x = ball->direction.x * ball->speed;
        ball->direction.y = ball->direction.y * ball->speed;
        vector2 d{ball->direction.x, ball->direction.y};
        maths::vector_add(d, push);
        ball->speed = static_cast<float>(maths::normalize_2d(d));
        ball->direction.x = d.x;
        ball->direction.y = d.y;
        ball->inv_dir.x = d.x == 0.0f ? 1e9f : 1.0f / d.x;
        ball->inv_dir.y = d.y == 0.0f ? 1e9f : 1.0f / d.y;
    }
    g_render_offset_y += ftol(std::floor(0.5 - y));
    g_render_offset_x += ftol(std::floor(x + 0.5));
}

void un_nudge_right(int, void*) {
    if (g_nudge_left) nudge(2.0f, -1.0f);  // 01014eee
    g_nudge_left = 0;
}
void un_nudge_left(int, void*) {
    if (g_nudge_right) nudge(-2.0f, -1.0f);  // 01014f20
    g_nudge_right = 0;
}
void un_nudge_up(int, void*) {
    if (g_nudge_bottom) nudge(0.0f, -1.0f);  // 01014ff6
    g_nudge_bottom = 0;
}
void nudge_right() {  // 01014f52
    nudge(-2.0f, 1.0f);
    if (g_nudge_timer) timer::kill(g_nudge_timer);
    g_nudge_timer = timer::set(0.4f, nullptr, un_nudge_right);
    g_nudge_left = 1;
}
void nudge_left() {  // 01014fa4
    nudge(2.0f, 1.0f);
    if (g_nudge_timer) timer::kill(g_nudge_timer);
    g_nudge_timer = timer::set(0.4f, nullptr, un_nudge_left);
    g_nudge_right = 1;
}
void nudge_up() {  // 01015024
    nudge(0.0f, 1.0f);
    if (g_nudge_timer) timer::kill(g_nudge_timer);
    g_nudge_timer = timer::set(0.4f, nullptr, un_nudge_up);
    g_nudge_bottom = 1;
}
}  // namespace

int g_demo_mode = 0;   // DAT_01025570
int g_paused = 0;      // DAT_01024fd8
int g_cheat_mode = 0;  // DAT_01024ff8
int g_players = 1;     // DAT_01028234
void (*g_on_menu_state)(int mode) = nullptr;
void (*g_music_hook)(bool play) = nullptr;

int mode_change(int mode) {  // 0101461a
    TPinballTable* t = g_main_table;
    if (mode == 1) {
        if (t && t->demo) t->demo->active_flag = g_demo_mode ? 1 : 0;
    } else if (mode == 2) {
        if (t && t->light_group) t->light_group->Message(0x1d, 1.4f);
    } else if (mode == 3 || mode == 4) {
        g_mode_timer = 5000;
    }
    g_mode = mode;
    if (g_on_menu_state) g_on_menu_state(mode);
    return 1;
}

int game_mode() { return g_mode; }

void render_offset(int& x, int& y) {
    x = g_render_offset_x;
    y = g_render_offset_y;
}

void end_game() {  // 0101479b
    mode_change(2);
    TPinballTable* t = g_main_table;
    int n = t->player_count;
    unsigned scores[4];
    int index[4];
    for (int i = 0; i < n; ++i) {
        scores[i] = static_cast<unsigned>(t->player_scores[i].score_struct->score);
        index[i] = i;
    }
    for (int i = 0; i < n; ++i)
        for (int j = i; j < n; ++j)
            if (scores[i] < scores[j]) {
                std::swap(scores[i], scores[j]);
                std::swap(index[i], index[j]);
            }
    if (!g_demo_mode && t->unknown_p3e == 0)
        for (int i = 0; i < n; ++i) {
            int pos = highscore::get_position(static_cast<int>(scores[i]));
            if (pos >= 0) highscore::place_new(static_cast<int>(scores[i]), pos, control::get_rc_string(index[i] + 0x1a));
        }
}

int chk_highscore() {  // 01014908
    if (g_demo_mode) return 0;
    for (int i = g_main_table->player_count - 1; i >= 0; --i)
        if (highscore::get_position(g_main_table->player_scores[i].score_struct->score) >= 0) return 1;
    return 0;
}

void replay_level(int demo) {  // 010153ae
    g_demo_mode = demo;
    mode_change(1);
    if (g_music_hook) g_music_hook(true);
    g_main_table->Message(0x3f6, static_cast<float>(g_players));
}

void toggle_demo() {  // 01015802
    if (g_demo_mode) {
        g_demo_mode = 0;
        g_main_table->Message(0x400, 0.0f);
        mode_change(2);
        control::mission_text_box->Clear();
        control::info_text_box->Display(control::get_rc_string(0x18), -1.0f);
        return;
    }
    replay_level(1);
}

void pause_continue() {  // 010156e4
    g_paused = g_paused == 0;
    control::info_text_box->Clear();
    control::mission_text_box->Clear();
    if (g_paused) {
        g_main_table->Message(0x3f0, g_time_now);
        control::info_text_box->Display(control::get_rc_string(0x16), -1.0f);
        if (g_music_hook) g_music_hook(false);
        return;
    }
    g_main_table->Message(0x3f1, 0.0f);
    if (!g_demo_mode) {
        if (g_mode == 2)
            control::info_text_box->Display(control::get_rc_string(0x18), -1.0f);
        else
            control::info_text_box->Display(control::get_rc_string(0x17), 5.0f);
    }
    if (g_music_hook && !g_paused) g_music_hook(true);
}

void ball_set(float, float) {}

void firsttime_setup() {  // 01014d59
    if (g_main_table->tilt_lock) control::info_text_box->Clear();
    g_main_table->tilt_lock = 0;
    g_nudge_count = -2.0f;  // 0xc0000000
}

void frame(int dt_ms) {  // 01014bf9
    if (dt_ms > 100) dt_ms = 100;
    float dt = static_cast<float>(dt_ms * 0.001);
    if (mode_blocks_frame(dt_ms)) return;
    float next = g_time_now + dt;
    timed_frame(g_time_now, dt, true);
    g_time_now = next;
    timer::g_now_ms += dt_ms;
    if (!g_nudge_left && !g_nudge_right && !g_nudge_bottom) {
        g_nudge_count -= dt;
        if (g_nudge_count <= 0.0f) g_nudge_count = 0.0f;
    } else {
        g_nudge_count = static_cast<float>(dt * 4.0 + g_nudge_count);
    }
    timer::check();
    render::update();
    score::update(g_main_table->cur_score_struct);
    if (!g_main_table->tilt_lock) {
        if (0.5f < g_nudge_count) control::info_text_box->Display(control::get_rc_string(0x19), 2.0f);
        if (1.0f < g_nudge_count) g_main_table->tilt(g_time_now);
    }
}

void keydown(int vk) {  // 01015072
    if (g_paused || g_demo_mode) return;
    if (g_mode != 1) {
        mode_blocks_frame(-1);
        return;
    }
    control::fn_0100e1b0(vk);
    TPinballTable* t = g_main_table;
    if (vk == g_key_left) {
        t->Message(1000, g_time_now);
    } else if (vk == g_key_right) {
        t->Message(0x3ea, g_time_now);
    } else if (vk == g_key_plunger) {
        t->Message(0x3ec, g_time_now);
    } else if (vk == g_key_bump_left) {
        if (!t->tilt_lock) nudge_left();
    } else if (vk == g_key_bump_right) {
        if (!t->tilt_lock) nudge_right();
    } else if (vk == g_key_bump_bottom) {
        if (!t->tilt_lock) nudge_up();
    } else if (control::g_dat_01024ff8 && vk == 'B') {
        TBall* ball = nullptr;
        for (TBall* b : t->ball_list)
            if (!b->active_flag) {
                ball = b;
                break;
            }
        if (!ball) {
            ball = new TBall(t);
            t->ball_list.push_back(ball);
        }
        ball->position.x = 1.0f;
        ball->active_flag = 1;
        ball->position.z = ball->offset;
        ball->position.y = 1.0f;
        ball->direction = {0.0f, 0.0f, 0.0f};
    } else if (control::g_dat_01024ff8 && vk == 'R') {
        control::fn_0100e111();
    }
}

void keyup(int vk) {  // 010152e4
    if (g_mode != 1 || g_paused || g_demo_mode) return;
    TPinballTable* t = g_main_table;
    if (vk == g_key_left)
        t->Message(0x3e9, g_time_now);
    else if (vk == g_key_right)
        t->Message(0x3eb, g_time_now);
    else if (vk == g_key_plunger)
        t->Message(0x3ed, g_time_now);
    else if (vk == g_key_bump_left)
        un_nudge_left(0, nullptr);
    else if (vk == g_key_bump_right)
        un_nudge_right(0, nullptr);
    else if (vk == g_key_bump_bottom)
        un_nudge_up(0, nullptr);
}

void set_keys(int left, int right, int plunger, int bump_left, int bump_right, int bump_bottom) {
    g_key_left = left;
    g_key_right = right;
    g_key_plunger = plunger;
    g_key_bump_left = bump_left;
    g_key_bump_right = bump_right;
    g_key_bump_bottom = bump_bottom;
}

}  // namespace pb

namespace highscore {
Entry g_table[5] = {};

int get_position(int score) {
    if (score <= 0) return -1;
    for (int i = 0; i < 5; ++i)
        if (g_table[i].score < score) return i;
    return -1;
}

void place_new(int score, int pos, const char* name) {
    for (int i = 4; i > pos; --i) g_table[i] = g_table[i - 1];
    std::snprintf(g_table[pos].name, sizeof(g_table[pos].name), "%s", name);
    g_table[pos].score = score;
}
}  // namespace highscore
