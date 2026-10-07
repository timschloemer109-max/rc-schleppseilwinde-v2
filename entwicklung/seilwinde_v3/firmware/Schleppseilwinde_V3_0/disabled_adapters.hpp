#pragma once
#include <stdint.h>
// The concrete uniLIGHT protocol/PWM command and 20-30 A high-side driver have
// not been specified. These adapters deliberately expose no guessed pin.
class DisabledPowerDriver {
 public:
  void off() {}
  void request(bool) {}
  bool healthy() const { return false; }
};
class DisabledUniLight {
 public:
  void request(uint16_t, bool) {}
};
