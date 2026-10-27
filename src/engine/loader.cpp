#include "loader.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace loader {
namespace {

const DatFile* g_dat = nullptr;
SoundPlayer g_sound_player = nullptr;
std::string g_data_dir;

struct SoundEntry {
    int group;
    int refcount;
    float duration;
    std::string wav;
};
std::vector<SoundEntry> g_sounds;  // DAT_01027c20, indeks 0 tidak dipakai

// FUN_01006270: entri diasumsikan urut menurut tipe; berhenti bila tipe sudah lewat.
const DatEntry* record(int group, EntryType type, int index = 0) {
    if (!g_dat || group < 0 || group >= static_cast<int>(g_dat->groups().size())) return nullptr;
    int n = 0;
    for (const auto& e : g_dat->groups()[group].entries) {
        if (e.type == type) {
            if (n++ == index) return &e;
        } else if (static_cast<int>(e.type) > static_cast<int>(type)) {
            return nullptr;
        }
    }
    return nullptr;
}

int16_t short_value(int group) {
    const DatEntry* e = record(group, EntryType::ShortValue);
    if (!e || e->raw.size() < 2) return 0;
    int16_t v;
    std::memcpy(&v, e->raw.data(), 2);
    return v;
}

const int16_t* shorts(const DatEntry* e) { return reinterpret_cast<const int16_t*>(e->raw.data()); }
const float* floats(const DatEntry* e) { return reinterpret_cast<const float*>(e->raw.data()); }
int16_t floor_short(float v) { return static_cast<int16_t>(ftol(std::floor(static_cast<double>(v)))); }

// FUN_01009486: grup state ke-n dari sebuah visual (grup dasar bertipe 200, state 201).
int state_group(int group, int state) {
    int count = query_visual_states(group);
    if (static_cast<int16_t>(count) < 1 || static_cast<int16_t>(count) < state) return -1;
    if (short_value(group) != 200) return -1;
    if (state == 0) return group;
    if (short_value(group + state) != 201) return -1;
    return group + state;
}

int query_material(int group, visual_struct& v) {  // FUN_01009249
    if (group < 0 || short_value(group) != 300) return -1;
    const DatEntry* e = record(group, EntryType::FloatArray);
    if (!e) return -1;
    const float* f = floats(e);
    int n = static_cast<int16_t>(e->raw.size() >> 2);
    for (int i = 0; i < n; i += 2) {
        switch (floor_short(f[i])) {
        case 0x12d: v.smoothness = f[i + 1]; break;
        case 0x12e: v.elasticity = f[i + 1]; break;
        case 0x130: v.sound_index4 = get_sound_id(floor_short(f[i + 1])); break;
        default: return -1;
        }
    }
    return 0;
}

int query_kicker(int group, visual_kicker& k) {  // FUN_01009349
    if (group < 0 || short_value(group) != 400) return -1;
    const DatEntry* e = record(group, EntryType::FloatArray);
    if (!e) return -1;
    const float* f = floats(e);
    int n = static_cast<int16_t>(e->raw.size() >> 2);
    for (int i = 0; i < n;) {
        switch (floor_short(f[i])) {
        case 0x191: k.threshold = f[i + 1]; i += 2; break;
        case 0x192: k.boost = f[i + 1]; i += 2; break;
        case 0x193: k.unknown193 = f[i + 1]; i += 2; break;
        case 0x194: k.vector = {f[i + 1], f[i + 2], f[i + 3]}; i += 4; break;
        case 0x195: k.unknown195 = f[i + 1]; i += 2; break;
        case 0x196: k.hard_hit_sound = get_sound_id(floor_short(f[i + 1])); i += 2; break;
        default: return -1;
        }
    }
    return 0;
}

void default_vsi(visual_struct& v) {  // FUN_01008fe0
    std::memset(&v, 0, sizeof(v));
    v.kicker.threshold = 89999998976.0f;  // 0x51a7a358
    v.smoothness = 0.95f;                 // 0x3f733333
    v.elasticity = 0.6f;                  // 0x3f19999a
}

}  // namespace

void init(const DatFile* dat, const std::string& data_dir) {
    g_dat = dat;
    g_data_dir = data_dir;
    g_sounds.assign(1, SoundEntry{0, 0, 0.0f, {}});
    for (int g = 0; g < static_cast<int>(dat->groups().size()) && g_sounds.size() < 0x41; ++g) {
        if (short_value(g) != 0xca) continue;
        std::string wav;
        if (const DatEntry* s = record(g, EntryType::String))
            wav.assign(reinterpret_cast<const char*>(s->raw.data()), strnlen(reinterpret_cast<const char*>(s->raw.data()), s->raw.size()));
        g_sounds.push_back({g, 0, 0.0f, wav});
    }
}

const DatFile* dat() { return g_dat; }

