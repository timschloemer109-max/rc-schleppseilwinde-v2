#include "control.hpp"

#include <limits.h>

namespace winch {
namespace {
constexpr uint32_t MAX_PULSES = 1000000U;
constexpr uint32_t MAX_TICK_PULSES = 10000U;
uint32_t elapsed(uint32_t now, uint32_t then) { return now - then; }
uint32_t addLimited(uint32_t a, uint32_t b) {
  return b > MAX_PULSES - a ? MAX_PULSES : a + b;
}
uint32_t subtractLimited(uint32_t a, uint32_t b) { return b > a ? 0U : a - b; }
bool validMode(Mode mode) {
  return mode == Mode::Neutral || mode == Mode::Learn || mode == Mode::Run;
}
}  // namespace

Controller::Controller(const Settings& settings)
    : settings_(validSettings(settings) ? settings : Settings::defaults()) {
  reset(0, 0, false);
}

void Controller::reset(uint32_t nowMs, uint32_t hallTotal, bool endSwitch) {
  output_ = Outputs();
  output_.escUs = settings_.stopUs;
  output_.arm = settings_.powerEnabled ? ArmState::PowerOff : ArmState::Arming;
  output_.positionKnown = endSwitch;
  output_.reason = endSwitch ? StopReason::EndSwitch : StopReason::None;
  acceptedRc_ = RcFrame();
  haveRc_ = false;
  haveSequence_ = false;
  seenSequence_ = 0;
  stableMode_ = Mode::Neutral;
  candidateMode_ = Mode::Neutral;
  candidateFrames_ = 0;
  modeReady_ = false;
  neutralTiming_ = false;
  neutralReleased_ = false;
  endLatched_ = endSwitch;
  settingsDirty_ = false;
  lastEndSwitch_ = endSwitch;
  lastHallTotal_ = hallTotal;
  lastPulseMs_ = nowMs;
  armSinceMs_ = nowMs;
  runSinceMs_ = nowMs;
  learnSinceMs_ = nowMs;
  learnStartPosition_ = 0;
  learnStartKnown_ = false;
  learnReference_ = false;
  lastRampMs_ = nowMs;
  rampRemainder_ = 0;
  rampTarget_ = settings_.stopUs;
  updateEstimates();
}

bool Controller::running() const {
  return output_.state == State::RunFast || output_.state == State::RunSlow;
}
void Controller::setState(State state) {
  if (output_.state != state) { output_.state = state; ++output_.events; }
}
void Controller::setArm(ArmState arm) {
  if (output_.arm != arm) { output_.arm = arm; ++output_.events; }
}
void Controller::invalidateRelease() {
  neutralReleased_ = false;
  neutralTiming_ = false;
}
void Controller::stop(StopReason reason, bool fault) {
  if (output_.reason != reason) { output_.reason = reason; ++output_.events; }
  setState(fault ? State::Fault : State::Idle);
  output_.escUs = settings_.stopUs;  // A safety stop never uses the ramp.
  rampRemainder_ = 0;
  invalidateRelease();
  if (reason == StopReason::EndSwitch) {
    output_.positionKnown = true;
    output_.remainingPulses = 0;
    endLatched_ = true;
  }
  if (fault) {
    output_.powerEnable = false;
    setArm(settings_.powerEnabled ? ArmState::PowerOff : ArmState::WaitNeutral);
  }
}

void Controller::beginLearn(uint32_t nowMs) {
  output_.reason = StopReason::ResetLearn;
  setState(State::Learning);
  output_.escUs = settings_.stopUs;
  output_.learnPulses = 0;
  output_.learnAccepted = false;
  learnSinceMs_ = nowMs;
  learnStartPosition_ = output_.remainingPulses;
  learnStartKnown_ = output_.positionKnown;
  learnReference_ = learnStartKnown_ && learnStartPosition_ == 0U;
  endLatched_ = false;
  invalidateRelease();
}

void Controller::finishLearn(uint32_t nowMs, bool commit) {
  const uint32_t pulses = output_.learnPulses;
  const uint32_t duration = elapsed(nowMs, learnSinceMs_);
  const uint32_t old = settings_.learnedPulses;
  const bool relativeOk = old == 0U ||
      (static_cast<uint64_t>(pulses) * 1000U >=
           static_cast<uint64_t>(old) * settings_.learnMinOldPermille &&
       static_cast<uint64_t>(pulses) * 1000U <=
           static_cast<uint64_t>(old) * settings_.learnMaxOldPermille);
  const bool accepted = commit && learnReference_ && relativeOk &&
      pulses >= settings_.learnMinPulses && pulses < MAX_PULSES &&
      duration >= settings_.learnMinMs && duration <= settings_.learnMaxMs;
  if (accepted) {
    settings_.learnedPulses = old == 0U ? pulses : static_cast<uint32_t>(
        (static_cast<uint64_t>(old) * (1000U - settings_.learnAlphaPermille) +
         static_cast<uint64_t>(pulses) * settings_.learnAlphaPermille + 500U) / 1000U);
    settingsDirty_ = true;  // Explicit Save is still required; no flash write here.
    output_.positionKnown = true;
    output_.remainingPulses = pulses;
  } else if (learnStartKnown_) {
    output_.positionKnown = true;
    output_.remainingPulses = addLimited(learnStartPosition_, pulses);
  } else {
    output_.positionKnown = false;
  }
  output_.learnAccepted = accepted;
  output_.reason = accepted ? StopReason::None : StopReason::LearnRejected;
  setState(State::Idle);
  invalidateRelease();
}

void Controller::beginRun(uint32_t nowMs) {
  const uint32_t slowThreshold = static_cast<uint32_t>(
      (static_cast<uint64_t>(settings_.learnedPulses) *
       (1000U - settings_.fastPermille) + 999U) / 1000U);
  // Unknown position after reboot/unclassified manual motion starts conservatively.
  setState(output_.positionKnown && settings_.learnedPulses != 0U &&
           output_.remainingPulses > slowThreshold ? State::RunFast : State::RunSlow);
  output_.reason = StopReason::None;
  output_.runPulses = 0;
  runSinceMs_ = nowMs;
  lastPulseMs_ = nowMs;
  lastRampMs_ = nowMs;
  rampRemainder_ = 0;
  rampTarget_ = settings_.stopUs;
  invalidateRelease();
}

bool Controller::canSave() const {
  return !running() && output_.state != State::Learning &&
         output_.escUs == settings_.stopUs;
}
bool Controller::applySettings(const Settings& settings) {
  if (!canSave() || !validSettings(settings)) return false;
  if (equalSettings(settings_, settings)) return true;
  const bool changedDrive = settings_.powerEnabled != settings.powerEnabled ||
      settings_.armMs != settings.armMs || settings_.powerSettleMs != settings.powerSettleMs;
  if (settings_.learnedPulses != settings.learnedPulses && !lastEndSwitch_)
    output_.positionKnown = false;
  settings_ = settings;
  settingsDirty_ = true;
  invalidateRelease();
  // A channel-map/configuration change must not reuse the old neutral sample.
  // Keep seenSequence_ so even a re-delivered copy needs a new channel frame.
  haveRc_ = false;
  modeReady_ = false;
  candidateFrames_ = 0;
  stableMode_ = Mode::Neutral;
  candidateMode_ = Mode::Neutral;
  if (changedDrive) {
    output_.powerEnable = false;
    setArm(settings_.powerEnabled ? ArmState::PowerOff : ArmState::Arming);
    armSinceMs_ = lastRampMs_;
  }
  updateEstimates();
  return true;
}

void Controller::updateEstimates() {
  output_.endLatched = endLatched_;
  output_.lengthLearned = settings_.learnedPulses != 0U;
  output_.remainingPermille = 0;
  output_.estimatedRemainingMm = 0;
  if (!output_.positionKnown || !output_.lengthLearned) return;
  uint64_t fraction = static_cast<uint64_t>(output_.remainingPulses) * 1000U /
                      settings_.learnedPulses;
  if (fraction > 1000U) fraction = 1000U;
  output_.remainingPermille = static_cast<uint16_t>(fraction);
  const uint64_t millimetres = static_cast<uint64_t>(output_.remainingPulses) *
      settings_.ropeLengthMm / settings_.learnedPulses;
  output_.estimatedRemainingMm = static_cast<uint32_t>(
      millimetres > UINT32_MAX ? UINT32_MAX : millimetres);
}

void Controller::updateRamp(uint32_t nowMs, uint16_t target) {
  const uint32_t dt = elapsed(nowMs, lastRampMs_);
  lastRampMs_ = nowMs;
  if (rampTarget_ != target) { rampTarget_ = target; rampRemainder_ = 0; }
  const uint32_t duration = target > output_.escUs ? settings_.rampUpMs : settings_.rampDownMs;
  if (duration == 0U || dt >= duration) { output_.escUs = target; rampRemainder_ = 0; return; }
  const uint64_t advance = static_cast<uint64_t>(dt) *
      (settings_.fastMaxUs - settings_.stopUs) + rampRemainder_;
  const uint32_t step = static_cast<uint32_t>(advance / duration);
  rampRemainder_ = advance % duration;
  if (target > output_.escUs) {
    output_.escUs = static_cast<uint16_t>(step >= static_cast<uint32_t>(target - output_.escUs) ?
        target : static_cast<uint32_t>(output_.escUs) + step);
  } else {
    output_.escUs = static_cast<uint16_t>(step >= static_cast<uint32_t>(output_.escUs - target) ?
        target : static_cast<uint32_t>(output_.escUs) - step);
  }
}

const Outputs& Controller::tick(const Inputs& in) {
  const uint32_t now = in.nowMs;
  lastEndSwitch_ = in.endSwitch;
  // Simultaneous Hall corruption/RC loss cannot precede an asserted end stop.
  if (in.endSwitch && running()) stop(StopReason::EndSwitch, false);
  const uint32_t delta = in.hallTotal - lastHallTotal_;
  lastHallTotal_ = in.hallTotal;
  const bool counterBad = delta > MAX_TICK_PULSES;
  if (counterBad) {
    output_.positionKnown = false;
    if (running() || output_.state == State::Learning) stop(StopReason::CounterInvalid, true);
  } else if (delta != 0U) {
    lastPulseMs_ = now;
    if (running()) {
      if (delta > MAX_PULSES - output_.runPulses) stop(StopReason::CounterInvalid, true);
      else output_.runPulses += delta;
      if (output_.positionKnown)
        output_.remainingPulses = subtractLimited(output_.remainingPulses, delta);
    } else if (output_.state == State::Learning) {
      if (delta >= MAX_PULSES - output_.learnPulses) stop(StopReason::CounterInvalid, true);
      else output_.learnPulses += delta;
      if (learnStartKnown_)
        output_.remainingPulses = addLimited(learnStartPosition_, output_.learnPulses);
    } else {
      output_.positionKnown = false;  // A single Hall sensor cannot identify direction.
    }
  }

  const Mode previousMode = stableMode_;
  bool newMode = false;
  if (!haveSequence_ || in.rc.sequence != seenSequence_) {
    haveSequence_ = true;
    seenSequence_ = in.rc.sequence;
    if (in.rc.valid && !in.rc.hold && validMode(in.rc.mode) &&
        in.rc.speedUs >= settings_.stopUs && in.rc.speedUs <= 2000U &&
        elapsed(now, in.rc.receivedMs) < settings_.rcTimeoutMs) {
      acceptedRc_ = in.rc;
      haveRc_ = true;
      if (candidateFrames_ == 0U || candidateMode_ != in.rc.mode) {
        candidateMode_ = in.rc.mode;
        candidateFrames_ = 1;
      } else if (candidateFrames_ < settings_.modeStableFrames) {
        ++candidateFrames_;
      }
      if (candidateFrames_ >= settings_.modeStableFrames) {
        newMode = !modeReady_ || stableMode_ != candidateMode_;
        stableMode_ = candidateMode_;
        modeReady_ = true;
      }
    } else {
      candidateFrames_ = 0;
    }
  }
  output_.rcFresh = haveRc_ && elapsed(now, acceptedRc_.receivedMs) < settings_.rcTimeoutMs;
  const bool neutral = output_.rcFresh && modeReady_ && stableMode_ == Mode::Neutral &&
      candidateMode_ == Mode::Neutral && candidateFrames_ >= settings_.modeStableFrames;
  if (neutral) {
    if (!neutralTiming_) { neutralTiming_ = true; neutralSinceMs_ = now; }
    if (elapsed(now, neutralSinceMs_) >= settings_.neutralMs) neutralReleased_ = true;
  } else {
    neutralTiming_ = false;
  }

  // End switch is examined before any start or output ramp in every tick.
  if (in.endSwitch) {
    if (running()) stop(StopReason::EndSwitch, false);
    if (output_.state != State::Learning) {
      output_.positionKnown = true;
      output_.remainingPulses = 0;
      endLatched_ = true;
    }
  }
  if (!output_.rcFresh) {
    if (running() || output_.state == State::Learning) stop(StopReason::RcLost, true);
    invalidateRelease();
    if (output_.powerEnable) {
      output_.powerEnable = false;
      setArm(ArmState::PowerOff);
    } else if (output_.arm == ArmState::Ready) {
      setArm(settings_.powerEnabled ? ArmState::PowerOff : ArmState::WaitNeutral);
    }
  }
  if (settings_.powerEnabled && !in.powerDriverHealthy) {
    stop(StopReason::PowerFault, true);
  } else if (settings_.powerEnabled && output_.rcFresh && !acceptedRc_.powerRequested) {
    if (running() || output_.state == State::Learning) stop(StopReason::PowerRemoved, true);
    output_.powerEnable = false;
    setArm(ArmState::PowerOff);
    invalidateRelease();
  }

  if (output_.state != State::Fault) {
    if (settings_.powerEnabled && output_.arm == ArmState::PowerOff &&
        neutralReleased_ && acceptedRc_.powerRequested && in.powerDriverHealthy) {
      output_.powerEnable = true;
      armSinceMs_ = now;
      setArm(ArmState::Arming);
      invalidateRelease();
    }
    const uint32_t armDuration = settings_.armMs +
        (settings_.powerEnabled ? settings_.powerSettleMs : 0U);
    if (output_.arm == ArmState::Arming && elapsed(now, armSinceMs_) >= armDuration) {
      setArm(ArmState::WaitNeutral);
      invalidateRelease();  // Require neutral after the full stop-PWM arming hold.
    }
    if (output_.arm == ArmState::WaitNeutral && neutralReleased_) setArm(ArmState::Ready);
  }

  // A fresh LEARN edge is an explicit reset; it is never a powered movement.
  const bool learnEdge = newMode && stableMode_ == Mode::Learn && previousMode != Mode::Learn;
  const bool runEdge = newMode && stableMode_ == Mode::Run && previousMode != Mode::Run;
  if (learnEdge && (running() ||
      (neutralReleased_ && (output_.arm == ArmState::Ready || output_.state == State::Fault)))) {
    if (running()) stop(StopReason::ResetLearn, false);
    if (output_.state == State::Fault) {
      setState(State::Idle);
      armSinceMs_ = now;
      setArm(settings_.powerEnabled ? ArmState::PowerOff : ArmState::Arming);
    }
    beginLearn(now);
  } else if (output_.state == State::Learning && newMode && stableMode_ != Mode::Learn) {
    finishLearn(now, stableMode_ == Mode::Neutral);
  } else if (runEdge && output_.state == State::Idle && output_.arm == ArmState::Ready &&
             neutralReleased_ && !endLatched_ && !in.endSwitch && output_.rcFresh) {
    beginRun(now);
  }

  if (output_.state == State::Learning && elapsed(now, learnSinceMs_) > settings_.learnMaxMs)
    stop(StopReason::MaxRuntime, true);
  if (running()) {
    if (elapsed(now, runSinceMs_) >= settings_.maxRunMs) stop(StopReason::MaxRuntime, true);
    else if (settings_.stallEnabled && settings_.currentCalibrated && !in.currentValid)
      stop(StopReason::CurrentInvalid, true);
    else if (settings_.stallEnabled && settings_.currentCalibrated && in.currentValid &&
        elapsed(now, runSinceMs_) >= settings_.stallMaskMs &&
        elapsed(now, lastPulseMs_) >= settings_.stallNoPulseMs) {
      const int64_t signedCurrent = in.currentMilliAmp;
      const int64_t magnitude = signedCurrent < 0 ? -signedCurrent : signedCurrent;
      if (magnitude >= settings_.stallMilliAmp) stop(StopReason::Stall, true);
    }
  }
  if (output_.state == State::RunFast) {
    const uint32_t slowThreshold = static_cast<uint32_t>(
        (static_cast<uint64_t>(settings_.learnedPulses) *
         (1000U - settings_.fastPermille) + 999U) / 1000U);
    if (!output_.positionKnown || output_.remainingPulses <= slowThreshold)
      setState(State::RunSlow);
  }
  if (running() && output_.arm == ArmState::Ready && !in.endSwitch) {
    uint16_t target = settings_.slowUs;
    if (output_.state == State::RunFast)
      target = acceptedRc_.speedUs > settings_.fastMaxUs ? settings_.fastMaxUs : acceptedRc_.speedUs;
    updateRamp(now, target);
  } else {
    output_.escUs = settings_.stopUs;
    lastRampMs_ = now;
    rampRemainder_ = 0;
  }
  if (!settings_.powerEnabled) output_.powerEnable = false;
  updateEstimates();
  output_.lightWarning = settings_.lightEnabled && output_.positionKnown &&
      output_.lengthLearned && output_.remainingPermille <= settings_.warningPermille;
  output_.lightUs = settings_.stopUs;
  if (settings_.lightEnabled && output_.rcFresh) {
    if (output_.lightWarning) output_.lightUs = settings_.warningLightUs;
    else if (acceptedRc_.lightUs >= 1000U && acceptedRc_.lightUs <= 2000U)
      output_.lightUs = acceptedRc_.lightUs;
  }
  return output_;
}

const char* stateName(State state) {
  switch (state) {
    case State::Idle: return "idle";
    case State::Learning: return "learn";
    case State::RunFast: return "fast";
    case State::RunSlow: return "slow";
    case State::Fault: return "fault";
  }
  return "invalid";
}
const char* reasonName(StopReason reason) {
  switch (reason) {
    case StopReason::None: return "none";
    case StopReason::EndSwitch: return "end_switch";
    case StopReason::RcLost: return "rc_lost";
    case StopReason::MaxRuntime: return "max_runtime";
    case StopReason::Stall: return "stall";
    case StopReason::ResetLearn: return "reset_learn";
    case StopReason::PowerRemoved: return "power_removed";
    case StopReason::PowerFault: return "power_fault";
    case StopReason::CounterInvalid: return "counter_invalid";
    case StopReason::LearnRejected: return "learn_rejected";
    case StopReason::CurrentInvalid: return "current_invalid";
  }
  return "invalid";
}
}  // namespace winch
