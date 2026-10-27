// Modul kontrol bagian 1: helper dan handler komponen, blok 0x0100c092-0x0100d5ca Pinball.exe.
// Setiap fungsi diport dari re/decomp/functions/0x<addr>.c dan diperiksa terhadap re/asm/0x<addr>.asm
// (argumen float Message, nilai return x87/EAX yang dihilangkan Ghidra).
#include <cstdio>

#include "../table.h"
#include "control_fns.h"

namespace control {

namespace {

// FUN_010144b7 dengan string resource (pola paling umum: info_text_box, rc id, waktu).
void info_rc(int id, float time) { info_text_box->Display(get_rc_string(id), time); }

// Pola sprintf(buf, rc_string(id), value) -> info_text_box (asli: buffer 64 byte di stack).
void info_rc_fmt(int id, int value, float time) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), get_rc_string(id), static_cast<long>(value));
    info_text_box->Display(buf, time);
}

}  // namespace

// 0100c092: lampu "aktif" bila light_on, toggled_on, atau flasher_on.
// Asli menerima pointer tag (struct {nama, komponen}) dan membaca komponen di tag+4; di port langsung TLight*.
int fn_0100c092(TLight* light) {
    if (light->light_on == 0 && light->toggled_on == 0 && light->flasher_on == 0) return 0;
    return 1;
}

// 0100c0be
void fn_0100c0be(int code) {
    if (code == 0x3f3) {
        g_flag_44 = 0;
        lite77->Message(7, 0.0f);
    }
}

// 0100c0ee: Replay Awarded.
void fn_0100c0ee(float time) {
    lite199->Message(0x13, 0.0f);
    info_rc(0, time);
}

// 0100c12a
void fn_0100c12a() { info_rc(0x10, 2.0f); }

// 0100c14f
void fn_0100c14f() {
    g_table->score_special3_flag = 1;
    lite60->Message(9, 60.0f);
    info_rc(0xf, 2.0f);
}

// 0100c199
void fn_0100c199() {
    g_table->score_special2_flag = 1;
    lite59->Message(9, 60.0f);
    info_rc(4, 2.0f);
}

// 0100c1e3
void fn_0100c1e3() {
    lite20->Message(9, 60.0f);
    lite19->Message(9, 60.0f);
    lite61->Message(9, 60.0f);
    info_rc(0x33, 2.0f);
}

// 0100c24a
void fn_0100c24a() {
    lite58->Message(0x13, 0.0f);
    info_rc(0x34, 2.0f);
}

// 0100c281: kunci bola (ke-3 -> fn_0100c12a dan reset counter).
void fn_0100c281() {
    int locked = g_table->ball_locked_counter;
    if (locked == 2) {
        fn_0100c12a();
        g_table->ball_locked_counter = 0;
        return;
    }
    g_table->ball_locked_counter = locked + 1;
    soundwave44->Play();
    info_rc(1, 2.0f);
    g_table->plunger->Message(0x3f8, 0.0f);
}

// 0100c2f3: extra ball.
void fn_0100c2f3(float time) {
    g_table->extra_balls = g_table->extra_balls + 1;
    soundwave28->Play();
    info_rc(9, time);
}

// 0100c335: AddScore tanpa multiplier/flag spesial; return hasil AddScore (EAX tidak diubah setelah call).
int fn_0100c335(int score) {
    TPinballTable* t = g_table;
    int special3_flag = t->score_special3_flag;
    t->score_special3_flag = 0;
    int special2_flag = t->score_special2_flag;
    t->score_special2_flag = 0;
    int multiplier = t->score_multiplier;
    t->score_multiplier = 0;
    int result = t->AddScore(score);
    t = g_table;
    t->score_special2_flag = special2_flag;
    t->score_multiplier = multiplier;
    t->score_special3_flag = special3_flag;
    return result;
}

