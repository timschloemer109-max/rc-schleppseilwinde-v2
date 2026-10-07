#ifndef WINCH_V3_CONTROL_HPP
#define WINCH_V3_CONTROL_HPP

#include "settings.hpp"

namespace winch {

enum class Mode : uint8_t { Neutral, Learn, Run };
enum class State : uint8_t { Idle, Learning, RunFast, RunSlow, Fault };
enum class ArmState : uint8_t { PowerOff, Arming, WaitNeutral, Ready };
enum class StopReason : uint8_t {
  None, EndSwitch, RcLost, MaxRuntime, Stall, ResetLearn,
  PowerRemoved, PowerFault, CounterInvalid, LearnRejected, CurrentInvalid
};

struct RcFrame {
  bool valid = false;
  bool hold = false;
  uint32_t receivedMs = 0;
  uint32_t sequence = 0;
  Mode mode = Mode::Neutral;
  uint16_t speedUs = 1500;
  uint16_t lightUs = 1000;
  bool powerRequested = true;
};

struct Inputs {
  uint32_t nowMs = 0;
  uint32_t hallTotal = 0;  // Adapter takes an atomic ISR snapshot.
  bool endSwitch = false;
  RcFrame rc;
  bool currentValid = false;
  int32_t currentMilliAmp = 0;
  bool powerDriverHealthy = true;  // Relevant only with powerEnabled.
};

struct Outputs {
  State state = State::Idle;
  ArmState arm = ArmState::Arming;
  StopReason reason = StopReason::None;
  uint16_t escUs = 1000;
  bool powerEnable = false;
  uint16_t lightUs = 1000;
  bool lightWarning = false;
  bool endLatched = false;
  bool rcFresh = false;
  bool positionKnown = false;
  bool lengthLearned = false;
  uint32_t remainingPulses = 0;
  uint32_t estimatedRemainingMm = 0;
  uint16_t remainingPermille = 0;
  uint32_t runPulses = 0;
  uint32_t learnPulses = 0;
  bool learnAccepted = false;
  uint32_t events = 0;
};

class Controller {
 public:
  explicit Controller(const Settings& settings = Settings::defaults());
  void reset(uint32_t nowMs, uint32_t hallTotal, bool endSwitch);
  const Outputs& tick(const Inputs& inputs);
  const Outputs& outputs() const { return output_; }
  const Settings& settings() const { return settings_; }
  // Staging/saving is allowed only with stopped motor and outside learning.
  bool applySettings(const Settings& settings);
  bool canSave() const;
  bool settingsDirty() const { return settingsDirty_; }
  void acknowledgeSaved() { settingsDirty_ = false; }

 private:
  Settings settings_;
  Outputs output_;
  RcFrame acceptedRc_;
  bool haveRc_ = false;
  bool haveSequence_ = false;
  uint32_t seenSequence_ = 0;
  Mode stableMode_ = Mode::Neutral;
  Mode candidateMode_ = Mode::Neutral;
  uint8_t candidateFrames_ = 0;
  bool modeReady_ = false;
  bool neutralTiming_ = false;
  uint32_t neutralSinceMs_ = 0;
  bool neutralReleased_ = false;
  bool endLatched_ = false;
  bool settingsDirty_ = false;
  bool lastEndSwitch_ = false;
  uint32_t lastHallTotal_ = 0;
  uint32_t lastPulseMs_ = 0;
  uint32_t armSinceMs_ = 0;
  uint32_t runSinceMs_ = 0;
  uint32_t learnSinceMs_ = 0;
  uint32_t learnStartPosition_ = 0;
  bool learnStartKnown_ = false;
  bool learnReference_ = false;
  uint32_t lastRampMs_ = 0;
  uint64_t rampRemainder_ = 0;
  uint16_t rampTarget_ = 1000;

  bool running() const;
  void stop(StopReason reason, bool fault);
  void invalidateRelease();
  void setState(State state);
  void setArm(ArmState arm);
  void beginLearn(uint32_t nowMs);
  void finishLearn(uint32_t nowMs, bool commit);
  void beginRun(uint32_t nowMs);
  void updateEstimates();
  void updateRamp(uint32_t nowMs, uint16_t target);
};

const char* stateName(State state);
const char* reasonName(StopReason reason);

}  // namespace winch
#endif
