// Objek meja spesifik. Tahap 2: flipper, plunger, drain; objek lain menyusul lewat create_object.
#pragma once

#include "component.h"

class TFlipperEdge : public TEdgeSegment {
public:
    TFlipperEdge(TCollisionComponent* owner, char* active_flag, unsigned collision_group, TPinballTable* table,
                 const float* origin, const float* vec_t1, const float* vec_t2, float extend_time, float retract_time,
                 float collision_mult, float elasticity, float smoothness);  // 0101c2ba

    void EdgeCollision(TBall* ball, float distance) override;   // 0101be06
    void port_draw() override;                                  // 0101c026
    void place_in_grid() override;                              // 0101b0d4
    double FindCollisionDistance(ray_type& ray) override;       // 0101b825
    void SetMotion(int code, float value);                      // 0101bf98

    int flipper_flag = 0;          // +0x15
    float elasticity, smoothness;  // +0x19, +0x1d
    vector3 rot_origin{};          // +0x21
    float circlebase_radius, circle_t1_radius;          // +0x2d, +0x31
    float circlebase_radius_sq, circle_t1_radius_sq;    // +0x35, +0x39
    float circlebase_radius_m_sq, circle_t1_radius_m_sq;  // +0x3d, +0x41
    float angle_max = 0, angle_src = 0, angle_dst = 0;   // +0x45, +0x49, +0x4d
    int edge_collision_flag2 = 0;  // +0x51 (bola di sisi muka flipper)
    int edge_collision_flag3 = 0;  // +0x55
    vector3 face_normal{};         // +0x59
    vector3 a1_src{}, a2_src{}, b1_src{}, b2_src{};  // +0x65, +0x71, +0x7d, +0x89
    float collision_mult;          // +0x95
    vector3 t1_src{}, t2_src{};    // +0x99, +0xa5
    float distance_div_sq;         // +0xb1
    float collision_time_advance;  // +0xb5
    vector3 collision_direction{}; // +0xb9
    int edge_collision_flag = 0;   // +0xc5
    float input_time = 0;          // +0xc9
    float move_end_time = 0;       // +0xcd
    float flipper_move_time = 0;   // +0xd1
    float extend_time, retract_time;  // +0xd5, +0xd9
    vector3 next_ball_position{};  // +0xdd

private:
    double GetCurrentAngle(float time);  // 0101b1f7
    void set_angle(float time);          // 0101b327
    void build_geometry();               // 0101b281
    int is_ball_inside(float x, float y);  // 0101b5a8
    static double flipper_calc(ray_type& ray, ray_type* out);  // 0101b43a (out: origin=posisi, direction=normal)
};

class TFlipper : public TCollisionComponent {
public:
    TFlipper(TPinballTable* table, int group_index);  // 0101c59d
    int Message(int code, float value) override;      // 0101c178
    void port_draw() override;                        // 0101c2ad
    void Collision(TBall*, vector2*, vector2*, float, TEdgeSegment*) override {}  // 0101c068

    int bmp_index = 0;               // +0x4e
    TFlipperEdge* flipper_edge;      // +0x52
    int timer = 0;                   // +0x56
    float extend_animation_frame_time;   // +0x5a
    float retract_animation_frame_time;  // +0x5e
    float timer_time = 0;            // +0x62
    float input_time = 0;            // +0x66

private:
    static void TimerExpired(int timer_id, void* caller);  // 0101c070
};

class TPlunger : public TCollisionComponent {
public:
    TPlunger(TPinballTable* table, int group_index);  // 0101a4f3
    int Message(int code, float value) override;      // 010185ad
    void Collision(TBall* ball, vector2* next_position, vector2* direction, float distance, TEdgeSegment* edge) override;  // 0101875f

    int pullback_timer_ = 0;    // +0x4e
    int ball_feed_timer_ = 0;   // +0x52
    int max_pullback = 100;     // +0x56
    int pullback_increment;     // +0x5a
    float pullback_delay = 0.025f;  // +0x5e
    int sound_index_p1, sound_index_p2;  // +0x62, +0x66

private:
    static void PullbackTimer(int timer_id, void* caller);  // 01017314
    static void BallFeedTimer(int timer_id, void* caller);  // 010172f0
    static void ReleasedTimer(int timer_id, void* caller);  // 010173c8
};

class TDrain : public TCollisionComponent {
public:
    TDrain(TPinballTable* table, int group_index);  // 0101a4b4
    int Message(int code, float value) override;    // 010172a3
    void Collision(TBall* ball, vector2* next_position, vector2* direction, float distance, TEdgeSegment* edge) override;  // 01018562

    float timer_time;  // +0x4e
    int timer = 0;     // +0x56

private:
    static void TimerCallback(int timer_id, void* caller);  // 010172d8
};

