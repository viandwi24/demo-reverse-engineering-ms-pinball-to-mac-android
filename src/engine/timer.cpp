#include "timer.h"

#include <vector>

#include "maths.h"

namespace timer {

int32_t g_now_ms = 0;

namespace {

struct TimerStruct {
    int32_t target_time;
    void* caller;
    Callback callback;
    TimerStruct* next;
    int id;
};

std::vector<TimerStruct> g_pool;
TimerStruct* g_active = nullptr;  // DAT_01024f70, urut menurut waktu
TimerStruct* g_free = nullptr;    // DAT_01024f7c
int g_count = 0;                  // DAT_01024f78
int g_max = 0;                    // DAT_01024f74
int g_next_id = 1;                // DAT_010235f4

void fire(TimerStruct* t) {
    --g_count;
    TimerStruct copy = *t;
    g_active = t->next;
    t->next = g_free;
    g_free = t;
    if (copy.callback) copy.callback(copy.id, copy.caller);
}

}  // namespace

void init(int count) {
    g_pool.assign(count, TimerStruct{});
    for (int i = 0; i < count - 1; ++i) g_pool[i].next = &g_pool[i + 1];
    g_pool[count - 1].next = nullptr;
    g_free = &g_pool[0];
    g_active = nullptr;
    g_count = 0;
    g_max = count;
    g_next_id = 1;
}

int set(float seconds, void* caller, Callback callback) {
    int32_t target = g_now_ms - ftol(static_cast<double>(seconds) * -1000.0);
    if (g_count >= g_max) return 0;

    TimerStruct* t = g_free;
    g_free = t->next;
    t->next = nullptr;

    TimerStruct* prev = g_active;
    TimerStruct* cur = g_active;
    for (int i = 0; i < g_count; ++i) {
        if (target < cur->target_time) break;
        prev = cur;
        cur = cur->next;
    }
    if (g_count > 0 && cur != prev) {
        t->next = prev->next;
        prev->next = t;
    } else {
        t->next = g_active;
        g_active = t;
    }
    t->caller = caller;
    t->callback = callback;
    ++g_count;
    t->id = g_next_id++;
    t->target_time = target;
    if (g_next_id < 1) g_next_id = 1;
    return t->id;
}

int check() {
    int fired = 0;
    while (g_active && fired < 2) {
        if (g_now_ms < g_active->target_time) return fired;
        fire(g_active);
        ++fired;
    }
    while (g_active && g_active->target_time + 100 <= g_now_ms) {
        fire(g_active);
        ++fired;
    }
    return fired;
}

int kill(int timer_id) {
    TimerStruct* prev = nullptr;
    TimerStruct* cur = g_active;
    for (int i = 0; i < g_count; ++i) {
        if (cur->id == timer_id) {
            if (prev)
                prev->next = cur->next;
            else
                g_active = cur->next;
            --g_count;
            cur->next = g_free;
            g_free = cur;
            return timer_id;
        }
        prev = cur;
        cur = cur->next;
    }
    return 0;
}

}  // namespace timer
