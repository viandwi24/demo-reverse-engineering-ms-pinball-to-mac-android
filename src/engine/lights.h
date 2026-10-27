// Lampu: TLight (ctor 0100ad21, vtable 0x1001a48), TLightGroup (ctor 0100adb1, vtable 0x1001a60),
// TLightBargraph (ctor 0100ae72, vtable 0x1001a30).
#pragma once

#include "component.h"

// Struct flasher di TLight+0x2a (dipakai FUN_01009910/01009950/0100996f).
struct flasher_type {
    render_sprite* sprite = nullptr;          // +0x2a
    const Bitmap8* bmp_arr[2] = {};           // +0x2e: [0] mati, [1] nyala
    int unknown3 = 0, unknown4 = 0;           // +0x36, +0x3a
    float timer_delay[2] = {};                // +0x3e, +0x42
    int timer = 0;                            // +0x46
    int bmp_index = 0;                        // +0x4a

    static void callback(int timer_id, void* caller);  // 01009910
    void start(int state);                             // 01009950
    void stop(int state);                              // 0100996f
};

class TLight : public TPinballComponent {
public:
    TLight(TPinballTable* table, int group_index);
    int Message(int code, float value) override;  // 01009ae3
    virtual void Reset();                         // 010099a8 (vtable [5])

    flasher_type flasher;          // +0x2a
    int light_on = 0;              // +0x4e
    int flasher_on = 0;            // +0x52
    int toggled_off = 0;           // +0x56
    int toggled_on = 0;            // +0x5a
    int turn_off_after_flashing = 0;  // +0x5e
    int bmp_index = 0;             // +0x62
    float source_delay[2] = {};    // +0x66, +0x6a
    int timeout_timer = 0;         // +0x6e
    int undo_override_timer = 0;   // +0x72
    float unknown7a = 1.0f;        // +0x7a
    struct PlayerState {
        int message_field, light_on, flasher_on, unused[2], bmp_index;
    } player_data[4]{};            // +0x7e (stride 0x18)

private:
    void schedule_timeout(float time);                   // 01009a8f
    static void TimeoutTimer(int timer_id, void* caller);  // 01009a2a
};

class TLightGroup : public TPinballComponent {
public:
    TLightGroup(TPinballTable* table, int group_index);
    int Message(int code, float value) override;  // 0100a166
    virtual void Reset();                         // 01009feb (vtable [5])

    std::vector<TLight*> list;   // +0x2a
    float timer_time = 0;        // +0x32
    float timer_time_default = 0;  // +0x36
    int animation_code = 0;      // +0x3a
    int animation_flag = 0;      // +0x3e
    int notify_timer = 0;        // +0x42
    int timer = 0;               // +0x46
    struct PlayerState {
        int message_field, animation_code, unused;
        float timer_time;
    } player_data[4]{};          // +0x4a (stride 0x10)

protected:
    int next_light_up() const;    // 0100a0b4
    int next_light_down() const;  // 0100a0de
    void reset_timer(float time); // 0100a04a
    void start_animation();       // 0100a104

private:
    static void TimerFunc(int timer_id, void* caller);        // 0100a027
    static void NotifyTimerExpired(int timer_id, void* caller);  // 0100a149
};

class TLightBargraph : public TLightGroup {
public:
    TLightBargraph(TPinballTable* table, int group_index);
    int Message(int code, float value) override;  // 0100ab28
    void Reset() override;                        // 0100aaa9

    std::vector<float> timer_time_array;  // +0x8a
    int timer_bargraph = 0;               // +0x8e
    int time_index = 0;                   // +0x92
    int player_time_index[4]{};           // +0x96

private:
    static void BargraphTimerExpired(int timer_id, void* caller);  // 0100aad9
};
