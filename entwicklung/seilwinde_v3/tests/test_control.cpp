#include "control.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace winch;
namespace {
unsigned checks = 0;
#define CHECK(condition) do { ++checks; if (!(condition)) { \
  std::cerr << __FILE__ << ':' << __LINE__ << ": " #condition "\n"; std::exit(1); \
} } while (false)

Settings testSettings() {
  Settings s = Settings::defaults();
  s.armMs = 1000;
  s.neutralMs = 50;
  s.modeStableFrames = 3;
  s.ropeLengthMm = 100000;
  s.rampUpMs = 500;
  return s;
}

struct Rig {
  Controller controller;
  Inputs in;
  explicit Rig(const Settings& s = testSettings(), bool end = false,
               uint32_t initialTime = 0, uint32_t initialHall = 0) : controller(s) {
    in.nowMs = initialTime;
    in.hallTotal = initialHall;
    in.endSwitch = end;
    in.rc.valid = true;
    in.rc.speedUs = 1800;
    controller.reset(initialTime, initialHall, end);
  }
  const Outputs& frame(Mode mode, uint32_t dt = 20, uint32_t hall = 0) {
    in.nowMs += dt;
    in.hallTotal += hall;
    ++in.rc.sequence;
    in.rc.receivedMs = in.nowMs;
    in.rc.mode = mode;
    return controller.tick(in);
  }
  const Outputs& tick(uint32_t dt, uint32_t hall = 0) {
    in.nowMs += dt;
    in.hallTotal += hall;
    return controller.tick(in);
  }
  const Outputs& mode(Mode requested) {
    frame(requested); frame(requested); return frame(requested);
  }
  void ready() {
    for (unsigned i = 0; i < 150U && controller.outputs().arm != ArmState::Ready; ++i)
      frame(Mode::Neutral);
    CHECK(controller.outputs().arm == ArmState::Ready);
  }
  void learn(uint32_t pulses = 100) {
    ready();
    CHECK(mode(Mode::Learn).state == State::Learning);
    CHECK(controller.outputs().escUs == 1000U);
    in.endSwitch = false;
    for (unsigned i = 0; i < 100U; ++i) frame(Mode::Learn, 20, i < pulses ? 1U : 0U);
    if (pulses > 100U) frame(Mode::Learn, 20, pulses - 100U);
    CHECK(mode(Mode::Neutral).state == State::Idle);
    for (unsigned i = 0; i < 5U; ++i) frame(Mode::Neutral);
  }
};

void bootAndEdges() {
  for (Mode initial : {Mode::Run, Mode::Learn}) {
    Rig r;
    for (unsigned i = 0; i < 100U; ++i) {
      const Outputs& o = r.frame(initial);
      CHECK(o.state == State::Idle);
      CHECK(o.escUs == 1000U);
    }
    CHECK(r.controller.outputs().arm == ArmState::WaitNeutral);
    r.ready();
    CHECK(r.mode(Mode::Run).state == State::RunSlow);  // Unknown position.
    r.frame(Mode::Run, 100);
    CHECK(r.controller.outputs().escUs > 1000U);
    CHECK(r.mode(Mode::Neutral).state == State::RunSlow);  // Autonomous retrieval.
    r.in.endSwitch = true;
    const Outputs& ended = r.tick(1);
    CHECK(ended.state == State::Idle);
    CHECK(ended.reason == StopReason::EndSwitch);
    CHECK(ended.escUs == 1000U);
    r.in.endSwitch = false;
    r.ready();
    CHECK(r.mode(Mode::Run).state == State::Idle);  // End latch needs explicit Learn.
  }
  Rig repeated;
  repeated.in.rc.mode = Mode::Neutral;
  repeated.in.rc.receivedMs = 0;
  for (unsigned i = 0; i < 100U; ++i) repeated.tick(20);
  CHECK(repeated.controller.outputs().arm != ArmState::Ready);  // Same frame is no new sample.
}

