#include "score.h"

#include <cstdio>
#include <cstring>

#include "../exe_resources.h"
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>

#include "loader.h"
#include "render.h"

namespace score {
namespace {
std::vector<std::unique_ptr<score_struct>> g_scores;
std::vector<std::unique_ptr<Bitmap8>> g_font_bitmaps;
score_msg_font_type g_font;
}  // namespace

score_struct* create(const char* field_name, const Bitmap8* background) {
    int group = loader::query_handle(field_name);
    const DatEntry* e = group < 0 ? nullptr : loader::dat()->find(group, EntryType::ShortArray);
    if (!e) return nullptr;
    auto s = std::make_unique<score_struct>();
    s->background = background;
    const int16_t* v = reinterpret_cast<const int16_t*>(e->raw.data());
    s->offset_x = v[1];
    s->offset_y = v[2];
    s->width = v[3];
    s->height = v[4];
    for (int i = 0; i < 10; ++i) {
        const DatEntry* b = loader::dat()->find(v[0] + i, EntryType::Bitmap8);
        s->char_bmp[i] = b ? b->bitmap.get() : nullptr;
    }
    g_scores.push_back(std::move(s));
    return g_scores.back().get();
}

score_struct* dup(const score_struct* s) {
    g_scores.push_back(std::make_unique<score_struct>(*s));
    return g_scores.back().get();
}

void set(score_struct* s, int value) {
    if (!s) return;
    s->score = value;
    s->dirty_flag = 1;
}

void erase(score_struct* s, int) {
    if (!s) return;
    if (!s->background)
        render::fill_rect(s->offset_x, s->offset_y, s->width, s->height, 0);
    else
        render::blit(*s->background, s->offset_x, s->offset_y, s->offset_x, s->offset_y, s->width, s->height);
}

void update(score_struct* s) {
    if (!s || !s->dirty_flag || s->score > 1000000000) return;
    int y = s->offset_y;
    int x = s->offset_x + s->width;
    s->dirty_flag = 0;
    erase(s, 0);
    if (s->score < 0) return;
    char buf[12];
    std::snprintf(buf, sizeof(buf), "%d", s->score);
    for (int i = static_cast<int>(std::strlen(buf)) - 1; i >= 0; --i) {
        const Bitmap8* digit = s->char_bmp[buf[i] - '0'];
        x -= digit->width;
        // FUN_01004870 (tanpa latar) / FUN_010048ec (dengan latar, piksel 0 transparan)
        if (!s->background) {
            render::blit(*digit, 0, 0, x, y, digit->width, digit->height);
        } else {
            Bitmap8& vs = render::vscreen_mut();
            for (int r = 0; r < digit->height; ++r)
                for (int c = 0; c < digit->width; ++c)
                    if (uint8_t p = digit->at(c, r)) vs.pixels[(y + r) * vs.stride + x + c] = p;
        }
    }
}

const score_msg_font_type* load_msg_font() {
    const std::vector<uint8_t>& font = exe_resources::msg_font();
    if (font.size() < 0x86) return nullptr;
    const uint8_t* d = font.data();
    int16_t gap, height;
    std::memcpy(&gap, &d[0], 2);
    std::memcpy(&height, &d[4], 2);
    g_font = score_msg_font_type{};
    g_font.gap_width = gap;
    g_font.height = height;
    size_t pos = 0x86;  // data setelah 128 byte lebar karakter
    for (int ch = 0; ch < 128; ++ch) {
        int w = static_cast<int8_t>(d[6 + ch]);
        if (w == 0) continue;
        auto bmp = std::make_unique<Bitmap8>();
        bmp->width = w;
        bmp->height = height;
        bmp->stride = w;
        bmp->pixels.assign(&d[pos + 1], &d[pos + 1] + w * height);  // 1 byte awalan dilewati
        pos += static_cast<size_t>(w) * height + 1;
        g_font.chars[ch] = bmp.get();
        g_font_bitmaps.push_back(std::move(bmp));
    }
    return &g_font;
}

}  // namespace score
