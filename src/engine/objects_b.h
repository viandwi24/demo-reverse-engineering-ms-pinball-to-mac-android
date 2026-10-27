// Objek meja grup B: rollover, light rollover, tripwire, kickout, hole, ramp, demo, sink.
// Semua turunan TCollisionComponent; vtable [0]Message [1]port_draw [2]get_scoring [3]put_scoring [4]dtor
// [5]Collision [6]FieldEffect.
#pragma once

#include <vector>

#include "component.h"

// Tipe 0x3f7, ctor 01019771 (vtable 0x1002614). Juga basis TLightRollover/TTripwire (ctor dasar 01019749).
class TRollover : public TCollisionComponent {
public:
    TRollover(TPinballTable* table, int group_index);  // 01019771
    int Message(int code, float value) override;      // 01016460
    void Collision(TBall* ball, vector2* next_position, vector2* direction, float distance, TEdgeSegment* edge) override;  // 01018351
    int get_scoring(int index) override;               // 01016496
    void put_scoring(int index, int score) override;   // 010164b4

    char rollover_flag = 0;  // +0x4e: juga flag aktif edge 0x25b
    int scoring[2]{};        // +0x4f

protected:
    TRollover(TPinballTable* table, int group_index, bool create_wall);  // 01019749
    void build_walls(int group_index);                                   // 010182e4
    static void TimerExpired(int timer_id, void* caller);                // 0101644b
};

// Tipe 0x3fc, ctor 010197bb (vtable 0x1002630).
class TLightRollover : public TRollover {
public:
    TLightRollover(TPinballTable* table, int group_index);  // 010197bb
    int Message(int code, float value) override;            // 010165a7
    void Collision(TBall* ball, vector2* next_position, vector2* direction, float distance, TEdgeSegment* edge) override;  // 010164f2

    float flag_timeout;  // +0x57 (atribut 0x197)
    int timer = 0;       // +0x5b

private:
    static void delay_expired(int timer_id, void* caller);  // 010164d1
};

// Tipe 0x400, ctor 01019816 (vtable 0x100264c; Ghidra salah menamai "moneypunct<>").
class TTripwire : public TRollover {
public:
    TTripwire(TPinballTable* table, int group_index);  // 01019816
    void Collision(TBall* ball, vector2* next_position, vector2* direction, float distance, TEdgeSegment* edge) override;  // 010165ed
};

// Tipe 0x3f4 (arg ctor 1) dan 0x405 (arg ctor 0), ctor 01019884 (vtable 0x1002690).
class TKickout : public TCollisionComponent {
public:
    TKickout(TPinballTable* table, int group_index, int some_flag);  // 01019884
    int Message(int code, float value) override;                     // 010168e3
    void Collision(TBall* ball, vector2* next_position, vector2* direction, float distance, TEdgeSegment* edge) override;  // 01016745
    int FieldEffect(TBall* ball, vector2* dst) override;             // 0101685b
    int get_scoring(int index) override;                             // 01016963
    void put_scoring(int index, int score) override;                 // 01016984

    int kickout_flag = 0;            // +0x4e: bola sedang ditahan
    int not_some_flag;               // +0x52: (some_flag == 0) -> nonaktif sampai di-reset
    int timer = 0;                   // +0x56
    float timer_time = 1.5f;         // +0x5a
    float timer_time2 = 0.05f;       // +0x5e
    float collision_ball_set_z;      // +0x62 (atribut 0x198 [2])
    TBall* ball = nullptr;           // +0x66
    float field_mult;                // +0x6a (atribut 0x131)
    circle_type field_circle{};      // +0x6e
    float orig_ball_z = 0;           // +0x7e
    vector3 ball_throw_dir{};        // +0x82 (visual kicker.vector)
    float throw_angle_mult;          // +0x8e (visual [11])
    float throw_speed_mult1;         // +0x92 (visual kicker.boost)
    float throw_speed_mult2;         // +0x96 (visual [7] * 0.01)
    field_effect_type field{};       // +0x9a
    int scoring[5]{};                // +0xa6

private:
    static void TimerExpired(int timer_id, void* caller);   // 010167dd: lempar bola keluar
    static void ResetTimerExpired(int timer_id, void* caller);  // 010167be
};

// Tipe 0x3fe, ctor 01019a1c (vtable 0x10026ac).
class THole : public TCollisionComponent {
public:
    THole(TPinballTable* table, int group_index);  // 01019a1c
    int Message(int code, float value) override;    // 01016ab1
    void Collision(TBall* ball, vector2* next_position, vector2* direction, float distance, TEdgeSegment* edge) override;  // 01018476
    int FieldEffect(TBall* ball, vector2* dst) override;  // 010169c0

    int ball_captured_flag = 0;  // +0x4e
    int ball_drop_flag = 0;      // +0x52: bola mulai jatuh ke lubang
    int timer = 0;               // +0x56
    float unknown5a;             // +0x5a (atribut 0x197, default 0.25; tidak dipakai)
    float unknown5e = 0.05f;     // +0x5e (tidak dipakai)
    float gravity_mult;          // +0x62 (atribut 0x2bd, default 0.5)
    float z_floor;               // +0x66 (atribut 0x198 [2])
    int field_flag;              // +0x6a (atribut 0x518)
    float field_mult;            // +0x6e (atribut 0x131)
    circle_type field_circle{};  // +0x72
    field_effect_type field{};   // +0x82

private:
    static void TimerExpired(int timer_id, void* caller);  // 010169a4
};

