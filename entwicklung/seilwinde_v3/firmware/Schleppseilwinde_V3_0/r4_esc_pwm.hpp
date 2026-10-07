#pragma once
#include <Arduino.h>
#include <FspTimer.h>
#include <stdint.h>

// Two bounded GPT intervals create 50 Hz servo PWM on any qualified GPIO.
// First and idle pulses are 1000 us; no Servo.attach() 1500 us default.
class R4EscPwm {
public:
    bool begin(uint8_t pin = 4);
    void writeMicroseconds(uint16_t pulseUs);
    // Shorten a currently active HIGH immediately; following frames are STOP.
    void stopNow();
    // Maintenance/reset path: cease waveform and drive output LOW.
    void disable();
    bool healthy() const { return healthy_; }
private:
    static void callback(timer_callback_args_t* args);
    void onTimer();
    uint32_t ticks(uint16_t microseconds) const;
    void low();
    FspTimer timer_;
    volatile uint16_t requestedUs_ = 1000;
    volatile bool high_ = false;
    volatile bool healthy_ = false;
    uint16_t activeUs_ = 1000;
    uint32_t frameTicks_ = 0;
    volatile uint32_t* output_ = nullptr;
    uint32_t mask_ = 0;
    bool opened_ = false;
};