void learningAndRestway() {
  Rig r(testSettings(), true);
  r.learn();
  CHECK(r.controller.outputs().learnAccepted);
  CHECK(r.controller.settings().learnedPulses == 100U);
  CHECK(r.controller.settingsDirty());
  CHECK(r.controller.outputs().remainingPulses == 100U);
  CHECK(r.controller.outputs().estimatedRemainingMm == 100000U);
  CHECK(r.controller.canSave());
  r.controller.acknowledgeSaved();
  CHECK(!r.controller.settingsDirty());
  CHECK(r.mode(Mode::Run).state == State::RunFast);
  CHECK(!r.controller.canSave());
  Settings changed = r.controller.settings();
  changed.fastPermille = 800;
  CHECK(!r.controller.applySettings(changed));
  r.frame(Mode::Run, 20, 70);
  CHECK(r.controller.outputs().remainingPulses == 30U);
  CHECK(r.controller.outputs().state == State::RunFast);
  CHECK(r.mode(Mode::Learn).state == State::Learning);
  r.frame(Mode::Learn, 20, 2);
  CHECK(r.mode(Mode::Neutral).state == State::Idle);
  CHECK(!r.controller.outputs().learnAccepted);
  CHECK(r.controller.settings().learnedPulses == 100U);
  CHECK(!r.controller.settingsDirty());
  CHECK(r.controller.outputs().remainingPulses == 32U);
  for (unsigned i = 0; i < 5U; ++i) r.frame(Mode::Neutral);
  CHECK(r.mode(Mode::Run).state == State::RunFast);
  r.frame(Mode::Run, 20, 19);
  CHECK(r.controller.outputs().state == State::RunSlow);
  CHECK(r.controller.outputs().remainingPulses == 13U);
  r.in.endSwitch = true;
  CHECK(r.tick(1).escUs == 1000U);
  CHECK(r.controller.outputs().positionKnown);
  CHECK(r.controller.outputs().remainingPulses == 0U);
  CHECK(r.controller.outputs().reason == StopReason::EndSwitch);

  Settings learned = testSettings();
  learned.learnedPulses = 100;
  for (uint32_t pulses : {0U, 16U, 60U, 151U}) {
    Rig invalid(learned, true);
    invalid.learn(pulses);
    CHECK(!invalid.controller.outputs().learnAccepted);
    CHECK(invalid.controller.settings().learnedPulses == 100U);
  }
  Rig tooShort(learned, true);
  tooShort.ready(); tooShort.mode(Mode::Learn); tooShort.in.endSwitch = false;
  tooShort.frame(Mode::Learn, 20, 100);
  tooShort.mode(Mode::Neutral);
  CHECK(!tooShort.controller.outputs().learnAccepted);
  CHECK(tooShort.controller.settings().learnedPulses == 100U);

  Rig noReference(learned, false);
  noReference.learn();
  CHECK(!noReference.controller.outputs().learnAccepted);
  CHECK(!noReference.controller.outputs().positionKnown);
  CHECK(noReference.mode(Mode::Run).state == State::RunSlow);
  Rig reboot(learned, false);
  reboot.ready();
  CHECK(reboot.mode(Mode::Run).state == State::RunSlow);
  CHECK(!reboot.controller.outputs().positionKnown);
  Rig unclassified(learned, true);
  unclassified.in.endSwitch = false;
  unclassified.frame(Mode::Neutral, 20, 1);
  CHECK(!unclassified.controller.outputs().positionKnown);
}

void endStopsAndRamps() {
  for (bool fast : {false, true}) {
    Rig r(testSettings(), fast);
    if (fast) r.learn(); else r.ready();
    r.mode(Mode::Run);
    CHECK(r.controller.outputs().state == (fast ? State::RunFast : State::RunSlow));
    r.frame(Mode::Run, 10);
    CHECK(r.controller.outputs().escUs > 1000U);
    CHECK(r.controller.outputs().escUs < 1800U);
    r.in.endSwitch = true;
    CHECK(r.tick(0).escUs == 1000U);  // Same supplied time: no ramp delay to stop.
    CHECK(r.controller.outputs().reason == StopReason::EndSwitch);
  }
  Rig passive(testSettings(), true);
  passive.ready(); passive.mode(Mode::Learn);
  for (unsigned i = 0; i < 1000U; ++i) {
    passive.in.endSwitch = (i % 2U) != 0U;
    CHECK(passive.frame(Mode::Learn, 1, 1).escUs == 1000U);
  }
}

