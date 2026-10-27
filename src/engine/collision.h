// Edge collision dan grid edge manager.
// Port dari TEdgeSegment (01015866), TCircle (01015898), TLine (0101c6fd/0101c77e),
// TEdgeManager (0100b072-0100bf8e).
#pragma once

#include <vector>

#include "maths.h"

class TBall;
class TCollisionComponent;

class TEdgeSegment {
public:
    TEdgeSegment(TCollisionComponent* owner, char* active_flag, unsigned collision_group);
    virtual ~TEdgeSegment() = default;

    // vtable [0]..[3]
    virtual void EdgeCollision(TBall* ball, float distance) = 0;
    virtual void port_draw() {}
    virtual void place_in_grid() = 0;
    virtual double FindCollisionDistance(ray_type& ray) = 0;

    TCollisionComponent* collision_component;  // +4
    char* active_flag;                         // +8
    bool processed_flag = false;               // +0xc
    int wall_value = 0;                        // +0xd
    unsigned collision_group;                  // +0x11
};

class TCircle : public TEdgeSegment {
public:
    TCircle(TCollisionComponent* owner, char* active_flag, unsigned collision_group, const vector3& center, float radius);
    void EdgeCollision(TBall* ball, float distance) override;  // 0101cc8d
    void place_in_grid() override;                              // 0101cb7c
    double FindCollisionDistance(ray_type& ray) override;       // 010158d6

    circle_type circle;  // +0x15
};

class TLine : public TEdgeSegment {
public:
    TLine(TCollisionComponent* owner, char* active_flag, unsigned collision_group, float x0, float y0, float x1, float y1);
    TLine(TCollisionComponent* owner, char* active_flag, unsigned collision_group, const vector2& start, const vector2& end);
    void EdgeCollision(TBall* ball, float distance) override;  // 0101c7e6
    void place_in_grid() override;                              // 0101c87b
    double FindCollisionDistance(ray_type& ray) override;       // 0101c764
    void Offset(float offset);                                  // 0101c813

    line_type line;  // +0x15
    vector2 start;   // +0x45
    vector2 end;     // +0x4d
};

struct field_effect_type {
    char* active_flag;  // +0
    int collision_group;
    TCollisionComponent* collision_comp;
};

class TEdgeManager {
public:
    TEdgeManager(float x_min, float y_min, float width, float height);  // 0100b883

    int box_x(float x) const;  // 0100b0f4
    int box_y(float y) const;  // 0100b1ae
    int increment_box_x(int x) const;  // 0100b268
    int increment_box_y(int y) const;  // 0100b284
    void add_edge_to_box(int x, int y, TEdgeSegment* edge);         // 0100b961
    void add_field_to_box(int x, int y, field_effect_type* field);  // 0100b989
    void FieldEffects(TBall* ball, vector2& accel);                 // 0100b316
    double FindCollisionDistance(ray_type& ray, TBall* ball, TEdgeSegment** edge);  // 0100b486

    float advance_x, advance_y;          // +0, +4 ukuran box
    float advance_x_inv, advance_y_inv;  // +8, +0xc
    int max_box_x, max_box_y;            // +0x10, +0x14 (10, 15)
    float x_min, y_min;                  // +0x18, +0x1c

private:
    struct Box {
        std::vector<TEdgeSegment*> edges;
        std::vector<field_effect_type*> fields;
    };
    int TestGridBox(int x, int y, float* distance, TEdgeSegment** edge, ray_type& ray, TBall* ball, int count);  // 0100b3ae

    std::vector<Box> boxes_;
    TEdgeSegment* processed_[1000];
};

extern TEdgeManager* g_edge_manager;  // DAT_0102503c

namespace edges {
void add_circle_to_grid(const circle_type& circle, TEdgeSegment* edge, field_effect_type* field);  // 0100b9c7
void add_box_to_grid(float y_min, float x_min, float y_max, float x_max, TEdgeSegment* edge,
                     field_effect_type* field);  // 0100be21
}
