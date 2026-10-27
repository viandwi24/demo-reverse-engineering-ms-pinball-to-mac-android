#include "misc_objects.h"

#include <cstring>

#include "control/control.h"
#include "table.h"
#include "timer.h"

// ---- TSound -----------------------------------------------------------------

TSound::TSound(TPinballTable* table, int group) : TPinballComponent(table, group, true) {
    visual_struct v;
    loader::query_visual(group, 0, v);
    sound_index = v.sound_index2;
}

float TSound::Play() { return loader::play_sound(sound_index); }

// ---- TTimerComponent --------------------------------------------------------

TTimerComponent::TTimerComponent(TPinballTable* table, int group) : TPinballComponent(table, group, true) {}

int TTimerComponent::Message(int code, float value) {
    if (code == 0x3b) {
        if (timer) timer::kill(timer);
        timer = timer::set(value, this, TimerExpired);
    } else if ((code == 0x3f3 || code == 0x3fe || code == 0x400) && timer) {
        timer::kill(timer);
        timer = 0;
    }
    return 0;
}

void TTimerComponent::TimerExpired(int, void* caller) {
    auto* t = static_cast<TTimerComponent*>(caller);
    t->timer = 0;
    control_handler(0x3c, t);
}

// ---- TComponentGroup --------------------------------------------------------

TComponentGroup::TComponentGroup(TPinballTable* table, int group) : TPinballComponent(table, group, false) {
    timer = 0;
    if (group > 0) {
        int count = 0;
        const int16_t* ids = loader::query_iattribute(group, 0x403, count);
        for (int i = 0; i < count; ++i)
            if (TPinballComponent* c = table->find_component(static_cast<int>(ids[i]))) list.push_back(c);
    }
}

int TComponentGroup::Message(int code, float value) {
    if (code == 0x30) {
        if (timer) {
            timer::kill(timer);
            timer = 0;
        }
        if (value > 0.0f) timer = timer::set(value, this, NotifyTimerExpired);
    } else if (code < 0x3f0 || (code > 0x3f3 && code != 0x3fc && code != 0x3fe)) {
        for (int i = static_cast<int>(list.size()) - 1; i >= 0; --i) list[i]->Message(code, value);
    }
    return 0;
}

void TComponentGroup::NotifyTimerExpired(int, void* caller) {
    auto* g = static_cast<TComponentGroup*>(caller);
    g->timer = 0;
    control_handler(0x3d, g);
}

// ---- TTextBox ---------------------------------------------------------------

TTextBoxMessage::TTextBoxMessage(const char* t, float tm) : text(t ? t : "") {
    time = tm;
    end_ticks = timer::g_now_ms - ftol(static_cast<double>(tm) * -1000.0);
}

float TTextBoxMessage::TimeLeft() const { return static_cast<float>((end_ticks - timer::g_now_ms) * 0.001); }

void TTextBoxMessage::Refresh(float tm) {
    time = tm;
    end_ticks = timer::g_now_ms - ftol(static_cast<double>(tm) * -1000.0);
}

namespace {
const score_msg_font_type* g_msg_font = nullptr;
}

void set_text_box_font(const score_msg_font_type* font) { g_msg_font = font; }

TTextBox::TTextBox(TPinballTable* table, int group) : TPinballComponent(table, group, true) {
    font = g_msg_font;
    if (group > 0) {
        int count = 0;
        const int16_t* v = loader::query_iattribute(group, 0x5dc, count);
        offset_x = v[0];
        offset_y = v[1];
        width = v[2];
        height = v[3];
    }
}

TTextBox::~TTextBox() {
    if (timer && timer != -1) timer::kill(timer);
    while (message1) {
        TTextBoxMessage* next = message1->next;
        delete message1;
        message1 = next;
    }
}

