#include "menu.hpp"
#include <cstdio>
#include <cstring>
namespace winch {
namespace {
constexpr uint8_t Pages = 24;
template <typename T> void adjust(T& value, bool up, uint32_t step, uint32_t lo, uint32_t hi) {
  uint32_t v = static_cast<uint32_t>(value);
  v = up ? (v > hi - step ? hi : v + step) : (v < lo + step ? lo : v - step);
  value = static_cast<T>(v);
}
const char* const Labels[Pages] = {
  "V3 status", "Rope metres", "FAST percent", "FAST max us", "SLOW us",
  "Warn percent", "Ramp up ms", "Ramp down ms", "Max run sec", "RC timeout ms",
  "Stall mA", "Stall gap ms", "Stall mask ms", "Stall enabled", "Speed channel",
  "Trigger channel", "Light channel", "Power channel", "Arm ms", "Neutral ms",
  "Apply RAM", "Save flash", "Discard edits", "Learned pulses"
};
}
void Menu::change(bool up) {
  switch (page_) {
    case 1: adjust(draft_.ropeLengthMm, up, 1000, 1000, 1000000); break;
    case 2: adjust(draft_.fastPermille, up, 10, 100, 990); break;
    case 3: adjust(draft_.fastMaxUs, up, 10, 1100, 2000); break;
    case 4: adjust(draft_.slowUs, up, 10, 1000, 1800); break;
    case 5: adjust(draft_.warningPermille, up, 10, 0, 1000); break;
    case 6: adjust(draft_.rampUpMs, up, 100, 0, 10000); break;
    case 7: adjust(draft_.rampDownMs, up, 100, 0, 10000); break;
    case 8: adjust(draft_.maxRunMs, up, 1000, 1000, 120000); break;
    case 9: adjust(draft_.rcTimeoutMs, up, 10, 50, 500); break;
    case 10: adjust(draft_.stallMilliAmp, up, 100, 100, 100000); break;
    case 11: adjust(draft_.stallNoPulseMs, up, 10, 50, 5000); break;
    case 12: adjust(draft_.stallMaskMs, up, 50, 0, 5000); break;
    case 13: draft_.stallEnabled = static_cast<uint8_t>(up && draft_.currentCalibrated != 0); break;
    case 14: adjust(draft_.speedChannel, up, 1, 0, 23); break;
    case 15: adjust(draft_.triggerChannel, up, 1, 0, 23); break;
    case 16: adjust(draft_.lightChannel, up, 1, 0, 23); break;
    case 17: adjust(draft_.powerChannel, up, 1, 0, 23); break;
    case 18: adjust(draft_.armMs, up, 100, 1000, 10000); break;
    case 19: adjust(draft_.neutralMs, up, 50, 100, 2000); break;
    default: break;
  }
}
void Menu::buttons(uint8_t raw, uint32_t nowMs, Controller& controller) {
  // Learned pulses are maintained only by the protected learning state machine.
  draft_.learnedPulses = controller.settings().learnedPulses;
  const uint8_t keys = static_cast<uint8_t>((~static_cast<unsigned>(raw)) & 0xf0U);
  if (keys == 0) { lastButtons_ = 0; return; }
  if (keys == lastButtons_ && static_cast<uint32_t>(nowMs - lastKeyMs_) < 350U) return;
  lastButtons_ = keys; lastKeyMs_ = nowMs;
  if (keys == 0x80) { page_ = static_cast<uint8_t>((page_ + Pages - 1U) % Pages); return; }
  if (keys == 0x10) { page_ = static_cast<uint8_t>((page_ + 1U) % Pages); return; }
  if (keys != 0x20 && keys != 0x40) return;
  if (locked_) { message_ = "Flash busy"; return; }
  if (page_ == 20) {
    message_ = controller.applySettings(draft_) ? "Applied RAM" : "Stop / invalid";
  } else if (page_ == 21) {
    // Save the reviewed current RAM settings, never silently apply a draft.
    if (controller.canSave() && equalSettings(draft_, controller.settings())) {
      save_ = true; message_ = "Saving power OFF";
    } else message_ = "Apply / stop first";
  } else if (page_ == 22) {
    draft_ = controller.settings(); message_ = "Edits discarded";
  } else if (page_ != 0 && page_ != 23) {
    change(keys == 0x20); message_ = "Draft / apply RAM";
  }
}
void Menu::render(char text[32], const Controller& controller) const {
  char value[48]{};
  uint32_t numeric = 0;
  switch (page_) {
    case 0: std::snprintf(value, sizeof(value), "%s %s", stateName(controller.outputs().state),
                          controller.outputs().rcFresh ? "RC ok" : "RC old"); break;
    case 1: numeric = draft_.ropeLengthMm / 1000U; break;
    case 2: numeric = draft_.fastPermille / 10U; break;
    case 3: numeric = draft_.fastMaxUs; break;
    case 4: numeric = draft_.slowUs; break;
    case 5: numeric = draft_.warningPermille / 10U; break;
    case 6: numeric = draft_.rampUpMs; break;
    case 7: numeric = draft_.rampDownMs; break;
    case 8: numeric = draft_.maxRunMs / 1000U; break;
    case 9: numeric = draft_.rcTimeoutMs; break;
    case 10: numeric = static_cast<uint32_t>(draft_.stallMilliAmp); break;
    case 11: numeric = draft_.stallNoPulseMs; break;
    case 12: numeric = draft_.stallMaskMs; break;
    case 13: numeric = draft_.stallEnabled; break;
    case 14: numeric = static_cast<uint32_t>(draft_.speedChannel) + 1U; break;
    case 15: numeric = static_cast<uint32_t>(draft_.triggerChannel) + 1U; break;
    case 16: numeric = static_cast<uint32_t>(draft_.lightChannel) + 1U; break;
    case 17: numeric = static_cast<uint32_t>(draft_.powerChannel) + 1U; break;
    case 18: numeric = draft_.armMs; break;
    case 19: numeric = draft_.neutralMs; break;
    case 23: numeric = controller.settings().learnedPulses; break;
    default: std::snprintf(value, sizeof(value), "%s", message_); break;
  }
  if (page_ > 0 && (page_ < 20 || page_ == 23)) {
    std::snprintf(value, sizeof(value), "%lu", static_cast<unsigned long>(numeric));
  }
  std::memset(text, ' ', 32);
  const std::size_t title = std::strlen(Labels[page_]);
  std::memcpy(text, Labels[page_], title < 16 ? title : 16);
  const std::size_t length = std::strlen(value);
  std::memcpy(text + 16, value, length < 16 ? length : 16);
}
}  // namespace winch
