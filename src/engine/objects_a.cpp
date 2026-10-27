#include "objects_a.h"

#include <cmath>

#include "ball.h"
#include "table.h"
#include "timer.h"

namespace {

constexpr float kThresholdOff = 1e9f;  // 0x4e6e6b28

// Jumlah elemen objlist bitmap (data+4 di objlist asli).
int bitmap_count(const TPinballComponent* c) { return static_cast<int>(c->list_bitmap.size()); }

}  // namespace

// ---- TOneway ----------------------------------------------------------------

TOneway::TOneway(TPinballTable* table, int group) : TCollisionComponent(table, group, false) {
    visual_struct v;
    loader::query_visual(group, 0, v);
    if (v.float_arr_count == 2) {
        vector2 p0{v.float_arr[0], v.float_arr[1]};
        vector2 p1{v.float_arr[2], v.float_arr[3]};
        // Sisi pantul: offset +jari-jari bola.
        auto* l = new TLine(this, &active_flag, v.collision_group, p0, p1);
        l->Offset(table->collision_comp_offset);
        l->place_in_grid();
        edge_list.push_back(l);
        // Sisi tembus: arah dibalik, offset -(r*0.8) (asm 0x010192d6: dihitung double lalu float).
        line = new TLine(this, &active_flag, v.collision_group, p1, p0);
        line->Offset(static_cast<float>(-(static_cast<double>(table->collision_comp_offset) * 0.8)));
        line->place_in_grid();
        edge_list.push_back(line);
    }
}

void TOneway::Collision(TBall* ball, vector2* next_position, vector2* direction, float distance, TEdgeSegment* edge) {
    if (edge == line) {
        // Bola menembus: lanjutkan ray, jangan tabrak edge ini lagi di langkah ini.
        ball->not_already_hit(edge);
        ball->position.x = next_position->x;
        ball->position.y = next_position->y;
        ball->ray_max_distance -= distance;
        if (!pinball_table->tilt_lock) {
            if (hard_hit_sound) loader::play_sound(hard_hit_sound);
            control_handler(0x3f, this);
        }
    } else if (!pinball_table->tilt_lock) {
        double proj = basic_collision(ball, next_position, direction, elasticity, smoothness, threshold, boost);
        if (0.2 < proj && soft_hit_sound) loader::play_sound(soft_hit_sound);
    } else {
        basic_collision(ball, next_position, direction, elasticity, smoothness, kThresholdOff, 0.0f);
    }
}

int TOneway::get_scoring(int index) { return index < 6 ? scoring[index] : 0; }

void TOneway::put_scoring(int index, int score) {
    if (index < 6) scoring[index] = score;
}

// ---- TBumper ----------------------------------------------------------------

TBumper::TBumper(TPinballTable* table, int group) : TCollisionComponent(table, group, true) {
    bmp_index = 0;
    timer = 0;
    timer_time = *loader::query_float_attribute(group, 0, 0x197);
    visual_struct v;
    loader::query_visual(group, 0, v);
    sound_index4 = v.sound_index2;
    sound_index5 = v.sound_index3;
    original_threshold = threshold;
}

int TBumper::Message(int code, float value) {
    switch (code) {
    case 0xb: {
        // Set level bumper; tiap level punya 2 bitmap (normal, kena).
        int next = ftol(std::floor(static_cast<double>(value)));
        int max = bitmap_count(this) - 1;
        if (max < next * 2) next = max / 2;
        if (next < 0) next = 0;
        if (next != bmp_index) {
            if (bmp_index <= next) loader::play_sound(sound_index4);
            if (next < bmp_index) loader::play_sound(sound_index5);
            bmp_index = next;
            Fire();
            control_handler(0xb, this);
        }
        break;
    }
    case 0xc: {
        int next = bmp_index + 1;
        int max = bitmap_count(this) - 1;
        if (max < next * 2) next = max / 2;
        TBumper::Message(0xb, static_cast<float>(next));
        break;
    }
    case 0xd: {
        int next = bmp_index - 1;
        if (next < 0) next = 0;
        TBumper::Message(0xb, static_cast<float>(next));
        break;
    }
    case 0x3fc: {
        int cur = pinball_table->current_player;
        player_data[cur].bmp_index = bmp_index;
        player_data[cur].message_field = message_field;
        int p = ftol(std::floor(static_cast<double>(value)));
        bmp_index = player_data[p].bmp_index;
        message_field = player_data[p].message_field;
        TBumper::Message(0xb, static_cast<float>(bmp_index));
        break;
    }
    case 0x400:
        if (timer) {
            timer::kill(timer);
            TimerExpired(timer, this);
        }
        bmp_index = 0;
        message_field = 0;
        for (int i = 0; i < pinball_table->player_count; ++i) {
            player_data[i].bmp_index = 0;
            player_data[i].message_field = 0;
        }
        TimerExpired(0, this);
        break;
    default:
        break;
    }
    return 0;
}

