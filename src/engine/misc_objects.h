// Komponen non-fisika: TSound (0x407, ctor 0101aa18, vtable 0x10027a4), TTimerComponent (0x408, ctor 0101aa56,
// vtable 0x10027bc), TComponentGroup (0x404, ctor 01018ffd, vtable 0x10024d8), TTextBox (0x409, ctor 01014092).
#pragma once

#include <string>

#include "component.h"

class TSound : public TPinballComponent {
public:
    TSound(TPinballTable* table, int group_index);
    virtual float Play();  // 0101797e (vtable [5]): return durasi
    int sound_index = 0;   // +0x2a
};

class TTimerComponent : public TPinballComponent {
public:
    TTimerComponent(TPinballTable* table, int group_index);
    int Message(int code, float value) override;  // 01018f5b
    int timer = 0;  // +0x2a

private:
    static void TimerExpired(int timer_id, void* caller);  // 0101798c
};

class TComponentGroup : public TPinballComponent {
public:
    TComponentGroup(TPinballTable* table, int group_index);
    int Message(int code, float value) override;  // 010159f9
    std::vector<TPinballComponent*> list;  // +0x2a
    int timer = 0;                         // +0x32

private:
    static void NotifyTimerExpired(int timer_id, void* caller);  // 010159dc
};

// Satu pesan di antrean kotak teks (struct 0x10 byte, ctor 01013af1).
struct TTextBoxMessage {
    TTextBoxMessage* next = nullptr;  // +0
    std::string text;                 // +4
    float time;                       // +8 (-1 = permanen)
    int end_ticks;                    // +0xc: waktu game (ms) saat pesan dibuat + durasi

    TTextBoxMessage(const char* text, float time);
    float TimeLeft() const;   // 01013b60
    void Refresh(float time); // 01013b82
};

class TTextBox : public TPinballComponent {
public:
    TTextBox(TPinballTable* table, int group_index);
    ~TTextBox() override;
    void Clear();                                 // 01014143
    void Display(const char* text, float time);   // 010144b7

    int offset_x = 0, offset_y = 0, width = 0, height = 0;  // +0x2a..+0x36
    int timer = 0;                       // +0x3a (-1 = pesan permanen ditampilkan)
    const Bitmap8* background = nullptr; // +0x3e
    const struct score_msg_font_type* font = nullptr;  // +0x42
    TTextBoxMessage* message1 = nullptr; // +0x46 kepala antrean
    TTextBoxMessage* message2 = nullptr; // +0x4a ekor antrean

private:
    void Draw();                                       // 01014269
    static void TimerExpired(int timer_id, void* caller);  // 010141e1
};

// Font pesan dari PINBALL.DAT (grup "font1"...), dipakai TTextBox. Diisi score.cpp.
struct score_msg_font_type {
    int gap_width = 0;
    int height = 0;
    const Bitmap8* chars[128] = {};
};

void set_text_box_font(const score_msg_font_type* font);
TPinballComponent* create_misc_object(TPinballTable* table, int type, int group);
