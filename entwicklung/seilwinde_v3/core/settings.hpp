#ifndef WINCH_V3_SETTINGS_HPP
#define WINCH_V3_SETTINGS_HPP

#include <stdint.h>

namespace winch {

// Physical calibration stays disabled until measured on the R4 rig.
struct Settings {
  uint32_t ropeLengthMm;
  uint32_t learnedPulses;
  uint16_t fastPermille;
  uint16_t warningPermille;
  uint16_t stopUs;
  uint16_t fastMaxUs;
  uint16_t slowUs;
  uint32_t maxRunMs;
  uint32_t rcTimeoutMs;
  uint32_t neutralMs;
  uint32_t armMs;
  uint32_t powerSettleMs;
  uint32_t rampUpMs;
  uint32_t rampDownMs;
  uint32_t stallMaskMs;
  uint32_t stallNoPulseMs;
  int32_t stallMilliAmp;
  uint32_t learnMinMs;
  uint32_t learnMaxMs;
  uint32_t learnMinPulses;
  uint16_t learnMinOldPermille;
  uint16_t learnMaxOldPermille;
  uint16_t learnAlphaPermille;
  uint16_t warningLightUs;
  uint8_t speedChannel;
  uint8_t triggerChannel;
  uint8_t lightChannel;
  uint8_t powerChannel;
  uint8_t modeStableFrames;
  uint8_t currentCalibrated;
  uint8_t stallEnabled;
  uint8_t lightEnabled;
  uint8_t powerEnabled;
  static Settings defaults();
};

bool validSettings(const Settings& settings);
bool equalSettings(const Settings& a, const Settings& b);

}  // namespace winch
#endif
