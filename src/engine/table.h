// Meja: TPinballTable (ctor 0101aaf3, vtable 0x1002790) dan TTableLayer (ctor 0101a5ab, vtable 0x100276c).
#pragma once

#include <vector>

#include "component.h"

class TBall;
class TFlipper;
class TPlunger;
class TDrain;
class TLightGroup;

class TTableLayer : public TCollisionComponent {
public:
    explicit TTableLayer(TPinballTable* table);
    int FieldEffect(TBall* ball, vector2* vec_dst) override;  // 010173e7

    const Bitmap8* visual_bitmap = nullptr;  // +0x4e
    float x_min, y_min, x_max, y_max;        // +0x52..+0x5e
    float gravity_dir_x, gravity_dir_y;      // +0x62, +0x66
    float gravity_mult;                      // +0x6e
    field_effect_type field;                 // +0x72
};

class TPinballTable : public TPinballComponent {
public:
    TPinballTable();
    ~TPinballTable() override;

    int Message(int code, float value) override;  // 010187d6 (sebagian: input flipper/plunger)

    std::vector<TPinballComponent*> component_list;  // +0xfa
    std::vector<TBall*> ball_list;                   // +0x102
    TFlipper* flipper_l = nullptr;   // +0x2a
    TFlipper* flipper_r = nullptr;   // +0x2e
    TPlunger* plunger = nullptr;     // +0xde
    TDrain* drain = nullptr;         // +0xe2
    TTableLayer* layer = nullptr;
    TLightGroup* light_group = nullptr;  // +0x10a
    int ball_in_drain_flag = 0;      // +0x4e
    int x_offset = 0, y_offset = 0;  // +0xea, +0xee
    int width = 0, height = 0;       // +0xf2, +0xf6
    float gravity_dir_vect[3] = {};  // +0x10e (magnitudo, sudut, sudut)
    float collision_comp_offset = 0; // +0x11a: jari-jari bola
    float plunger_position[2] = {};  // +0x11e
    int tilt_lock = 0;               // +0x172
    int player_count = 1;            // +0xd6
    int current_player = 0;          // +0xda

    // ---- Status game (dipakai TPinballTable::Message dan modul kontrol) ----
    struct score_struct* cur_score_struct = nullptr;   // +0x32
    struct score_struct* score_ballcount = nullptr;    // +0x36
    struct score_struct* score_player_number1 = nullptr;  // +0x3a
    int unknown_p3e = 0;           // +0x3e
    int cur_score = 0;             // +0x52
    int cur_score_e9 = 0;          // +0x56 (miliar)
    int light_show_timer = 0;      // +0x5a
    int end_game_timeout_timer = 0;  // +0x5e
    int tilt_timeout_timer = 0;    // +0x62
    struct PlayerData {
        struct score_struct* score_struct;  // +0x66
        int score;              // +0x6a
        int score_e9;           // +0x6e
        int jackpot_score;      // +0x72
        int ball_count;         // +0x76
        int extra_balls;        // +0x7a
        int ball_locked_counter;  // +0x7e
    } player_scores[4]{};          // +0x66 (stride 0x1c)
    int score_multiplier = 0;      // +0x126 (indeks ke tabel DAT_01024758)
    int score_added = 0;           // +0x12a
    int score_special1 = 0;        // +0x12e
    int score_special2 = 0;        // +0x132 (bonus)
    int score_special2_flag = 0;   // +0x136
    int score_special3 = 0;        // +0x13a (jackpot)
    int score_special3_flag = 0;   // +0x13e
    int unknown_p71 = 0;           // +0x142
    int ball_count = 0;            // +0x146
    int max_ball_count = 3;        // +0x14a
    int extra_balls = 0;           // +0x14e
    int unknown_p75 = 0;           // +0x152
    int ball_locked_counter = 0;   // +0x156
    int multiball_flag = 0;        // +0x15a
    int unknown_p78 = 0;           // +0x15e
    int replay_active_flag = 0;    // +0x162
    int replay_timer = 0;          // +0x166
    int unknown_p81 = 0;           // +0x16a
    int unknown_p82 = 0;           // +0x16e
    class TComponentGroup* component_group_unused = nullptr;

    int AddScore(int score);        // 01017526
    void ChangeBallCount(int count);  // 010175aa
    void tilt(float time);          // 01017791
    TPinballComponent* find_component(const char* name);  // 01017648
    TPinballComponent* find_component(int group_index);   // 010176be
    static void TiltTimeout(int timer_id, void* caller);       // 01017738
    static void LightShowTimer(int timer_id, void* caller);    // 01017467
    static void EndGame_timeout(int timer_id, void* caller);   // 0101748b
    static void replay_timer_callback(int timer_id, void* caller);  // 010175ef

    // ---- Field tambahan dari porting objek (tambahkan di bawah ini, beri komentar offset) ----
    int sound_index1 = 0, sound_index2 = 0, sound_index3 = 0;  // +0x42..+0x4a
    TPinballComponent* demo = nullptr;  // +0xe6: TDemo (diisi ctor 0101a2b5)
};

namespace pb {

extern TPinballTable* g_main_table;  // DAT_01025658
extern float g_time_now;             // DAT_01025630
extern float g_ball_speed_limit;     // DAT_010253d0
extern int g_demo_mode;              // DAT_01025570
extern int g_paused;                 // DAT_01024fd8
extern int g_cheat_mode;             // DAT_01024ff8
extern int g_players;                // DAT_01028234 (opsi jumlah pemain)
extern void (*g_on_menu_state)(int mode);  // hook UI menu (FUN_010056a9/d3)
extern void (*g_music_hook)(bool play);    // FUN_0100afdc(0) putar / FUN_0100afb9 stop

void create_table(const DatFile* dat, const std::string& data_dir);  // bagian dari 01015424
void frame(int dt_ms);                                                // 01014bf9
void timed_frame(float time_now, float dt, bool draw_balls);          // 01014a68
double collide(float time_now, float dt, TBall* ball);                // 01014956
int mode_change(int mode);   // 0101461a
int game_mode();
void end_game();             // 0101479b
int chk_highscore();         // 01014908
void replay_level(int demo); // 010153ae
void toggle_demo();          // 01015802
void pause_continue();       // 010156e4
void firsttime_setup();      // 01014d59
void keydown(int vk);        // 01015072 (kode virtual-key Windows)
void keyup(int vk);          // 010152e4
void set_keys(int left, int right, int plunger, int bump_left, int bump_right, int bump_bottom);
void render_offset(int& x, int& y);  // geseran tampilan akibat nudge (DAT_010253ac/b0)

}  // namespace pb

namespace highscore {
struct Entry {
    char name[32];
    int score;
};
extern Entry g_table[5];  // DAT_01025578
int get_position(int score);                            // 01004c60
void place_new(int score, int pos, const char* name);  // 01005452 (tanpa dialog nama)
}  // namespace highscore
