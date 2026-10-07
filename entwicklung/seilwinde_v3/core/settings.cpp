#include "settings.hpp"

namespace winch {
Settings Settings::defaults() {
  Settings s = {};
  s.ropeLengthMm = 0;  // Unknown; never advertise a fabricated metre value.
  s.learnedPulses = 0;
  s.fastPermille = 870;
  s.warningPermille = 200;
  s.stopUs = 1000;
  s.fastMaxUs = 2000;
  s.slowUs = 1280;  // V2 starting point, R4/ESC bench acceptance is still required.
  s.maxRunMs = 60000;
  s.rcTimeoutMs = 100;  // Short glitches retain last good command, then latch.
  s.neutralMs = 250;
  s.armMs = 3000;
  s.powerSettleMs = 500;
  s.rampUpMs = 500;
  s.rampDownMs = 200;
  s.stallMaskMs = 400;
  s.stallNoPulseMs = 200;
  s.stallMilliAmp = 0;  // No unmeasured threshold is enabled.
  s.learnMinMs = 2000;
  s.learnMaxMs = 120000;
  s.learnMinPulses = 20;
  s.learnMinOldPermille = 700;
  s.learnMaxOldPermille = 1500;
  s.learnAlphaPermille = 200;
  s.warningLightUs = 0;  // No invented uniLIGHT command.
  s.speedChannel = 0;  // Zero-based channels, provisional until mapped on JETI.
  s.triggerChannel = 1;
  s.lightChannel = 2;
  s.powerChannel = 3;
  s.modeStableFrames = 3;
  return s;
}

bool validSettings(const Settings& s) {
  if (s.ropeLengthMm > 1000000U || s.learnedPulses > 1000000U ||
      (s.learnedPulses != 0U && s.learnedPulses < s.learnMinPulses) ||
      s.fastPermille < 100U || s.fastPermille > 990U ||
      s.warningPermille > 1000U || s.stopUs != 1000U ||
      s.fastMaxUs < 1100U || s.fastMaxUs > 2000U ||
      s.slowUs < s.stopUs || s.slowUs > s.fastMaxUs) {
    return false;
  }
  if (s.maxRunMs < 1000U || s.maxRunMs > 120000U ||
      s.rcTimeoutMs < 30U || s.rcTimeoutMs > 500U ||
      s.neutralMs < 50U || s.neutralMs > 5000U ||
      s.armMs < 1000U || s.armMs > 10000U ||
      s.powerSettleMs < 100U || s.powerSettleMs > 5000U ||
      s.rampUpMs > 10000U || s.rampDownMs > 10000U ||
      s.stallMaskMs > 5000U || s.stallNoPulseMs < 50U ||
      s.stallNoPulseMs > 5000U || s.stallMilliAmp < 0 ||
      s.stallMilliAmp > 100000 || s.learnMinMs < 1000U ||
      s.learnMinMs > 30000U || s.learnMaxMs < s.learnMinMs ||
      s.learnMaxMs > 300000U || s.learnMinPulses < 20U ||
      s.learnMinPulses > 10000U || s.learnMinOldPermille < 500U ||
      s.learnMinOldPermille > 1000U || s.learnMaxOldPermille < 1000U ||
      s.learnMaxOldPermille > 2000U || s.learnAlphaPermille < 1U ||
      s.learnAlphaPermille > 1000U) {
    return false;
  }
  if (s.speedChannel >= 24U || s.triggerChannel >= 24U ||
      s.lightChannel >= 24U || s.powerChannel >= 24U ||
      s.speedChannel == s.triggerChannel || s.modeStableFrames < 1U ||
      s.modeStableFrames > 10U || s.currentCalibrated > 1U ||
      s.stallEnabled > 1U || s.lightEnabled > 1U || s.powerEnabled > 1U ||
      (s.stallEnabled && (!s.currentCalibrated || s.stallMilliAmp == 0)) ||
      (s.lightEnabled && (s.warningLightUs < 1000U || s.warningLightUs > 2000U)) ||
      (!s.lightEnabled && s.warningLightUs != 0U &&
       (s.warningLightUs < 1000U || s.warningLightUs > 2000U))) {
    return false;
  }
  return true;
}

bool equalSettings(const Settings& a, const Settings& b) {
#define WINCH_EQ(field) if (a.field != b.field) return false
  WINCH_EQ(ropeLengthMm); WINCH_EQ(learnedPulses);
  WINCH_EQ(fastPermille); WINCH_EQ(warningPermille);
  WINCH_EQ(stopUs); WINCH_EQ(fastMaxUs); WINCH_EQ(slowUs);
  WINCH_EQ(maxRunMs); WINCH_EQ(rcTimeoutMs); WINCH_EQ(neutralMs);
  WINCH_EQ(armMs); WINCH_EQ(powerSettleMs); WINCH_EQ(rampUpMs);
  WINCH_EQ(rampDownMs); WINCH_EQ(stallMaskMs); WINCH_EQ(stallNoPulseMs);
  WINCH_EQ(stallMilliAmp); WINCH_EQ(learnMinMs); WINCH_EQ(learnMaxMs);
  WINCH_EQ(learnMinPulses); WINCH_EQ(learnMinOldPermille);
  WINCH_EQ(learnMaxOldPermille); WINCH_EQ(learnAlphaPermille);
  WINCH_EQ(warningLightUs); WINCH_EQ(speedChannel); WINCH_EQ(triggerChannel);
  WINCH_EQ(lightChannel); WINCH_EQ(powerChannel); WINCH_EQ(modeStableFrames);
  WINCH_EQ(currentCalibrated); WINCH_EQ(stallEnabled);
  WINCH_EQ(lightEnabled); WINCH_EQ(powerEnabled);
#undef WINCH_EQ
  return true;
}
}  // namespace winch
