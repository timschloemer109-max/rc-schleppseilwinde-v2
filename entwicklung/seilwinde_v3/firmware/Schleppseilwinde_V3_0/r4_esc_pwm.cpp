#include "r4_esc_pwm.hpp"

#if !defined(ARDUINO_NANO_R4)
#error "R4 ESC PWM is qualified only for Nano R4 / Renesas core 1.6.0."
#endif

namespace {
class CriticalSection {
public:
    CriticalSection() : previous_(__get_PRIMASK()) { __disable_irq(); }
    ~CriticalSection() { __set_PRIMASK(previous_); }
private:
    uint32_t previous_;
};
}

bool R4EscPwm::begin(uint8_t pin) {
    if (opened_ || pin >= PINS_COUNT) return false;
    const auto bspPin = g_pin_cfg[pin].pin;
    const uint32_t port = static_cast<uint32_t>(bspPin) >> 8U;
    mask_ = 1U << (static_cast<uint32_t>(bspPin) & 0xffU);
    // PCNTR3 provides atomic POSR/PORR set/reset; no read-modify-write race.
    output_ = &((R_PORT0 + ((R_PORT1 - R_PORT0) * port))->PCNTR3);
    if (R_IOPORT_PinCfg(&g_ioport_ctrl, bspPin,
        IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_LOW) != FSP_SUCCESS) return false;
    low();
    uint8_t type = GPT_TIMER;
    const int8_t channel = FspTimer::get_available_timer(type);
    if (channel < 0 || type != GPT_TIMER) return false;
    if (!timer_.begin(TIMER_MODE_PERIODIC, type, static_cast<uint8_t>(channel),
        50.0f, 0.0f, callback, this)) return false;
    if (!timer_.setup_overflow_irq(8) || !timer_.open()) return false;
    opened_ = true;
    frameTicks_ = timer_.get_period_raw();
    if (frameTicks_ < 20000U || !timer_.set_period_buffer(false)) return false;
    requestedUs_ = 1000;
    activeUs_ = 1000;
    high_ = false;
    healthy_ = timer_.start();
    return healthy_;
}

uint32_t R4EscPwm::ticks(uint16_t microseconds) const {
    // 64-bit product protects 32-bit GPT periods without floating point in ISR.
    return static_cast<uint32_t>((static_cast<uint64_t>(frameTicks_) * microseconds) / 20000U);
}

void R4EscPwm::writeMicroseconds(uint16_t pulseUs) {
    if (pulseUs < 1000 || pulseUs > 2000) pulseUs = 1000;
    CriticalSection guard;
    requestedUs_ = pulseUs;
}

void R4EscPwm::low() {
    if (output_ != nullptr) *output_ = mask_ << 16U;
}

void R4EscPwm::stopNow() {
    CriticalSection guard;
    requestedUs_ = 1000;
    // Preserve a valid 1000-us STOP pulse if stopNow() is called every loop.
    // Only a longer active drive pulse may be shortened immediately.
    if (high_ && activeUs_ > 1000 && opened_) {
        low();
        high_ = false;
        // Begin a new low frame; do not wait for the preceding run pulse.
        if (!timer_.stop() || !timer_.set_period(frameTicks_) || !timer_.reset() || !timer_.start()) {
            healthy_ = false;
            timer_.stop();
        }
    }
}

void R4EscPwm::disable() {
    CriticalSection guard;
    requestedUs_ = 1000;
    healthy_ = false;
    if (opened_) timer_.stop();
    high_ = false;
    low();
}

void R4EscPwm::callback(timer_callback_args_t* args) {
    if (args == nullptr || args->p_context == nullptr) return;
    auto* instance = static_cast<R4EscPwm*>(const_cast<void*>(args->p_context));
    instance->onTimer();
}

void R4EscPwm::onTimer() {
    if (!healthy_) { low(); return; }
    uint32_t nextTicks;
    if (high_) {
        low();
        high_ = false;
        nextTicks = frameTicks_ - ticks(activeUs_);
    } else {
        activeUs_ = requestedUs_;
        *output_ = mask_;
        high_ = true;
        nextTicks = ticks(activeUs_);
    }
    if (!timer_.set_period(nextTicks)) {
        healthy_ = false;
        low();
        high_ = false;
        timer_.stop();
    }
}
