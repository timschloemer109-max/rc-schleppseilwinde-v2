#pragma once
#include "control.hpp"
#include "exbus.h"
#include "menu.hpp"
#include "diagnostics.hpp"
namespace winch {
struct Reply {
  uint8_t bytes[exbus::MaxFrameBytes]{};
  std::size_t length = 0;
  uint32_t requestedUs = 0;
};
// The same composition is executed by the Nano adapter and integration tests.
// Hardware service has its own byte/time budgets; this API handles one byte.
class Application {
 public:
  explicit Application(const Settings& settings) : control_(settings), menu_(settings) {}
  void reset(uint32_t nowMs, uint32_t hall, bool endSwitch) { control_.reset(nowMs, hall, endSwitch); }
  const Outputs& tick(Inputs input) { input.rc = rc_; return control_.tick(input); }
  void receive(uint8_t byte, uint32_t receivedUs, uint32_t nowUs, uint32_t nowMs);
  void expire(uint32_t nowUs) { parser_.expire(nowUs); }
  void transportError() { parser_.reset(); ++transportErrors_; reply_.length = 0; }
  Reply& reply() { return reply_; }
  Controller& controller() { return control_; }
  Menu& menu() { return menu_; }
  Diagnostics& diagnostics() { return diagnostics_; }
  const RcFrame& rc() const { return rc_; }
  uint16_t triggerUs() const { return triggerUs_; }
  bool takeSaveRequest() { return menu_.takeSaveRequest(); }
  uint32_t busErrors() const;
  void setTelemetry(int32_t currentMa, bool currentValid, uint32_t pulseRate, uint32_t hallTotal);
 private:
  Controller control_;
  Menu menu_;
  Diagnostics diagnostics_;
  exbus::Parser parser_;
  RcFrame rc_;
  Reply reply_;
  uint32_t transportErrors_ = 0;
  uint16_t triggerUs_ = 1500;
  uint8_t telemetryIndex_ = 0, descriptor_ = 0;
  int32_t currentMa_ = 0;
  bool currentValid_ = false;
  uint32_t pulseRate_ = 0, hallTotal_ = 0;
  void handle(const exbus::Frame& frame, uint32_t nowUs, uint32_t nowMs);
  std::size_t telemetry(uint8_t* dest, std::size_t capacity);
};
}  // namespace winch
