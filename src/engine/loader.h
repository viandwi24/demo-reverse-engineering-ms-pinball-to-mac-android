// Query data objek dari PINBALL.DAT. Port dari blok "loader" 0x0100901f-0x01009895.
#pragma once

#include <string>
#include <vector>

#include "../dat_file.h"
#include "maths.h"

struct visual_kicker {
    float threshold;  // 0x191
    float boost;      // 0x192
    float unknown193;
    vector3 vector;   // 0x194
    float unknown195;
    int hard_hit_sound;  // 0x196
};

// Layout mengikuti array float yang diisi FUN_0100950a (indeks 0..17).
struct visual_struct {
    float smoothness;     // [0]  material 0x12d
    float elasticity;     // [1]  material 0x12e
    int float_arr_count;  // [2]  jumlah titik dinding (atau kode 1=lingkaran, 2=garis)
    const float* float_arr;  // [3]
    int sound_index4;     // [4]  material 0x130 / 0x130
    visual_kicker kicker; // [5..12]
    int collision_group;  // [13] mask bit dari 0x25a
    int sound_index2;     // [14] 0x44c
    int sound_index3;     // [15] 0x44d
    const Bitmap8* bitmap;  // [16]
    const ZMap* zmap;       // [17]
};

namespace loader {

void init(const DatFile* dat, const std::string& data_dir);  // FUN_0100916a: kumpulkan grup suara (tipe 0xca)
const DatFile* dat();

int query_handle(const std::string& name);  // 010091eb, -1 bila tidak ada
int query_visual_states(int group);         // 01009207
int query_visual(int group, int state, visual_struct& out);   // 0100950a
const char* query_name(int group);                             // 0100975d
const float* query_float_attribute(int group, int state, int key);  // 0100978e
const int16_t* query_iattribute(int group, int key, int& count);    // 0100981a
int get_sound_id(int group);   // 0100901f
float play_sound(int sound_id);  // 01009895: return durasi (detik)

// Hook untuk modul audio (tahap 4).
using SoundPlayer = void (*)(const std::string& wav_name);
void set_sound_player(SoundPlayer player);

}  // namespace loader
