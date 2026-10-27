#include "render.h"

#include <algorithm>
#include <cstring>
#include <memory>

#include "maths.h"

namespace render {
namespace {

Bitmap8 g_vscreen;  // DAT_01025088
Bitmap8 g_frame;    // salinan vscreen + bola untuk ditampilkan
ZMap g_zscreen;     // DAT_01025058
rectangle_type g_screen_rect;  // DAT_01025074

std::vector<std::unique_ptr<render_sprite>> g_storage;
std::vector<render_sprite*> g_sprites;  // DAT_01025084 (maks 1000)
std::vector<render_sprite*> g_dirty;    // DAT_01025068 (maks 999)
std::vector<render_sprite*> g_balls;    // DAT_0102506c (maks 20)
Bitmap8 g_ball_bg[20];                  // DAT_010250c8, 64x64

const ZMap* g_table_zmap = nullptr;  // DAT_010253b8
int g_table_zmap_x = 0, g_table_zmap_y = 0;

float g_z_min = 0, g_z_scaler = 0, g_z_max = 0;  // 010250bc, 010250b0, 010250b8

Bitmap8 make_bitmap(int w, int h) {
    Bitmap8 b;
    b.width = w;
    b.height = h;
    b.stride = w % 4 ? w - w % 4 + 4 : w;
    b.pixels.assign(static_cast<size_t>(b.stride) * h, 0);
    return b;
}

// FUN_0100400b
bool overlap(const rectangle_type& r1, const rectangle_type& r2, rectangle_type* out) {
    int r2_right = r2.x + r2.width, r2_bottom = r2.y + r2.height;
    if (!(r2.x <= r1.x + r1.width && r1.x < r2_right && r2.y <= r1.y + r1.height && r1.y < r2_bottom)) return false;
    int x = r1.x, w = r1.width;
    if (x < r2.x) {
        w += x - r2.x;
        x = r2.x;
    }
    if (r2_right < w + x) w = r2_right - x;
    int y = r1.y, h = r1.height;
    if (y < r2.y) {
        h += y - r2.y;
        y = r2.y;
    }
    if (r2_bottom < h + y) h = r2_bottom - y;
    if (w == 0 || h == 0) return false;
    if (out) *out = {x, y, w, h};
    return true;
}

// FUN_010040c2
void enclosing_box(const rectangle_type& a, const rectangle_type& b, rectangle_type& out) {
    int x = a.x, y = a.y, w = a.width, h = a.height;
    if (b.x < x) {
        w += x - b.x;
        x = b.x;
    }
    if (b.y < y) {
        h += y - b.y;
        y = b.y;
    }
    if (w + x < b.x + b.width) w = b.width - x + b.x;
    if (h + y < b.height + b.y) h = b.height - y + b.y;
    out = {x, y, w, h};
}

void repaint(render_sprite* s) {  // FUN_0101382a
    if (s->type != VisualType::Ball && g_dirty.size() < 999) g_dirty.push_back(s);
}

// FUN_01004870
void copy_rect(Bitmap8& dst, int w, int h, int dx, int dy, const Bitmap8& src, int sx, int sy) {
    for (int y = 0; y < h; ++y)
        std::memcpy(&dst.pixels[(dy + y) * dst.stride + dx], &src.pixels[(sy + y) * src.stride + sx], w);
}

void reset_area(const rectangle_type& r) {
    for (int y = 0; y < r.height; ++y) {
        std::fill_n(&g_zscreen.depth[(r.y + y) * g_zscreen.stride + r.x], r.width, 0xffff);
        std::fill_n(&g_vscreen.pixels[(r.y + y) * g_vscreen.stride + r.x], r.width, 0);
    }
}

// FUN_01004a76: gambar piksel bila z sprite <= z layar (juga menulis z).
void paint_zbuffered(const rectangle_type& r, const Bitmap8& bmp, int bx, int by, const ZMap& zmap, int zx, int zy) {
    for (int y = 0; y < r.height; ++y) {
        uint8_t* dst = &g_vscreen.pixels[(r.y + y) * g_vscreen.stride + r.x];
        uint16_t* zdst = &g_zscreen.depth[(r.y + y) * g_zscreen.stride + r.x];
        const uint8_t* src = &bmp.pixels[(by + y) * bmp.stride + bx];
        const uint16_t* zsrc = &zmap.depth[(zy + y) * zmap.stride + zx];
        for (int x = 0; x < r.width; ++x) {
            if (zsrc[x] <= zdst[x]) {
                dst[x] = src[x];
                zdst[x] = zsrc[x];
            }
        }
    }
}

// FUN_0101304d
void paint_sprite(render_sprite* dirty_sprite) {
    for (render_sprite* s : dirty_sprite->overlaps) {
        rectangle_type r;
        if (s->destroy_pending || !s->bmp || !overlap(s->rect, dirty_sprite->dirty, &r)) continue;
        paint_zbuffered(r, *s->bmp, r.x - s->rect.x, r.y - s->rect.y, *s->zmap, s->zmap_offset_x - s->rect.x + r.x,
                        s->zmap_offset_y - s->rect.y + r.y);
    }
}

void remove_sprite(render_sprite* s) {  // FUN_01013283
    auto it = std::find(g_sprites.begin(), g_sprites.end(), s);
    if (it != g_sprites.end()) g_sprites.erase(it);
}

// FUN_010130f3
void paint_balls() {
    int n = static_cast<int>(g_balls.size());
    if (n > 1) {
        for (int i = 0; i < n; ++i)
            for (int j = i; j < n / 2; ++j)
                if (g_balls[j]->depth < g_balls[i]->depth) std::swap(g_balls[i], g_balls[j]);
    }
    for (int i = 0; i < n; ++i) {
        render_sprite* b = g_balls[i];
        if (!b->bmp || !overlap(b->rect, g_screen_rect, &b->dirty)) {
            b->dirty.width = -1;
            continue;
        }
        const rectangle_type& d = b->dirty;
        copy_rect(g_ball_bg[i], d.width, d.height, 0, 0, g_vscreen, d.x, d.y);
        // FUN_01004b75: piksel bola non-nol yang lebih dekat dari z layar
        for (int y = 0; y < d.height; ++y) {
            uint8_t* dst = &g_vscreen.pixels[(d.y + y) * g_vscreen.stride + d.x];
            const uint16_t* z = &g_zscreen.depth[(d.y + y) * g_zscreen.stride + d.x];
            const uint8_t* src = &b->bmp->pixels[(d.y - b->rect.y + y) * b->bmp->stride + (d.x - b->rect.x)];
            for (int x = 0; x < d.width; ++x)
                if (src[x] != 0 && b->depth < z[x]) dst[x] = src[x];
        }
    }
}

// FUN_0101320d
void restore_balls() {
    for (int i = static_cast<int>(g_balls.size()) - 1; i >= 0; --i) {
        render_sprite* b = g_balls[i];
        if (b->dirty.width > 0) copy_rect(g_vscreen, b->dirty.width, b->dirty.height, b->dirty.x, b->dirty.y, g_ball_bg[i], 0, 0);
        b->prev_dirty = b->dirty;
    }
}

}  // namespace

void init(float z_min, float z_scaler, int width, int height) {
    g_z_scaler = z_scaler;
    g_z_min = z_min;
    g_z_max = static_cast<float>(4.2949673e+09 / z_scaler + z_min);
    g_vscreen = make_bitmap(width, height);
    g_vscreen.stride = width;
    g_vscreen.pixels.assign(static_cast<size_t>(width) * height, 0);
    g_zscreen.width = width;
    g_zscreen.height = height;
    g_zscreen.stride = width;
    g_zscreen.depth.assign(static_cast<size_t>(width) * height, 0xffff);
    g_screen_rect = {0, 0, width, height};
    for (auto& b : g_ball_bg) b = make_bitmap(64, 64);
    g_sprites.clear();
    g_dirty.clear();
    g_balls.clear();
    g_storage.clear();
}

void uninit() {
    g_sprites.clear();
    g_dirty.clear();
    g_balls.clear();
    g_storage.clear();
}

render_sprite* create_sprite(VisualType type, const Bitmap8* bmp, const ZMap* zmap, int x, int y,
                             const rectangle_type* bounding) {
    g_storage.push_back(std::make_unique<render_sprite>());
    render_sprite* s = g_storage.back().get();
    s->rect = {x, y, bmp ? bmp->width : 0, bmp ? bmp->height : 0};
    s->bmp = bmp;
    s->zmap = zmap;
    s->type = type;
    s->bounding = bounding ? *bounding : rectangle_type{0, 0, -1, -1};
    if (!zmap && type != VisualType::Ball) {
        s->zmap = g_table_zmap;
        s->zmap_offset_x = x - g_table_zmap_x;
        s->zmap_offset_y = y - g_table_zmap_y;
    }
    s->prev_dirty = s->rect;
    if (type == VisualType::Ball) {
        g_balls.push_back(s);
    } else {
        g_sprites.push_back(s);
        repaint(s);
    }
    return s;
}

void set_table_zmap(const ZMap* zmap, int offset_x, int offset_y) {
    g_table_zmap = zmap;
    g_table_zmap_x = offset_x;
    g_table_zmap_y = offset_y;
}

void sprite_set(render_sprite* s, const Bitmap8* bmp, const ZMap* zmap, int x, int y) {
    if (!s) return;
    s->rect.x = x;
    s->rect.y = y;
    s->bmp = bmp;
    if (bmp) {
        s->rect.width = bmp->width;
        s->rect.height = bmp->height;
    }
    s->zmap = zmap;
    repaint(s);
}

void sprite_set_bitmap(render_sprite* s, const Bitmap8* bmp) {
    if (!s || s->bmp == bmp) return;
    s->bmp = bmp;
    if (bmp) {
        s->rect.width = bmp->width;
        s->rect.height = bmp->height;
    }
    repaint(s);
}

void ball_set(render_sprite* s, const Bitmap8* bmp, float depth, int x, int y) {
    if (!s) return;
    s->bmp = bmp;
    if (bmp) {
        s->rect = {x, y, bmp->width, bmp->height};
    }
    if (depth < g_z_min) {
        s->depth = 0;
    } else {
        double v = (static_cast<double>(depth) - g_z_min) * g_z_scaler;
        s->depth = v <= g_z_max ? static_cast<uint16_t>(ftol(v)) : 0xffff;
    }
}

void build_occlude_list() {
    for (render_sprite* s1 : g_sprites) {
        s1->overlaps.clear();
        if (s1->destroy_pending || s1->bounding.width == -1) continue;
        std::vector<render_sprite*> list;
        for (render_sprite* s2 : g_sprites)
            if (!s2->destroy_pending && s2->bounding.width != -1 && overlap(s1->bounding, s2->bounding, nullptr))
                list.push_back(s2);
        if (s1->type == VisualType::Static && s1->bmp && list.size() < 2) list.clear();
        s1->overlaps = std::move(list);
    }
}

void update() {
    for (render_sprite* s : g_dirty) {
        if (s->type == VisualType::Static) {
            if (!overlap(s->rect, g_screen_rect, &s->dirty)) {
                s->dirty.width = -1;
                continue;
            }
            if (s->bmp) continue;
        } else if (s->type == VisualType::Dynamic) {
            if (s->prev_dirty.width > 0) enclosing_box(s->prev_dirty, s->rect, s->dirty);
            if (!overlap(s->dirty, g_screen_rect, &s->dirty)) {
                s->dirty.width = -1;
                continue;
            }
        } else {
            continue;
        }
        reset_area(s->dirty);
    }
    for (render_sprite* s : g_dirty)
        if (s->dirty.width > 0 && (s->type == VisualType::Static || s->type == VisualType::Dynamic)) paint_sprite(s);

    paint_balls();
    g_frame = g_vscreen;

    for (render_sprite* s : g_dirty) {
        s->prev_dirty = s->dirty;
        if (s->destroy_pending) remove_sprite(s);
    }
    g_dirty.clear();
    restore_balls();
}

void paint_background(const Bitmap8& bmp, int x, int y) { copy_rect(g_vscreen, bmp.width, bmp.height, x, y, bmp, 0, 0); }

const Bitmap8& frame() { return g_frame; }
const Bitmap8& vscreen() { return g_vscreen; }
Bitmap8& vscreen_mut() { return g_vscreen; }

void fill_rect(int x, int y, int w, int h, uint8_t color) {
    for (int r = 0; r < h; ++r)
        if (y + r >= 0 && y + r < g_vscreen.height)
            for (int c = 0; c < w; ++c)
                if (x + c >= 0 && x + c < g_vscreen.width) g_vscreen.pixels[(y + r) * g_vscreen.stride + x + c] = color;
}

void blit(const Bitmap8& src, int sx, int sy, int dx, int dy, int w, int h) { copy_rect(g_vscreen, w, h, dx, dy, src, sx, sy); }

}  // namespace render
