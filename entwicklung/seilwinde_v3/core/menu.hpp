#pragma once
#include "control.hpp"
#include <cstddef>
namespace winch {
class Menu {
 public:
  explicit Menu(const Settings& settings) : draft_(settings) {}
  void buttons(uint8_t raw, uint32_t nowMs, Controller& controller);
  void render(char text[32], const Controller& controller) const;
  bool takeSaveRequest() { const bool result = save_; save_ = false; return result; }
  const Settings& draft() const { return draft_; }
  uint8_t page() const { return page_; }
  void lock(bool locked) { locked_ = locked; }
  void saved(bool success) { message_ = success ? "Saved / re-arm" : "Save rejected"; }
 private:
  Settings draft_;
  uint8_t page_ = 0;
  uint8_t lastButtons_ = 0;
  uint32_t lastKeyMs_ = 0;
  bool save_ = false;
  bool locked_ = false;
  const char* message_ = "L/R page U/D edit";
  void change(bool up);
};
}  // namespace winch
