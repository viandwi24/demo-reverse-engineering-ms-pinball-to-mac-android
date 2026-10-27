#include "lights.h"

#include <cmath>

#include "table.h"
#include "timer.h"

namespace {
int floor_int(float v) { return ftol(std::floor(static_cast<double>(v))); }
}  // namespace

// ---- flasher ----------------------------------------------------------------

void flasher_type::callback(int, void* caller) {
    auto* f = static_cast<flasher_type*>(caller);
    int old = f->bmp_index;
    f->bmp_index = 1 - old;
    render::sprite_set_bitmap(f->sprite, f->bmp_arr[1 - old]);
    f->timer = timer::set(f->timer_delay[f->bmp_index], f, callback);
}

void flasher_type::start(int state) {
    bmp_index = state;
    callback(0, this);
}

void flasher_type::stop(int state) {
    if (timer) timer::kill(timer);
    timer = 0;
    if (state >= 0) {
        bmp_index = state;
        render::sprite_set_bitmap(sprite, bmp_arr[state]);
    }
}

// ---- TLight -----------------------------------------------------------------

TLight::TLight(TPinballTable* table, int group) : TPinballComponent(table, group, true) {
    timeout_timer = 0;
    flasher_on = 0;
    undo_override_timer = 0;
    flasher.timer = 0;
    Reset();
    float on_time = *loader::query_float_attribute(group, 0, 900);
    flasher.timer_delay[0] = on_time;
    source_delay[0] = on_time;
    float off_time = *loader::query_float_attribute(group, 0, 0x385);
    flasher.timer_delay[1] = off_time;
    source_delay[1] = off_time;
}

void TLight::Reset() {
    if (timeout_timer) timer::kill(timeout_timer);
    if (undo_override_timer) timer::kill(undo_override_timer);
    if (flasher_on) flasher.stop(-1);
    unknown7a = 1.0f;
    timeout_timer = 0;
    undo_override_timer = 0;
    light_on = 0;
    bmp_index = 0;
    toggled_off = 0;
    toggled_on = 0;
    flasher_on = 0;
    turn_off_after_flashing = 0;
    render::sprite_set_bitmap(render_sprite_, nullptr);
    flasher.sprite = render_sprite_;
    flasher.bmp_arr[0] = nullptr;
    if (!list_bitmap.empty()) flasher.bmp_arr[1] = list_bitmap[0];
    flasher.unknown4 = 0;
    flasher.unknown3 = 0;
    message_field = 0;
}

void TLight::schedule_timeout(float time) {
    flasher.timer_delay[0] = source_delay[0];
    flasher.timer_delay[1] = source_delay[1];
    if (timeout_timer) timer::kill(timeout_timer);
    timeout_timer = 0;
    if (time > 0.0f) timeout_timer = timer::set(time, this, TimeoutTimer);
}

void TLight::TimeoutTimer(int, void* caller) {
    auto* l = static_cast<TLight*>(caller);
    if (l->flasher_on) l->flasher.stop(-1);
    render::sprite_set_bitmap(l->render_sprite_, l->flasher.bmp_arr[l->light_on]);
    l->toggled_off = 0;
    l->toggled_on = 0;
    l->flasher_on = 0;
    l->timeout_timer = 0;
    if (l->turn_off_after_flashing) {
        l->turn_off_after_flashing = 0;
        l->Message(0x14, 0.0f);
    }
    if (l->control) control_handler(0x3c, l);
}

