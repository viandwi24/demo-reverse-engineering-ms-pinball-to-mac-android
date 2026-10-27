#include "collision.h"

#include <cmath>

#include "ball.h"
#include "component.h"

TEdgeManager* g_edge_manager = nullptr;

TEdgeSegment::TEdgeSegment(TCollisionComponent* owner, char* active_flag, unsigned collision_group)
    : collision_component(owner), active_flag(active_flag), collision_group(collision_group) {}

// ---- TCircle ----------------------------------------------------------------

TCircle::TCircle(TCollisionComponent* owner, char* active_flag, unsigned collision_group, const vector3& center, float radius)
    : TEdgeSegment(owner, active_flag, collision_group) {
    circle.radius_sq = radius * radius;
    circle.center = center;
}

void TCircle::EdgeCollision(TBall* ball, float distance) {
    vector2 next{static_cast<float>(static_cast<double>(distance) * ball->direction.x + ball->position.x),
                 static_cast<float>(static_cast<double>(distance) * ball->direction.y + ball->position.y)};
    vector2 normal{next.x - circle.center.x, next.y - circle.center.y};
    maths::normalize_2d(normal);
    collision_component->Collision(ball, &next, &normal, distance, this);
}

void TCircle::place_in_grid() { edges::add_circle_to_grid(circle, this, nullptr); }

double TCircle::FindCollisionDistance(ray_type& ray) { return maths::ray_intersect_circle(ray, circle); }

// ---- TLine ------------------------------------------------------------------

TLine::TLine(TCollisionComponent* owner, char* active_flag, unsigned collision_group, float x0, float y0, float x1, float y1)
    : TEdgeSegment(owner, active_flag, collision_group) {
    start = {x0, y0};
    end = {x1, y1};
    maths::line_init(line, start.x, start.y, end.x, end.y);
}

TLine::TLine(TCollisionComponent* owner, char* active_flag, unsigned collision_group, const vector2& s, const vector2& e)
    : TLine(owner, active_flag, collision_group, s.x, s.y, e.x, e.y) {}

void TLine::EdgeCollision(TBall* ball, float distance) {
    vector2 normal{line.perpendicular.x, line.perpendicular.y};
    collision_component->Collision(ball, &line.ray_intersect, &normal, distance, this);
}

double TLine::FindCollisionDistance(ray_type& ray) { return maths::ray_intersect_line(ray, line); }

void TLine::Offset(float offset) {
    float ox = offset * line.perpendicular.x;
    float oy = offset * line.perpendicular.y;
    start.x = ox + start.x;
    start.y = oy + start.y;
    end.x = ox + end.x;
    end.y = oy + end.y;
    maths::line_init(line, start.x, start.y, end.x, end.y);
}

void TLine::place_in_grid() {
    TEdgeManager* m = g_edge_manager;
    int x0 = m->box_x(start.x), y0 = m->box_y(start.y);
    int x1 = m->box_x(end.x), y1 = m->box_y(end.y);
    int dir_x = start.x < end.x ? 1 : -1;
    int dir_y = start.y < end.y ? 1 : -1;

    if (y0 == y1) {
        if (dir_x == 1)
            for (; x0 <= x1; ++x0) m->add_edge_to_box(x0, y0, this);
        else
            for (; x1 <= x0; --x0) m->add_edge_to_box(x0, y0, this);
        return;
    }
    if (x0 == x1) {
        if (dir_y == 1)
            for (; y0 <= y1; ++y0) m->add_edge_to_box(x0, y0, this);
        else
            for (; y1 <= y0; --y0) m->add_edge_to_box(x0, y0, this);
        return;
    }

    // Garis miring: DDA melalui batas box.
    float slope = (start.y - end.y) / (start.x - end.x);
    float intercept = static_cast<float>(start.y - static_cast<double>(slope) * start.x);
    m->add_edge_to_box(x0, y0, this);
    auto y_at = [&](int bx) {
        return static_cast<float>((static_cast<float>(static_cast<double>(bx) * m->advance_x + m->x_min)) * static_cast<double>(slope) + intercept);
    };
    auto y_line = [&](int by) { return static_cast<float>(static_cast<double>(by) * m->advance_y + m->y_min); };
    if (dir_x == 1) {
        int next_x = x0 + 1;
        if (dir_y == 1) {
            int next_y = y0 + 1;
            do {
                float yl = y_line(next_y), ya = y_at(next_x);
                if (yl <= ya) {
                    if (ya == yl) {
                        ++x0;
                        ++next_x;
                    }
                    ++y0;
                    ++next_y;
                } else {
                    ++x0;
                    ++next_x;
                }
                m->add_edge_to_box(x0, y0, this);
            } while (x0 != x1 || y0 != y1);
        } else {
            do {
                float yl = y_line(y0), ya = y_at(next_x);
                if (ya <= yl) {
                    if (ya == yl) {
                        ++x0;
                        ++next_x;
                    }
                    --y0;
                } else {
                    ++x0;
                    ++next_x;
                }
                m->add_edge_to_box(x0, y0, this);
            } while (x0 != x1 || y0 != y1);
        }
    } else if (dir_y == 1) {
        int next_y = y0 + 1;
        do {
            float yl = y_line(next_y), ya = y_at(x0);
            if (yl <= ya) {
                if (ya == yl) --x0;
                ++y0;
                ++next_y;
            } else {
                --x0;
            }
            m->add_edge_to_box(x0, y0, this);
        } while (x0 != x1 || y0 != y1);
    } else {
        do {
            float yl = y_line(y0), ya = y_at(x0);
            if (ya <= yl) {
                if (ya == yl) --x0;
                --y0;
            } else {
                --x0;
            }
            m->add_edge_to_box(x0, y0, this);
        } while (x0 != x1 || y0 != y1);
    }
}