void rcLossAndRestart() {
  Rig r;
  r.ready(); r.mode(Mode::Run);
  const uint32_t acceptedAt = r.in.nowMs;
  r.in.rc.valid = false;
  ++r.in.rc.sequence;
  r.in.rc.receivedMs += 20;
  CHECK(r.tick(20).state == State::RunSlow);  // CRC/missing-frame glitch tolerated.
  CHECK(r.tick(59).state == State::RunSlow);
  CHECK(r.in.nowMs - acceptedAt == 79U);
  CHECK(r.tick(21).state == State::Fault);
  CHECK(r.controller.outputs().reason == StopReason::RcLost);
  CHECK(r.controller.outputs().escUs == 1000U);
  r.in.rc.valid = true;
  for (unsigned i = 0; i < 100U; ++i)
    CHECK(r.frame(Mode::Run).state == State::Fault);
  for (unsigned i = 0; i < 10U; ++i) r.frame(Mode::Neutral);
  CHECK(r.mode(Mode::Learn).state == State::Learning);
  CHECK(r.mode(Mode::Run).state == State::Idle);  // Leave Learn with RUN cannot start/save.
  CHECK(!r.controller.outputs().learnAccepted);
  r.ready();
  CHECK(r.mode(Mode::Run).state == State::RunSlow);

  Rig hold;
  hold.ready(); hold.mode(Mode::Run);
  hold.in.rc.hold = true;
  for (unsigned i = 0; i < 5U; ++i) hold.frame(Mode::Run);
  CHECK(hold.controller.outputs().state == State::Fault);
  CHECK(hold.controller.outputs().reason == StopReason::RcLost);
  Rig telemetryOnly;
  telemetryOnly.ready(); telemetryOnly.mode(Mode::Run);
  for (unsigned i = 0; i < 100U; ++i) {
    // A telemetry request is not a channel frame: no acceptedRc timestamp update.
    telemetryOnly.in.rc.valid = false;
    telemetryOnly.frame(Mode::Neutral, 1);
  }
  CHECK(telemetryOnly.controller.outputs().state == State::Fault);

  Rig stale;
  stale.in.rc.valid = true;
  stale.in.nowMs = 1000;
  stale.in.rc.receivedMs = 0;
  CHECK(!stale.controller.tick(stale.in).rcFresh);
}

void stallAndRuntime() {
  Settings s = testSettings();
  s.stallEnabled = 1; s.currentCalibrated = 1; s.stallMilliAmp = 5000;
  Rig stall(s);
  stall.in.currentValid = true; stall.in.currentMilliAmp = 7000;
  stall.ready(); stall.mode(Mode::Run);
  for (unsigned i = 0; i < 19U; ++i) CHECK(stall.frame(Mode::Run).state == State::RunSlow);
  CHECK(stall.frame(Mode::Run).state == State::Fault);  // 400 ms start mask.
  CHECK(stall.controller.outputs().reason == StopReason::Stall);
  CHECK(stall.controller.outputs().escUs == 1000U);
  Rig turning(s);
  turning.in.currentValid = true;
  turning.in.currentMilliAmp = std::numeric_limits<int32_t>::min();
  turning.ready(); turning.mode(Mode::Run);
  for (unsigned i = 0; i < 100U; ++i)
    CHECK(turning.frame(Mode::Run, 20, 1).state == State::RunSlow);
  Rig low(s);
  low.in.currentValid = true; low.in.currentMilliAmp = 4999;
  low.ready(); low.mode(Mode::Run);
  for (unsigned i = 0; i < 100U; ++i) CHECK(low.frame(Mode::Run).state == State::RunSlow);
  low.in.currentValid = false;
  CHECK(low.tick(0).state == State::Fault);
  CHECK(low.controller.outputs().reason == StopReason::CurrentInvalid);
  CHECK(low.controller.outputs().escUs == 1000U);
  Rig uncalibrated;
  uncalibrated.ready(); uncalibrated.mode(Mode::Run);
  uncalibrated.in.currentValid = true; uncalibrated.in.currentMilliAmp = 100000;
  for (unsigned i = 0; i < 100U; ++i)
    CHECK(uncalibrated.frame(Mode::Run).state == State::RunSlow);
  s.maxRunMs = 1000; s.stallEnabled = 0;
  Rig runtime(s);
  runtime.ready(); runtime.mode(Mode::Run);
  for (unsigned i = 0; i < 49U; ++i) CHECK(runtime.frame(Mode::Run).state == State::RunSlow);
  CHECK(runtime.frame(Mode::Run).state == State::Fault);
  CHECK(runtime.controller.outputs().reason == StopReason::MaxRuntime);
  Rig runtimeWrap(s, false, 0xfffff998U);
  runtimeWrap.ready(); runtimeWrap.mode(Mode::Run);
  for (unsigned i = 0; i < 49U; ++i) CHECK(runtimeWrap.frame(Mode::Run).state == State::RunSlow);
  CHECK(runtimeWrap.frame(Mode::Run).state == State::Fault);
  CHECK(runtimeWrap.controller.outputs().reason == StopReason::MaxRuntime);
  CHECK(runtimeWrap.in.nowMs < 1000U);
}

