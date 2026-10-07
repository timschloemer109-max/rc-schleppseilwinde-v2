#pragma once
#include "bounded_queue.hpp"
#include "control.hpp"
namespace winch {
struct DiagnosticsInput {
  uint32_t nowMs = 0, hallTotal = 0, pulseRate = 0, lastHallAgeMs = 0;
  int32_t currentMilliAmp = 0;
  bool currentValid = false, endSwitch = false;
  uint32_t busErrors = 0, loopMaxUs = 0;
  uint16_t triggerUs = 1500;
  uint16_t actualEscUs = 1000;
  bool hardwareInhibited = true;
  RcFrame rc;
};
class Diagnostics {
 public:
  Diagnostics();
  void tick(const Controller& controller, const DiagnosticsInput& input);
  ByteQueue<2048>& queue() { return queue_; }
  static const char* header();
 private:
  ByteQueue<2048> queue_;
  uint32_t lastMs_ = 0, lastHall_ = 0, lastEvents_ = 0;
  bool boot_ = true;
};
}  // namespace winch