int TLight::Message(int code, float value) {
    if (code > 0x3fc) {
        if (code != 0x400) return 0;
        Reset();
        for (int i = 0; i < pinball_table->player_count; ++i) {
            player_data[i].flasher_on = flasher_on;
            player_data[i].bmp_index = bmp_index;
            player_data[i].light_on = light_on;
            player_data[i].message_field = message_field;
        }
        return 0;
    }
    if (code == 0x3fc) {
        PlayerState& cur = player_data[pinball_table->current_player];
        cur.flasher_on = flasher_on;
        cur.bmp_index = bmp_index;
        cur.light_on = light_on;
        cur.message_field = message_field;
        Reset();
        const PlayerState& next = player_data[floor_int(value)];
        flasher_on = next.flasher_on;
        bmp_index = next.bmp_index;
        light_on = next.light_on;
        message_field = next.message_field;
        if (bmp_index) Message(0xb, static_cast<float>(bmp_index));
        if (light_on) Message(1, 0.0f);
        if (flasher_on) Message(4, 0.0f);
        return 0;
    }

    switch (code) {
    case 0:
        light_on = 0;
        if (flasher_on || toggled_off || toggled_on) return 0;
        render::sprite_set_bitmap(render_sprite_, flasher.bmp_arr[0]);
        break;
    case 1:
        light_on = 1;
        if (flasher_on || toggled_off || toggled_on) return 0;
        render::sprite_set_bitmap(render_sprite_, flasher.bmp_arr[1]);
        break;
    case 2:
        return light_on;
    case 3:
        return flasher_on;
    case 4:
        schedule_timeout(0.0f);
        if (!flasher_on || !flasher.timer) {
            flasher_on = 1;
            toggled_on = 0;
            toggled_off = 0;
            turn_off_after_flashing = 0;
            flasher.start(light_on);
        }
        break;
    case 5:
        flasher.timer_delay[0] = value * source_delay[0];
        flasher.timer_delay[1] = value * source_delay[1];
        break;
    case 6:
        flasher.timer_delay[0] = source_delay[0];
        flasher.timer_delay[1] = source_delay[1];
        break;
    case 7:
        if (!flasher_on) flasher.start(light_on);
        flasher_on = 1;
        toggled_on = 0;
        turn_off_after_flashing = 0;
        toggled_off = 0;
        schedule_timeout(value);
        break;
    case 8:
        if (!toggled_off) {
            if (!flasher_on) {
                render::sprite_set_bitmap(render_sprite_, flasher.bmp_arr[0]);
            } else {
                flasher.stop(0);
                flasher_on = 0;
            }
            toggled_off = 1;
            toggled_on = 0;
        }
        schedule_timeout(value);
        break;
    case 9:
        if (!toggled_on) {
            if (!flasher_on) {
                render::sprite_set_bitmap(render_sprite_, flasher.bmp_arr[1]);
            } else {
                flasher.stop(1);
                flasher_on = 0;
            }
            toggled_on = 1;
            toggled_off = 0;
        }
        schedule_timeout(value);
        break;
    case 0xb: {
        int index = floor_int(value);
        bmp_index = index;
        int count = static_cast<int>(list_bitmap.size());
        if (count < index) bmp_index = count;
        if (bmp_index < 0) bmp_index = 0;
        flasher.bmp_arr[0] = nullptr;
        flasher.bmp_arr[1] = list_bitmap[bmp_index];
        int state = 0;
        if (flasher_on)
            state = flasher.bmp_index;
        else if (!toggled_off)
            state = toggled_on ? 1 : light_on;
        render::sprite_set_bitmap(render_sprite_, flasher.bmp_arr[state]);
        break;
    }
    case 0xc: {
        int count = static_cast<int>(list_bitmap.size());
        int next = bmp_index + 1;
        if (count < next) next = count;
        Message(0xb, static_cast<float>(next));
        break;
    }
    case 0xd: {
        int next = bmp_index - 1;
        if (next < 0) next = 0;
        Message(0xb, static_cast<float>(next));
        break;
    }
    case 0xe:
        if (timeout_timer) timer::kill(timeout_timer);
        timeout_timer = 0;
        if (flasher_on) flasher.stop(-1);
        flasher_on = 0;
        toggled_off = 0;
        toggled_on = 0;
        render::sprite_set_bitmap(render_sprite_, flasher.bmp_arr[light_on]);
        break;
    case 0xf:
        turn_off_after_flashing = 0;
        if (undo_override_timer) timer::kill(undo_override_timer);
        undo_override_timer = 0;
        Message(1, 0.0f);
        Message(7, 0.0f);
        break;
    case 0x10:
        if (undo_override_timer) timer::kill(undo_override_timer);
        undo_override_timer = 0;
        Message(7, value);
        turn_off_after_flashing = 1;
        break;
    case 0x11:
        Message(floor_int(value) != 0 ? 1 : 0, 0.0f);
        return light_on;
    case 0x12:
        Message(0x11, value);
        Message(0xe, 0.0f);
        return light_on;
    case 0x13:
        Message(1, 0.0f);
        Message(0xe, 0.0f);
        break;
    case 0x14:
        Message(0, 0.0f);
        Message(0xe, 0.0f);
        break;
    case 0x15:
        Message(0x11, static_cast<float>(light_on == 0));
        return light_on;
    case 0x16:
        Message(0x12, static_cast<float>(light_on == 0));
        return light_on;
    case 0x17:
        message_field = floor_int(value);
        break;
    default:
        break;
    }
    return 0;
}

