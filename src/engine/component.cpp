#include "component.h"

#include <cmath>

#include "ball.h"
#include "control/control.h"
#include "table.h"
#include "timer.h"

// ---- TPinballComponent ------------------------------------------------------

TPinballComponent::TPinballComponent(TPinballTable* table, int group_index, bool load_visuals)
    : pinball_table(table) {
    if (table) table->component_list.push_back(this);
    if (group_index >= 0) group_name = loader::query_name(group_index);
    if (load_visuals && group_index >= 0) {
        int states = loader::query_visual_states(group_index);
        for (int i = 0; i < states; ++i) {
            visual_struct v;
            loader::query_visual(group_index, i, v);
            if (v.bitmap) list_bitmap.push_back(v.bitmap);
            if (v.zmap) list_zmap.push_back(v.zmap);
        }
        const ZMap* zmap = list_zmap.empty() ? nullptr : list_zmap[0];
        if (!list_bitmap.empty()) {
            const Bitmap8* first = list_bitmap[0];
            rectangle_type bounds{first->x - table->x_offset, first->y - table->y_offset, first->width, first->height};
            for (size_t i = 1; i < list_bitmap.size(); ++i) {
                const Bitmap8* b = list_bitmap[i];
                rectangle_type r{b->x - table->x_offset, b->y - table->y_offset, b->width, b->height};
                // FUN_010040c2
                int x = bounds.x, y = bounds.y, w = bounds.width, h = bounds.height;
                if (r.x < x) {
                    w += x - r.x;
                    x = r.x;
                }
                if (r.y < y) {
                    h += y - r.y;
                    y = r.y;
                }
                if (w + x < r.x + r.width) w = r.width - x + r.x;
                if (h + y < r.height + r.y) h = r.height - y + r.y;
                bounds = {x, y, w, h};
            }
            render_sprite_ = render::create_sprite(states > 0 ? VisualType::Dynamic : VisualType::Static, first, zmap,
                                                   first->x - table->x_offset, first->y - table->y_offset, &bounds);
        }
    }
    this->group_index = group_index;
}

int TPinballComponent::Message(int code, float) {
    message_field = code;
    if (code == 0x400) message_field = 0;
    return 0;
}

void TPinballComponent::SetSpriteState(int index) {
    const Bitmap8* bmp = list_bitmap[index];
    const ZMap* zmap = index < static_cast<int>(list_zmap.size()) ? list_zmap[index] : nullptr;
    render::sprite_set(render_sprite_, bmp, zmap, bmp->x - pinball_table->x_offset, bmp->y - pinball_table->y_offset);
}

// ---- TCollisionComponent ----------------------------------------------------

TCollisionComponent::TCollisionComponent(TPinballTable* table, int group_index, bool create_wall)
    : TPinballComponent(table, group_index, true) {
    active_flag = 1;
    if (group_name) unknown_c4f = true;
    visual_struct v;
    if (group_index < 1) {
        loader::query_visual(-1, 0, v);  // isi default saja
    } else {
        loader::query_visual(group_index, 0, v);
        if (create_wall) {
            float offset = table->collision_comp_offset;
            const float* f = loader::query_float_attribute(group_index, 0, 600);
            install_wall(f, this, &active_flag, v.collision_group, offset, 0);
        }
    }
    threshold = v.kicker.threshold;
    elasticity = v.elasticity;
    smoothness = v.smoothness;
    boost = v.kicker.boost;
    hard_hit_sound = v.kicker.hard_hit_sound;
    soft_hit_sound = v.sound_index4;
    this->group_index = group_index;
}

void TCollisionComponent::port_draw() {
    for (int i = static_cast<int>(edge_list.size()) - 1; i >= 0; --i) edge_list[i]->port_draw();
}

void TCollisionComponent::Collision(TBall* ball, vector2* next_position, vector2* direction, float, TEdgeSegment*) {
    if (pinball_table->tilt_lock) {
        basic_collision(ball, next_position, direction, elasticity, smoothness, 1e9f, 0.0f);
        return;
    }
    double proj = basic_collision(ball, next_position, direction, elasticity, smoothness, threshold, boost);
    int sound;
    if (proj <= threshold) {
        if (proj <= 0.2) return;
        sound = soft_hit_sound;
    } else {
        sound = hard_hit_sound;
    }
    if (sound) loader::play_sound(sound);
}

int TCollisionComponent::DefaultCollision(TBall* ball, vector2* next_position, vector2* direction) {
    if (pinball_table->tilt_lock) {
        basic_collision(ball, next_position, direction, elasticity, smoothness, 1e9f, 0.0f);
        return 0;
    }
    double proj = basic_collision(ball, next_position, direction, elasticity, smoothness, threshold, boost);
    if (threshold < proj) {
        if (hard_hit_sound) loader::play_sound(hard_hit_sound);
        return 1;
    }
    if (0.2 < proj && soft_hit_sound) loader::play_sound(soft_hit_sound);
    return 0;
}

// ---- TWall ------------------------------------------------------------------

TWall::TWall(TPinballTable* table, int group_index) : TCollisionComponent(table, group_index, true) {
    if (render_sprite_) render::sprite_set_bitmap(render_sprite_, nullptr);
    if (!list_bitmap.empty()) bitmap_hit = list_bitmap[0];
    timer = 0;
}

int TWall::Message(int code, float) {
    if (code == 0x400 && timer) {
        timer::kill(timer);
        TimerExpired(timer, this);
    }
    return 0;
}

