// Renderer sprite dengan z-buffer. Port dari blok 0x01012f2a-0x01013a8f Pinball.exe.
// Semua bitmap disimpan top-down; buffer asli bottom-up (DIB), tapi semantik pikselnya sama.
#pragma once

#include <cstdint>
#include <vector>

#include "../dat_file.h"

struct rectangle_type {
    int x, y, width, height;
};

enum class VisualType : uint8_t { Static = 0, Dynamic = 1, Ball = 2 };

struct render_sprite {
    rectangle_type rect;          // [0..3] posisi + ukuran bitmap
    const Bitmap8* bmp = nullptr; // [4]
    const ZMap* zmap = nullptr;   // [5]
    bool destroy_pending = false; // +0x18
    VisualType type;              // +0x19
    uint16_t depth = 0;           // +0x1a (bola)
    rectangle_type prev_dirty;    // +0x1c
    int zmap_offset_x = 0;        // [0xb]
    int zmap_offset_y = 0;        // [0xc]
    rectangle_type dirty{};       // +0x34
    std::vector<render_sprite*> overlaps;  // +0x44/+0x48
    rectangle_type bounding;      // +0x4c
};

namespace render {

// FUN_01013705: buffer layar w x h, rentang kedalaman kamera.
void init(float z_min, float z_scaler, int width, int height);
void uninit();

render_sprite* create_sprite(VisualType type, const Bitmap8* bmp, const ZMap* zmap, int x, int y,
                             const rectangle_type* bounding);  // 0101385c
void set_table_zmap(const ZMap* zmap, int offset_x, int offset_y);  // 0101394e
void sprite_set(render_sprite* s, const Bitmap8* bmp, const ZMap* zmap, int x, int y);  // 01013974
void sprite_set_bitmap(render_sprite* s, const Bitmap8* bmp);                          // 010139b6
void ball_set(render_sprite* s, const Bitmap8* bmp, float depth, int x, int y);        // 010139ec
void build_occlude_list();  // 01012f2a
// FUN_01015424: panel skor digambar langsung ke vscreen sekali saat init.
void paint_background(const Bitmap8& bmp, int x, int y);
void update();              // 010133c7 (+ 010130f3 bola, 0101320d restore)

// Hasil akhir frame (sudah termasuk bola) untuk ditampilkan.
const Bitmap8& frame();
const Bitmap8& vscreen();
Bitmap8& vscreen_mut();  // gambar langsung ke layar (skor, kotak teks)
// FUN_010047fa: isi kotak dengan warna indeks.
void fill_rect(int x, int y, int w, int h, uint8_t color);
// FUN_01004870 ke vscreen.
void blit(const Bitmap8& src, int sx, int sy, int dx, int dy, int w, int h);

}  // namespace render
