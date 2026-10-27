// Objek meja grup A: oneway, bumper, popup target, solo target, blocker, gate, kicker, flag spinner.
// Semua turunan TCollisionComponent; vtable [0]Message [1]port_draw [2]get_scoring [3]put_scoring [4]dtor
// [5]Collision [6]FieldEffect.
#pragma once

#include "component.h"

// Tipe 0x3f8, ctor 01019204 (vtable 0x1002530).
class TOneway : public TCollisionComponent {
public:
    TOneway(TPinballTable* table, int group_index);
    void Collision(TBall* ball, vector2* next_position, vector2* direction, float distance, TEdgeSegment* edge) override;  // 01018032
    int get_scoring(int index) override;              // 01015b61
    void put_scoring(int index, int score) override;  // 01015b7f

    TLine* line = nullptr;  // +0x52: sisi tembus (dilewati bola)
    int scoring[6]{};       // +0x56
};

// Tipe 0x3ed, ctor 0101944a (vtable 0x100256c).
class TBumper : public TCollisionComponent {
public:
    TBumper(TPinballTable* table, int group_index);
    int Message(int code, float value) override;      // 01015d79
    void Collision(TBall* ball, vector2* next_position, vector2* direction, float distance, TEdgeSegment* edge) override;  // 0101811d
    int get_scoring(int index) override;              // 01015ef9
    void put_scoring(int index, int score) override;  // 01015f17

    struct PlayerData {
        int message_field;  // +0
        int bmp_index;      // +4
    };

    int bmp_index = 0;          // +0x4e
    int timer = 0;              // +0x52
    float timer_time;           // +0x56
    float original_threshold;   // +0x5a
    int sound_index4;           // +0x5e (visual [14]): naik level
    int sound_index5;           // +0x62 (visual [15]): turun level
    int scoring[4]{};           // +0x66
    PlayerData player_data[4]{};  // +0x76

private:
    void Fire();                                           // 01015d0f
    static void TimerExpired(int timer_id, void* caller);  // 01015cb2
};

// Tipe 0x3ee, ctor 010195e0 (vtable 0x10025a4).
class TPopupTarget : public TCollisionComponent {
public:
    TPopupTarget(TPinballTable* table, int group_index);
    int Message(int code, float value) override;      // 010160f4
    void Collision(TBall* ball, vector2* next_position, vector2* direction, float distance, TEdgeSegment* edge) override;  // 010181ed
    int get_scoring(int index) override;              // 010161d0
    void put_scoring(int index, int score) override;  // 010161ee

    int timer = 0;               // +0x4e
    float timer_time;            // +0x52
    int scoring[3]{};            // +0x56
    int player_message_field[4]{};  // +0x62

private:
    static void TimerExpired(int timer_id, void* caller);  // 010160b3
};

// Tipe 0x3fb, ctor 0101961f (vtable 0x10025c0).
class TSoloTarget : public TCollisionComponent {
public:
    TSoloTarget(TPinballTable* table, int group_index);
    int Message(int code, float value) override;      // 01016230
    void Collision(TBall* ball, vector2* next_position, vector2* direction, float distance, TEdgeSegment* edge) override;  // 01018295
    int get_scoring(int index) override;              // 010162c4
    void put_scoring(int index, int score) override;  // 010162e2

    int timer = 0;              // +0x52
    float timer_time = 0.1f;    // +0x56
    int sound_index4;           // +0x5a (visual [14], tidak dipakai)
    int scoring[1]{};           // +0x5e

private:
    static void TimerExpired(int timer_id, void* caller);  // 0101620b
};

// Tipe 0x3f3, ctor 01019677 (vtable 0x10025dc). Collision = dasar (01017e85).
class TBlocker : public TCollisionComponent {
public:
    TBlocker(TPinballTable* table, int group_index);
    int Message(int code, float value) override;  // 0101631c

    int initial_value = 0x37;  // +0x4e (tidak dipakai di kelas ini)
    int unknown_52 = 5;        // +0x52 (tidak dipakai di kelas ini)
    int timer = 0;             // +0x56
    int sound_index4;          // +0x5a (visual [14]): blocker naik
    int sound_index3;          // +0x5e (visual [15]): blocker turun

private:
    static void TimerExpired(int timer_id, void* caller);  // 010162ff
};

// Tipe 0x3f5, ctor 010196e5 (vtable 0x10025f8). Collision = dasar (01017e85).
class TGate : public TCollisionComponent {
public:
    TGate(TPinballTable* table, int group_index);
    int Message(int code, float value) override;  // 010163db

    int sound_index4;  // +0x4e (visual [14]): gate terbuka
    int sound_index3;  // +0x52 (visual [15]): gate tertutup
};

// Tipe 0x3f6, ctor 0101983d (vtable 0x1002668).
class TKicker : public TCollisionComponent {
public:
    TKicker(TPinballTable* table, int group_index);
    int Message(int code, float value) override;  // 010166f6
    void Collision(TBall* ball, vector2* next_position, vector2* direction, float distance, TEdgeSegment* edge) override;  // 010183e8

    float timer_time = 0.7f;   // +0x4e
    float timer_time2 = 0.1f;  // +0x52
    int timer = 0;             // +0x56
    int kicker_flag = 0;       // +0x5a

private:
    static void TimerExpired(int timer_id, void* caller);  // 0101663f
};

// Tipe 0x3fa, ctor 010194b0 (vtable 0x1002588).
class TFlagSpinner : public TCollisionComponent {
public:
    TFlagSpinner(TPinballTable* table, int group_index);
    int Message(int code, float value) override;      // 01016014
    void Collision(TBall* ball, vector2* next_position, vector2* direction, float distance, TEdgeSegment* edge) override;  // 01018150
    int get_scoring(int index) override;              // 01016078
    void put_scoring(int index, int score) override;  // 01016096

    float speed = 0;                // +0x4e
    float max_speed = 50000.0f;     // +0x52 (atribut 0x4b0)
    float min_speed = 5.0f;         // +0x56 (atribut 0x4b1)
    float speed_decrement = 0.65f;  // +0x5a (atribut 0x4b2)
    int bmp_index_step = 0;         // +0x5e (+1 / -1)
    int bmp_index = 0;              // +0x62
    int timer = 0;                  // +0x66 (asli tidak pernah menyimpan id timer ke sini)
    TLine* prev_collider = nullptr; // +0x6a
    int scoring[2]{};               // +0x6e

private:
    void NextFrame();                                      // 01015f4e
    static void SpinTimer(int timer_id, void* caller);    // 01015f34
};

TPinballComponent* create_object_a(TPinballTable* table, int type, int group);