void TBumper::Collision(TBall* ball, vector2* next_position, vector2* direction, float, TEdgeSegment*) {
    if (DefaultCollision(ball, next_position, direction)) {
        Fire();
        control_handler(0x3f, this);
    }
}

int TBumper::get_scoring(int index) { return index < 4 ? scoring[index] : 0; }

void TBumper::put_scoring(int index, int score) {
    if (index < 4) scoring[index] = score;
}

void TBumper::Fire() {
    // Bitmap "kena", matikan kick selama animasi. Timer lama tidak di-kill (sama dengan asli).
    threshold = kThresholdOff;
    SetSpriteState(bmp_index * 2 + 1);
    timer = timer::set(timer_time, this, TimerExpired);
}

void TBumper::TimerExpired(int, void* caller) {
    auto* bump = static_cast<TBumper*>(caller);
    bump->timer = 0;
    bump->SetSpriteState(bump->bmp_index * 2);
    bump->threshold = bump->original_threshold;
}

// ---- TPopupTarget -----------------------------------------------------------

TPopupTarget::TPopupTarget(TPinballTable* table, int group) : TCollisionComponent(table, group, true) {
    timer_time = *loader::query_float_attribute(group, 0, 0x197);
    timer = 0;
}

int TPopupTarget::Message(int code, float value) {
    switch (code) {
    case 0x31:
        // Target turun.
        active_flag = 0;
        render::sprite_set_bitmap(render_sprite_, nullptr);
        break;
    case 0x32:
        // Target naik setelah timer_time (timer lama tidak di-kill, sama dengan asli).
        timer = timer::set(timer_time, this, TimerExpired);
        break;
    case 0x3fc: {
        player_message_field[pinball_table->current_player] = message_field;
        int p = ftol(std::floor(static_cast<double>(value)));
        message_field = player_message_field[p];
        TPopupTarget::Message(message_field != 0 ? 0x31 : 0x32, 0.0f);
        break;
    }
    case 0x400:
        message_field = 0;
        for (int i = 0; i < pinball_table->player_count; ++i) player_message_field[i] = 0;
        if (timer) timer::kill(timer);
        TimerExpired(0, this);
        break;
    default:
        break;
    }
    return 0;
}

void TPopupTarget::Collision(TBall* ball, vector2* next_position, vector2* direction, float, TEdgeSegment*) {
    if (!pinball_table->tilt_lock) {
        double proj = basic_collision(ball, next_position, direction, elasticity, smoothness, threshold, boost);
        if (threshold < proj) {
            if (hard_hit_sound) loader::play_sound(hard_hit_sound);
            Message(0x31, 0.0f);  // virtual
            control_handler(0x3f, this);
        }
    } else {
        basic_collision(ball, next_position, direction, elasticity, smoothness, kThresholdOff, 0.0f);
    }
}

int TPopupTarget::get_scoring(int index) { return index < 3 ? scoring[index] : 0; }

void TPopupTarget::put_scoring(int index, int score) {
    if (index < 3) scoring[index] = score;
}

void TPopupTarget::TimerExpired(int timer_id, void* caller) {
    auto* target = static_cast<TPopupTarget*>(caller);
    target->timer = 0;
    target->active_flag = 1;
    render::sprite_set_bitmap(target->render_sprite_, target->list_bitmap[0]);
    // Suara hanya bila dipanggil oleh timer sungguhan (id != 0).
    if (timer_id && target->soft_hit_sound) loader::play_sound(target->soft_hit_sound);
}

// ---- TSoloTarget ------------------------------------------------------------

TSoloTarget::TSoloTarget(TPinballTable* table, int group) : TCollisionComponent(table, group, true) {
    timer = 0;
    timer_time = 0.1f;
    visual_struct v;
    loader::query_visual(group, 0, v);
    sound_index4 = v.sound_index2;
    TSoloTarget::Message(0x32, 0.0f);
}

int TSoloTarget::Message(int code, float) {
    if (code <= 0x30) return 0;
    if (code > 0x32) {
        if (code != 0x400) return 0;
        if (timer) timer::kill(timer);
        timer = 0;
    }
    active_flag = (code == 0x32 || code == 0x400) ? 1 : 0;
    // Aktif → bitmap[0], mati → bitmap[1].
    if (!list_bitmap.empty()) SetSpriteState(1 - active_flag);
    return 0;
}

