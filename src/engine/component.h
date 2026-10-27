// Kelas dasar objek meja.
// TPinballComponent (ctor 01017c6d, vtable 0x10024a8), TCollisionComponent (ctor 010190b3, vtable 0x10024ec),
// TWall (ctor 010191ba, vtable 0x1002508).
#pragma once

#include <string>
#include <vector>

#include "collision.h"
#include "loader.h"
#include "render.h"

class TPinballTable;
class TBall;

class TPinballComponent {
public:
    TPinballComponent(TPinballTable* table, int group_index, bool load_visuals);
    virtual ~TPinballComponent() = default;

    // vtable
    virtual int Message(int code, float value);  // 010159a9
    virtual void port_draw() {}
    virtual int get_scoring(int) { return 0; }
    virtual void put_scoring(int, int) {}

    // Pasang state visual ke-n ke sprite (pola yang dipakai banyak komponen).
    void SetSpriteState(int index);

    bool unknown_c4f = false;   // +4
    char active_flag = 0;       // +5
    int message_field = 0;      // +6
    const char* group_name = nullptr;  // +10
    void* control = nullptr;    // +0x12: diisi control init (FUN_0100c01d); semua objek asli di-calloc (nol)
    int group_index = -1;       // +0x16
    render_sprite* render_sprite_ = nullptr;  // +0x1a
    TPinballTable* pinball_table;  // +0x1e
    std::vector<const Bitmap8*> list_bitmap;  // +0x22
    std::vector<const ZMap*> list_zmap;       // +0x26
};

class TCollisionComponent : public TPinballComponent {
public:
    TCollisionComponent(TPinballTable* table, int group_index, bool create_wall);

    void port_draw() override;  // 01015a9a
    virtual void Collision(TBall* ball, vector2* next_position, vector2* direction, float distance, TEdgeSegment* edge);  // 01017e85
    virtual int FieldEffect(TBall*, vector2*) { return 0; }  // 01019168
    int DefaultCollision(TBall* ball, vector2* next_position, vector2* direction);  // 01017f29

    std::vector<TEdgeSegment*> edge_list;  // +0x2a
    float elasticity;    // +0x36
    float smoothness;    // +0x3a
    float boost;         // +0x3e
    float threshold;     // +0x42
    int soft_hit_sound;  // +0x46
    int hard_hit_sound;  // +0x4a
};

class TWall : public TCollisionComponent {
public:
    TWall(TPinballTable* table, int group_index);
    int Message(int code, float value) override;  // 01015af3
    void Collision(TBall* ball, vector2* next_position, vector2* direction, float distance, TEdgeSegment* edge) override;  // 01017fde
    int get_scoring(int index) override;           // 01015b26
    void put_scoring(int index, int score) override;  // 01015b44

    int timer = 0;                         // +0x4e
    const Bitmap8* bitmap_hit = nullptr;   // +0x52
    int scoring = 0;                       // +0x56

private:
    static void TimerExpired(int timer_id, void* caller);  // 01015ace
};

// FUN_01017883: pantulan dasar; return proyeksi kecepatan ke normal.
double basic_collision(TBall* ball, const vector2* next_position, const vector2* direction, float elasticity,
                       float smoothness, float threshold, float boost);

// FUN_010179a9: buat edge dinding dari array float (lingkaran / garis / poligon).
TEdgeSegment* install_wall(const float* floats, TCollisionComponent* owner, char* active_flag, unsigned collision_group,
                           float offset, int wall_value);

// FUN_01011ec9: kontrol aturan game (tahap 3). Saat ini hanya stub.
void control_handler(int code, TPinballComponent* caller);
