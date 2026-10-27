#include "dat_file.h"

#include <cstring>
#include <fstream>
#include <iterator>

namespace {

// DAT_010235d0: ukuran tetap per tipe entri; -1 berarti ukuran dibaca sebagai uint32 dari file.
constexpr int kFieldSize[16] = {2, -1, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, -1, -1};

constexpr size_t kHeaderSize = 0xb7;

struct Reader {
    const std::vector<uint8_t>& data;
    size_t pos = 0;

    bool has(size_t n) const { return pos + n <= data.size(); }
    uint8_t u8() { return data[pos++]; }
    uint16_t u16() {
        uint16_t v;
        std::memcpy(&v, &data[pos], 2);
        pos += 2;
        return v;
    }
    int16_t i16() { return static_cast<int16_t>(u16()); }
    uint32_t u32() {
        uint32_t v;
        std::memcpy(&v, &data[pos], 4);
        pos += 4;
        return v;
    }
};

std::shared_ptr<Bitmap8> parse_bitmap8(const uint8_t* p, size_t size) {
    // Header 14 byte: resolution(u8) width height x y (i16) data_size(u32) flags(u8)
    if (size < 14) return nullptr;
    auto bmp = std::make_shared<Bitmap8>();
    int16_t w, h, x, y;
    uint32_t data_size;
    std::memcpy(&w, p + 1, 2);
    std::memcpy(&h, p + 3, 2);
    std::memcpy(&x, p + 5, 2);
    std::memcpy(&y, p + 7, 2);
    std::memcpy(&data_size, p + 9, 4);
    uint8_t flags = p[13];
    bmp->width = w;
    bmp->height = h;
    bmp->x = x;
    bmp->y = y;
    // flags & 1: baris diratakan ke 4 byte (FUN_010045aa param_4)
    bmp->stride = (flags & 1) && (w % 4) ? w - w % 4 + 4 : w;
    if (h <= 0 || data_size < static_cast<uint32_t>(bmp->stride * h) || 14 + data_size > size)
        return nullptr;
    bmp->pixels.resize(bmp->stride * h);
    const uint8_t* src = p + 14;
    for (int row = 0; row < h; ++row)
        std::memcpy(&bmp->pixels[row * bmp->stride], src + (h - 1 - row) * bmp->stride, bmp->stride);
    return bmp;
}

std::shared_ptr<ZMap> parse_zmap(const uint8_t* p, size_t size) {
    // Header 14 byte: width height stride (i16) + 8 byte lain, lalu stride*height uint16 bottom-up.
    // Beberapa grup (mis. "drain_kicker") berisi z-map kosong berdimensi 0.
    constexpr size_t kZHeader = 14;
    if (size < kZHeader) return nullptr;
    int16_t w, h, stride;
    std::memcpy(&w, p, 2);
    std::memcpy(&h, p + 2, 2);
    std::memcpy(&stride, p + 4, 2);
    auto z = std::make_shared<ZMap>();
    if (w <= 0 || h <= 0) return z;
    if (stride < w || kZHeader + static_cast<size_t>(stride) * h * 2 > size) return nullptr;
    z->width = w;
    z->height = h;
    z->stride = stride;
    z->depth.resize(static_cast<size_t>(stride) * h);
    const uint8_t* src = p + kZHeader;
    for (int row = 0; row < h; ++row)
        std::memcpy(&z->depth[row * stride], src + static_cast<size_t>(h - 1 - row) * stride * 2, stride * 2);
    return z;
}

}  // namespace

std::string DatGroup::name() const {
    for (const auto& e : entries)
        if (e.type == EntryType::GroupName)
            return std::string(reinterpret_cast<const char*>(e.raw.data()), strnlen(reinterpret_cast<const char*>(e.raw.data()), e.raw.size()));
    return {};
}

std::unique_ptr<DatFile> DatFile::load(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return nullptr;
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (data.size() < kHeaderSize || std::strcmp(reinterpret_cast<const char*>(data.data()), "PARTOUT(4.0)RESOURCE") != 0)
        return nullptr;

    auto dat = std::make_unique<DatFile>();
    dat->description_.assign(reinterpret_cast<const char*>(&data[0x47]), strnlen(reinterpret_cast<const char*>(&data[0x47]), 100));

    Reader r{data, 0xaf};
    uint16_t group_count = r.u16();
    r.u32();  // ukuran body
    uint16_t unknown_size = r.u16();
    r.pos = kHeaderSize + unknown_size;

    dat->groups_.reserve(group_count);
    for (int g = 0; g < group_count; ++g) {
        if (!r.has(1)) return nullptr;
        DatGroup group;
        int count = r.u8();
        for (int i = 0; i < count; ++i) {
            if (!r.has(1)) return nullptr;
            auto type = r.u8();
            if (type >= 16) return nullptr;
            int64_t size = kFieldSize[type];
            if (size < 0) {
                if (!r.has(4)) return nullptr;
                size = r.u32();
            }
            if (!r.has(size)) return nullptr;
            DatEntry entry{static_cast<EntryType>(type), {data.begin() + r.pos, data.begin() + r.pos + size}, nullptr, nullptr};
            if (entry.type == EntryType::Bitmap8) {
                entry.bitmap = parse_bitmap8(entry.raw.data(), entry.raw.size());
                if (!entry.bitmap) return nullptr;
            } else if (entry.type == EntryType::Bitmap16) {
                entry.zmap = parse_zmap(entry.raw.data(), entry.raw.size());
                if (!entry.zmap) return nullptr;
            }
            r.pos += size;
            group.entries.push_back(std::move(entry));
        }
        dat->groups_.push_back(std::move(group));
    }
    return dat;
}

int DatFile::find_group(const std::string& name) const {
    for (size_t i = 0; i < groups_.size(); ++i)
        if (groups_[i].name() == name) return static_cast<int>(i);
    return -1;
}

const DatEntry* DatFile::find(int group, EntryType type) const {
    if (group < 0 || group >= static_cast<int>(groups_.size())) return nullptr;
    for (const auto& e : groups_[group].entries)
        if (e.type == type) return &e;
    return nullptr;
}

const DatEntry* DatFile::find(const std::string& group_name, EntryType type) const {
    return find(find_group(group_name), type);
}