void TSoloTarget::Collision(TBall* ball, vector2* next_position, vector2* direction, float, TEdgeSegment*) {
    if (DefaultCollision(ball, next_position, direction)) {
        Message(0x31, 0.0f);  // virtual
        timer = timer::set(timer_time, this, TimerExpired);
        control_handler(0x3f, this);
    }
}

int TSoloTarget::get_scoring(int index) { return index < 1 ? scoring[index] : 0; }

void TSoloTarget::put_scoring(int index, int score) {
    if (index < 1) scoring[index] = score;
}

void TSoloTarget::TimerExpired(int, void* caller) {
    auto* target = static_cast<TSoloTarget*>(caller);
    target->Message(0x32, 0.0f);  // virtual
    target->timer = 0;
}

// ---- TBlocker ---------------------------------------------------------------

TBlocker::TBlocker(TPinballTable* table, int group) : TCollisionComponent(table, group, true) {
    visual_struct v;
    loader::query_visual(group, 0, v);
    sound_index4 = v.sound_index2;
    sound_index3 = v.sound_index3;
    initial_value = 0x37;
    unknown_52 = 5;
    threshold = kThresholdOff;
    timer = 0;
    message_field = 0;
    active_flag = 0;
    render::sprite_set_bitmap(render_sprite_, nullptr);
}

int TBlocker::Message(int code, float value) {
    switch (code) {
    case 0x33:
    case 0x3f3:
    case 0x3fc:
    case 0x400:
        if (timer) {
            timer::kill(timer);
            timer = 0;
        }
        message_field = 0;
        active_flag = 0;
        render::sprite_set_bitmap(render_sprite_, nullptr);
        if (code == 0x33) loader::play_sound(sound_index3);
        break;
    case 0x34:
        active_flag = 1;
        loader::play_sound(sound_index4);
        render::sprite_set_bitmap(render_sprite_, list_bitmap[0]);
        [[fallthrough]];
    case 0x3b:
        // Timer durasi `value` detik (<= 0 → 0).
        if (timer) timer::kill(timer);
        timer = timer::set(value > 0.0f ? value : 0.0f, this, TimerExpired);
        break;
    default:
        break;
    }
    return 0;
}

void TBlocker::TimerExpired(int, void* caller) {
    auto* blocker = static_cast<TBlocker*>(caller);
    blocker->timer = 0;
    control_handler(0x3c, blocker);
}

// ---- TGate ------------------------------------------------------------------

TGate::TGate(TPinballTable* table, int group) : TCollisionComponent(table, group, true) {
    visual_struct v;
    loader::query_visual(group, 0, v);
    sound_index4 = v.sound_index2;
    sound_index3 = v.sound_index3;
    active_flag = 1;
    render::sprite_set_bitmap(render_sprite_, list_bitmap[0]);
    control_handler(0x400, this);
}

int TGate::Message(int code, float) {
    if (code == 0x3fc) return 0;
    if (code == 0x35) {
        active_flag = 0;
        render::sprite_set_bitmap(render_sprite_, nullptr);
        loader::play_sound(sound_index3);
    } else if (code == 0x36 || code == 0x400) {
        active_flag = 1;
        render::sprite_set_bitmap(render_sprite_, list_bitmap[0]);
        if (code == 0x36) loader::play_sound(sound_index4);
    }
    // Semua kode selain 0x3fc diteruskan ke control.
    control_handler(code, this);
    return 0;
}

// ---- TKicker ----------------------------------------------------------------

TKicker::TKicker(TPinballTable* table, int group) : TCollisionComponent(table, group, true) {
    message_field = 0;
    timer = 0;
    kicker_flag = 0;
    timer_time = 0.7f;
    timer_time2 = 0.1f;
    threshold = kThresholdOff;
}

int TKicker::Message(int code, float) {
    if ((code == 0x3f3 || code == 0x400) && timer) {
        timer::kill(timer);
        if (!list_bitmap.empty()) render::sprite_set_bitmap(render_sprite_, nullptr);
        timer = 0;
        kicker_flag = 0;
        threshold = kThresholdOff;
    }
    return 0;
}

void TKicker::Collision(TBall* ball, vector2* next_position, vector2* direction, float, TEdgeSegment*) {
    if (pinball_table->tilt_lock) {
        basic_collision(ball, next_position, direction, elasticity, smoothness, 0.0f, boost);
        return;
    }
    if (kicker_flag == 0) {
        // Bola masuk: tahan, kick setelah timer_time.
        threshold = kThresholdOff;
        kicker_flag = 1;
        timer = timer::set(timer_time, this, TimerExpired);
    }
    if (DefaultCollision(ball, next_position, direction)) kicker_flag = 0;
}

