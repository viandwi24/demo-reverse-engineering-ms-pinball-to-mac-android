// Modul kontrol bagian 3: fungsi 0x01010ad3-0x01012ee8 (kecuali dispatcher 0x01011ec9).
// Isi: handler misi 0x16..0x20, handler global pasca-kontrol (01011bb5), kickout hyperspace a_kout2
// (01011f1a), plunger (01012285), target popup (0101240d/01012570), drain (010126ba), dan helper
// objlist (01012e51..01012ee8).
//
// Catatan port:
// - Argumen float Message() diverifikasi dari .asm (FLDZ = 0.0f, konstanta 0x0100xxxx).
// - FUN_01003752 menerima 2 argumen (id, 0); port memakai get_rc_string(id).
// - FUN_0100c092(tag) dipanggil lewat light_on(komponen TLight), sesuai prototipe port.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <type_traits>

#include "../ball.h"
#include "../table.h"
#include "control.h"
#include "control_fns.h"

namespace control {

// ---- Hook ke modul lain (diisi integrator; nullptr = nilai 0) ----
// score_struct->score (int pertama struct score, dibaca di 01011985).
int (*g_hook_score_struct_score)(score_struct* s) = nullptr;
// DAT_01025598 + idx*0x24: skor tabel high score ke-idx (0..4), dibaca di 01011985.
int (*g_hook_highscore_score)(int index) = nullptr;

namespace {

// DAT_010246ec: id string pangkat (stride 4 byte, dibaca sebagai short). Entri [9] = 60 (data berikutnya).
const short kRankStringIds[10] = {0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 60};

// FUN_0100c092(tag): lampu menyala / berkedip?
inline int light_on(TLight* light) { return fn_0100c092(light); }

// FUN_0100c335 mengembalikan hasil AddScore di EAX (Ghidra: void). Bila prototipe masih void,
// pakai salinan identik isi 0100c335 agar nilai kembali tersedia.
template <typename F>
int add_score_no_mult(F f, int score) {
    if constexpr (std::is_void_v<std::invoke_result_t<F, int>>) {
        TPinballTable* t = g_table;
        int flag3 = t->score_special3_flag;
        t->score_special3_flag = 0;
        int flag2 = t->score_special2_flag;
        t->score_special2_flag = 0;
        int mult = t->score_multiplier;
        t->score_multiplier = 0;
        int r = table_add_score_ret(score);
        t = g_table;
        t->score_special2_flag = flag2;
        t->score_multiplier = mult;
        t->score_special3_flag = flag3;
        return r;
    } else {
        return f(score);
    }
}

// FUN_0100e50b mengembalikan EBX (1 = outer_circle penuh) — Ghidra: void. Bila prototipe masih void,
// nilai kembali dianggap 0 (perlu prototipe int agar akurat).
template <typename F>
int outer_circle_add(F f, int count) {
    if constexpr (std::is_void_v<std::invoke_result_t<F, int>>) {
        f(count);
        return 0;
    } else {
        return f(count);
    }
}

inline int score_value(score_struct* s) { return g_hook_score_struct_score ? g_hook_score_struct_score(s) : 0; }
inline int highscore_value(int i) { return g_hook_highscore_score ? g_hook_highscore_score(i) : 0; }

// FUN_0100dd7a(0x40, skor): exe asli mengoper skor (int) di slot pointer caller.
inline TPinballComponent* int_as_caller(int v) {
    return reinterpret_cast<TPinballComponent*>(static_cast<std::intptr_t>(v));
}

}  // namespace

// ---------------------------------------------------------------------------------------------
// Misi 0x16 (01010ad3): masuk sink 1.
CONTROL_FN(01010ad3) {
    if (code == 0x3f) {
        if (v_sink1 == caller) {
            lite198->message_field = 0x17;
            fn_01011bb5(0x42, nullptr);
        }
        return;
    }
    if (code == 0x42) {
        lite5->Message(0x13, 0.0f);
        lite4->Message(0xb, 2.0f);
        lite4->Message(0x13, 0.0f);
        lite4->Message(7, 0.0f);
    } else if (code != 0x43) {
        return;
    }
    mission_text_box->Display(get_rc_string(0x8f), -1.0f);
}

// Misi 0x17 (01010b85): masuk sink 2.
CONTROL_FN(01010b85) {
    char buffer[64];
    if (code == 0x3f) {
        if (v_sink2 == caller) {
            lite198->message_field = 1;
            fn_01011bb5(0x42, nullptr);
            mission_text_box->Display(get_rc_string(0x91), 4.0f);
            int score = add_score_no_mult(fn_0100c335, 1500000);
            std::snprintf(buffer, sizeof(buffer), get_rc_string(0x4e), score);
            if (outer_circle_add(fn_0100e50b, 10) == 0) {
                mission_text_box->Display(buffer, 8.0f);
                soundwave9->Play();
            }
        }
        return;
    }
    if (code == 0x42) {
        lite6->Message(0x13, 0.0f);
        lite2->Message(0xb, 1.0f);
        lite2->Message(0x13, 0.0f);
        lite2->Message(7, 0.0f);
    } else if (code != 0x43) {
        return;
    }
    mission_text_box->Display(get_rc_string(0x90), -1.0f);
}

// Misi 0x11 (01010cc2): kena rebound 25x.
CONTROL_FN(01010cc2) {
    char buffer[64];
    if (code == 0x3f) {
        if (v_rebo1 == caller || v_rebo2 == caller || v_rebo3 == caller || v_rebo4 == caller) {
            int left = --lite56->message_field;
            int next_code;
            if (left == 0) {
                lite300->Message(0x14, 0.0f);
                lite322->Message(0x14, 0.0f);
                caller = nullptr;
                lite198->message_field = 0x18;
                next_code = 0x42;
            } else {
                next_code = 0x43;
            }
            fn_01011bb5(next_code, caller);
        }
        return;
    }
    if (code == 0x42) {
        lite56->message_field = 0x19;
        lite300->Message(7, 0.0f);
        lite322->Message(7, 0.0f);
    } else if (code != 0x43) {
        return;
    }
    std::snprintf(buffer, sizeof(buffer), get_rc_string(0x92), lite56->message_field);
    mission_text_box->Display(buffer, -1.0f);
}

// Misi 0x18 (01010dd3): wormhole/ramp → naik pangkat.
CONTROL_FN(01010dd3) {
    // Quirk asli: di cabang ramp dengan middle_circle >= 9, buffer tidak diisi sprintf lalu bisa
    // ditampilkan (isi stack sisa). Port: buffer diawali string kosong.
    char buffer[64];
    buffer[0] = '\0';
    if (code == 0x3f) {
        if (a_kout2 == caller) {
            mission_text_box->Display(get_rc_string(0x2f), 4.0f);
            int rank = middle_circle->Message(0x25, 0.0f);
            if (1 < rank) {
                middle_circle->Message(0x21, 5.0f);
                rank = middle_circle->Message(0x25, 0.0f);
                const char* rank_name = get_rc_string(kRankStringIds[rank - 1]);
                const char* fmt = get_rc_string(0xae);
                std::snprintf(buffer, sizeof(buffer), fmt, rank_name);
                mission_text_box->Display(buffer, 8.0f);
            }
        } else {
            if (ramp != caller) return;
            mission_text_box->Display(get_rc_string(0x2e), 4.0f);
            int rank = middle_circle->Message(0x25, 0.0f);
            if (rank < 9) {
                rank = middle_circle->Message(0x25, 0.0f);
                middle_circle->Message(0x29, 5.0f);
                const char* rank_name = get_rc_string(kRankStringIds[rank]);
                const char* fmt = get_rc_string(0xad);
                std::snprintf(buffer, sizeof(buffer), fmt, rank_name);
            }
            if (outer_circle_add(fn_0100e50b, 0xc) == 0) {
                mission_text_box->Display(buffer, 8.0f);
                soundwave10->Play();
            }
        }
        add_score_no_mult(fn_0100c335, 2000000);
        lite55->Message(0x14, 0.0f);
        lite26->Message(0x14, 0.0f);
        lite304->Message(0x14, 0.0f);
        lite317->Message(0x14, 0.0f);
        lite198->message_field = 1;
        fn_01011bb5(0x42, nullptr);
        return;
    }
    if (code == 0x42) {
        lite55->Message(7, -1.0f);
        lite26->Message(7, -1.0f);
        lite304->Message(7, 0.0f);
        lite317->Message(7, 0.0f);
    } else if (code != 0x43) {
        return;
    }
    mission_text_box->Display(get_rc_string(0x93), -1.0f);
}

// Misi 0x12 (0101106f): kena drop target 3x.
CONTROL_FN(0101106f) {
    char buffer[64];
    if (code == 0x3f) {
        if (a_targ1 == caller || a_targ2 == caller || a_targ3 == caller || a_targ6 == caller ||
            a_targ5 == caller || a_targ4 == caller || a_targ9 == caller || a_targ8 == caller ||
            a_targ7 == caller) {
            int left = --lite56->message_field;
            int next_code;
            if (left == 0) {
                lite303->Message(0x14, 0.0f);
                lite309->Message(0x14, 0.0f);
                lite315->Message(0x14, 0.0f);
                caller = nullptr;
                lite198->message_field = 0x19;
                next_code = 0x42;
            } else {
                next_code = 0x43;
            }
            fn_01011bb5(next_code, caller);
        }
        return;
    }
    if (code == 0x42) {
        lite56->message_field = 3;
        lite303->Message(7, 0.0f);
        lite309->Message(7, 0.0f);
        lite315->Message(7, 0.0f);
    } else if (code != 0x43) {
        return;
    }
    std::snprintf(buffer, sizeof(buffer), get_rc_string(0x94), lite56->message_field);
    mission_text_box->Display(buffer, -1.0f);
}

// Misi 0x19 (010111d3): kena solo target 3x.
CONTROL_FN(010111d3) {
    char buffer[64];
    if (code == 0x3f) {
        if (a_targ10 == caller || a_targ11 == caller || a_targ12 == caller || a_targ13 == caller ||
            a_targ14 == caller || a_targ15 == caller || a_targ16 == caller || a_targ17 == caller ||
            a_targ18 == caller || a_targ19 == caller || a_targ20 == caller || a_targ21 == caller ||
            a_targ22 == caller) {
            int left = --lite56->message_field;
            int next_code;
            if (left == 0) {
                lite306->Message(0x14, 0.0f);
                lite308->Message(0x14, 0.0f);
                lite310->Message(0x14, 0.0f);
                lite313->Message(0x14, 0.0f);
                lite319->Message(0x14, 0.0f);
                caller = nullptr;
                lite198->message_field = 0x1a;
                next_code = 0x42;
            } else {
                next_code = 0x43;
            }
            fn_01011bb5(next_code, caller);
        }
        return;
    }
    if (code == 0x42) {
        lite56->message_field = 3;
        lite306->Message(7, 0.0f);
        lite308->Message(7, 0.0f);
        lite310->Message(7, 0.0f);
        lite313->Message(7, 0.0f);
        lite319->Message(7, 0.0f);
    } else if (code != 0x43) {
        return;
    }
    std::snprintf(buffer, sizeof(buffer), get_rc_string(0x95), lite56->message_field);
    mission_text_box->Display(buffer, -1.0f);
}

// Misi 0x1a (010113a1): lewati rollover 5x.
CONTROL_FN(010113a1) {
    char buffer[64];
    if (code == 0x3f) {
        if (a_roll3 == caller || a_roll2 == caller || a_roll1 == caller || a_roll112 == caller ||
            a_roll111 == caller || a_roll110 == caller || a_roll4 == caller || a_roll8 == caller ||
            a_roll6 == caller || a_roll7 == caller || a_roll5 == caller) {
            int left = --lite56->message_field;
            int next_code;
            if (left == 0) {
                lite301->Message(0x14, 0.0f);
                lite302->Message(0x14, 0.0f);
                lite307->Message(0x14, 0.0f);
                lite316->Message(0x14, 0.0f);
                lite320->Message(0x14, 0.0f);
                lite321->Message(0x14, 0.0f);
                caller = nullptr;
                lite198->message_field = 0x1b;
                next_code = 0x42;
            } else {
                next_code = 0x43;
            }
            fn_01011bb5(next_code, caller);
        }
        return;
    }
    if (code == 0x42) {
        lite56->message_field = 5;
        lite301->Message(7, 0.0f);
        lite302->Message(7, 0.0f);
        lite307->Message(7, 0.0f);
        lite316->Message(7, 0.0f);
        lite320->Message(7, 0.0f);
        lite321->Message(7, 0.0f);
    } else if (code != 0x43) {
        return;
    }
    std::snprintf(buffer, sizeof(buffer), get_rc_string(0x96), lite56->message_field);
    mission_text_box->Display(buffer, -1.0f);
}

// Misi 0x1b (01011582): rollover a_roll184.
CONTROL_FN(01011582) {
    if (code == 0x3f) {
        if (a_roll184 == caller) {
            lite318->Message(0x14, 0.0f);
            lite198->message_field = 0x1c;
            fn_01011bb5(0x42, nullptr);
        }
        return;
    }
    if (code == 0x42) {
        lite56->message_field = 0;
        lite318->Message(7, 0.0f);
    } else if (code != 0x43) {
        return;
    }
    mission_text_box->Display(get_rc_string(0x97), -1.0f);
}

// Misi 0x1c (01011612): ramp.
CONTROL_FN(01011612) {
    if (code == 0x3f) {
        if (ramp == caller) {
            lite317->Message(0x14, 0.0f);
            lite198->message_field = 0x1d;
            fn_01011bb5(0x42, nullptr);
        }
        return;
    }
    if (code == 0x42) {
        lite317->Message(7, 0.0f);
    } else if (code != 0x43) {
        return;
    }
    mission_text_box->Display(get_rc_string(0x98), -1.0f);
}

// Misi 0x1d (01011699): flag spinner.
CONTROL_FN(01011699) {
    if (code == 0x3f) {
        if (a_flag1 == caller || a_flag2 == caller) {
            lite305->Message(0x14, 0.0f);
            lite312->Message(0x14, 0.0f);
            lite198->message_field = 0x1e;
            fn_01011bb5(0x42, nullptr);
            lite20->Message(0x14, 0.0f);
            lite19->Message(0x14, 0.0f);
        }
        return;
    }
    if (code == 0x42) {
        lite20->Message(0x13, 0.0f);
        lite19->Message(0x13, 0.0f);
        lite305->Message(7, 0.0f);
        lite312->Message(7, 0.0f);
    } else if (code != 0x43) {
        return;
    }
    mission_text_box->Display(get_rc_string(0x99), -1.0f);
}

// Misi 0x1e (01011798): sink mana saja.
CONTROL_FN(01011798) {
    if (code == 0x3f) {
        if (v_sink1 == caller || v_sink2 == caller || v_sink3 == caller) {
            lite198->message_field = 0x1f;
            fn_01011bb5(0x42, nullptr);
        }
        return;
    }
    if (code == 0x42) {
        fn_0100c397(1);
        v_sink1->Message(7, 0.0f);
        v_sink2->Message(7, 0.0f);
        v_sink3->Message(7, 0.0f);
    } else if (code != 0x43) {
        return;
    }
    mission_text_box->Display(get_rc_string(0x9a), -1.0f);
}

// Misi 0x1f (0101184c): hyperspace kickout.
CONTROL_FN(0101184c) {
    char buffer[64];
    if (code == 0x3f) {
        if (a_kout2 == caller) {
            lite304->Message(0x14, 0.0f);
            lite130->Message(0x14, 0.0f);
            lite198->message_field = 1;
            fn_01011bb5(0x42, nullptr);
            int score = add_score_no_mult(fn_0100c335, 5000000);
            std::snprintf(buffer, sizeof(buffer), get_rc_string(0x4e), score);
            info_text_box->Display(get_rc_string(0x30), 4.0f);
            if (outer_circle_add(fn_0100e50b, 0x12) == 0) {
                mission_text_box->Display(buffer, 8.0f);
                soundwave9->Play();
            }
        }
        return;
    }
    if (code == 0x42) {
        lite130->Message(0x13, 0.0f);
        lite304->Message(7, 0.0f);
    } else if (code != 0x43) {
        return;
    }
    mission_text_box->Display(get_rc_string(0x9b), -1.0f);
}

// Misi 0x20 (01011985): game over — gilir tampilkan skor pemain lalu tabel high score.
// mission_text_box->message_field: bit 0x100 = fase skor pemain, 0x200 = fase high score, bit 0..3 = indeks.
void fn_01011985(int code) {
    char buffer[128];
    if (code == 0x42) {
        goal_lights->Message(0x14, 0.0f);
        pb_mode_change(2);
        a_flip1->Message(0x3fe, 0.0f);
        a_flip2->Message(0x3fe, 0.0f);
        mission_text_box->message_field = 0;
        return;
    }
    if (code != 0x43) return;

    unsigned state = static_cast<unsigned>(mission_text_box->message_field);
    unsigned idx;
    if ((state & 0x100) == 0) {
        if ((state & 0x200) == 0) goto default_msg;
        idx = state;
    } else {
        unsigned player = state & 0xf;
        int score = score_value(g_table->player_scores[player].score_struct);
        idx = player + 1;
        if (score >= 0) {
            short id;
            if (player == 0) id = 0xb4;
            else if (player == 1) id = 0xb5;
            else if (player == 2) id = 0xb6;
            else if (player == 3) id = 0xb7;
            else goto to_highscores;
            std::snprintf(buffer, sizeof(buffer), get_rc_string(id), score);
            mission_text_box->Display(buffer, 3.0f);
            if (idx == static_cast<unsigned>(g_table->player_count)) idx = 0x200;
            else idx |= 0x100;
            mission_text_box->message_field = static_cast<int>(idx);
            return;
        }
    to_highscores:
        mission_text_box->message_field = 0x200;
    }
    {
        idx &= 0xf;
        int score = highscore_value(static_cast<int>(idx));
        if (score < 1) goto default_msg;
        short id;
        if (idx == 0) id = 0xb8;
        else if (idx == 1) id = 0xb9;
        else if (idx == 2) id = 0xba;
        else if (idx == 3) id = 0xbb;
        else if (idx == 4) id = 0xbc;
        else goto default_msg;
        std::snprintf(buffer, sizeof(buffer), get_rc_string(id), score);
        mission_text_box->Display(buffer, 3.0f);
        if (idx + 1 == 5) idx = 0;
        else idx = (idx + 1) | 0x200;
        mission_text_box->message_field = static_cast<int>(idx);
        return;
    }
default_msg:
    mission_text_box->message_field = 0x100;
    mission_text_box->Display(get_rc_string(0xac), 10.0f);
}

// 01011b98: misi tak dikenal → kembali ke misi 1 (pilih misi).
void fn_01011b98() {
    lite198->message_field = 1;
    fn_01011bb5(0x42, nullptr);
}

// 01011bb5: handler global setelah setiap handler kontrol; meneruskan event ke misi aktif
// (lite198->message_field).
CONTROL_FN(01011bb5) {
    if (lite198 == nullptr) return;
    int mission = lite198->message_field;
    if (code == 0x2f) {
        if (fuel_bargraph == caller && 1 < mission) {
            l_trek_lights->Message(0x22, 0.0f);
            l_trek_lights->Message(0x14, 0.0f);
            r_trek_lights->Message(0x22, 0.0f);
            r_trek_lights->Message(0x14, 0.0f);
            mission_text_box->Display(get_rc_string(0x6d), 4.0f);
            lite198->message_field = 1;
            fn_01011bb5(0x42, nullptr);
        }
    } else if (code == 0x3c) {
        if (fuel_bargraph == caller && mission != 0) {
            if (fuel_bargraph->Message(0x25, 0.0f) == 1) {
                mission_text_box->Display(get_rc_string(0x74), 4.0f);
            }
        } else if (mission_text_box == caller) {
            code = 0x43;
        }
    } else if (code == 0x3f1) {
        code = 0x43;
    }

    // Switch memakai nilai misi SEBELUM perubahan di atas (EBX dimuat ulang dari [EBP-4]).
    switch (mission) {
    case 0: fn_0100e657(code, caller); break;
    case 1: fn_0100e6d2(code, caller); break;
    case 2: fn_0100ebf3(code, caller); break;
    case 3: fn_0100ed83(code, caller); break;
    case 4: fn_0100eed7(code, caller); break;
    case 5: fn_0100f0b3(code, caller); break;
    case 6: fn_0100f381(code, caller); break;
    case 7: fn_0100f6f4(code, caller); break;
    case 8: fn_0100f539(code, caller); break;
    case 9: fn_0100f900(code, caller); break;
    case 10: fn_0100ff35(code, caller); break;
    case 0xb: fn_0100fcd9(code, caller); break;
    case 0xc: fn_0101022d(code, caller); break;
    case 0xd: fn_01010385(code, caller); break;
    case 0xe: fn_010105e3(code, caller); break;
    case 0xf: fn_01010767(code, caller); break;
    case 0x10: fn_010109d5(code, caller); break;
    case 0x11: fn_01010cc2(code, caller); break;
    case 0x12: fn_0101106f(code, caller); break;
    case 0x14: fn_01010051(code, caller); break;
    case 0x15: fn_010108bc(code, caller); break;
    case 0x16: fn_01010ad3(code, caller); break;
    case 0x17: fn_01010b85(code, caller); break;
    case 0x18: fn_01010dd3(code, caller); break;
    case 0x19: fn_010111d3(code, caller); break;
    case 0x1a: fn_010113a1(code, caller); break;
    case 0x1b: fn_01011582(code, caller); break;
    case 0x1c: fn_01011612(code, caller); break;
    case 0x1d: fn_01011699(code, caller); break;
    case 0x1e: fn_01011798(code, caller); break;
    case 0x1f: fn_0101184c(code, caller); break;
    case 0x20: fn_01011985(code); break;
    default: fn_01011b98(); break;  // termasuk 0x13 (tabel lompat → default)
    }
}

// 01011f1a: kickout hyperspace (a_kout2).
CONTROL_FN(01011f1a) {
    char buffer[64];
    if (code != 0x3f) return;

    int level = hyperspace_lights->Message(0x25, 0.0f);
    fn_0100d286(0x29, hyperspace_lights);
    short msg_id = 0;
    bool show = false;
    int score = 0;
    switch (level) {
    case 0:
        score = table_add_score_ret(caller->get_scoring(0));
        msg_id = 0xc;
        show = true;
        break;
    case 1: {
        int jackpot = add_score_no_mult(fn_0100c335, g_table->score_special3);
        std::snprintf(buffer, sizeof(buffer), get_rc_string(0xe), jackpot);
        info_text_box->Display(buffer, 2.0f);
        g_table->score_special3 = 20000;
        break;
    }
    case 2:
        fn_0100c764(0x34, v_bloc1);
        score = table_add_score_ret(caller->get_scoring(2));
        msg_id = 2;
        show = true;
        break;
    case 3:
        fn_0100cd12(0x13, nullptr);
        score = table_add_score_ret(caller->get_scoring(3));
        msg_id = 8;
        show = true;
        break;
    case 4: {
        hyperspace_lights->Message(0, 0.0f);
        int s = table_add_score_ret(caller->get_scoring(4));
        fn_0100dd7a(0x40, int_as_caller(s));
        break;
    }
    default:
        break;
    }
    if (show) {
        std::snprintf(buffer, sizeof(buffer), get_rc_string(msg_id), score);
        info_text_box->Display(buffer, 2.0f);
    }

    int bonus = 0;
    if (light_on(lite25)) {
        bonus = 1;
        int s = add_score_no_mult(fn_0100c335, g_table->score_special1);
        std::snprintf(buffer, sizeof(buffer), get_rc_string(10), s);
        info_text_box->Display(buffer, 2.0f);
    }
    if (light_on(lite26)) bonus |= 2;
    if (light_on(lite130)) {
        bonus |= 4;
        lite130->Message(0x14, 0.0f);
        fn_0100d7bb(0x40, top_target_lights);
        bumper_target_lights->Message(0x13, 0.0f);
        fn_0100c14f();
        fn_0100c199();
        fn_0100c1e3();
        fn_0100c24a();
        lite27->Message(0x13, 0.0f);
        lite28->Message(0x13, 0.0f);
        fn_0100cd12(0x13, nullptr);
        fn_0100c764(0x34, v_bloc1);
        if (g_table->multiball_flag != 0) fn_0100c12a();
        TPinballTable* t = g_table;
        if (t->score_special3 < 100000) t->score_special3 = 100000;
        if (t->score_special2 < 100000) t->score_special2 = 100000;
        fn_0100dd7a(0x40, nullptr);
    }

    float duration;
    if (bonus == 0) {
        TSound* snd;
        if (level == 1) snd = soundwave36;        // DAT_01023d8c
        else if (level == 2) snd = soundwave35;   // DAT_01023d94
        else if (level == 3) snd = soundwave38;   // DAT_01023d9c
        else if (level == 4) snd = soundwave39;   // DAT_01023da4
        else snd = soundwave35;                   // DAT_01023d84
        duration = snd->Play();
    } else if (bonus == 1) {
        duration = soundwave21->Play();           // DAT_01023d24
    } else if (bonus <= 3) {
        duration = soundwave40->Play();           // DAT_01023d64
    } else {
        duration = soundwave41->Play();           // DAT_01023d6c
        soundwave36->Play();                      // DAT_01023d74
        soundwave50->Play();                      // DAT_01023d7c
    }
    lite25->Message(7, 5.0f);
    caller->Message(0x37, duration);
}

// 01012285: plunger.
CONTROL_FN(01012285) {
    (void)caller;
    if (code == 0x3f7) {
        fn_01011bb5(0x43, nullptr);
    } else if (code == 0x3f8) {
        g_flag_44 = 0;
        if (middle_circle->Message(0x25, 0.0f) == 0) {
            middle_circle->Message(0x20, 0.0f);
        }
        if (!light_on(lite200)) {
            skill_shot_lights->Message(0x14, 0.0f);
            lite67->Message(0x13, 0.0f);
            skill_shot_lights->Message(0x1a, 0.25f);
            l_trek_lights->Message(0x14, 0.0f);
            l_trek_lights->Message(0x20, 0.2f);
            l_trek_lights->Message(0x1a, 0.2f);
            r_trek_lights->Message(0x14, 0.0f);
            r_trek_lights->Message(0x20, 0.2f);
            r_trek_lights->Message(0x1a, 0.2f);
            g_table->score_special1 = 25000;
            fn_0100d7bb(0x41, top_target_lights);
            fuel_bargraph->Message(0x13, 0.0f);
            lite200->Message(0x13, 0.0f);
            v_gate1->Message(0x35, 0.0f);
            v_gate2->Message(0x35, 0.0f);
        }
        lite200->message_field = 0;
    }
}

// 0101240d: popup target kiri (a_targ4/5/6).
CONTROL_FN(0101240d) {
    if (code != 0x3f) return;
    if (caller->message_field != 0) return;
    caller->message_field = 1;
    if (a_targ4->message_field + a_targ5->message_field + a_targ6->message_field != 3) {
        table_add_score_ret(caller->get_scoring(0));
        return;
    }
    fn_0100d73e(0x29, bumper_target_lights);
    int level = bumper_target_lights->Message(0x25, 0.0f);
    if (level == 1) {
        table_add_score_ret(caller->get_scoring(1));
        info_text_box->Display(get_rc_string(0x35), 2.0f);
    } else if (level == 2) {
        table_add_score_ret(caller->get_scoring(2));
        info_text_box->Display(get_rc_string(0x36), 2.0f);
    } else {
        info_text_box->Display(get_rc_string(0x37), 2.0f);
        fn_0100c2f3(4.0f);
    }
    a_targ6->message_field = 0;
    a_targ6->Message(0x32, 0.0f);
    a_targ5->message_field = 0;
    a_targ5->Message(0x32, 0.0f);
    a_targ4->message_field = 0;
    a_targ4->Message(0x32, 0.0f);
}

// 01012570: popup target atas (a_targ7/8/9) → multiplier skor.
CONTROL_FN(01012570) {
    if (code != 0x3f || caller->message_field != 0) return;
    caller->message_field = 1;
    if (a_targ7->message_field + a_targ8->message_field + a_targ9->message_field == 3) {
        table_add_score_ret(caller->get_scoring(1));
        fn_0100d7bb(0x29, top_target_lights);
        int level = top_target_lights->Message(0x25, 0.0f);
        short id;
        if (level == 1) {
            g_table->score_multiplier = 1;
            id = 0x38;
        } else if (level == 2) {
            g_table->score_multiplier = 2;
            id = 0x39;
        } else if (level == 3) {
            g_table->score_multiplier = 3;
            id = 0x3a;
        } else {
            g_table->score_multiplier = 4;
            id = 0x3b;
        }
        info_text_box->Display(get_rc_string(id), 2.0f);
        a_targ9->message_field = 0;
        a_targ9->Message(0x32, 0.0f);
        a_targ8->message_field = 0;
        a_targ8->Message(0x32, 0.0f);
        a_targ7->message_field = 0;
        a_targ7->Message(0x32, 0.0f);
    } else {
        table_add_score_ret(caller->get_scoring(0));
    }
}

// 010126ba: drain — bola hilang → bola berikut / game over.
CONTROL_FN(010126ba) {
    (void)caller;
    char buffer[64];
    if (code == 0x3c) {
        if (lite199->message_field != 0) {
            g_table->Message(0x3fe, 0.0f);
            if (pb_chk_highscore() != 0) {
                soundwave3->Play();
                g_table->light_group->Message(0x10, 3.0f);
                mission_text_box->Display(get_rc_string(0xb1), -1.0f);
            }
            return;
        }
        plunger->Message(0x3f8, 0.0f);
        return;
    }
    if (code != 0x3f) return;

    if (g_flag_44 != 0) {
        drain->Message(0x400, 0.0f);
        v_sink3->Message(0x38, 0.0f);
        return;
    }

    TPinballTable* t = g_table;
    if (t->tilt_lock != 0) {
        lite200->Message(0x14, 0.0f);
        lite199->Message(0x14, 0.0f);
        t = g_table;
    }
    if (light_on(lite200)) {
        soundwave27->Play();
        lite200->Message(0x13, 0.0f);
        info_text_box->Display(get_rc_string(0x60), -1.0f);
        soundwave59->Play();
        return;
    }
    if (light_on(lite199)) {
        soundwave27->Play();
        lite199->Message(0x14, 0.0f);
        lite200->Message(0x13, 0.0f);
        info_text_box->Display(get_rc_string(0x5f), 2.0f);
        soundwave59->Play();
        g_table->unknown_p78 -= 1;  // +0x15e
        return;
    }
    if (t->unknown_p75 != 0) {  // +0x152
        soundwave27->Play();
        g_table->unknown_p75 -= 1;
        return;
    }
    if (t->tilt_lock == 0) {
        int bonus = add_score_no_mult(fn_0100c335, t->score_special2);
        std::snprintf(buffer, sizeof(buffer), get_rc_string(0x5e), bonus);
        info_text_box->Display(buffer, 2.0f);
        t = g_table;
    }
    if (t->extra_balls == 0) {
        t->ChangeBallCount(t->ball_count - 1);
        TPinballTable* t2 = g_table;
        if (t2->current_player + 1 == t2->player_count && t2->ball_count == 0) {
            lite199->message_field = 1;
        } else {
            t2->Message(0x3fd, 0.0f);
            lite199->message_field = 0;
        }
        soundwave27->Play();
    } else {
        t->extra_balls -= 1;
        soundwave59->Play();
        int player = g_table->current_player;
        short id;
        if (player == 0) id = 0x61;
        else if (player == 1) id = 0x62;
        else if (player == 2) id = 99;
        else id = 100;
        info_text_box->Display(get_rc_string(id), -1.0f);
    }

    bmpr_inc_lights->Message(0x14, 0.0f);
    ramp_bmpr_inc_lights->Message(0x14, 0.0f);
    lite30->Message(0x14, 0.0f);
    lite29->Message(0x14, 0.0f);
    lite1->Message(0x14, 0.0f);
    lite54->Message(0x14, 0.0f);
    lite55->Message(0x14, 0.0f);
    lite56->Message(0x14, 0.0f);
    lite17->Message(0x14, 0.0f);
    lite18->Message(0x14, 0.0f);
    lite27->Message(0x14, 0.0f);
    lite28->Message(0x14, 0.0f);
    lite16->Message(0x14, 0.0f);
    lite20->Message(0x14, 0.0f);
    hyperspace_lights->Message(0x14, 0.0f);
    lite25->Message(0x14, 0.0f);
    lite26->Message(0x14, 0.0f);
    lite130->Message(0x14, 0.0f);
    lite19->Message(0x14, 0.0f);
    worm_hole_lights->Message(0x14, 0.0f);
    bsink_arrow_lights->Message(0x14, 0.0f);
    l_trek_lights->Message(0x14, 0.0f);
    r_trek_lights->Message(0x14, 0.0f);
    lite60->Message(0x14, 0.0f);
    lite59->Message(0x14, 0.0f);
    lite61->Message(0x14, 0.0f);
    bumper_target_lights->Message(0x14, 0.0f);
    top_target_lights->Message(0x14, 0.0f);
    top_circle_tgt_lights->Message(0x14, 0.0f);
    ramp_tgt_lights->Message(0x14, 0.0f);
    lchute_tgt_lights->Message(0x14, 0.0f);
    bpr_solotgt_lights->Message(0x14, 0.0f);
    lite110->Message(0x14, 0.0f);
    skill_shot_lights->Message(0x14, 0.0f);
    lite77->Message(0x14, 0.0f);
    lite198->Message(0x14, 0.0f);
    lite196->Message(0x14, 0.0f);
    lite195->Message(0x14, 0.0f);
    fuel_bargraph->Message(0x14, 0.0f);
    fuel_bargraph->Message(0x400, 0.0f);
    fn_0100dd7a(0x400, nullptr);
    lite62->Message(0x14, 0.0f);
    lite4->message_field = 0;
    lite101->message_field = 0;
    lite102->message_field = 0;
    lite103->message_field = 0;
    ramp_tgt_lights->message_field = 0;
    outer_circle->Message(0x22, 0.0f);
    middle_circle->Message(0x22, 0.0f);
    attack_bumpers->Message(0x400, 0.0f);
    launch_bumpers->Message(0x400, 0.0f);
    v_gate1->Message(0x400, 0.0f);
    v_gate2->Message(0x400, 0.0f);
    v_bloc1->Message(0x400, 0.0f);
    a_targ1->Message(0x400, 0.0f);
    a_targ2->Message(0x400, 0.0f);
    a_targ3->Message(0x400, 0.0f);
    a_targ6->Message(0x400, 0.0f);
    a_targ5->Message(0x400, 0.0f);
    a_targ4->Message(0x400, 0.0f);
    a_targ9->Message(0x400, 0.0f);
    a_targ8->Message(0x400, 0.0f);
    a_targ7->Message(0x400, 0.0f);
    lite198->message_field = (lite199->message_field != 0) ? 0x20 : 0;
    fn_01011bb5(0x42, nullptr);
    g_table->Message(0x3f4, 0.0f);
    if (light_on(lite58)) {
        lite58->Message(0x14, 0.0f);
    } else {
        g_table->score_special2 = 25000;
    }
}

// ---- objlist_class (01012e51..01012ee8): {int size; int count; void* array[size + 1]} ----
namespace {
struct objlist_struct1 {
    int size;
    int count;
    void* array[1];
};

std::size_t objlist_bytes(int size) {
    // Asli: size*4 + 0xc (header 8 byte + size+1 slot).
    return offsetof(objlist_struct1, array) + static_cast<std::size_t>(size + 1) * sizeof(void*);
}
}  // namespace

// 01012e51: tambah elemen; return count baru, 0 bila penuh.
int fn_01012e51(void* list, void* value) {
    auto* l = static_cast<objlist_struct1*>(list);
    if (l->count < l->size) {
        l->array[l->count] = value;
        l->count += 1;
        return l->count;
    }
    return 0;
}

// 01012e7a: hapus elemen (cari dari belakang; slot diisi elemen terakhir); return 1 bila ketemu.
int fn_01012e7a(void* list, void* value) {
    auto* l = static_cast<objlist_struct1*>(list);
    for (int i = l->count - 1; i >= 0; --i) {
        if (l->array[i] == value) {
            l->array[i] = l->array[l->count - 1];
            l->count -= 1;
            return 1;
        }
    }
    return 0;
}

// 01012ebe: alokasi list berkapasitas size (FUN_010054c2 = memory::allocate).
void* fn_01012ebe(int size) {
    auto* l = static_cast<objlist_struct1*>(std::malloc(objlist_bytes(size)));
    if (l != nullptr) {
        l->count = 0;
        l->size = size;
    }
    return l;
}

// 01012ee8: perbesar kapasitas bila count+grow > size (FUN_01005579 = memory::realloc).
void* fn_01012ee8(void* list, int grow) {
    auto* l = static_cast<objlist_struct1*>(list);
    if (l != nullptr && l->size < l->count + grow) {
        auto* n = static_cast<objlist_struct1*>(std::realloc(l, objlist_bytes(l->count + grow)));
        if (n != nullptr) {
            n->size = n->count + grow;
            l = n;
        }
    }
    return l;
}

}  // namespace control
