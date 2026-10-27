// Modul kontrol (aturan game). Port dari blok 0x0100bf8e-0x01012ee8 Pinball.exe.
//
// Setiap komponen yang punya aturan menyimpan pointer ke entri tabel kontrol di +0x12
// (TPinballComponent::control). control_handler(code, caller) memanggil fungsi kontrol itu,
// lalu fungsi global FUN_01011bb5.
#pragma once

#include "tags.h"

struct score_struct;

namespace control {

using ControlFn = void (*)(int code, TPinballComponent* caller);

// Entri tabel di 0x01023ef0 (88 entri x 16 byte).
struct component_control {
    ControlFn fn;
    int score_count;
    const int* scores;
};

// Variabel global kontrol (alamat asli di komentar).
extern TPinballTable* g_table;    // DAT_01025040
extern int g_flag_44;             // DAT_01025044
extern int g_flag_48;             // DAT_01025048
extern int g_flag_4c;             // DAT_0102504c
extern int g_mission;
extern int g_dat_01024ff8;          // DAT_01024ff8: mode "hidden test" (cheat)             // DAT_01025050: id misi aktif

void make_links(TPinballTable* table);  // 0100c01d
void handler(int code, TPinballComponent* caller);  // 01011ec9

// ---- Fungsi kontrol: satu fungsi per alamat asli, ditulis di control_*.cpp ----
// Nama: fn_<alamat> agar mudah dicocokkan dengan decompile. Tanda tangan seragam.
#define CONTROL_FN(addr) void fn_##addr(int code, TPinballComponent* caller)

// ---- API pendukung yang dipanggil kode kontrol ----
const char* get_rc_string(int id);            // FUN_01003752
void table_add_score(int points);             // FUN_01017526 (TPinballTable::AddScore) — return poin final
int table_add_score_ret(int points);
void table_set_ball_count(int count);         // FUN_010175aa
int pb_mode_change(int mode);                 // FUN_0101461a
int pb_chk_highscore();                       // FUN_01014908

// Akses data skor untuk fn_01011985 (diisi pb::create_table).
extern int (*g_hook_score_struct_score)(score_struct* s);
extern int (*g_hook_highscore_score)(int index);

}  // namespace control
