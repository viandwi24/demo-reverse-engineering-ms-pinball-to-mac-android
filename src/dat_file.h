// Loader PINBALL.DAT (container "PARTOUT(4.0)RESOURCE").
// Port dari FUN_010064e2 (load), FUN_010063db (cari entri per nama grup) di Pinball.exe.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

enum class EntryType : uint8_t {
    ShortValue = 0,
    Bitmap8 = 1,
    Unknown2 = 2,
    GroupName = 3,
    Palette = 5,
    String = 9,
    ShortArray = 10,
    FloatArray = 11,
    Bitmap16 = 12,  // z-map, dipakai untuk oklusi bola
};

// Bitmap 8-bit berpalet. Disimpan top-down (baris 0 = atas) agar mudah dipakai;
// file aslinya bottom-up seperti DIB Windows.
struct Bitmap8 {
    int width = 0;
    int height = 0;
    int stride = 0;  // width dibulatkan ke kelipatan 4 (FUN_010045aa)
    int x = 0;       // posisi di koordinat meja (lihat render: x - 137, y - 2)
    int y = 0;
    std::vector<uint8_t> pixels;

    uint8_t at(int px, int py) const { return pixels[py * stride + px]; }
};

struct ZMap {
    int width = 0;
    int height = 0;
    int stride = 0;
    std::vector<uint16_t> depth;  // top-down
};

struct DatEntry {
    EntryType type;
    std::vector<uint8_t> raw;
    std::shared_ptr<Bitmap8> bitmap;  // untuk Bitmap8
    std::shared_ptr<ZMap> zmap;       // untuk Bitmap16
};

struct DatGroup {
    std::vector<DatEntry> entries;
    std::string name() const;
};

class DatFile {
public:
    // Mengembalikan nullptr jika file tidak ada atau signature salah,
    // sama seperti aslinya yang mengembalikan NULL.
    static std::unique_ptr<DatFile> load(const std::string& path);

    const std::string& description() const { return description_; }
    const std::vector<DatGroup>& groups() const { return groups_; }

    int find_group(const std::string& name) const;
    const DatEntry* find(int group, EntryType type) const;
    const DatEntry* find(const std::string& group_name, EntryType type) const;

private:
    std::string description_;
    std::vector<DatGroup> groups_;
};