void overflowAndSettings() {
  Rig wrap(testSettings(), true, 0xfffffc00U, 0xfffffff0U);
  wrap.learn();
  CHECK(wrap.controller.outputs().learnAccepted);
  CHECK(wrap.in.hallTotal == 84U);
  CHECK(wrap.controller.outputs().remainingPulses == 100U);
  CHECK(wrap.mode(Mode::Run).state == State::RunFast);
  wrap.frame(Mode::Run, 20, 10);
  CHECK(wrap.controller.outputs().remainingPulses == 90U);
  wrap.frame(Mode::Run, 20, 10001);
  CHECK(wrap.controller.outputs().state == State::Fault);
  CHECK(wrap.controller.outputs().reason == StopReason::CounterInvalid);
  CHECK(wrap.controller.outputs().escUs == 1000U);
  Settings s = testSettings();
  CHECK(validSettings(s));
  s.fastPermille = 1000; CHECK(!validSettings(s));
  Controller bad(s); CHECK(bad.settings().fastPermille == 870U);
  s = testSettings(); s.stallEnabled = 1; CHECK(!validSettings(s));
  s = testSettings(); s.rcTimeoutMs = 0xffffffffU; CHECK(!validSettings(s));
  s = testSettings(); s.speedChannel = s.triggerChannel; CHECK(!validSettings(s));
  s = testSettings(); s.powerEnabled = 2; CHECK(!validSettings(s));
  s = testSettings(); s.lightEnabled = 1; CHECK(!validSettings(s));
  Rig stopped;
  s = testSettings(); s.fastPermille = 800;
  CHECK(stopped.controller.applySettings(s));
  CHECK(stopped.controller.settingsDirty());
  CHECK(stopped.controller.settings().fastPermille == 800U);
  Rig reconfigured;
  reconfigured.ready();
  const uint32_t savedSequence = reconfigured.in.rc.sequence;
  s = reconfigured.controller.settings(); s.triggerChannel = 5;
  CHECK(reconfigured.controller.applySettings(s));
  CHECK(!reconfigured.tick(0).rcFresh);
  CHECK(reconfigured.in.rc.sequence == savedSequence);
  CHECK(reconfigured.mode(Mode::Run).state == State::Idle);
  reconfigured.ready();
  CHECK(reconfigured.mode(Mode::Run).state == State::RunSlow);
  s = testSettings(); s.ropeLengthMm = 1000000; s.learnedPulses = 20;
  Rig metricLimit(s, true);
  metricLimit.ready(); metricLimit.mode(Mode::Learn); metricLimit.in.endSwitch = false;
  for (unsigned i = 0; i < 99U; ++i) metricLimit.frame(Mode::Learn, 20, 10000);
  metricLimit.frame(Mode::Learn, 20, 9999);
  metricLimit.mode(Mode::Neutral);
  CHECK(metricLimit.controller.outputs().estimatedRemainingMm == UINT32_MAX);
  CHECK(!metricLimit.controller.outputs().learnAccepted);
  CHECK(metricLimit.controller.settings().learnedPulses == 20U);
}