// ---- TLightGroup ------------------------------------------------------------

TLightGroup::TLightGroup(TPinballTable* table, int group) : TPinballComponent(table, group, false) {
    timer = 0;
    notify_timer = 0;
    TLightGroup::Reset();
    if (group > 0) {
        timer_time_default = *loader::query_float_attribute(group, 0, 0x387);
        int count = 0;
        const int16_t* ids = loader::query_iattribute(group, 0x403, count);
        for (int i = 0; i < count; ++i) {
            // FUN_010176be: cari komponen menurut indeks grup.
            for (TPinballComponent* c : table->component_list)
                if (c->group_index == ids[i]) {
                    list.push_back(static_cast<TLight*>(c));
                    break;
                }
        }
    }
}

void TLightGroup::Reset() {
    if (timer) timer::kill(timer);
    timer = 0;
    if (notify_timer) timer::kill(notify_timer);
    notify_timer = 0;
    animation_code = 0;
    animation_flag = 0;
    timer_time = timer_time_default;
}

void TLightGroup::TimerFunc(int, void* caller) {
    auto* g = static_cast<TLightGroup*>(caller);
    g->timer = 0;
    g->Message(g->animation_code, g->timer_time);
}

void TLightGroup::NotifyTimerExpired(int, void* caller) {
    auto* g = static_cast<TLightGroup*>(caller);
    g->notify_timer = 0;
    control_handler(0x3d, g);
}

void TLightGroup::reset_timer(float time) {
    if (timer) timer::kill(timer);
    timer = 0;
    if (time <= 0.0f) {
        if (0.0f <= time) {
            animation_code = 0;
            animation_flag = 0;
            return;
        }
        timer_time = timer_time_default;
    } else {
        timer_time = time;
    }
    timer = timer::set(timer_time, this, TimerFunc);
}

int TLightGroup::next_light_up() const {
    for (int i = 0; i < static_cast<int>(list.size()); ++i)
        if (!list[i]->light_on) return i;
    return -1;
}

int TLightGroup::next_light_down() const {
    for (int i = static_cast<int>(list.size()) - 1; i >= 0; --i)
        if (list[i]->light_on) return i;
    return -1;
}

void TLightGroup::start_animation() {
    for (int i = static_cast<int>(list.size()) - 1; i >= 0; --i) list[i]->Message(list[i]->light_on ? 9 : 8, 0.0f);
}

