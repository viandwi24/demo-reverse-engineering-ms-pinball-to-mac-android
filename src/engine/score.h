// Tampilan angka (skor, bola, nomor pemain) dan font pesan. Port dari 0x01013af1-0x01013e73.
#pragma once

#include <string>

#include "../dat_file.h"
#include "misc_objects.h"

// struct 0x44 byte (FUN_01013bb3)
struct score_struct {
    int score = -9999;        // [0]
    int dirty_flag = 0;       // [1]
    const Bitmap8* background = nullptr;  // [2]
    int offset_x = 0, offset_y = 0, width = 0, height = 0;  // [3..6]
    const Bitmap8* char_bmp[10] = {};  // [7..16] digit 0-9
};

namespace score {

score_struct* create(const char* field_name, const Bitmap8* background);  // 01013bb3
score_struct* dup(const score_struct* s);                                  // 01013c48
void set(score_struct* s, int value);                                      // 01013c89
void erase(score_struct* s, int blit_flag);                                // 01013caa
void update(score_struct* s);                                              // 01013d2d

// FUN_01013e73: muat font bitmap "PBMSG_FT" (resource RCDATA exe, dibaca dari Pinball.exe pengguna).
const score_msg_font_type* load_msg_font();

}  // namespace score
