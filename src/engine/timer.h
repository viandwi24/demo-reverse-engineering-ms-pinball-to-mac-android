// Timer berbasis waktu game (ms). Port dari FUN_0100742f-FUN_01007659.
#pragma once

#include <cstdint>

namespace timer {

using Callback = void (*)(int timer_id, void* caller);

void init(int count);                                   // 0100742f
int set(float seconds, void* caller, Callback callback);  // 010074b7, return id (0 = gagal)
int check();                                            // 0100758d: jalankan timer yang jatuh tempo
int kill(int timer_id);                                 // 01007659

// DAT_01024f84: waktu game dalam ms, ditambah di langkah simulasi.
extern int32_t g_now_ms;

}  // namespace timer