// 0100c397: geser panah black hole sink.
void fn_0100c397(int force) {
    int mf = lite198->message_field;
    if (mf == 0x10 || mf == 0x16 || mf == 0x17) return;
    int cur = lite4->message_field;
    if (force == 0 && cur == 0) return;
    int idx = cur + 1;
    if (idx == 4) idx = 1;
    bsink_arrow_lights->Message(0x17, static_cast<float>(idx));
    bsink_arrow_lights->Message(0xb, static_cast<float>(3 - idx));
    if (fn_0100c092(lite4) == 0) {
        worm_hole_lights->Message(0x13, 0.0f);
        bsink_arrow_lights->Message(0x13, 0.0f);
    }
}

// 0100c44b: v_rebo1
void fn_0100c44b(int code, TPinballComponent* caller) {
    if (code == 0x3f) {
        lite84->Message(9, 0.1f);
        g_table->AddScore(caller->get_scoring(0));
    }
}

// 0100c48b: v_rebo2
void fn_0100c48b(int code, TPinballComponent* caller) {
    if (code == 0x3f) {
        lite85->Message(9, 0.1f);
        g_table->AddScore(caller->get_scoring(0));
    }
}

// 0100c4cb: v_rebo3, v_rebo4
void fn_0100c4cb(int code, TPinballComponent* caller) {
    if (code == 0x3f) g_table->AddScore(caller->get_scoring(0));
}

// 0100c4f5: bumper (skor per level bmp_index)
void fn_0100c4f5(int code, TPinballComponent* caller) {
    if (code == 0x3f) g_table->AddScore(caller->get_scoring(static_cast<TBumper*>(caller)->bmp_index));
}

// 0100c520: a_kick1
void fn_0100c520(int code, TPinballComponent* /*caller*/) {
    if (code == 0x3c) v_gate1->Message(0x36, 0.0f);
}

// 0100c546: a_kick2
void fn_0100c546(int code, TPinballComponent* /*caller*/) {
    if (code == 0x3c) v_gate2->Message(0x36, 0.0f);
}

// 0100c56c: v_gate1
void fn_0100c56c(int code, TPinballComponent* /*caller*/) {
    if (code == 0x35) {
        lite30->Message(0xf, 5.0f);
        lite196->Message(7, 5.0f);
    } else if (code == 0x36) {
        lite30->Message(0x14, 0.0f);
        lite196->Message(0x14, 0.0f);
    }
}

// 0100c5d3: v_gate2
void fn_0100c5d3(int code, TPinballComponent* /*caller*/) {
    if (code == 0x35) {
        lite29->Message(0xf, 5.0f);
        lite195->Message(7, 5.0f);
    } else if (code == 0x36) {
        lite29->Message(0x14, 0.0f);
        lite195->Message(0x14, 0.0f);
    }
}

// 0100c63a: s_onewy4 (skill shot)
void fn_0100c63a(int code, TPinballComponent* caller) {
    if (code != 0x3f) return;
    int count = skill_shot_lights->Message(0x25, 0.0f);
    if (count == 0) return;
    soundwave3->Play();
    int score = g_table->AddScore(caller->get_scoring(count - 1));
    info_rc_fmt(0x15, score, 2.0f);
    if (fn_0100c092(lite56) == 0) {
        l_trek_lights->Message(0x22, 0.0f);
        l_trek_lights->Message(0x14, 0.0f);
        r_trek_lights->Message(0x22, 0.0f);
        r_trek_lights->Message(0x14, 0.0f);
    }
    skill_shot_lights->Message(0x2c, 1.0f);
}

// 0100c73e: s_onewy10
void fn_0100c73e(int code, TPinballComponent* /*caller*/) {
    if (code == 0x3f) skill_shot_lights->Message(0x14, 0.0f);
}

// 0100c764: v_bloc1
void fn_0100c764(int code, TPinballComponent* caller) {
    auto* blocker = static_cast<TBlocker*>(caller);
    if (code == 0x34) {
        blocker->message_field = 1;
        blocker->Message(0x34, static_cast<float>(blocker->initial_value));
        lite1->Message(9, static_cast<float>(blocker->initial_value));
    } else if (code == 0x3c) {
        if (blocker->message_field != 1) {
            blocker->message_field = 0;
            blocker->Message(0x33, 0.0f);
            return;
        }
        blocker->message_field = 2;
        blocker->Message(0x3b, static_cast<float>(blocker->unknown_52));
        lite1->Message(7, static_cast<float>(blocker->unknown_52));
    }
}

