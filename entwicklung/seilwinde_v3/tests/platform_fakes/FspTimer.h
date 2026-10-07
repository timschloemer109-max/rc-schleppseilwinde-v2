#pragma once
#include "Arduino.h"
constexpr uint8_t GPT_TIMER = 0;
constexpr int TIMER_MODE_PERIODIC = 1;
struct timer_callback_args_t { const void* p_context; };
extern void (*fakeTimerCallback)(timer_callback_args_t*);
extern const void* fakeTimerContext;
extern uint32_t fakeTimerPeriod;
extern unsigned fakeTimerStarts, fakeTimerStops, fakeTimerResets;
extern bool fakeTimerRunning;
class FspTimer {
public:
    static int8_t get_available_timer(uint8_t& type) { type = GPT_TIMER; return 2; }
    bool begin(int, uint8_t, uint8_t, float, float,
               void (*callback)(timer_callback_args_t*), void* context) {
        fakeTimerCallback = callback; fakeTimerContext = context;
        fakeTimerPeriod = 60000; return true;
    }
    bool setup_overflow_irq(uint8_t) { return true; }
    bool open() { return true; }
    uint32_t get_period_raw() { return 60000; }
    bool set_period_buffer(bool) { return true; }
    bool start() { ++fakeTimerStarts; fakeTimerRunning = true; return true; }
    bool stop() { ++fakeTimerStops; fakeTimerRunning = false; return true; }
    bool reset() { ++fakeTimerResets; return true; }
    bool set_period(uint32_t period) { fakeTimerPeriod = period; return true; }
};
inline void fakeTimerEvent() {
    timer_callback_args_t args{fakeTimerContext}; fakeTimerCallback(&args);
}
