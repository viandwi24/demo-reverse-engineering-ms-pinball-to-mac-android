// Modul kontrol bagian 2: fungsi asli di rentang [0x0100d5ea, 0x01010ad3).
// Handler kontrol komponen (target, kickout, tripwire, ...), deteksi kode cheat (0100e1b0),
// promosi pangkat (0100e50b / 0100e111) dan handler misi (0100e657 .. 010109d5).
//
// Konvensi pemanggilan Message: argumen kedua selalu float (dicek dari .asm: FLDZ / FLD konstanta).
// Nilai konstanta: 0x010015ac=-1.0, 0x01002374=2.0, 0x0100237c=5.0, 0x01002384=11.0, 0x0100238c=3.0,
// 0x01002398=4.0, 0x0100239c=30.0, 0x010023a0=8.0, 0x010023a4=0.2.

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <string>

#include "../table.h"
#include "control.h"
#include "control_fns.h"

namespace control {

// DAT_01024ff8: mode "hidden test" (diaktifkan kode cheat "HIDDEN TEST"). Dibaca keydown pb (01015072)
// dan window proc (01007a3e) — bukan bagian modul kontrol, tapi ditulis di sini.
int g_dat_01024ff8 = 0;  // DAT_01024ff8

namespace {

// Salinan perilaku FUN_0100c092 untuk sebuah TLight: 1 bila light_on / toggled_on / flasher_on.
// (Asli menerima pointer ke tag; di port tag = pointer komponen.)
int light_active(TLight* light) {
    if (light->light_on == 0 && light->toggled_on == 0 && light->flasher_on == 0) return 0;
    return 1;
}

// Salinan perilaku FUN_0100c335 *dengan* nilai kembali: asli mengembalikan EAX dari AddScore
// (prototipe fn_0100c335 di control_fns.h bertipe void). AddScore tanpa pengali/bonus/jackpot.
int add_score_plain(int score) {
    TPinballTable* t = g_table;
    int jackpot_flag = t->score_special3_flag;  // +0x13e
    t->score_special3_flag = 0;
    int bonus_flag = t->score_special2_flag;  // +0x136
    t->score_special2_flag = 0;
    int mult = t->score_multiplier;  // +0x126
    t->score_multiplier = 0;
    int result = t->AddScore(score);
    t = g_table;
    t->score_special2_flag = bonus_flag;
    t->score_multiplier = mult;
    t->score_special3_flag = jackpot_flag;
    return result;
}

// sprintf (import 0x0100133c) ke buffer lokal 64 byte.
void fmt(char (&buf)[64], const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    std::vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
}

// Tabel skor misi (PTR_PTR_010246a0, indeks = lite56->message_field). Indeks 0/1 adalah data lain
// di exe (pointer) dan tidak pernah dipakai.
const int k_mission_score[19] = {16923984, 16924176, 10000, 10000, 10000, 10000, 20000, 20000, 20000, 20000,
                                 20000,    20000,    20000, 20000, 20000, 30000, 30000, 30000, 30000};
// Id string nama pangkat (DAT_010246ec, short stride 4).
const short k_rank_string[9] = {84, 85, 86, 87, 88, 89, 90, 91, 92};
// Id string nama misi (DAT_01024708, short stride 4; indeks = lite56->message_field).
const short k_mission_name_string[19] = {91, 92, 60, 61, 62, 63, 64, 65, 66, 67,
                                         68, 69, 70, 71, 72, 73, 74, 75, 76};

// Blok penutup misi selesai yang sama di semua handler misi:
// pesan selesai (4 detik), skor tanpa pengali, "Mission Completed\n%ld", naik pangkat.
void mission_award(int text_id, int score, int rank_steps) {
    char buf[64];
    mission_text_box->Display(get_rc_string(text_id), 4.0f);
    int s = add_score_plain(score);
    fmt(buf, get_rc_string(0x4e), static_cast<long>(s));
    if (fn_0100e50b(rank_steps) == 0) {
        mission_text_box->Display(buf, 8.0f);
        soundwave9->Play();
    }
}

// Tampilkan hitungan sisa misi (lite56->message_field) dengan format rc_id, permanen.
void mission_show_count(int rc_id) {
    char buf[64];
    int count = lite56->message_field;
    fmt(buf, get_rc_string(rc_id), count);
    mission_text_box->Display(buf, -1.0f);
}

// Reset satu popup target (message_field = 0, Message(0x32)).
void reset_popup(TPinballComponent* t) {
    t->message_field = 0;
    t->Message(0x32, 0.0f);
}

}  // namespace

// a_targ1..3 (popup target kiri atas).
void fn_0100d5ea(int code, TPinballComponent* caller) {
    if (code != 0x3f) return;
    if (caller->message_field != 0) return;
    caller->message_field = 1;
    int score_index;
    if (a_targ3->message_field + a_targ2->message_field + a_targ1->message_field != 3) {
        score_index = 0;
    } else {
        TSound* sound = nullptr;
        if (light_active(lite61) == 0) {
            if (lite198->message_field != 0xf && lite198->message_field != 0x1d) {
                fn_0100c1e3();
                sound = soundwave47;
            }
        } else if (light_active(lite60) == 0) {
            fn_0100c14f();
            sound = soundwave45;
        } else if (light_active(lite59) == 0) {
            fn_0100c199();
            sound = soundwave46;
        } else if (light_active(lite58) == 0) {
            fn_0100c24a();
            sound = soundwave48;
        } else {
            g_table->AddScore(caller->get_scoring(1));
            sound = soundwave48;
        }
        if (sound) sound->Play();
        reset_popup(a_targ1);
        reset_popup(a_targ2);
        reset_popup(a_targ3);
        score_index = 1;
    }
    g_table->AddScore(caller->get_scoring(score_index));
}

// bumper_target_lights.
void fn_0100d73e(int code, TPinballComponent* caller) {
    if (code == 0) {
        caller->Message(0, 0.0f);
        return;
    }
    if (code == 0x29) {
        caller->Message(0x29, 2.0f);
    } else {
        if (code != 0x3d) return;
        caller->Message(0x21, 0.0f);
        if (caller->Message(0x25, 0.0f) == 0) return;
    }
    caller->Message(0x2b, 30.0f);
}

// top_target_lights (pengali skor).
void fn_0100d7bb(int code, TPinballComponent* caller) {
    float value;
    if (code == 0) {
        caller->Message(0, 0.0f);
        return;
    }
    if (code == 0x29) {
        caller->Message(0x29, 2.0f);
        value = 30.0f;
    } else if (code == 0x3d) {
        if (g_table->score_multiplier != 0) g_table->score_multiplier = g_table->score_multiplier - 1;
        caller->Message(0x21, 0.0f);
        if (caller->Message(0x25, 0.0f) == 0) return;
        value = 30.0f;
    } else if (code == 0x40) {
        g_table->score_multiplier = 4;
        caller->Message(0x13, 0.0f);
        caller->Message(0x2b, 30.0f);
        info_text_box->Display(get_rc_string(0x3b), 2.0f);
        return;
    } else {
        if (code != 0x41) return;
        g_table->score_multiplier = 0;
        caller->Message(0x14, 0.0f);
        value = -1.0f;
    }
    caller->Message(0x2b, value);
}

// a_targ10..12.
void fn_0100d8de(int code, TPinballComponent* caller) {
    if (code != 0x3f) return;
    TLight* light = lite70;
    if (a_targ10 != caller) {
        light = lite71;
        if (a_targ11 != caller) light = lite72;
    }
    light->Message(0xf, 2.0f);
    g_table->AddScore(caller->get_scoring(0));
    if (top_circle_tgt_lights->Message(0x25, 0.0f) == 3) {
        top_circle_tgt_lights->Message(0x10, 2.0f);
        fuel_bargraph->Message(0x2d, 11.0f);
        soundwave25->Play();
        info_text_box->Display(get_rc_string(0x2c), 2.0f);
    } else {
        soundwave49D->Play();
    }
}

// a_targ13..15 (pemilih misi).
void fn_0100d9c2(int code, TPinballComponent* caller) {
    if (code != 0x3f) return;
    TLight* light;
    if (a_targ13 == caller) {
        lite101->message_field = lite101->message_field | 1;
        light = lite101;
    } else if (a_targ14 == caller) {
        lite101->message_field = lite101->message_field | 2;
        light = lite102;
    } else {
        lite101->message_field = lite101->message_field | 4;
        light = lite103;
    }
    light->Message(0xf, 2.0f);
    TSound* sound;
    if (light_active(lite198) == 0 || (sound = soundwave49D, lite198->flasher_on != 0)) sound = soundwave52;
    sound->Play();
    g_table->AddScore(caller->get_scoring(0));
    if (ramp_tgt_lights->Message(0x25, 0.0f) == 3) ramp_tgt_lights->Message(0x10, 2.0f);
}

// a_targ16..18.
void fn_0100da94(int code, TPinballComponent* caller) {
    if (code != 0x3f) return;
    TLight* light;
    if (a_targ16 == caller) {
        lite104->message_field = lite104->message_field | 1;
        light = lite104;
    } else if (a_targ17 == caller) {
        lite104->message_field = lite104->message_field | 2;
        light = lite105;
    } else {
        lite104->message_field = lite104->message_field | 4;
        light = lite106;
    }
    light->Message(0xf, 2.0f);
    g_table->AddScore(caller->get_scoring(0));
    if (lchute_tgt_lights->Message(0x25, 0.0f) == 3) {
        soundwave14->Play();
        v_gate1->Message(0x35, 0.0f);
        lchute_tgt_lights->Message(0x10, 2.0f);
    } else {
        soundwave49D->Play();
    }
}

// a_targ19..21.
void fn_0100db68(int code, TPinballComponent* caller) {
    if (code != 0x3f) return;
    TLight* light;
    if (a_targ19 == caller) {
        lite107->message_field = lite107->message_field | 1;
        light = lite107;
    } else if (a_targ20 == caller) {
        lite107->message_field = lite107->message_field | 2;
        light = lite108;
    } else {
        lite107->message_field = lite107->message_field | 4;
        light = lite109;
    }
    light->Message(0xf, 2.0f);
    g_table->AddScore(caller->get_scoring(0));
    if (bpr_solotgt_lights->Message(0x25, 0.0f) == 3) {
        soundwave14->Play();
        v_gate2->Message(0x35, 0.0f);
        bpr_solotgt_lights->Message(0x10, 2.0f);
    } else {
        soundwave49D->Play();
    }
}

// a_targ22.
void fn_0100dc3c(int code, TPinballComponent* caller) {
    if (code != 0x3f) return;
    if (light_active(lite110) == 0) {
        lite110->Message(0xf, 3.0f);
        info_text_box->Display(get_rc_string(0x5d), 2.0f);
    }
    g_table->AddScore(caller->get_scoring(0));
    fn_0100c397(1);
}

// a_kout3 (black hole).
void fn_0100dcb0(int code, TPinballComponent* caller) {
    if (code != 0x3f) return;
    char buf[64];
    int s = g_table->AddScore(caller->get_scoring(0));
    fmt(buf, get_rc_string(0x50), static_cast<long>(s));
    info_text_box->Display(buf, 2.0f);
    caller->Message(0x37, -1.0f);
}

// a_flag1, a_flag2.
void fn_0100dd33(int code, TPinballComponent* caller) {
    if (code == 0x3e) {
        fn_0100c397(0);
    } else if (code == 0x3f) {
        int idx = light_active(lite20);
        g_table->AddScore(caller->get_scoring(idx));
    }
}

// a_kout1 (gravity well). Kode 0x40: `caller` sebenarnya nilai skor int (0 = tanpa skor),
// dipanggil dari cheat 0100e1b0 dan modul lain.
void fn_0100dd7a(int code, TPinballComponent* caller) {
    char buf[64];
    if (code == 0x3f) {
        int s = g_table->AddScore(caller->get_scoring(0));
        fmt(buf, get_rc_string(0x51), static_cast<long>(s));
        info_text_box->Display(buf, 2.0f);
        lite62->Message(0x14, 0.0f);
        caller->active_flag = 0;
        float duration = soundwave7->Play();
        caller->Message(0x37, duration);
    } else if (code == 0x40) {
        int score = static_cast<int>(reinterpret_cast<intptr_t>(caller));
        if (score == 0) {
            fmt(buf, get_rc_string(0x2d));
        } else {
            fmt(buf, get_rc_string(0x52), static_cast<long>(score));
        }
        info_text_box->Display(buf, 2.0f);
        lite62->Message(4, 0.0f);
        a_kout1->active_flag = 1;
    } else if (code == 0x400) {
        a_kout1->active_flag = 0;
    }
}

// s_onewy1 (skill shot).
void fn_0100dea5(int code, TPinballComponent*) {
    if (code != 0x3f) return;
    lite200->Message(9, 5.0f);
    if (light_active(lite67) != 0) {
        skill_shot_lights->Message(0x22, 0.0f);
        skill_shot_lights->Message(0x14, 0.0f);
        lite67->Message(0x13, 0.0f);
        lite54->Message(7, 5.0f);
        lite25->Message(7, 5.0f);
        fuel_bargraph->Message(0x2d, 11.0f);
        soundwave14->Play();
    }
}

// s_trip1..5 (skill shot).
void fn_0100df6a(int code, TPinballComponent*) {
    if (code == 0x3f && light_active(lite67) != 0) {
        lite68->Message(0x13, 0.0f);
        soundwave14->Play();
    }
}

void fn_0100dfab(int code, TPinballComponent*) {
    if (code == 0x3f && light_active(lite67) != 0) {
        lite69->Message(0x13, 0.0f);
        soundwave14->Play();
    }
}

void fn_0100dfec(int code, TPinballComponent*) {
    if (code == 0x3f && light_active(lite67) != 0) {
        lite131->Message(0x13, 0.0f);
        soundwave14->Play();
    }
}

void fn_0100e02d(int code, TPinballComponent*) {
    if (code == 0x3f && light_active(lite67) != 0) {
        lite132->Message(0x13, 0.0f);
        soundwave14->Play();
    }
}

void fn_0100e06e(int code, TPinballComponent*) {
    if (code == 0x3f && light_active(lite67) != 0) {
        lite133->Message(0x13, 0.0f);
        soundwave14->Play();
    }
}

// lite200.
void fn_0100e0af(int code, TPinballComponent* caller) {
    if (code != 0x3c) return;
    if (caller->message_field == 0) {
        caller->Message(0x10, 5.0f);
        caller->message_field = 1;
    } else {
        caller->message_field = 0;
    }
}

// v_sink7.
void fn_0100e0ed(int code, TPinballComponent* caller) {
    if (code == 0x3f) {
        TSink* sink = static_cast<TSink*>(caller);
        sink->Message(0x38, sink->timer_time);
    }
}

// Naik satu pangkat (cheat "RMAX" / tombol debug).
void fn_0100e111() {
    TLightGroup* group = middle_circle;
    int rank = group->Message(0x25, 0.0f);
    if (rank < 9) {
        group->Message(0x29, 2.0f);
        std::string rank_name = get_rc_string(k_rank_string[rank]);
        char buf[64];
        fmt(buf, get_rc_string(0x53), rank_name.c_str());
        mission_text_box->Display(buf, 8.0f);
        soundwave10->Play();
    }
}

// Deteksi kode cheat dari keydown (kode tombol virtual Windows). State di DAT_01025050.
// Kode: "1MAX" (bola ekstra), "BMAX" (g_flag_44), "GMAX" (gravity well), "RMAX" (naik pangkat),
// "HIDDEN TEST" (DAT_01024ff8). Setiap cheat yang berhasil menandai table->unknown_p3e (+0x3e) = 1.
void fn_0100e1b0(int key) {
    if (lite198->message_field != 0) return;
    int& state = g_mission;
    auto step_if = [&](std::initializer_list<int> states) {
        for (int s : states) {
            if (state == s) {
                state = state + 1;
                return;
            }
        }
        state = 0;
    };
    switch (key) {
    case 0x4d:  // M
        step_if({4, 0x3d, 0x51, 0x65, 0x79});
        return;
    case 0x45:  // E
        step_if({3, 0x18, 0x1c, 0x2c});
        return;
    case 0x47:  // G
        state = state != 0 ? 0 : 0x65;
        return;
    case 0x48:  // H
        state = state != 0 ? 0 : 0x15;
        return;
    case 0x49:  // I
        step_if({1, 0xa, 0x15});
        return;
    case 0x44:  // D
        step_if({0x16, 0x17});
        return;
    case 0x20:  // spasi
        state = state == 0x1a ? 0x1b : 0;
        return;
    case 0x31:  // 1
        state = state != 0 ? 0 : 0x3d;
        return;
    case 0x41:  // A
        step_if({5, 0x3e, 0x52, 0x66, 0x7a});
        return;
    case 0x42:  // B
        state = state != 0 ? 0 : 0x51;
        return;
    case 0x43:  // C
        if (state == 0) {
            state = 1;
        } else if (state == 0xb) {
            state = 0xc;
        } else {
            state = 0;
        }
        return;
    case 0x53:  // S
        step_if({0xc, 0x1d, 0x2d});
        return;
    case 0x4e:  // N
        step_if({2, 9, 0x19});
        return;
    case 0x4f:  // O
        step_if({8, 0x2a});
        return;
    case 0x51:  // Q
        state = state != 0 ? 0 : 0x29;
        return;
    case 0x52:  // R
        if (state == 0) {
            state = 0x79;
        } else if (state == 7) {
            state = 8;
        } else {
            state = 0;
        }
        return;
    case 0x54:  // T
        if (state == 0x1e) {
            g_dat_01024ff8 = 1;
            g_table->unknown_p3e = 1;
            state = 0;
            return;
        }
        step_if({0x1b, 6, 0x2b});
        return;
    case 0x55:  // U
        state = state == 0x29 ? 0x2a : 0;
        return;
    case 0x58:  // X
        if (state == 0x3f) {
            fn_0100c2f3(2.0f);
        } else if (state == 0x53) {
            g_flag_44 = 1;
        } else if (state == 0x67) {
            fn_0100dd7a(0x40, nullptr);
        } else if (state == 0x7b) {
            fn_0100e111();
        } else {
            state = 0;
            return;
        }
        g_table->unknown_p3e = 1;
        state = 0;
        return;
    default:  // termasuk 0xbe (asli: CMP state,0xd tanpa efek, lalu state = 0)
        state = 0;
        return;
    }
}

// Lampu luar (outer_circle) maju `steps` kali; bila penuh naik pangkat. Return 1 bila naik pangkat
// (asli: EBX), 0 bila tidak.
int fn_0100e50b(int steps) {
    int promoted = 0;
    lite16->Message(0x13, 0.0f);
    TLightGroup* outer = outer_circle;
    for (; steps > 0; --steps) outer->Message(0x29, 2.0f);
    int lit = outer->Message(0x25, 0.0f);
    int total = outer->Message(0x26, 0.0f);
    if (lit == total) {
        outer->Message(0x10, 5.0f);
        promoted = 1;
        TLightGroup* group = middle_circle;
        group->Message(0x22, 0.0f);
        int rank = group->Message(0x25, 0.0f);
        if (rank < 9) {
            group->Message(0x29, 5.0f);
            std::string rank_name = get_rc_string(k_rank_string[rank]);
            char buf[64];
            fmt(buf, get_rc_string(0x53), rank_name.c_str());
            mission_text_box->Display(buf, 8.0f);
            soundwave10->Play();
        }
    } else if ((total * 3) / 4 <= lit) {
        middle_circle->Message(0x1b, -1.0f);
    }
    return promoted;
}

// Misi: menunggu peluncuran (Awaiting Deployment).
void fn_0100e657(int code, TPinballComponent* caller) {
    if (code == 0x3f) {
        if (s_onewy4 == caller || s_onewy10 == caller) {
            lite198->message_field = 1;
            fn_01011bb5(0x42, nullptr);
        }
    } else if (code == 0x42) {
        mission_text_box->Clear();
        g_flag_48 = 0;
    } else if (code == 0x43) {
        mission_text_box->Display(get_rc_string(0x32), -1.0f);
    }
}

// Misi: pemilihan misi (target misi / isi bahan bakar / luncurkan ramp untuk menerima).
void fn_0100e6d2(int code, TPinballComponent* caller) {
    char buf[64];
    if (code == 0x2d || code == 0x2f) {
        if (fuel_bargraph != caller) return;
        fn_01011bb5(0x43, caller);
        return;
    }
    if (code != 0x3f) {
        if (code == 0x42) {
            lite198->Message(0x14, 0.0f);
            outer_circle->Message(0x22, 0.0f);
            ramp_tgt_lights->Message(0x14, 0.0f);
            lite56->message_field = 0;
            lite101->message_field = 0;
            l_trek_lights->Message(0x22, 0.0f);
            l_trek_lights->Message(0x14, 0.0f);
            r_trek_lights->Message(0x22, 0.0f);
            r_trek_lights->Message(0x14, 0.0f);
            goal_lights->Message(0x14, 0.0f);
        } else if (code != 0x43) {
            return;
        }
        int active;
        TLight* light;
        if (fuel_bargraph->Message(0x25, 0.0f) == 0) {
            mission_text_box->Display(get_rc_string(0x69), -1.0f);
            if (light_active(lite317) != 0) lite317->Message(0x14, 0.0f);
            if (light_active(lite319) != 0) lite319->Message(0x14, 0.0f);
            active = light_active(lite318);
            light = lite318;
        } else if (light_active(lite56) == 0) {
            mission_text_box->Display(get_rc_string(0x68), -1.0f);
            if (light_active(lite317) != 0) lite317->Message(0x14, 0.0f);
            if (light_active(lite318) != 0) lite318->Message(0x14, 0.0f);
            active = light_active(lite319);
            light = lite319;
        } else {
            std::string name = get_rc_string(k_mission_name_string[lite56->message_field]);
            fmt(buf, get_rc_string(0x6a), name.c_str());
            mission_text_box->Display(buf, -1.0f);
            if (light_active(lite318) != 0) lite318->Message(0x14, 0.0f);
            if (light_active(lite319) != 0) lite319->Message(0x14, 0.0f);
            active = light_active(lite317);
            light = lite317;
        }
        if (active == 0) light->Message(7, 0.0f);
        return;
    }

    // code == 0x3f
    int target = a_targ13 == caller ? 1 : 0;
    if (a_targ14 == caller) target = 2;
    if (a_targ15 == caller) target = 3;
    if (target == 0) {
        if (ramp == caller && light_active(lite56) != 0 && fuel_bargraph->Message(0x25, 0.0f) != 0) {
            lite56->Message(0x14, 0.0f);
            lite198->Message(0x13, 0.0f);
            outer_circle->Message(0x1a, -1.0f);
            if (light_active(lite317) != 0) lite317->Message(0x14, 0.0f);
            if (light_active(lite318) != 0) lite318->Message(0x14, 0.0f);
            if (light_active(lite319) != 0) lite319->Message(0x14, 0.0f);
            int mission = lite56->message_field;
            lite198->message_field = mission;
            fn_01011bb5(0x42, nullptr);
            int s = add_score_plain(k_mission_score[mission]);
            fmt(buf, get_rc_string(0x4d), static_cast<long>(s));
            mission_text_box->Display(buf, 4.0f);
        }
        return;
    }
    if (lite101->message_field == 7) {
        lite101->message_field = 0;
        target = 4;
    }
    int mission;
    switch (middle_circle->Message(0x25, 0.0f)) {
    case 1:
        mission = target == 1 ? 3 : target == 2 ? 4 : target == 3 ? 2 : 5;
        break;
    case 2:
    case 3:
        mission = target == 1 ? 9 : target == 2 ? 0xb : target == 3 ? 10 : 0x10;
        break;
    case 4:
    case 5:
        mission = target == 1 ? 6 : target == 2 ? 8 : target == 3 ? 7 : 0xf;
        break;
    case 6:
    case 7:
        mission = target == 1 ? 0xc : target == 2 ? 0xd : target == 3 ? 0xe : 0x11;
        break;
    case 8:
    case 9:
        mission = target == 1 ? 0xf : target == 2 ? 0x10 : target == 3 ? 0x11 : 0x12;
        break;
    default:
        return;
    }
    lite56->message_field = mission;
    lite56->Message(0xf, 2.0f);
    lite198->Message(4, 0.0f);
    fn_01011bb5(0x43, caller);
}

// Misi: Target Training (attack bumper).
void fn_0100ebf3(int code, TPinballComponent* caller) {
    if (code == 0x3f) {
        if (a_bump1 == caller || a_bump2 == caller || a_bump3 == caller || a_bump4 == caller) {
            int left = lite56->message_field - 1;
            lite56->message_field = left;
            if (left == 0) {
                lite308->Message(0x14, 0.0f);
                lite311->Message(0x14, 0.0f);
                lite198->message_field = 1;
                fn_01011bb5(0x42, nullptr);
                mission_award(0x6c, 500000, 6);
            } else {
                fn_01011bb5(0x43, caller);
            }
        }
        return;
    }
    if (code == 0x42) {
        lite308->Message(7, 0.0f);
        lite311->Message(7, 0.0f);
        lite56->message_field = 8;
    } else if (code != 0x43) {
        return;
    }
    mission_show_count(0x6b);
}

// Misi: Launch Training.
void fn_0100ed83(int code, TPinballComponent* caller) {
    if (code == 0x3f) {
        if (ramp == caller) {
            int left = lite56->message_field - 1;
            lite56->message_field = left;
            if (left == 0) {
                lite317->Message(0x14, 0.0f);
                lite198->message_field = 1;
                fn_01011bb5(0x42, nullptr);
                mission_award(0x6f, 500000, 6);
            } else {
                fn_01011bb5(0x43, caller);
            }
        }
        return;
    }
    if (code == 0x42) {
        lite317->Message(7, 0.0f);
        lite56->message_field = 3;
    } else if (code != 0x43) {
        return;
    }
    mission_show_count(0x6e);
}

// Misi: Re-Entry Training.
void fn_0100eed7(int code, TPinballComponent* caller) {
    if (code == 0x3f) {
        if (a_roll3 == caller || a_roll2 == caller || a_roll1 == caller) {
            int left = lite56->message_field - 1;
            lite56->message_field = left;
            if (left == 0) {
                lite307->Message(0x14, 0.0f);
                lite198->message_field = 1;
                fn_01011bb5(0x42, nullptr);
                mission_award(0x71, 500000, 6);
            } else {
                fn_01011bb5(0x43, caller);
            }
        }
        return;
    }
    if (code == 0x42) {
        lite56->message_field = 3;
        l_trek_lights->Message(0x14, 0.0f);
        l_trek_lights->Message(0x20, 0.2f);
        l_trek_lights->Message(0x1a, 0.2f);
        r_trek_lights->Message(0x14, 0.0f);
        r_trek_lights->Message(0x20, 0.2f);
        r_trek_lights->Message(0x1a, 0.2f);
        lite307->Message(7, 0.0f);
    } else if (code != 0x43) {
        return;
    }
    mission_show_count(0x70);
}

// Misi: Science (drop targets).
void fn_0100f0b3(int code, TPinballComponent* caller) {
    if (code == 0x3f) {
        if (a_targ1 == caller || a_targ2 == caller || a_targ3 == caller || a_targ6 == caller ||
            a_targ5 == caller || a_targ4 == caller || a_targ9 == caller || a_targ8 == caller ||
            a_targ7 == caller) {
            int left = lite56->message_field - 1;
            lite56->message_field = left;
            if (left == 0) {
                lite303->Message(0x14, 0.0f);
                lite309->Message(0x14, 0.0f);
                lite315->Message(0x14, 0.0f);
                lite198->message_field = 1;
                fn_01011bb5(0x42, nullptr);
                mission_award(0x73, 750000, 9);
            } else {
                fn_01011bb5(0x43, caller);
            }
        }
        return;
    }
    if (code == 0x42) {
        lite56->message_field = 9;
        reset_popup(a_targ1);
        reset_popup(a_targ2);
        reset_popup(a_targ3);
        reset_popup(a_targ6);
        reset_popup(a_targ5);
        reset_popup(a_targ4);
        reset_popup(a_targ9);
        reset_popup(a_targ8);
        reset_popup(a_targ7);
        lite303->Message(7, 0.0f);
        lite309->Message(7, 0.0f);
        lite315->Message(7, 0.0f);
    } else if (code != 0x43) {
        return;
    }
    mission_show_count(0x72);
}

// Misi: Stray Comet (isi bank kanan, lalu hyperspace / a_kout2).
void fn_0100f381(int code, TPinballComponent* caller) {
    if (code == 0x3f) {
        if (a_targ19 == caller || a_targ20 == caller || a_targ21 == caller) {
            if (lite107->message_field == 7) {
                lite306->Message(0x14, 0.0f);
                lite304->Message(7, 0.0f);
                lite107->message_field = 0xf;
                fn_01011bb5(0x43, caller);
            }
        } else if (a_kout2 == caller && lite107->message_field == 0xf) {
            lite304->Message(0x14, 0.0f);
            lite198->message_field = 1;
            fn_01011bb5(0x42, nullptr);
            mission_award(0x77, 1000000, 8);
        }
        return;
    }
    if (code == 0x42) {
        bpr_solotgt_lights->Message(0x14, 0.0f);
        lite107->message_field = 0;
        lite306->Message(7, 0.0f);
    } else if (code != 0x43) {
        return;
    }
    int text = lite107->message_field == 0xf ? 0x76 : 0x75;
    mission_text_box->Display(get_rc_string(text), -1.0f);
}

// Misi: Black Hole Threat / Radiation (isi bank kiri, lalu wormhole).
void fn_0100f539(int code, TPinballComponent* caller) {
    if (code == 0x3f) {
        if (a_targ16 == caller || a_targ17 == caller || a_targ18 == caller) {
            if (lite104->message_field == 7) {
                lite104->message_field = 0xf;
                bsink_arrow_lights->Message(7, 0.0f);
                lite313->Message(0x14, 0.0f);
                fn_01011bb5(0x43, caller);
                fn_0100c397(1);
            }
        } else if ((v_sink1 == caller || v_sink2 == caller || v_sink3 == caller) && lite104->message_field == 0xf) {
            lite198->message_field = 1;
            fn_01011bb5(0x42, nullptr);
            mission_award(0x79, 1000000, 8);
        }
        return;
    }
    if (code == 0x42) {
        lchute_tgt_lights->Message(0x14, 0.0f);
        lite104->message_field = 0;
        lite313->Message(7, 0.0f);
    } else if (code != 0x43) {
        return;
    }
    int text = lite104->message_field == 0xf ? 0x78 : 0xb0;
    mission_text_box->Display(get_rc_string(text), -1.0f);
}

// Misi: Black Hole (upgrade launch bumper, lalu a_kout3).
void fn_0100f6f4(int code, TPinballComponent* caller) {
    if (code == 0xb) {
        if (a_bump5 == caller) fn_01011bb5(0x43, caller);
        return;
    }
    if (code == 0x3f) {
        if (a_kout3 == caller && a_bump5->bmp_index != 0) {
            if (light_active(lite316) != 0) lite316->Message(0x14, 0.0f);
            if (light_active(lite314) != 0) lite314->Message(0x14, 0.0f);
            lite198->message_field = 1;
            fn_01011bb5(0x42, nullptr);
            mission_award(0x7c, 1000000, 8);
        }
        return;
    }
    if (code == 0x42) {
        launch_bumpers->Message(0xb, 0.0f);
    } else if (code != 0x43) {
        return;
    }
    int active;
    TLight* light;
    if (a_bump5->bmp_index == 0) {
        mission_text_box->Display(get_rc_string(0x7a), -1.0f);
        if (light_active(lite314) != 0) lite314->Message(0x14, 0.0f);
        active = light_active(lite316);
        light = lite316;
    } else {
        mission_text_box->Display(get_rc_string(0x7b), -1.0f);
        if (light_active(lite316) != 0) lite316->Message(0x14, 0.0f);
        active = light_active(lite314);
        light = lite314;
    }
    if (active == 0) light->Message(7, 0.0f);
}

// Misi: Bug Hunt / Xenomorph (semua target).
void fn_0100f900(int code, TPinballComponent* caller) {
    if (code == 0x3f) {
        if (a_targ1 == caller || a_targ2 == caller || a_targ3 == caller || a_targ6 == caller ||
            a_targ5 == caller || a_targ4 == caller || a_targ9 == caller || a_targ8 == caller ||
            a_targ7 == caller || a_targ10 == caller || a_targ11 == caller || a_targ12 == caller ||
            a_targ13 == caller || a_targ14 == caller || a_targ15 == caller || a_targ16 == caller ||
            a_targ17 == caller || a_targ18 == caller || a_targ19 == caller || a_targ20 == caller ||
            a_targ21 == caller || a_targ22 == caller) {
            int left = lite56->message_field - 1;
            lite56->message_field = left;
            if (left == 0) {
                lite306->Message(0x14, 0.0f);
                lite308->Message(0x14, 0.0f);
                lite310->Message(0x14, 0.0f);
                lite313->Message(0x14, 0.0f);
                lite319->Message(0x14, 0.0f);
                lite198->message_field = 1;
                fn_01011bb5(0x42, nullptr);
                mission_award(0x7e, 750000, 7);
            } else {
                fn_01011bb5(0x43, caller);
            }
        }
        return;
    }
    if (code == 0x42) {
        lite56->message_field = 0xf;
        reset_popup(a_targ1);
        reset_popup(a_targ2);
        reset_popup(a_targ3);
        reset_popup(a_targ6);
        reset_popup(a_targ5);
        reset_popup(a_targ4);
        reset_popup(a_targ9);
        reset_popup(a_targ8);
        reset_popup(a_targ7);
        top_circle_tgt_lights->Message(0x14, 0.0f);
        ramp_tgt_lights->Message(0x14, 0.0f);
        lchute_tgt_lights->Message(0x14, 0.0f);
        bpr_solotgt_lights->Message(0x14, 0.0f);
        lite306->Message(7, 0.0f);
        lite308->Message(7, 0.0f);
        lite310->Message(7, 0.0f);
        lite313->Message(7, 0.0f);
        lite319->Message(7, 0.0f);
    } else if (code != 0x43) {
        return;
    }
    mission_show_count(0x7d);
}

// Misi: Rescue (flag naik, lalu a_kout2).
void fn_0100fcd9(int code, TPinballComponent* caller) {
    if (code != 0x3f) {
        if (code == 0x42) {
            lite20->Message(0x14, 0.0f);
            lite19->Message(0x14, 0.0f);
            lite56->message_field = 1;
        } else if (code != 0x43) {
            return;
        }
        int active;
        TLight* light;
        if (light_active(lite20) == 0) {
            mission_text_box->Display(get_rc_string(0x7f), -1.0f);
            if (light_active(lite304) != 0) lite304->Message(0x14, 0.0f);
            active = light_active(lite303);
            light = lite303;
        } else {
            mission_text_box->Display(get_rc_string(0x80), -1.0f);
            if (light_active(lite303) != 0) lite303->Message(0x14, 0.0f);
            active = light_active(lite304);
            light = lite304;
        }
        if (active == 0) light->Message(7, 0.0f);
        return;
    }
    if (a_targ1 != caller && a_targ2 != caller && a_targ3 != caller) {
        if (a_kout2 != caller || light_active(lite20) == 0) return;
        int left = lite56->message_field - 1;
        lite56->message_field = left;
        if (left == 0) {
            if (light_active(lite303) != 0) lite303->Message(0x14, 0.0f);
            if (light_active(lite304) != 0) lite304->Message(0x14, 0.0f);
            lite198->message_field = 1;
            fn_01011bb5(0x42, nullptr);
            mission_award(0x81, 750000, 7);
            return;
        }
    }
    fn_01011bb5(0x43, caller);
}

// Misi: Alien Menace (upgrade attack bumper a_bump1).
void fn_0100ff35(int code, TPinballComponent* caller) {
    if (code == 0xb) {
        if (a_bump1 == caller && a_bump1->bmp_index != 0) {
            lite307->Message(0x14, 0.0f);
            lite198->message_field = 0x14;
            fn_01011bb5(0x42, nullptr);
        }
        return;
    }
    if (code == 0x42) {
        attack_bumpers->Message(0xb, 0.0f);
        l_trek_lights->Message(0x14, 0.0f);
        l_trek_lights->Message(0x20, 0.2f);
        l_trek_lights->Message(0x1a, 0.2f);
        r_trek_lights->Message(0x14, 0.0f);
        r_trek_lights->Message(0x20, 0.2f);
        r_trek_lights->Message(0x1a, 0.2f);
        lite307->Message(7, 0.0f);
    } else if (code != 0x43) {
        return;
    }
    mission_text_box->Display(get_rc_string(0xaf), -1.0f);
}

// Misi: Alien Menace bagian 2 / Aliens Repelled (attack bumper hits).
void fn_01010051(int code, TPinballComponent* caller) {
    if (code == 0x3f) {
        if (a_bump1 == caller || a_bump2 == caller || a_bump3 == caller || a_bump4 == caller) {
            int left = lite56->message_field - 1;
            lite56->message_field = left;
            if (left == 0) {
                lite308->Message(0x14, 0.0f);
                lite311->Message(0x14, 0.0f);
                lite198->message_field = 1;
                fn_01011bb5(0x42, nullptr);
                mission_award(0x82, 750000, 7);
            } else {
                fn_01011bb5(0x43, caller);
            }
        }
        return;
    }
    if (code == 0x42) {
        lite56->message_field = 8;
        l_trek_lights->Message(0x22, 0.0f);
        l_trek_lights->Message(0x14, 0.0f);
        r_trek_lights->Message(0x22, 0.0f);
        r_trek_lights->Message(0x14, 0.0f);
        lite308->Message(7, 0.0f);
        lite311->Message(7, 0.0f);
    } else if (code != 0x43) {
        return;
    }
    mission_show_count(0x6b);
}

// Misi: Satellite Retrieval (a_bump4 / remote attack bumper).
void fn_0101022d(int code, TPinballComponent* caller) {
    if (code == 0x3f) {
        if (a_bump4 == caller) {
            int left = lite56->message_field - 1;
            lite56->message_field = left;
            if (left == 0) {
                lite308->Message(0x14, 0.0f);
                lite198->message_field = 1;
                fn_01011bb5(0x42, nullptr);
                mission_award(0x85, 1250000, 9);
            } else {
                fn_01011bb5(0x43, caller);
            }
        }
        return;
    }
    if (code == 0x42) {
        lite56->message_field = 3;
        lite308->Message(7, 0.0f);
    } else if (code != 0x43) {
        return;
    }
    mission_show_count(0x84);
}

// Misi: Recon (lane passes).
void fn_01010385(int code, TPinballComponent* caller) {
    if (code == 0x3f) {
        if (a_roll3 == caller || a_roll2 == caller || a_roll1 == caller || a_roll112 == caller ||
            a_roll111 == caller || a_roll110 == caller || a_roll4 == caller || a_roll8 == caller ||
            a_roll6 == caller || a_roll7 == caller || a_roll5 == caller) {
            int left = lite56->message_field - 1;
            lite56->message_field = left;
            if (left == 0) {
                lite301->Message(0x14, 0.0f);
                lite302->Message(0x14, 0.0f);
                lite307->Message(0x14, 0.0f);
                lite316->Message(0x14, 0.0f);
                lite320->Message(0x14, 0.0f);
                lite321->Message(0x14, 0.0f);
                lite198->message_field = 1;
                fn_01011bb5(0x42, nullptr);
                mission_award(0x88, 1250000, 9);
            } else {
                fn_01011bb5(0x43, nullptr);  // quirk asli: caller tidak diteruskan
            }
        }
        return;
    }
    if (code == 0x42) {
        lite56->message_field = 0xf;
        lite301->Message(7, 0.0f);
        lite302->Message(7, 0.0f);
        lite307->Message(7, 0.0f);
        lite316->Message(7, 0.0f);
        lite320->Message(7, 0.0f);
        lite321->Message(7, 0.0f);
    } else if (code != 0x43) {
        return;
    }
    mission_show_count(0x86);
}

// Misi: Doomsday Machine (out lane passes).
void fn_010105e3(int code, TPinballComponent* caller) {
    if (code == 0x3f) {
        if (a_roll4 == caller || a_roll8 == caller) {
            int left = lite56->message_field - 1;
            lite56->message_field = left;
            if (left == 0) {
                lite301->Message(0x14, 0.0f);
                lite320->Message(0x14, 0.0f);
                lite198->message_field = 1;
                fn_01011bb5(0x42, nullptr);
                mission_award(0x8a, 1250000, 9);
            } else {
                fn_01011bb5(0x43, caller);
            }
        }
        return;
    }
    if (code == 0x42) {
        lite56->message_field = 3;
        lite301->Message(7, 0.0f);
        lite320->Message(7, 0.0f);
    } else if (code != 0x43) {
        return;
    }
    mission_show_count(0x89);
}

// Misi: Cosmic Plague (roll flags) — tahap 1.
void fn_01010767(int code, TPinballComponent* caller) {
    if (code == 0x3f) {
        if (a_flag1 == caller || a_flag2 == caller) {
            int left = lite56->message_field - 1;
            lite56->message_field = left;
            if (left == 0) {
                lite305->Message(0x14, 0.0f);
                lite312->Message(0x14, 0.0f);
                lite198->message_field = 0x15;
                fn_01011bb5(0x42, nullptr);
                lite20->Message(0x14, 0.0f);
                lite19->Message(0x14, 0.0f);
            } else {
                fn_01011bb5(0x43, caller);
            }
        }
        return;
    }
    if (code == 0x42) {
        lite56->message_field = 0x4b;
        lite20->Message(0x13, 0.0f);
        lite19->Message(0x13, 0.0f);
        lite305->Message(7, 0.0f);
        lite312->Message(7, 0.0f);
    } else if (code != 0x43) {
        return;
    }
    mission_show_count(0x8b);
}

// Misi: Cosmic Plague tahap 2 (space warp rollover a_roll9).
void fn_010108bc(int code, TPinballComponent* caller) {
    if (code == 0x3f) {
        if (a_roll9 == caller) {
            lite310->Message(0x14, 0.0f);
            lite198->message_field = 1;
            fn_01011bb5(0x42, nullptr);
            mission_award(0x8d, 1750000, 0xb);
        }
        return;
    }
    if (code == 0x42) {
        lite310->Message(7, 0.0f);
    } else if (code != 0x43) {
        return;
    }
    mission_text_box->Display(get_rc_string(0x8c), -1.0f);
}

// Misi: Time Warp / Maelstrom tahap (Hit Yellow Wormhole: v_sink3).
void fn_010109d5(int code, TPinballComponent* caller) {
    if (code == 0x3f) {
        if (v_sink3 == caller) {
            lite198->message_field = 0x16;
            fn_01011bb5(0x42, nullptr);
        }
        return;
    }
    if (code == 0x42) {
        worm_hole_lights->Message(0x14, 0.0f);
        bsink_arrow_lights->Message(0x14, 0.0f);
        bsink_arrow_lights->Message(0x17, 0.0f);
        lite110->Message(0x14, 0.0f);
        lite7->Message(0x13, 0.0f);
        lite3->Message(0xb, 0.0f);
        lite3->Message(0x13, 0.0f);
        lite3->Message(7, 0.0f);
    } else if (code != 0x43) {
        return;
    }
    mission_text_box->Display(get_rc_string(0x8e), -1.0f);
}

}  // namespace control
