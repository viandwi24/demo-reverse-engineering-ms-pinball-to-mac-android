#include "control.h"

#include <cmath>
#include <cstring>

#include "../../exe_resources.h"
#include "../table.h"
#include "control_fns.h"

namespace control {

TPinballTable* g_table = nullptr;
int g_flag_44 = 0;
int g_flag_48 = 0;
int g_flag_4c = 0;
int g_mission = 0;

namespace {

// Tabel skor per komponen (dari data exe).
const int kScores0[] = {500, 1000, 1500, 2000};
const int kScores1[] = {500, 1000, 1500, 2000};
const int kScores2[] = {500, 1000, 1500, 2000};
const int kScores3[] = {500, 1000, 1500, 2000};
const int kScores4[] = {2000};
const int kScores5[] = {2000};
const int kScores6[] = {2000};
const int kScores8[] = {1500, 2500, 3500, 4500};
const int kScores9[] = {1500, 2500, 3500, 4500};
const int kScores10[] = {1500, 2500, 3500, 4500};
const int kScores11[] = {500};
const int kScores12[] = {500};
const int kScores13[] = {500};
const int kScores15[] = {500};
const int kScores16[] = {500};
const int kScores17[] = {500};
const int kScores18[] = {500};
const int kScores23[] = {15000, 30000, 75000, 30000, 15000, 7500};
const int kScores26[] = {5000};
const int kScores28[] = {20000};
const int kScores29[] = {20000};
const int kScores31[] = {5000, 25000};
const int kScores32[] = {5000, 25000};
const int kScores33[] = {10000};
const int kScores34[] = {500};
const int kScores35[] = {500};
const int kScores36[] = {500};
const int kScores37[] = {500};
const int kScores38[] = {500};
const int kScores39[] = {500};
const int kScores40[] = {500, 2500};
const int kScores41[] = {10000, 0, 20000, 50000, 150000};
const int kScores43[] = {500, 2500};
const int kScores44[] = {2500, 5000, 7500};
const int kScores45[] = {2500, 5000, 7500};
const int kScores46[] = {2500, 5000, 7500};
const int kScores50[] = {500, 5000};
const int kScores51[] = {500, 5000};
const int kScores52[] = {500, 5000};
const int kScores55[] = {1500, 10000, 50000};
const int kScores56[] = {1500, 10000, 50000};
const int kScores57[] = {1500, 10000, 50000};
const int kScores59[] = {500, 1500};
const int kScores60[] = {500, 1500};
const int kScores61[] = {500, 1500};
const int kScores63[] = {750};
const int kScores64[] = {750};
const int kScores65[] = {750};
const int kScores66[] = {1000};
const int kScores67[] = {1000};
const int kScores68[] = {1000};
const int kScores69[] = {750};
const int kScores70[] = {750};
const int kScores71[] = {750};
const int kScores72[] = {750};
const int kScores73[] = {750};
const int kScores74[] = {750};
const int kScores75[] = {750};
const int kScores76[] = {10000};
const int kScores77[] = {20000};
const int kScores78[] = {50000};

struct ControlEntry {
    const char* name;
    component_control control;
};

// 0x01023ef0: {tag, fungsi, jumlah skor, skor}
ControlEntry g_entries[] = {
    {"a_bump1", {fn_0100c4f5, 4, kScores0}},
    {"a_bump2", {fn_0100c4f5, 4, kScores1}},
    {"a_bump3", {fn_0100c4f5, 4, kScores2}},
    {"a_bump4", {fn_0100c4f5, 4, kScores3}},
    {"a_roll3", {fn_0100c939, 1, kScores4}},
    {"a_roll2", {fn_0100c939, 1, kScores5}},
    {"a_roll1", {fn_0100c939, 1, kScores6}},
    {"attack_bumpers", {fn_0100cabf, 0, nullptr}},
    {"a_bump5", {fn_0100c4f5, 4, kScores8}},
    {"a_bump6", {fn_0100c4f5, 4, kScores9}},
    {"a_bump7", {fn_0100c4f5, 4, kScores10}},
    {"a_roll112", {fn_0100cafe, 1, kScores11}},
    {"a_roll111", {fn_0100cafe, 1, kScores12}},
    {"a_roll110", {fn_0100cafe, 1, kScores13}},
    {"launch_bumpers", {fn_0100cabf, 0, nullptr}},
    {"v_rebo1", {fn_0100c44b, 1, kScores15}},
    {"v_rebo2", {fn_0100c48b, 1, kScores16}},
    {"v_rebo3", {fn_0100c4cb, 1, kScores17}},
    {"v_rebo4", {fn_0100c4cb, 1, kScores18}},
    {"a_kick1", {fn_0100c520, 0, nullptr}},
    {"a_kick2", {fn_0100c546, 0, nullptr}},
    {"v_gate1", {fn_0100c56c, 0, nullptr}},
    {"v_gate2", {fn_0100c5d3, 0, nullptr}},
    {"s_onewy4", {fn_0100c63a, 6, kScores23}},
    {"s_onewy10", {fn_0100c73e, 0, nullptr}},
    {"v_bloc1", {fn_0100c764, 0, nullptr}},
    {"ramp", {fn_0100c7e9, 1, kScores26}},
    {"ramp_hole", {fn_0100c8d7, 0, nullptr}},
    {"a_roll4", {fn_0100cc20, 1, kScores28}},
    {"a_roll8", {fn_0100cc20, 1, kScores29}},
    {"lite17", {fn_0100cd12, 0, nullptr}},
    {"a_roll6", {fn_0100cda0, 2, kScores31}},
    {"a_roll7", {fn_0100cda0, 2, kScores32}},
    {"a_roll5", {fn_0100ce40, 1, kScores33}},
    {"a_roll179", {fn_0100cf36, 1, kScores34}},
    {"a_roll180", {fn_0100cfc0, 1, kScores35}},
    {"a_roll181", {fn_0100d04e, 1, kScores36}},
    {"a_roll182", {fn_0100d0dc, 1, kScores37}},
    {"a_roll183", {fn_0100d16a, 1, kScores38}},
    {"a_roll184", {fn_0100d1f8, 1, kScores39}},
    {"a_flag1", {fn_0100dd33, 2, kScores40}},
    {"a_kout2", {fn_01011f1a, 5, kScores41}},
    {"hyperspace_lights", {fn_0100d286, 0, nullptr}},
    {"a_flag2", {fn_0100dd33, 2, kScores43}},
    {"v_sink1", {fn_0100d303, 3, kScores44}},
    {"v_sink2", {fn_0100d303, 3, kScores45}},
    {"v_sink3", {fn_0100d303, 3, kScores46}},
    {"a_flip1", {fn_0100d53a, 0, nullptr}},
    {"a_flip2", {fn_0100d572, 0, nullptr}},
    {"plunger", {fn_01012285, 0, nullptr}},
    {"a_targ1", {fn_0100d5ea, 2, kScores50}},
    {"a_targ2", {fn_0100d5ea, 2, kScores51}},
    {"a_targ3", {fn_0100d5ea, 2, kScores52}},
    {"lite60", {fn_0100d5aa, 0, nullptr}},
    {"lite59", {fn_0100d5ca, 0, nullptr}},
    {"a_targ6", {fn_0101240d, 3, kScores55}},
    {"a_targ5", {fn_0101240d, 3, kScores56}},
    {"a_targ4", {fn_0101240d, 3, kScores57}},
    {"bumper_target_lights", {fn_0100d73e, 0, nullptr}},
    {"a_targ9", {fn_01012570, 2, kScores59}},
    {"a_targ8", {fn_01012570, 2, kScores60}},
    {"a_targ7", {fn_01012570, 2, kScores61}},
    {"top_target_lights", {fn_0100d7bb, 0, nullptr}},
    {"a_targ10", {fn_0100d8de, 1, kScores63}},
    {"a_targ11", {fn_0100d8de, 1, kScores64}},
    {"a_targ12", {fn_0100d8de, 1, kScores65}},
    {"a_targ13", {fn_0100d9c2, 1, kScores66}},
    {"a_targ14", {fn_0100d9c2, 1, kScores67}},
    {"a_targ15", {fn_0100d9c2, 1, kScores68}},
    {"a_targ16", {fn_0100da94, 1, kScores69}},
    {"a_targ17", {fn_0100da94, 1, kScores70}},
    {"a_targ18", {fn_0100da94, 1, kScores71}},
    {"a_targ19", {fn_0100db68, 1, kScores72}},
    {"a_targ20", {fn_0100db68, 1, kScores73}},
    {"a_targ21", {fn_0100db68, 1, kScores74}},
    {"a_targ22", {fn_0100dc3c, 1, kScores75}},
    {"a_roll9", {fn_0100c901, 1, kScores76}},
    {"a_kout3", {fn_0100dcb0, 1, kScores77}},
    {"a_kout1", {fn_0100dd7a, 1, kScores78}},
    {"drain", {fn_010126ba, 0, nullptr}},
    {"s_onewy1", {fn_0100dea5, 0, nullptr}},
    {"s_trip1", {fn_0100df6a, 0, nullptr}},
    {"s_trip2", {fn_0100dfab, 0, nullptr}},
    {"s_trip3", {fn_0100dfec, 0, nullptr}},
    {"s_trip4", {fn_0100e02d, 0, nullptr}},
    {"s_trip5", {fn_0100e06e, 0, nullptr}},
    {"lite200", {fn_0100e0af, 0, nullptr}},
    {"v_sink7", {fn_0100e0ed, 0, nullptr}},
};

}  // namespace

void make_links(TPinballTable* table) {
    g_table = table;
    for (ControlEntry& e : g_entries) {
        TPinballComponent* comp = nullptr;
        for (TPinballComponent* c : table->component_list)
            if (c->group_name && std::strcmp(c->group_name, e.name) == 0) {
                comp = c;
                break;
            }
        if (!comp) continue;
        comp->control = &e.control;
        for (int i = 0; i < e.control.score_count; ++i) comp->put_scoring(i, e.control.scores[i]);
    }
    resolve_tags(table);
}

void handler(int code, TPinballComponent* caller) {
    auto* entry = static_cast<component_control*>(caller->control);
    if (entry) {
        if (code == 0x3fb)
            for (int i = 0; i < entry->score_count; ++i) caller->put_scoring(i, entry->scores[i]);
        entry->fn(code, caller);
    }
    fn_01011bb5(code, caller);
}

const char* get_rc_string(int id) { return exe_resources::get_string(id); }

void table_add_score(int points) { g_table->AddScore(points); }
int table_add_score_ret(int points) { return g_table->AddScore(points); }
void table_set_ball_count(int count) { g_table->ChangeBallCount(count); }
int pb_mode_change(int mode) { return pb::mode_change(mode); }
int pb_chk_highscore() { return pb::chk_highscore(); }

}  // namespace control