int TLightGroup::Message(int code, float value) {
    const int n = static_cast<int>(list.size());
    auto forward = [&]() {
        for (int i = n - 1; i >= 0; --i) list[i]->Message(code, value);
        return 0;
    };

    if (code > 0x3f1) {
        if (code == 0x3f3 || code == 0x3fe) return 0;
        if (code == 0x3fc) {
            PlayerState& cur = player_data[pinball_table->current_player];
            cur.message_field = message_field;
            cur.animation_code = animation_code;
            cur.timer_time = timer_time;
            Reset();
            const PlayerState& next = player_data[floor_int(value)];
            message_field = next.message_field;
            animation_code = next.animation_code;
            timer_time = next.timer_time;
            if (message_field) TimerFunc(0, this);
            return 0;
        }
        if (code == 0x400) {
            Reset();
            for (int i = 0; i < pinball_table->player_count; ++i) {
                player_data[i].message_field = message_field;
                player_data[i].animation_code = animation_code;
                player_data[i].timer_time = timer_time;
            }
            return 0;
        }
        return forward();
    }
    if (code > 0x3ef) return 0;

    switch (code) {
    case 0x18:
    case 0x19: {
        TLight* edge = code == 0x18 ? list[n - 1] : list[0];
        if (edge->flasher_on || edge->toggled_on || edge->toggled_off) return 0;
        if (animation_code) Message(0x22, 0.0f);
        animation_flag = 1;
        animation_code = code;
        int saved_on = edge->light_on;
        int saved_field = edge->message_field;
        if (code == 0x18) {
            for (int i = n - 1; i > 0; --i) {
                list[i]->Message(list[i - 1]->light_on != 0, 0.0f);
                list[i]->message_field = list[i - 1]->message_field;
            }
            list[0]->Message(saved_on != 0, 0.0f);
            list[0]->message_field = saved_field;
        } else {
            for (int i = 0; i < n - 1; ++i) {
                list[i]->Message(list[i + 1]->light_on != 0, 0.0f);
                list[i]->message_field = list[i + 1]->message_field;
            }
            list[n - 1]->Message(saved_on != 0, 0.0f);
            list[n - 1]->message_field = saved_field;
        }
        reset_timer(value);
        break;
    }
    case 0x1a:
    case 0x1b: {
        if (animation_flag || !animation_code) start_animation();
        animation_code = code;
        animation_flag = 0;
        if (code == 0x1a) {
            int saved = list[n - 1]->toggled_on;
            for (int i = n - 1; i > 0; --i) list[i]->Message((list[i - 1]->toggled_on != 0) + 8, 0.0f);
            list[0]->Message((saved != 0) + 8, 0.0f);
        } else {
            int saved = list[0]->toggled_on;
            for (int i = 0; i < n - 1; ++i) list[i]->Message((list[i + 1]->toggled_on != 0) + 8, 0.0f);
            list[n - 1]->Message((saved != 0) + 8, 0.0f);
        }
        reset_timer(value);
        break;
    }
    case 0x1c:
        if (animation_flag || !animation_code) start_animation();
        animation_code = code;
        animation_flag = 0;
        for (int i = 0; i < n - 1; ++i) {
            if (msvcrt_rand() % 100 > 70) {
                int r = msvcrt_rand();
                list[i]->Message(9, static_cast<float>(r * kRandScale * value * 3.0 + 0.1));
            }
        }
        reset_timer(value);
        break;
    case 0x1d:
        if (animation_flag || !animation_code) start_animation();
        animation_code = code;
        animation_flag = 0;
        for (int i = 0; i < n - 1; ++i) list[i]->Message(0x12, static_cast<float>(msvcrt_rand() % 100 > 70));
        reset_timer(value);
        break;
    case 0x1e:
    case 0x1f: {
        bool want_on = code == 0x1f;  // 0x1e: nyalakan lampu mati acak; 0x1f: matikan lampu nyala acak
        int count = 0;
        for (int i = n - 1; i >= 0; --i)
            if ((list[i]->light_on != 0) == want_on) ++count;
        if (count == 0) return 0;
        int r = msvcrt_rand() % count;
        for (int i = n - 1; i >= 0; --i) {
            if ((list[i]->light_on != 0) != want_on) continue;
            if (r-- == 0) {
                list[i]->Message(want_on ? 0 : 1, 0.0f);
                break;
            }
        }
        if (animation_code) start_animation();
        break;
    }
    case 0x20:
    case 0x21: {
        int i = code == 0x20 ? next_light_up() : next_light_down();
        if (i < 0) return 0;
        list[i]->Message(code == 0x20 ? 1 : 0, 0.0f);
        if (animation_code) start_animation();
        return 1;
    }
    case 0x22:
        if (timer) timer::kill(timer);
        timer = 0;
        if (animation_code == 0x1a || animation_code == 0x1b || animation_code == 0x1c) Message(0xe, 0.0f);
        animation_code = 0;
        animation_flag = 0;
        break;
    case 0x23:
    case 0x24: {
        int i = floor_int(value);
        if (n <= i || i < 0) return 0;
        list[i]->Message(code == 0x23 ? 1 : 0, 0.0f);
        if (animation_code) start_animation();
        break;
    }
    case 0x25: {
        int count = 0;
        for (int i = n - 1; i >= 0; --i)
            if (list[i]->light_on) ++count;
        return count;
    }
    case 0x26:
        return n;
    case 0x27:
        return animation_code;
    case 0x28:
        return animation_flag;
    case 0x29:
    case 0x2a: {
        int i = code == 0x29 ? next_light_up() : next_light_down();
        if (i < 0) return 0;
        if (animation_code || animation_flag) Message(0x22, 0.0f);
        list[i]->Message(code == 0x29 ? 0xf : 0x10, value);
        return 1;
    }
    case 0x2b:
        if (notify_timer) timer::kill(notify_timer);
        notify_timer = 0;
        if (value > 0.0f) notify_timer = timer::set(value, this, NotifyTimerExpired);
        break;
    case 0x2c:
        for (int i = n - 1; i >= 0; --i) {
            if (list[i]->light_on) {
                list[i]->Message(0, 0.0f);
                list[i]->Message(0x10, value);
            }
        }
        break;
    case 0x2d: {
        control_handler(code, this);
        int index = floor_int(value);
        if (index >= 0 && index <= n) {
            int last = n - 1;
            for (int i = last; i > index; --i) list[i]->Message(0x14, 0.0f);
            if (index < last) last = index;
            for (int i = last; i >= 0; --i) list[i]->Message(0x13, 0.0f);
        }
        break;
    }
    case 0x2e: {
        int i = next_light_down();
        if (i >= 0) list[i]->Message(4, 0.0f);
        break;
    }
    default:
        return forward();
    }
    return 0;
}