// Bidang ramp dari atribut 0x514: 13 float per bidang (0x34 byte).
struct ramp_plane_type {
    vector3 normal;          // +0x00: z = a*x + b*y + c
    vector2 point0;          // +0x0c
    vector2 point1;          // +0x14
    vector2 point2;          // +0x1c
    float gravity_angle1;    // +0x24
    float gravity_angle2;    // +0x28
    vector2 field_force;     // +0x2c: dihitung ctor (asli menulis ke data DAT)
};

// Tipe 0x3fd, ctor 01019bb8 (vtable 0x10026cc). Message = default 010159a9.
class TRamp : public TCollisionComponent {
public:
    TRamp(TPinballTable* table, int group_index);  // 01019bb8
    void Collision(TBall* ball, vector2* next_position, vector2* direction, float distance, TEdgeSegment* edge) override;  // 01016c24
    int FieldEffect(TBall* ball, vector2* dst) override;  // 01016d2e
    int get_scoring(int index) override;                  // 01016aec
    void put_scoring(int index, int score) override;      // 01016b0a

    int scoring[4]{};             // +0x4e
    field_effect_type field{};    // +0x5e
    int collision_group;          // +0x6a
    int ramp_flag1;               // +0x6e (atribut 0x519)
    int line_count;               // +0x72: jumlah bidang
    float field_mult;             // +0x76 (atribut 0x2bd, default 0.2)
    ramp_plane_type* plane;       // +0x7a
    TLine* line2 = nullptr;       // +0x7e (atribut 0x515)
    TLine* line3 = nullptr;       // +0x82 (atribut 0x516)
    TLine* line1 = nullptr;       // +0x86 (atribut 0x517)
    int wall1_collision_group;    // +0x8a
    int wall2_collision_group;    // +0x8e
    float wall1_ball_offset;      // +0x92
    float wall2_ball_offset;      // +0x96

    // Port: salinan bidang yang bisa ditulis. Edge menyimpan (indeks bidang + 1) di wall_value
    // sebagai ganti pointer bidang (+0xd) di exe asli; 0 = bukan edge bidang.
    std::vector<ramp_plane_type> planes_;

private:
    // 01016b27: cari sisi bidang yang paling dekat dengan garis (a, b).
    static void ramp_plane_closest(ramp_plane_type* planes, int count, const float* line_points, vector2** out_start,
                                   vector2** out_end);
};

// Tipe 0x3ff, ctor 0101a2b5 (vtable 0x10026e8): mode demo (flipper/plunger otomatis).
class TDemo : public TCollisionComponent {
public:
    TDemo(TPinballTable* table, int group_index);  // 0101a2b5
    int Message(int code, float value) override;    // 01017074
    void Collision(TBall* ball, vector2* next_position, vector2* direction, float distance, TEdgeSegment* edge) override;  // 01016f21

    float min_reaction_time1;   // +0x4e (atribut 0x197 [0])
    float reaction_spread1;     // +0x52 ([1])
    float min_reaction_time2;   // +0x56 ([2])
    float reaction_spread2;     // +0x5a ([3])
    int flipper_l_active = 0;   // +0x5e
    int flipper_r_active = 0;   // +0x62
    int flipper_l_timer = 0;    // +0x66
    int flipper_r_timer = 0;    // +0x6a
    int plunger_timer = 0;      // +0x6e
    int restart_timer = 0;      // +0x72
    TEdgeSegment* edge1 = nullptr;  // +0x76 (0x578)
    TEdgeSegment* edge2 = nullptr;  // +0x7a (0x57a)
    TEdgeSegment* edge3 = nullptr;  // +0x7e (0x57c)

private:
    static void PlungerRelease(int timer_id, void* caller);     // 01016d62
    static void UnFlipRight(int timer_id, void* caller);        // 01016d8d
    static void UnFlipLeft(int timer_id, void* caller);         // 01016dc0
    static void FlipRight(int timer_id, void* caller);          // 01016df3
    static void FlipLeft(int timer_id, void* caller);           // 01016e70
    static void NewGameRestartTimer(int timer_id, void* caller);  // 01016eed
};

// Tipe 0x3f9, ctor 0101a416 (vtable 0x1002704).
class TSink : public TCollisionComponent {
public:
    TSink(TPinballTable* table, int group_index);  // 0101a416
    int Message(int code, float value) override;    // 01017197
    void Collision(TBall* ball, vector2* next_position, vector2* direction, float distance, TEdgeSegment* edge) override;  // 010184ed
    int get_scoring(int index) override;            // 01017262
    void put_scoring(int index, int score) override;  // 01017283

    int timer = 0;              // +0x4e
    float timer_time;           // +0x52 (atribut 0x197)
    vector2 ball_position;      // +0x56 (atribut 0x259)
    vector3 ball_throw_dir;     // +0x62 (visual kicker.vector)
    float throw_angle_mult;     // +0x6e (visual [11])
    float throw_speed_mult1;    // +0x72 (visual kicker.boost)
    float throw_speed_mult2;    // +0x76 (visual [7] * 0.01)
    int sound_index4;           // +0x7a (visual [14]): bola masuk
    int sound_index3;           // +0x7e (visual [15]): bola keluar
    int scoring[3]{};           // +0x82
    int player_message_field_backup[4]{};  // +0x8e

private:
    static void TimerExpired(int timer_id, void* caller);  // 01017130
};

// FUN_010153ae (dipanggil timer demo 01016eed): mulai ulang permainan (modul pb, belum diport).
// Integrator memasang fungsi ini; nullptr = dilewati.
extern void (*g_pb_replay_level)(int demo_mode);

// Objek tipe 0x3f4/0x3f7/0x3f9/0x3fc/0x3fd/0x3fe/0x3ff/0x400/0x405; nullptr untuk tipe lain.
TPinballComponent* create_object_b(TPinballTable* table, int type, int group);