// 0100c7e9: ramp
void fn_0100c7e9(int code, TPinballComponent* caller) {
    if (code != 0x3f) return;
    int flags = 0;
    if (fn_0100c092(lite54) != 0) {
        flags = 1;
        int score = fn_0100c335(g_table->score_special1);
        info_rc_fmt(10, score, 2.0f);
    }
    if (fn_0100c092(lite55) != 0) flags |= 2;
    if (fn_0100c092(lite56) != 0) flags |= 4;
    TSound* sound;
    if (flags == 0) {
        g_table->AddScore(caller->get_scoring(0));
        sound = soundwave30;
    } else if (flags == 1) {
        sound = soundwave21;
    } else if (flags > 1 && flags <= 3) {
        sound = soundwave23;
    } else {
        sound = soundwave24;
    }
    sound->Play();
}

// 0100c8d7: ramp_hole
void fn_0100c8d7(int code, TPinballComponent* /*caller*/) {
    if (code == 0x3a) lite54->Message(7, 5.0f);
}

// 0100c901: a_roll9
void fn_0100c901(int code, TPinballComponent* /*caller*/) {
    if (code == 0x3f) {
        lite27->Message(0x13, 0.0f);
        lite28->Message(0x13, 0.0f);
    }
}

// 0100c939: a_roll1..3 (rollover attack bumper)
void fn_0100c939(int code, TPinballComponent* caller) {
    if (code != 0x3f) return;
    if (fn_0100c092(lite56) == 0 && l_trek_lights->Message(0x27, 0.0f) != 0) {
        l_trek_lights->Message(0x22, 0.0f);
        l_trek_lights->Message(0x14, 0.0f);
        r_trek_lights->Message(0x22, 0.0f);
        r_trek_lights->Message(0x14, 0.0f);
    }
    TLight* light;
    if (caller == a_roll3)
        light = lite8;
    else if (caller == a_roll2)
        light = lite9;
    else
        light = lite10;
    if (light->flasher_on == 0) {
        if (light->light_on == 0) {
            light->Message(0x13, 0.0f);
            TLightGroup* group = bmpr_inc_lights;
            int lit = bmpr_inc_lights->Message(0x25, 0.0f);
            int total = bmpr_inc_lights->Message(0x26, 0.0f);
            if (lit == total) {
                group->Message(7, 5.0f);
                group->Message(0, 0.0f);
                if (a_bump1->bmp_index < 3) {
                    attack_bumpers->Message(0xc, 0.0f);
                    info_rc(5, 2.0f);
                }
                attack_bumpers->Message(0x30, 60.0f);
            }
        } else {
            light->Message(0x14, 0.0f);
        }
    }
    g_table->AddScore(caller->get_scoring(0));
}

// 0100cabf: attack_bumpers, launch_bumpers (timer grup habis -> turunkan level bumper).
// Quirk: asli mengecek *(int*)(caller+0x4e), padahal TComponentGroup hanya 0x36 byte (calloc). Offset itu
// jatuh di blok heap berikutnya, yaitu buffer objlist grup itu sendiri (dialokasikan tepat sesudahnya di ctor:
// {max, count, item[0..3]}) -> +0xe = 2 byte atas item[1] + 2 byte bawah item[2]. Kedua grup berisi >= 3
// bumper, jadi nilai itu selalu bukan nol (pointer heap). Port: kondisi dianggap selalu benar.
void fn_0100cabf(int code, TPinballComponent* caller) {
    if (code == 0x3d) {
        caller->Message(0x30, 60.0f);
        caller->Message(0xd, 0.0f);
    }
}

// 0100cafe: a_roll110..112 (rollover launch bumper)
void fn_0100cafe(int code, TPinballComponent* caller) {
    if (code != 0x3f) return;
    TLight* light;
    if (caller == a_roll112)
        light = lite171;
    else if (caller == a_roll111)
        light = lite170;
    else
        light = lite169;
    if (light->flasher_on == 0) {
        if (light->light_on == 0) {
            light->Message(0x13, 0.0f);
            TLightGroup* group = ramp_bmpr_inc_lights;
            int lit = ramp_bmpr_inc_lights->Message(0x25, 0.0f);
            int total = ramp_bmpr_inc_lights->Message(0x26, 0.0f);
            if (lit == total) {
                group->Message(7, 5.0f);
                group->Message(0, 0.0f);
                if (a_bump5->bmp_index < 3) {
                    launch_bumpers->Message(0xc, 0.0f);
                    info_rc(6, 2.0f);
                }
                launch_bumpers->Message(0x30, 60.0f);
            }
        } else {
            light->Message(0x14, 0.0f);
        }
    }
    g_table->AddScore(caller->get_scoring(0));
}

