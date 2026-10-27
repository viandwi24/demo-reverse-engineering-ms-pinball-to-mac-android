#include "ball.h"

#include "table.h"

TBall::TBall(TPinballTable* table) : TPinballComponent(table, -1, false) {
    active_flag = 1;
    int group = loader::query_handle("ball");
    offset = *loader::query_float_attribute(group, 0, 500);
    int states = loader::query_visual_states(group);
    for (int i = 0; i < states; ++i) {
        visual_struct v;
        loader::query_visual(group, i, v);
        list_bitmap.push_back(v.bitmap);
        const float* z = loader::query_float_attribute(group, i, 501);
        visual_z_array[i] = static_cast<float>(proj::z_distance(vector3{z[0], z[1], z[2]}));
    }
    render_sprite_ = render::create_sprite(VisualType::Ball, nullptr, nullptr, 0, 0, nullptr);
    table->collision_comp_offset = offset;
    position.z = offset;
}

int TBall::Message(int code, float) {
    if (code == 0x400) {
        render::ball_set(render_sprite_, nullptr, 0.0f, 0, 0);
        position.x = 0;
        collision_comp = nullptr;
        position.y = 0;
        active_flag = 0;
        collision_flag = 0;
        field_flag = 1;
        direction.y = 0;
        position.z = offset;
        direction.x = 0;
        speed = 0;
        ray_max_distance = 0;
    }
    return 0;
}

void TBall::Repaint() {
    if (collision_flag)
        position.z = static_cast<float>(static_cast<double>(collision_offset.x) * position.x +
                                        static_cast<double>(collision_offset.y) * position.y + offset + collision_offset.z);
    int pos2d[2];
    proj::xform_to_2d(position, pos2d);
    float z_depth = static_cast<float>(proj::z_distance(position));

    int count = static_cast<int>(list_bitmap.size()) - 1;
    int index = 0;
    for (; index < count; ++index)
        if (visual_z_array[index] <= z_depth) break;

    const Bitmap8* bmp = list_bitmap[index];
    render::ball_set(render_sprite_, bmp, z_depth, pos2d[0] + bmp->width / -2, pos2d[1] + bmp->height / -2);
}

bool TBall::already_hit(TEdgeSegment* edge) const {
    int n = edge_collision_count < 5 ? edge_collision_count : 5;
    for (int i = 0; i < n; ++i)
        if (collisions[i] == edge) return true;
    return false;
}

void TBall::not_already_hit(TEdgeSegment* edge) {
    if (edge_collision_count < 5) collisions[edge_collision_count++] = edge;
}