void TKicker::TimerExpired(int, void* caller) {
    auto* kick = static_cast<TKicker*>(caller);
    if (kick->kicker_flag == 0) {
        if (!kick->list_bitmap.empty()) kick->SetSpriteState(0);
        kick->timer = 0;
        control_handler(0x3c, kick);
    } else {
        // Aktifkan kick (threshold 0) dan ulangi tiap timer_time2 sampai bola terpantul.
        kick->threshold = 0.0f;
        kick->timer = timer::set(kick->timer_time2, kick, TimerExpired);
        loader::play_sound(kick->hard_hit_sound);
        if (!kick->list_bitmap.empty()) kick->SetSpriteState(1);
    }
}

// ---- TFlagSpinner -----------------------------------------------------------

TFlagSpinner::TFlagSpinner(TPinballTable* table, int group) : TCollisionComponent(table, group, false) {
    timer = 0;
    visual_struct v;
    loader::query_visual(group, 0, v);
    vector2 p0{v.float_arr[0], v.float_arr[1]};
    vector2 p1{v.float_arr[2], v.float_arr[3]};
    auto* l = new TLine(this, &active_flag, v.collision_group, p1, p0);
    l->place_in_grid();
    edge_list.push_back(l);
    prev_collider = new TLine(this, &active_flag, v.collision_group, p0, p1);
    prev_collider->place_in_grid();
    edge_list.push_back(prev_collider);
    speed_decrement = 0.65f;
    max_speed = 50000.0f;
    min_speed = 5.0f;
    if (const float* f = loader::query_float_attribute(group, 0, 0x4b2)) speed_decrement = *f;
    if (const float* f = loader::query_float_attribute(group, 0, 0x4b0)) max_speed = *f;
    if (const float* f = loader::query_float_attribute(group, 0, 0x4b1)) min_speed = *f;
}

int TFlagSpinner::Message(int code, float) {
    if (code == 0x400) {
        if (timer) {
            timer::kill(timer);
            timer = 0;
        }
        bmp_index = 0;
        SetSpriteState(0);
    }
    return 0;
}

void TFlagSpinner::Collision(TBall* ball, vector2* next_position, vector2*, float distance, TEdgeSegment* edge) {
    // Bola selalu menembus; kecepatan bola memutar bendera.
    ball->position.x = next_position->x;
    ball->position.y = next_position->y;
    ball->ray_max_distance -= distance;
    ball->not_already_hit(edge);
    bmp_index_step = edge != prev_collider ? 1 : -1;
    if (ball->speed == 0.0f)
        speed = min_speed;
    else
        speed = static_cast<float>(static_cast<double>(ball->speed) * 20.0);
    if (speed < min_speed) speed = min_speed;
    if (max_speed < speed) speed = max_speed;
    NextFrame();
}

int TFlagSpinner::get_scoring(int index) { return index < 2 ? scoring[index] : 0; }

void TFlagSpinner::put_scoring(int index, int score) {
    if (index < 2) scoring[index] = score;
}

void TFlagSpinner::NextFrame() {
    bmp_index += bmp_index_step;
    int count = bitmap_count(this);
    if (bmp_index >= count)
        bmp_index = 0;
    else if (bmp_index < 0)
        bmp_index = count - 1;
    if (!pinball_table->tilt_lock) {
        control_handler(0x3f, this);
        if (soft_hit_sound) loader::play_sound(soft_hit_sound);
        if (bmp_index == 0) control_handler(0x3e, this);
    }
    SetSpriteState(bmp_index);
    // asm 0x01015fe0: perbandingan & 1/x memakai hasil kali di register (double), field disimpan float.
    double s = static_cast<double>(speed_decrement) * speed;
    speed = static_cast<float>(s);
    if (min_speed <= s) {
        // Bug asli: id timer tidak disimpan ke field timer (+0x66).
        timer::set(static_cast<float>(1.0 / s), this, SpinTimer);
    }
}

void TFlagSpinner::SpinTimer(int, void* caller) {
    auto* flag = static_cast<TFlagSpinner*>(caller);
    flag->timer = 0;
    flag->NextFrame();
}

// ---- Pabrik -----------------------------------------------------------------

TPinballComponent* create_object_a(TPinballTable* table, int type, int group) {
    switch (type) {
    case 0x3ed: return new TBumper(table, group);
    case 0x3ee: return new TPopupTarget(table, group);
    case 0x3f3: return new TBlocker(table, group);
    case 0x3f5: return new TGate(table, group);
    case 0x3f6: return new TKicker(table, group);
    case 0x3f8: return new TOneway(table, group);
    case 0x3fa: return new TFlagSpinner(table, group);
    case 0x3fb: return new TSoloTarget(table, group);
    default: return nullptr;
    }
}