void TTextBox::Clear() {
    if (!background)
        render::fill_rect(offset_x, offset_y, width, height, 0);
    else
        render::blit(*background, offset_x, offset_y, offset_x, offset_y, width, height);
    if (timer) {
        if (timer != -1) timer::kill(timer);
        timer = 0;
    }
    while (message1) {
        TTextBoxMessage* next = message1->next;
        delete message1;
        message1 = next;
    }
}

void TTextBox::TimerExpired(int, void* caller) {
    auto* tb = static_cast<TTextBox*>(caller);
    tb->timer = 0;
    if (TTextBoxMessage* head = tb->message1) {
        tb->message1 = head->next;
        delete head;
        tb->Draw();
        control_handler(0x3c, tb);
    }
}

void TTextBox::Display(const char* text, float time) {
    if (!text) return;
    if (message1 && std::strcmp(text, message2->text.c_str()) == 0) {
        message2->Refresh(time);
        if (message2 != message1) return;
        if (timer && timer != -1) timer::kill(timer);
        if (time != -1.0f)
            timer = timer::set(time, this, TimerExpired);
        else
            timer = -1;
        return;
    }
    if (timer == -1) Clear();
    auto* msg = new TTextBoxMessage(text, time);
    if (!message1)
        message1 = msg;
    else
        message2->next = msg;
    message2 = msg;
    if (timer == 0) Draw();
}

void TTextBox::Draw() {
    if (!background)
        render::fill_rect(offset_x, offset_y, width, height, 0);
    else
        render::blit(*background, offset_x, offset_y, offset_x, offset_y, width, height);

    while (message1) {
        TTextBoxMessage* msg = message1;
        TTextBoxMessage* next = msg->next;
        if (msg->time == -1.0f) {
            if (!next) {
                timer = -1;
                break;
            }
        } else {
            float left = msg->TimeLeft();
            if (left >= -2.0f) {
                timer = timer::set(left >= 0.25f ? left : 0.25f, this, TimerExpired);
                break;
            }
        }
        delete msg;
        message1 = next;
    }
    if (!message1 || !font) {
        if (!message1) message2 = nullptr;
        return;
    }

    // Gambar teks dengan font bitmap, bungkus kata di spasi.
    const char* text = message1->text.c_str();
    int y = offset_y;
    Bitmap8& vs = render::vscreen_mut();
    while (*text && !(offset_y + height < font->height + y)) {
        int line_width = 0;
        const char* end = text;
        const char* last_space = nullptr;
        for (;;) {
            unsigned c = static_cast<unsigned char>(*end) & 0x7f;
            if (c == 0 || c == 10) break;
            const Bitmap8* g = font->chars[c];
            if (!g) {  // asli: loop tak berujung pada karakter tanpa glyph; di sini dilewati
                ++end;
                continue;
            }
            line_width += font->gap_width + g->width;
            if (line_width > width) {
                if (last_space) end = last_space;
                break;
            }
            if (*end == ' ') last_space = end;
            ++end;
        }
        int x = offset_x;
        for (; text < end; ++text) {
            const Bitmap8* g = font->chars[static_cast<unsigned char>(*text) & 0x7f];
            if (!g) continue;
            for (int r = 0; r < g->height; ++r)
                for (int c = 0; c < g->width; ++c)
                    if (y + r < vs.height && x + c < vs.width) vs.pixels[(y + r) * vs.stride + x + c] = g->at(c, r);
            x += font->gap_width + g->width;
        }
        while ((static_cast<unsigned char>(*text) & 0x7f) == ' ') ++text;
        if ((static_cast<unsigned char>(*text) & 0x7f) == 10) ++text;
        y += font->height;
    }
}

// ---- Pabrik -----------------------------------------------------------------

TPinballComponent* create_misc_object(TPinballTable* table, int type, int group) {
    switch (type) {
    case 0x404: return new TComponentGroup(table, group);
    case 0x407: return new TSound(table, group);
    case 0x408: return new TTimerComponent(table, group);
    case 0x409: return new TTextBox(table, group);
    default: return nullptr;
    }
}