void lightAndPower() {
  Settings s = testSettings(); s.lightEnabled = 1; s.warningLightUs = 1900;
  Rig light(s, true); light.in.rc.lightUs = 1400;
  light.learn();
  CHECK(!light.controller.outputs().lightWarning);
  CHECK(light.controller.outputs().lightUs == 1400U);
  light.mode(Mode::Run); light.frame(Mode::Run, 20, 80);
  CHECK(light.controller.outputs().lightWarning);
  CHECK(light.controller.outputs().lightUs == 1900U);
  light.in.endSwitch = true; light.tick(0);
  CHECK(light.controller.outputs().escUs == 1000U);
  CHECK(light.controller.outputs().lightUs == 1900U);
  light.tick(100);
  CHECK(light.controller.outputs().lightUs == 1000U);  // Stale command => light safe default.
  Rig disabled;
  disabled.in.rc.lightUs = 1900;
  disabled.in.powerDriverHealthy = false;
  disabled.ready(); disabled.mode(Mode::Run);
  CHECK(disabled.controller.outputs().state == State::RunSlow);
  CHECK(disabled.controller.outputs().lightUs == 1000U);
  CHECK(!disabled.controller.outputs().powerEnable);

  s = testSettings(); s.powerEnabled = 1;
  Rig power(s);
  CHECK(!power.controller.outputs().powerEnable);
  for (unsigned i = 0; i < 100U; ++i) {
    power.frame(Mode::Run);
    CHECK(!power.controller.outputs().powerEnable);
    CHECK(power.controller.outputs().escUs == 1000U);
  }
  power.ready();
  CHECK(power.controller.outputs().powerEnable);
  power.mode(Mode::Run); power.frame(Mode::Run, 50);
  CHECK(power.controller.outputs().escUs > 1000U);
  power.in.rc.powerRequested = false;
  power.frame(Mode::Run);
  CHECK(power.controller.outputs().state == State::Fault);
  CHECK(!power.controller.outputs().powerEnable);
  CHECK(power.controller.outputs().escUs == 1000U);
  power.in.rc.powerRequested = true;
  for (unsigned i = 0; i < 100U; ++i) power.frame(Mode::Run);
  CHECK(power.controller.outputs().state == State::Fault);
  CHECK(!power.controller.outputs().powerEnable);
  for (unsigned i = 0; i < 10U; ++i) power.frame(Mode::Neutral);
  power.mode(Mode::Learn); power.mode(Mode::Neutral);
  power.ready();
  CHECK(power.controller.outputs().powerEnable);
  CHECK(power.controller.outputs().state == State::Idle);
  power.in.powerDriverHealthy = false;
  CHECK(power.tick(0).state == State::Fault);
  CHECK(power.controller.outputs().reason == StopReason::PowerFault);
  CHECK(!power.controller.outputs().powerEnable);
}

void deterministicSafetySequences() {
  uint32_t seed = 0x3a74139fU;
  for (bool withPower : {false, true}) {
    Settings s = testSettings(); s.powerEnabled = withPower ? 1U : 0U;
    Rig r(s, true, 0xffffe000U);
    for (unsigned i = 0; i < 6000U; ++i) {
      seed ^= seed << 13U; seed ^= seed >> 17U; seed ^= seed << 5U;
      const uint32_t choice = seed;
      r.in.endSwitch = choice % 19U == 0U;
      r.in.powerDriverHealthy = choice % 41U != 0U;
      r.in.rc.powerRequested = choice % 13U != 0U;
      r.in.rc.valid = choice % 7U != 0U;
      r.in.rc.hold = choice % 17U == 0U;
      r.in.rc.lightUs = static_cast<uint16_t>(900U + choice % 1200U);
      r.in.rc.speedUs = static_cast<uint16_t>(1000U + choice % 1001U);
      const Mode mode = choice % 3U == 0U ? Mode::Run :
          (choice % 3U == 1U ? Mode::Neutral : Mode::Learn);
      const Outputs& out = r.frame(mode, 1U + choice % 40U, choice % 4U);
      CHECK(out.escUs >= 1000U && out.escUs <= 2000U);
      CHECK(!r.in.endSwitch || out.escUs == 1000U);
      CHECK(out.state != State::Fault || (out.escUs == 1000U && !out.powerEnable));
      CHECK(out.state == State::RunFast || out.state == State::RunSlow || out.escUs == 1000U);
      CHECK(out.remainingPulses <= 1000000U);
      CHECK(withPower || !out.powerEnable);
    }
  }
}
}  // namespace

int main() {
  bootAndEdges(); learningAndRestway(); endStopsAndRamps(); rcLossAndRestart();
  stallAndRuntime(); overflowAndSettings(); lightAndPower(); deterministicSafetySequences();
  std::cout << "PASS control: " << checks << " checks; safety, learning, rollover, power, light\n";
  return 0;
}
