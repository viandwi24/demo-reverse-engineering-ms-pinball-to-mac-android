#include "exe_resources.h"

#include <dirent.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <map>

namespace exe_resources {
namespace {

std::map<int, std::string> g_strings;
std::vector<uint8_t> g_font;

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string find_exe(const std::string& data_dir) {
    if (DIR* d = opendir(data_dir.c_str())) {
        while (dirent* e = readdir(d)) {
            if (lower(e->d_name) == "pinball.exe") {
                std::string path = data_dir + "/" + e->d_name;
                closedir(d);
                return path;
            }
        }
        closedir(d);
    }
    return {};
}

// Pembaca PE32 minimal: hanya yang dibutuhkan untuk menelusuri direktori .rsrc.
struct Pe {
    std::vector<uint8_t> d;
    uint32_t rsrc_off = 0;  // offset file awal direktori resource
    uint32_t rsrc_rva = 0;

    uint16_t u16(size_t o) const { return o + 2 <= d.size() ? static_cast<uint16_t>(d[o] | d[o + 1] << 8) : 0; }
    uint32_t u32(size_t o) const { return o + 4 <= d.size() ? u16(o) | static_cast<uint32_t>(u16(o + 2)) << 16 : 0; }

    bool open() {
        uint32_t pe = u32(0x3c);
        if (u32(pe) != 0x4550) return false;  // "PE\0\0"
        int sections = u16(pe + 6);
        uint32_t opt = pe + 24;
        if (u16(opt) != 0x10b) return false;  // PE32
        rsrc_rva = u32(opt + 96 + 8 * 2);     // data directory #2: resource
        uint32_t sec = opt + u16(pe + 20);
        for (int i = 0; i < sections; ++i, sec += 40) {
            uint32_t va = u32(sec + 12), vsize = u32(sec + 8), raw = u32(sec + 20);
            if (rsrc_rva >= va && rsrc_rva < va + vsize) {
                rsrc_off = raw + (rsrc_rva - va);
                return rsrc_off < d.size();
            }
        }
        return false;
    }

    uint32_t rva_to_off(uint32_t rva) const { return rsrc_off + (rva - rsrc_rva); }

    // Panggil fn(id_atau_-1, nama, offset_entri_data) untuk setiap daun di bawah direktori dir.
    template <class Fn>
    void walk(uint32_t dir, int depth, int type, int id, const std::string& name, Fn&& fn) const {
        int count = u16(rsrc_off + dir + 12) + u16(rsrc_off + dir + 14);
        for (int i = 0; i < count; ++i) {
            uint32_t entry = rsrc_off + dir + 16 + 8 * i;
            uint32_t key = u32(entry), target = u32(entry + 4);
            int key_id = (key & 0x80000000u) ? -1 : static_cast<int>(key);
            std::string key_name;
            if (key & 0x80000000u) {  // nama UTF-16 dengan awalan panjang
                uint32_t s = rsrc_off + (key & 0x7fffffffu);
                for (int k = 0, n = u16(s); k < n; ++k) key_name += static_cast<char>(u16(s + 2 + 2 * k));
            }
            int t = depth == 0 ? key_id : type;
            int nid = depth == 1 ? key_id : id;
            std::string nm = depth == 1 ? key_name : name;
            if (target & 0x80000000u)
                walk(target & 0x7fffffffu, depth + 1, t, nid, nm, fn);
            else
                fn(t, nid, nm, rsrc_off + target);
        }
    }
};

}  // namespace

bool load(const std::string& data_dir) {
    std::string path = find_exe(data_dir);
    FILE* f = path.empty() ? nullptr : std::fopen(path.c_str(), "rb");
    if (!f) return false;
    Pe pe;
    std::fseek(f, 0, SEEK_END);
    pe.d.resize(static_cast<size_t>(std::ftell(f)));
    std::fseek(f, 0, SEEK_SET);
    size_t got = std::fread(pe.d.data(), 1, pe.d.size(), f);
    std::fclose(f);
    if (got != pe.d.size() || !pe.open()) return false;

    g_strings.clear();
    g_font.clear();
    pe.walk(0, 0, 0, 0, "", [&](int type, int id, const std::string& name, uint32_t data_entry) {
        uint32_t off = pe.rva_to_off(pe.u32(data_entry));
        uint32_t size = pe.u32(data_entry + 4);
        if (off + size > pe.d.size()) return;
        if (type == 6 && id > 0) {  // RT_STRING: blok 16 string, id = (blok - 1) * 16 + indeks
            uint32_t p = off;
            for (int k = 0; k < 16 && p + 2 <= off + size; ++k) {
                int len = pe.u16(p);
                std::string s;
                for (int c = 0; c < len; ++c) s += static_cast<char>(pe.u16(p + 2 + 2 * c) & 0xff);
                if (len) g_strings.emplace((id - 1) * 16 + k, s);
                p += 2 + 2 * len;
            }
        } else if (type == 10 && name == "PBMSG_FT" && g_font.empty()) {  // RT_RCDATA
            g_font.assign(pe.d.begin() + off, pe.d.begin() + off + size);
        }
    });
    return !g_strings.empty() && !g_font.empty();
}

const char* get_string(int id) {
    auto it = g_strings.find(id);
    return it == g_strings.end() ? "" : it->second.c_str();
}

const std::vector<uint8_t>& msg_font() { return g_font; }

}  // namespace exe_resources