// 0100cc20: a_roll4, a_roll8 (outlane/extra ball)
void fn_0100cc20(int code, TPinballComponent* caller) {
    if (code != 0x3f) return;
    if (fn_0100c092(lite17) == 0 && fn_0100c092(lite18) == 0) {
        soundwave26->Play();
    } else {
        fn_0100c2f3(2.0f);
        lite17->Message(0x14, 0.0f);
        lite18->Message(0x14, 0.0f);
    }
    if (caller == a_roll4) {
        if (fn_0100c092(lite30) != 0) {
            lite30->Message(4, 0.0f);
            lite196->Message(4, 0.0f);
        }
    } else {
        if (fn_0100c092(lite29) != 0) {
            lite29->Message(4, 0.0f);
            lite195->Message(4, 0.0f);
        }
    }
    g_table->AddScore(caller->get_scoring(0));
}

// 0100cd12: lite17
void fn_0100cd12(int code, TPinballComponent* /*caller*/) {
    if (code == 0x13) {
        lite17->Message(9, 55.0f);
        lite18->Message(9, 55.0f);
        g_flag_4c = 1;
    } else if (code == 0x3c && g_flag_4c != 0) {
        lite17->Message(7, 5.0f);
        lite18->Message(7, 5.0f);
        g_flag_4c = 0;
    }
}

// 0100cda0: a_roll6, a_roll7
void fn_0100cda0(int code, TPinballComponent* caller) {
    if (code != 0x3f) return;
    int index = 0;
    if (caller == a_roll6) {
        if (fn_0100c092(lite27) != 0) {
            lite59->Message(0x14, 0.0f);
            lite27->Message(0x14, 0.0f);
            index = 1;
        }
    } else {
        if (caller != a_roll7) return;
        if (fn_0100c092(lite28) != 0) {
            lite59->Message(0x14, 0.0f);
            lite28->Message(0x14, 0.0f);
            index = 1;
        }
    }
    g_table->AddScore(caller->get_scoring(index));
}

// 0100ce40: a_roll5 (bonus)
void fn_0100ce40(int code, TPinballComponent* caller) {
    if (code != 0x3f) return;
    if (fn_0100c092(lite16) == 0) {
        g_table->AddScore(caller->get_scoring(0));
        soundwave25->Play();
        info_rc(0x2c, 2.0f);
    } else {
        int score = fn_0100c335(g_table->score_special2);
        info_rc_fmt(3, score, 2.0f);
        lite16->Message(0x14, 0.0f);
        soundwave50->Play();
    }
    fuel_bargraph->Message(0x2d, 11.0f);
}

namespace {

// Pola bersama 0100cf36..0100d1f8: rollover bahan bakar.
void fuel_rollover(TPinballComponent* caller, int threshold, float fuel, TLight* light) {
    if (fuel_bargraph->Message(0x25, 0.0f) < threshold) {
        fuel_bargraph->Message(0x2d, fuel);
        info_rc(0x2c, 2.0f);
    } else {
        light->Message(8, 0.05f);
    }
    g_table->AddScore(caller->get_scoring(0));
}

}  // namespace

// 0100cf36: a_roll179
void fn_0100cf36(int code, TPinballComponent* caller) {
    if (code == 0x3f) fuel_rollover(caller, 2, 1.0f, literoll179);
}

// 0100cfc0: a_roll180
void fn_0100cfc0(int code, TPinballComponent* caller) {
    if (code == 0x3f) fuel_rollover(caller, 4, 3.0f, literoll180);
}

