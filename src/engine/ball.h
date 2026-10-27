// Bola. Port TBall (ctor 01019309, vtable 0x1002554).
#pragma once

#include "component.h"

class TBall : public TPinballComponent {
public:
    explicit TBall(TPinballTable* table);

    int Message(int code, float value) override;  // 01015b9c
    void Repaint();                                 // 01015c08
    bool already_hit(TEdgeSegment* edge) const;     // 0100b072
    void not_already_hit(TEdgeSegment* edge);       // 0101590f

    vector3 position{};         // +0x2a
    vector3 direction{};        // +0x36 (vektor satuan)
    float speed = 0;            // +0x42
    float ray_max_distance = 0; // +0x46
    float time_delta = 0;       // +0x4a
    float time_now = 0;         // +0x4e
    vector2 inv_dir{1e9f, 1e9f};  // +0x52
    TCollisionComponent* collision_comp = nullptr;  // +0x6a: komponen yang sedang "memegang" bola
    int field_flag = 1;         // +0x6e
    TEdgeSegment* collisions[5]{};  // +0x72
    int edge_collision_count = 0;   // +0x86
    vector3 collision_offset{};     // +0x8a: bidang miring (ramp)
    int collision_flag = 0;         // +0x96
    float offset;                   // +0x9a: jari-jari bola
    float visual_z_array[50]{};     // +0xa2
    vector2 ramp_field_force{};     // +0x5e: gaya bidang ramp (diisi TRamp::Collision 01016c24)
};