// ---- TEdgeManager -----------------------------------------------------------

TEdgeManager::TEdgeManager(float x, float y, float width, float height) {
    x_min = x;
    advance_x = static_cast<float>(width * 0.1);
    y_min = y;
    max_box_x = 10;
    advance_y = static_cast<float>(height * 0.06666667);
    max_box_y = 15;
    advance_x_inv = static_cast<float>(1.0 / (width * 0.1));
    advance_y_inv = static_cast<float>(1.0 / (height * 0.06666667));
    boxes_.resize(max_box_x * max_box_y);
}

int TEdgeManager::box_x(float x) const {
    double v = std::floor((static_cast<double>(x) - x_min) * advance_x_inv);
    double hi = max_box_x - 1;
    if (v < hi) hi = v;
    return hi > 0.0 ? ftol(hi) : 0;
}

int TEdgeManager::box_y(float y) const {
    double v = std::floor((static_cast<double>(y) - y_min) * advance_y_inv);
    double hi = max_box_y - 1;
    if (v < hi) hi = v;
    return hi > 0.0 ? ftol(hi) : 0;
}

int TEdgeManager::increment_box_x(int x) const { return x + 1 < max_box_x - 1 ? x + 1 : max_box_x - 1; }
int TEdgeManager::increment_box_y(int y) const { return y + 1 < max_box_y - 1 ? y + 1 : max_box_y - 1; }

void TEdgeManager::add_edge_to_box(int x, int y, TEdgeSegment* edge) { boxes_[max_box_x * y + x].edges.push_back(edge); }

void TEdgeManager::add_field_to_box(int x, int y, field_effect_type* field) {
    boxes_[max_box_x * y + x].fields.push_back(field);
}

void TEdgeManager::FieldEffects(TBall* ball, vector2& accel) {
    Box& box = boxes_[max_box_x * box_y(ball->position.y) + box_x(ball->position.x)];
    for (int i = static_cast<int>(box.fields.size()) - 1; i >= 0; --i) {
        field_effect_type* f = box.fields[i];
        if (*f->active_flag && (f->collision_group & ball->field_flag)) {
            vector2 v;
            if (f->collision_comp->FieldEffect(ball, &v)) maths::vector_add(accel, v);
        }
    }
}

int TEdgeManager::TestGridBox(int x, int y, float* distance, TEdgeSegment** edge_out, ray_type& ray, TBall* ball, int count) {
    if (x < 0 || x >= 10 || y < 0 || y >= 15) return count;
    Box& box = boxes_[max_box_x * y + x];
    for (int i = static_cast<int>(box.edges.size()) - 1; i >= 0; --i) {
        TEdgeSegment* e = box.edges[i];
        if (e->processed_flag || !*e->active_flag || !(ray.field_flag & e->collision_group) || ball->already_hit(e)) continue;
        processed_[count++] = e;
        e->processed_flag = true;
        double d = e->FindCollisionDistance(ray);
        if (d < *distance) {
            *distance = static_cast<float>(d);
            *edge_out = e;
        }
    }
    return count;
}