// ---- TLightBargraph ---------------------------------------------------------

TLightBargraph::TLightBargraph(TPinballTable* table, int group) : TLightGroup(table, group) {
    TLightBargraph::Reset();
    if (group > 0) {
        if (const float* t = loader::query_float_attribute(group, 0, 0x388))
            timer_time_array.assign(t, t + list.size() * 2);
    }
}

void TLightBargraph::Reset() {
    if (timer_bargraph) {
        timer::kill(timer_bargraph);
        timer_bargraph = 0;
    }
    time_index = 0;
    TLightGroup::Reset();
}

void TLightBargraph::BargraphTimerExpired(int, void* caller) {
    auto* b = static_cast<TLightBargraph*>(caller);
    b->timer_bargraph = 0;
    if (b->time_index == 0) {
        b->Message(0x14, 0.0f);
        control_handler(0x2f, b);
    } else {
        b->Message(0x2d, static_cast<float>(b->time_index - 1));
        control_handler(0x3c, b);
    }
}

int TLightBargraph::Message(int code, float value) {
    switch (code) {
    case 0x25:
        return time_index;
    case 0x2d: {
        if (timer_bargraph) {
            timer::kill(timer_bargraph);
            timer_bargraph = 0;
        }
        int index = floor_int(value);
        int max = static_cast<int>(list.size()) * 2;
        if (max <= index) index = max - 1;
        if (index < 0) {
            TLightGroup::Message(0x14, 0.0f);
            time_index = 0;
        } else {
            TLightGroup::Message(0x2d, static_cast<float>(index / 2));
            if ((index & 1) == 0) TLightGroup::Message(0x2e, 0.0f);
            if (!timer_time_array.empty())
                timer_bargraph = timer::set(timer_time_array[index], this, BargraphTimerExpired);
            time_index = index;
        }
        break;
    }
    case 0x3f3:
        Reset();
        break;
    case 0x3fc: {
        if (timer_bargraph) {
            timer::kill(timer_bargraph);
            timer_bargraph = 0;
        }
        player_time_index[pinball_table->current_player] = time_index;
        Reset();
        time_index = player_time_index[floor_int(value)];
        if (time_index) Message(0x2d, static_cast<float>(time_index));
        break;
    }
    case 0x400:
        Reset();
        for (int i = 0; i < pinball_table->player_count; ++i) player_time_index[i] = time_index;
        TLightGroup::Message(0x400, value);
        break;
    default:
        TLightGroup::Message(code, value);
        break;
    }
    return 0;
}