int query_handle(const std::string& name) {
    // FUN_01006369 mencari dari grup terakhir ke depan.
    for (int g = static_cast<int>(g_dat->groups().size()) - 1; g >= 0; --g) {
        const DatEntry* e = record(g, EntryType::GroupName);
        if (e && std::strcmp(reinterpret_cast<const char*>(e->raw.data()), name.c_str()) == 0) return g;
    }
    return -1;
}

int query_visual_states(int group) {
    if (group < 0) return -1;
    const DatEntry* e = record(group, EntryType::ShortArray);
    if (!e || shorts(e)[0] != 100) return 1;
    return shorts(e)[1];
}

int query_visual(int group, int state, visual_struct& v) {
    default_vsi(v);
    if (group < 0) return -1;
    int g = state_group(group, state);
    if (g < 0) return -1;

    const DatEntry* bmp = record(g, EntryType::Bitmap8);
    v.bitmap = bmp ? bmp->bitmap.get() : nullptr;
    const DatEntry* zmap = record(g, EntryType::Bitmap16);
    v.zmap = zmap ? zmap->zmap.get() : nullptr;

    if (const DatEntry* e = record(g, EntryType::ShortArray)) {
        const int16_t* s = shorts(e);
        int n = static_cast<int16_t>(e->raw.size() >> 1);
        for (int i = 0; i < n;) {
            int16_t key = s[i];
            int16_t val = s[i + 1];
            switch (key) {
            case 100:
                if (state != 0) return -1;
                break;
            case 300:
                if (query_material(val, v) != 0) return -1;
                break;
            case 0x130: v.sound_index4 = get_sound_id(val); break;
            case 400:
                if (query_kicker(val, v.kicker) != 0) return -1;
                break;
            case 0x196: v.kicker.hard_hit_sound = get_sound_id(val); break;
            case 0x25a: v.collision_group |= 1 << (static_cast<uint8_t>(val) & 0x1f); break;
            case 0x44c: v.sound_index2 = get_sound_id(val); break;
            case 0x44d: v.sound_index3 = get_sound_id(val); break;
            case 0x5dc: i += 9; continue;
            default: return -1;
            }
            i += 2;
        }
    }
    if (v.collision_group == 0) v.collision_group = 1;

    if (const DatEntry* e = record(g, EntryType::FloatArray)) {
        const float* f = floats(e);
        if (f[0] == 600.0f) {
            v.float_arr_count = static_cast<int16_t>(e->raw.size() >> 2) / 2 - 2;
            // Kode bentuk disimpan +1 (asm 0x01009705: floor(f) - 1.0, Ghidra melewatkan pengurangannya).
            int16_t shape = static_cast<int16_t>(ftol(std::floor(static_cast<double>(f[1])) - 1.0));
            if (shape == 0)
                v.float_arr_count = 1;
            else if (shape == 1)
                v.float_arr_count = 2;
            else if (shape != v.float_arr_count)
                return -1;
            v.float_arr = f + 2;
        }
    }
    return 0;
}

const char* query_name(int group) {
    const DatEntry* e = group < 0 ? nullptr : record(group, EntryType::GroupName);
    return e ? reinterpret_cast<const char*>(e->raw.data()) : nullptr;
}

const float* query_float_attribute(int group, int state, int key) {
    if (group < 0) return nullptr;
    int g = state_group(group, state);
    if (g < 0) return nullptr;
    for (int i = 0;; ++i) {
        const DatEntry* e = record(g, EntryType::FloatArray, i);
        if (!e) return nullptr;
        if (floor_short(floats(e)[0]) == key) return floats(e) + 1;
    }
}

const int16_t* query_iattribute(int group, int key, int& count) {
    count = 0;
    if (group < 0) return nullptr;
    for (int i = 0;; ++i) {
        const DatEntry* e = record(group, EntryType::ShortArray, i);
        if (!e) return nullptr;
        if (shorts(e)[0] == key) {
            count = static_cast<int>(e->raw.size()) / 2 - 1;
            return shorts(e) + 1;
        }
    }
}

int get_sound_id(int group) {
    for (int i = 1; i < static_cast<int>(g_sounds.size()); ++i) {
        if (g_sounds[i].group != group) continue;
        if (g_sounds[i].refcount == 0 && g_sounds[i].duration == 0.0f) {
            // Durasi = ukuran file WAV * 9.090909e-05 detik (~1/11000).
            if (FILE* f = std::fopen((g_data_dir + "/" + g_sounds[i].wav).c_str(), "rb")) {
                std::fseek(f, 0, SEEK_END);
                g_sounds[i].duration = static_cast<float>(std::ftell(f)) * 9.090909e-05f;
                std::fclose(f);
            }
        }
        g_sounds[i].refcount++;
        return i;
    }
    return -1;
}

float play_sound(int sound_id) {
    if (sound_id < 1 || sound_id >= static_cast<int>(g_sounds.size())) return 0.0f;
    if (g_sound_player) g_sound_player(g_sounds[sound_id].wav);
    return g_sounds[sound_id].duration;
}

void set_sound_player(SoundPlayer player) { g_sound_player = player; }

}  // namespace loader