double TEdgeManager::FindCollisionDistance(ray_type& ray, TBall* ball, TEdgeSegment** edge) {
    float distance = 1e9f;
    int count = 0;
    float ox = ray.origin.x, oy = ray.origin.y;
    int x0 = box_x(ox), y0 = box_y(oy);
    float ex = static_cast<float>(static_cast<double>(ray.direction.x) * ray.max_distance + ray.origin.x);
    int x1 = box_x(ex);
    float ey = static_cast<float>(static_cast<double>(ray.direction.y) * ray.max_distance + ray.origin.y);
    int y1 = box_y(ey);
    int dir_x = ox < ex ? 1 : -1;
    int dir_y = oy < ey ? 1 : -1;

    if (y0 == y1) {
        if (dir_x == 1)
            for (; x0 <= x1; ++x0) count = TestGridBox(x0, y0, &distance, edge, ray, ball, count);
        else
            for (; x1 <= x0; --x0) count = TestGridBox(x0, y0, &distance, edge, ray, ball, count);
    } else if (x0 == x1) {
        if (dir_y == 1)
            for (; y0 <= y1; ++y0) count = TestGridBox(x0, y0, &distance, edge, ray, ball, count);
        else
            for (; y1 <= y0; --y0) count = TestGridBox(x0, y0, &distance, edge, ray, ball, count);
    } else {
        float slope = (oy - ey) / (ox - ex);
        float intercept = static_cast<float>(oy - static_cast<double>(slope) * ox);
        auto y_at = [&](int bx) {
            return static_cast<float>((static_cast<float>(static_cast<double>(bx) * advance_x + x_min)) * static_cast<double>(slope) + intercept);
        };
        auto y_line = [&](int by) { return static_cast<float>(static_cast<double>(by) * advance_y + y_min); };
        int x = x0, y = y0;
        count = TestGridBox(x, y, &distance, edge, ray, ball, 0);
        if (dir_x == 1) {
            int next_x = x0 + 1;
            if (dir_y == 1) {
                int next_y = y0 + 1;
                do {
                    float yl = y_line(next_y), ya = y_at(next_x);
                    if (yl <= ya) {
                        if (ya == yl) {
                            ++x;
                            ++next_x;
                        }
                        ++y;
                        ++next_y;
                    } else {
                        ++x;
                        ++next_x;
                    }
                    count = TestGridBox(x, y, &distance, edge, ray, ball, count);
                } while (x < x1 || y < y1);
            } else {
                do {
                    float yl = y_line(y), ya = y_at(next_x);
                    if (ya <= yl) {
                        if (ya == yl) {
                            ++x;
                            ++next_x;
                        }
                        --y;
                    } else {
                        ++x;
                        ++next_x;
                    }
                    count = TestGridBox(x, y, &distance, edge, ray, ball, count);
                } while (x < x1 || y1 < y);
            }
        } else if (dir_y == 1) {
            int next_y = y0 + 1;
            do {
                float yl = y_line(next_y), ya = y_at(x);
                if (yl <= ya) {
                    if (ya == yl) --x;
                    ++y;
                    ++next_y;
                } else {
                    --x;
                }
                count = TestGridBox(x, y, &distance, edge, ray, ball, count);
            } while (x1 < x || y < y1);
        } else {
            do {
                float yl = y_line(y), ya = y_at(x);
                if (ya <= yl) {
                    if (ya == yl) --x;
                    --y;
                } else {
                    --x;
                }
                count = TestGridBox(x, y, &distance, edge, ray, ball, count);
            } while (x1 < x || y1 < y);
        }
    }
    for (int i = 0; i < count; ++i) processed_[i]->processed_flag = false;
    return distance;
}

// ---- Penempatan ke grid -----------------------------------------------------