void TWall::Collision(TBall* ball, vector2* next_position, vector2* direction, float, TEdgeSegment*) {
    if (DefaultCollision(ball, next_position, direction)) {
        if (bitmap_hit) {
            render::sprite_set_bitmap(render_sprite_, bitmap_hit);
            timer = timer::set(0.1f, this, TimerExpired);
        }
        control_handler(0x3f, this);
    }
}

int TWall::get_scoring(int index) { return index < 1 ? scoring : 0; }

void TWall::put_scoring(int index, int score) {
    if (index < 1) scoring = score;
}

void TWall::TimerExpired(int, void* caller) {
    auto* wall = static_cast<TWall*>(caller);
    render::sprite_set_bitmap(wall->render_sprite_, nullptr);
    wall->timer = 0;
    wall->message_field = 0;
}

// ---- Fungsi bebas -----------------------------------------------------------

double basic_collision(TBall* ball, const vector2* next_position, const vector2* direction, float elasticity,
                       float smoothness, float threshold, float boost) {
    ball->position.x = next_position->x;
    ball->position.y = next_position->y;
    // Urutan pembulatan mengikuti asm 0x01017883: sebagian memakai nilai register (double),
    // sebagian memakai salinan float yang sudah disimpan.
    float nx = direction->x, ny = direction->y;
    double p = -(static_cast<double>(ny) * ball->direction.y + static_cast<double>(nx) * ball->direction.x);
    float proj = static_cast<float>(p);
    if (p <= 0.0) {
        proj = static_cast<float>(-p);
    } else {
        double a = p * nx;
        double b = static_cast<double>(ny) * proj;
        float t1 = static_cast<float>(a + ball->direction.x);
        float t2 = static_cast<float>(b + ball->direction.y);
        float t3 = static_cast<float>(a * elasticity);
        double be = b * elasticity;
        float t2s = static_cast<float>(static_cast<double>(t2) * smoothness);
        ball->direction.x = static_cast<float>(static_cast<double>(t1) * smoothness + t3);
        ball->direction.y = static_cast<float>(t2s + be);
        vector2 d{ball->direction.x, ball->direction.y};
        maths::normalize_2d(d);
        ball->direction.x = d.x;
        ball->direction.y = d.y;
    }
    float reflected = static_cast<float>(static_cast<double>(proj) * ball->speed);
    double speed = ball->speed - (1.0 - elasticity) * reflected;
    ball->speed = static_cast<float>(speed);
    if (!(reflected < threshold)) {
        ball->direction.x = static_cast<float>(speed * ball->direction.x + static_cast<double>(nx) * boost);
        ball->direction.y = static_cast<float>(speed * ball->direction.y + static_cast<double>(ny) * boost);
        vector2 d{ball->direction.x, ball->direction.y};
        ball->speed = static_cast<float>(maths::normalize_2d(d));
        ball->direction.x = d.x;
        ball->direction.y = d.y;
    }
    return reflected;
}

TEdgeSegment* install_wall(const float* f, TCollisionComponent* owner, char* active_flag, unsigned collision_group,
                           float offset, int wall_value) {
    int shape = ftol(std::floor(static_cast<double>(f[0])) - 1.0);  // asm 0x010179bd: floor - 1.0
    const float* pts = f + 1;
    TEdgeSegment* last = nullptr;
    if (shape == 0) {
        vector3 center{pts[0], pts[1], 0.0f};
        auto* c = new TCircle(owner, active_flag, collision_group, center, offset + f[3]);
        c->wall_value = wall_value;
        c->place_in_grid();
        owner->edge_list.push_back(c);
        return c;
    }
    if (shape == 1) {
        auto* l = new TLine(owner, active_flag, collision_group, pts[0], pts[1], pts[2], pts[3]);
        l->wall_value = wall_value;
        l->Offset(offset);
        l->place_in_grid();
        owner->edge_list.push_back(l);
        return l;
    }
    float prev_x = f[shape * 2 - 1], prev_y = f[shape * 2];
    const float* p = pts;
    for (int i = 0; i < shape; ++i, p += 2) {
        float next_x, next_y;
        if (i < shape - 1) {
            next_x = p[2];
            next_y = p[3];
        } else {
            next_x = pts[0];
            next_y = pts[1];
        }
        float cur_x = p[0], cur_y = p[1];
        if (offset != 0.0f) {
            vector3 a{cur_x - prev_x, cur_y - prev_y, 0.0f};
            vector3 b{next_x - cur_x, next_y - cur_y, 0.0f};
            vector3 c;
            maths::cross(a, b, c);
            if ((c.z > 0.0f && offset > 0.0f) || (c.z < 0.0f && offset < 0.0f)) {
                vector3 center{cur_x, cur_y, 0.0f};
                auto* circle = new TCircle(owner, active_flag, collision_group, center, static_cast<float>(offset * 1.001));
                circle->wall_value = wall_value;
                circle->place_in_grid();
                owner->edge_list.push_back(circle);
            }
        }
        auto* line = new TLine(owner, active_flag, collision_group, p[0], p[1], p[2], p[3]);
        last = line;
        line->wall_value = wall_value;
        line->Offset(offset);
        line->place_in_grid();
        owner->edge_list.push_back(line);
        prev_x = cur_x;
        prev_y = cur_y;
    }
    return last;
}

void control_handler(int code, TPinballComponent* caller) { control::handler(code, caller); }