// 0100d04e: a_roll181
void fn_0100d04e(int code, TPinballComponent* caller) {
    if (code == 0x3f) fuel_rollover(caller, 6, 5.0f, literoll181);
}

// 0100d0dc: a_roll182
void fn_0100d0dc(int code, TPinballComponent* caller) {
    if (code == 0x3f) fuel_rollover(caller, 8, 7.0f, literoll182);
}

// 0100d16a: a_roll183
void fn_0100d16a(int code, TPinballComponent* caller) {
    if (code == 0x3f) fuel_rollover(caller, 10, 9.0f, literoll183);
}

// 0100d1f8: a_roll184
void fn_0100d1f8(int code, TPinballComponent* caller) {
    if (code == 0x3f) fuel_rollover(caller, 12, 11.0f, literoll184);
}

// 0100d286: hyperspace_lights
void fn_0100d286(int code, TPinballComponent* caller) {
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
    caller->Message(0x2b, 60.0f);
}

// 0100d303: v_sink1..3 (black hole / wormhole)
void fn_0100d303(int code, TPinballComponent* caller) {
    if (code != 0x3f) return;
    // Tabel tag asli: 0x01023a4c {lite5, lite6, lite7}, 0x01023a58 {lite4, lite2, lite3},
    // 0x01023a40 {v_sink1, v_sink2, v_sink3}.
    TLight* const arrow_lights[3] = {lite5, lite6, lite7};
    TLight* const sink_lights[3] = {lite4, lite2, lite3};
    TSink* const sinks[3] = {v_sink1, v_sink2, v_sink3};
    auto* sink = static_cast<TSink*>(caller);

    int sink_no = 0;  // EBX: 1..3
    if (v_sink1 != caller) sink_no = (v_sink2 != caller) + 1;
    int target = lite4->message_field;
    sink_no += 1;
    int idx;
    if (target == 0) {
        g_table->AddScore(caller->get_scoring(0));
        idx = sink_no - 1;
        arrow_lights[idx]->Message(0x10, sink->timer_time);
    } else {
        lite4->message_field = 0;
        worm_hole_lights->Message(0x14, 0.0f);
        bsink_arrow_lights->Message(0x14, 0.0f);
        lite110->Message(0x14, 0.0f);
        if (target == sink_no) {
            if (g_table->multiball_flag != 0) {
                fn_0100c281();
                g_table->AddScore(10000);
                return;
            }
            info_rc(0x31, 2.0f);
            fn_0100c0ee(4.0f);
            g_table->AddScore(caller->get_scoring(1));
            idx = sink_no - 1;
            arrow_lights[idx]->Message(0x10, sink->timer_time);
            sink_lights[idx]->Message(0xb, static_cast<float>(2 - idx));
            sink_lights[idx]->Message(0x10, sink->timer_time);
            sinks[idx]->Message(0x38, sink->timer_time);
            return;
        }
        g_table->AddScore(caller->get_scoring(2));
        idx = target - 1;
        arrow_lights[idx]->Message(0x10, sink->timer_time);
    }
    sink_lights[idx]->Message(0xb, static_cast<float>(2 - idx));
    sink_lights[idx]->Message(0x10, sink->timer_time);
    sinks[idx]->Message(0x38, sink->timer_time);
    info_rc(0x31, 2.0f);
}

// 0100d53a: a_flip1
void fn_0100d53a(int code, TPinballComponent* /*caller*/) {
    if (code == 1) {
        bmpr_inc_lights->Message(0x18, 0.0f);
        ramp_bmpr_inc_lights->Message(0x18, 0.0f);
    }
}

// 0100d572: a_flip2
void fn_0100d572(int code, TPinballComponent* /*caller*/) {
    if (code == 1) {
        bmpr_inc_lights->Message(0x19, 0.0f);
        ramp_bmpr_inc_lights->Message(0x19, 0.0f);
    }
}

// 0100d5aa: lite60
void fn_0100d5aa(int code, TPinballComponent* /*caller*/) {
    if (code == 0x3c) g_table->score_special3_flag = 0;
}

// 0100d5ca: lite59
void fn_0100d5ca(int code, TPinballComponent* /*caller*/) {
    if (code == 0x3c) g_table->score_special2_flag = 0;
}

}  // namespace control