namespace edges {

void add_circle_to_grid(const circle_type& c, TEdgeSegment* edge, field_effect_type* field) {
    TEdgeManager* m = g_edge_manager;
    float r = static_cast<float>(m->advance_x * 0.001 + std::sqrt(static_cast<double>(c.radius_sq)));
    float r_sq = r * r;
    float x_lo = c.center.x - r, y_lo = c.center.y - r;
    float x_hi = r + c.center.x, y_hi = r + c.center.y;
    int bx0 = m->box_x(x_lo) - 1;
    int by0 = m->box_y(y_lo) - 1;
    int bx1 = m->increment_box_x(m->box_x(x_hi));
    int by1 = m->increment_box_y(m->box_y(y_hi));
    if (bx0 < 1) bx0 = 0;
    if (by0 < 1) by0 = 0;

    float box_left = static_cast<float>(static_cast<double>(bx0) * m->advance_x + m->x_min);
    float box_top_start = static_cast<float>(static_cast<double>(by0) * m->advance_y + m->y_min);
    for (int bx = bx0; bx <= bx1; ++bx) {
        float box_top = box_top_start;
        for (int by = by0; by <= by1; ++by) {
            float box_right = box_left + m->advance_x;
            float box_bottom = box_top + m->advance_y;
            if (box_left <= x_hi && x_lo <= box_right && box_top <= y_hi && y_lo <= box_bottom) {
                bool inside = !(c.center.x > box_right || c.center.x < box_left || box_bottom < c.center.y ||
                                c.center.y < box_top);
                bool hit = inside;
                if (!hit) {
                    // Sudut box di dalam lingkaran?
                    float cx = box_left, cy = box_top;
                    hit = maths::distance_sq(cx, cy, c.center.x, c.center.y) <= r_sq;
                    cx = cx + m->advance_x;
                    hit = hit || maths::distance_sq(cx, cy, c.center.x, c.center.y) <= r_sq;
                    cy = cy + m->advance_y;
                    hit = hit || maths::distance_sq(cx, cy, c.center.x, c.center.y) <= r_sq;
                    cx = cx - m->advance_x;
                    hit = hit || maths::distance_sq(cx, cy, c.center.x, c.center.y) <= r_sq;
                }
                if (!hit) {
                    // Sisi box memotong lingkaran? (raycast sepanjang 4 sisi, dua arah)
                    ray_type ray{};
                    ray.origin = {box_left, box_top, 0.0f};
                    struct Step {
                        float dx, dy;
                        int len_axis;  // 0 = advance_x, 1 = advance_y
                        float move_x, move_y;  // pergeseran origin sebelum tes
                    };
                    const Step steps[8] = {
                        {1.0f, 0.0f, 0, 0, 0},  {-1.0f, 0.0f, 0, 1, 0}, {0.0f, 1.0f, 1, 0, 0},  {0.0f, -1.0f, 1, 0, 1},
                        {-1.0f, 0.0f, 0, 0, 0}, {1.0f, 0.0f, 0, -1, 0}, {0.0f, -1.0f, 1, 0, 0}, {0.0f, 1.0f, 1, 0, -1},
                    };
                    for (const Step& s : steps) {
                        ray.direction = {s.dx, s.dy, 0.0f};
                        ray.origin.x += s.move_x * m->advance_x;
                        ray.origin.y += s.move_y * m->advance_y;
                        ray.max_distance = s.len_axis == 0 ? m->advance_x : m->advance_y;
                        if (maths::ray_intersect_circle(ray, c) < 1e9) {
                            hit = true;
                            break;
                        }
                    }
                }
                if (hit) {
                    if (edge) m->add_edge_to_box(bx, by, edge);
                    if (field) m->add_field_to_box(bx, by, field);
                }
            }
            box_top = box_top + m->advance_y;
        }
        box_left = box_left + m->advance_x;
    }
}

void add_box_to_grid(float y_min, float x_min, float y_max, float x_max, TEdgeSegment* edge, field_effect_type* field) {
    TEdgeManager* m = g_edge_manager;
    int margin_x = ftol(m->advance_x * 0.001);
    int margin_y = ftol(m->advance_y * 0.001);
    x_min = static_cast<float>(x_min - static_cast<double>(margin_x));
    x_max = static_cast<float>(margin_x + static_cast<double>(x_max));
    y_min = static_cast<float>(y_min - static_cast<double>(margin_y));
    y_max = static_cast<float>(margin_y + static_cast<double>(y_max));
    int bx0 = m->box_x(x_min), by0 = m->box_y(y_min);
    int bx1 = m->box_x(x_max), by1 = m->box_y(y_max);
    float box_left = static_cast<float>(static_cast<double>(bx0) * m->advance_x + m->x_min);
    for (int bx = bx0; bx <= bx1; ++bx) {
        float box_top = static_cast<float>(static_cast<double>(by0) * m->advance_y + m->y_min);
        for (int by = by0; by <= by1; ++by) {
            if (box_left <= x_max && x_min <= box_left + m->advance_x && box_top <= y_max && y_min <= box_top + m->advance_y) {
                if (edge) m->add_edge_to_box(bx, by, edge);
                if (field) m->add_field_to_box(bx, by, field);
            }
            box_top = box_top + m->advance_y;
        }
        box_left = box_left + m->advance_x;
    }
}

}  // namespace edges
