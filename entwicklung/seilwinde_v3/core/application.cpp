#include "application.hpp"
#include <limits>
namespace winch {
namespace {
constexpr uint16_t Manufacturer = 0xa400, Device = 0x0300;
const char* const Labels[] = {"Winch V3", "Rope remain", "Remain", "State", "Current", "Hall pulses",
  "Pulse rate", "Drum RPM", "ESC PWM", "Stop reason", "Validity bits", "RC fresh", "Power", "Light warn", "Bus errors", "Log drops"};
const char* const Units[] = {"", "m", "%", "", "A", "", "Hz", "rpm", "us", "", "", "", "", "", "", ""};
int32_t boundedValue(uint32_t v) { return static_cast<int32_t>(v > 2097151U ? 2097151U : v); }
}
uint32_t Application::busErrors() const {
  const exbus::Statistics& s = parser_.statistics();
  return s.crcErrors + s.formatErrors + s.timeouts + transportErrors_;
}
void Application::setTelemetry(int32_t currentMa, bool currentValid, uint32_t pulseRate, uint32_t hallTotal) {
  currentMa_ = currentMa; currentValid_ = currentValid; pulseRate_ = pulseRate; hallTotal_ = hallTotal;
}
void Application::receive(uint8_t byte, uint32_t receivedUs, uint32_t nowUs, uint32_t nowMs) {
  exbus::Frame frame;
  if (parser_.feed(byte, receivedUs, frame)) handle(frame, nowUs, nowMs);
}
void Application::handle(const exbus::Frame& frame, uint32_t nowUs, uint32_t nowMs) {
  const Settings& settings = control_.settings();
  if (frame.kind == exbus::Kind::Channels) {
    const std::size_t speed = settings.speedChannel;
    const std::size_t trigger = settings.triggerChannel;
    const std::size_t light = settings.lightChannel;
    const std::size_t power = settings.powerChannel;
    if (speed >= frame.channelCount || trigger >= frame.channelCount ||
        (settings.lightEnabled != 0 && light >= frame.channelCount) ||
        (settings.powerEnabled != 0 && power >= frame.channelCount)) return;
    // A valid packet CRC does not imply plausible servo data.
    for (std::size_t i = 0; i < frame.channelCount; ++i) {
      if (frame.channelsUs[i] < 900 || frame.channelsUs[i] > 2100) return;
    }
    const uint32_t ageUs = nowUs - frame.receivedUs;
    if (ageUs / 1000U >= settings.rcTimeoutMs) return;
    rc_.valid = true; rc_.hold = false;
    rc_.receivedMs = nowMs - ageUs / 1000U;
    ++rc_.sequence;
    rc_.speedUs = frame.channelsUs[speed]; triggerUs_ = frame.channelsUs[trigger];
    rc_.mode = triggerUs_ <= 1200 ? Mode::Learn : (triggerUs_ >= 1800 ? Mode::Run : Mode::Neutral);
    rc_.lightUs = light < frame.channelCount ? frame.channelsUs[light] : 1000;
    rc_.powerRequested = settings.powerEnabled == 0 || frame.channelsUs[power] >= 1800;
    return;
  }
  if (!frame.busReleased || reply_.length != 0 ||
      static_cast<uint32_t>(nowUs - frame.receivedUs) > 2000U) return;
  if (frame.kind == exbus::Kind::TelemetryRequest) {
    uint8_t ex[exbus::MaxExBytes]{};
    const std::size_t n = telemetry(ex, sizeof(ex));
    reply_.length = exbus::telemetryResponse(frame.packetId, ex, n, reply_.bytes, sizeof(reply_.bytes));
  } else if (frame.kind == exbus::Kind::JetiboxRequest) {
    menu_.buttons(frame.buttons, nowMs, control_);
    char text[32]; menu_.render(text, control_);
    reply_.length = exbus::jetiboxResponse(frame.packetId, text, reply_.bytes, sizeof(reply_.bytes));
  }
  reply_.requestedUs = frame.receivedUs;
}
std::size_t Application::telemetry(uint8_t* dest, std::size_t capacity) {
  // Repeated descriptor carousel avoids depending on when the sender connects.
  // Every second reply carries three signed numeric values; no shared encoder
  // is used as the sole oracle in the independently sourced fixture tests.
  const uint8_t index = telemetryIndex_++;
  if ((index & 1U) == 0) {
    const uint8_t id = descriptor_;
    descriptor_ = static_cast<uint8_t>((descriptor_ + 1U) % 16U);
    return exbus::exText(Manufacturer, Device, id, Labels[id], Units[id], dest, capacity);
  }
  const Outputs& out = control_.outputs();
  const int32_t current = currentValid_ ? currentMa_ / 10 : 0;
  exbus::ExValue values[15] = {
    {1, boundedValue(out.estimatedRemainingMm / 100U), 1},
    {2, static_cast<int32_t>(out.remainingPermille), 1},
    {3, static_cast<int32_t>(out.state), 0}, {4, current, 2},
    {5, boundedValue(hallTotal_), 0}, {6, boundedValue(pulseRate_), 0},
    {7, boundedValue(pulseRate_ > 69905U ? 2097151U : pulseRate_ * 30U), 0},
    {8, static_cast<int32_t>(out.escUs), 0}, {9, static_cast<int32_t>(out.reason), 0},
    {10, (out.positionKnown ? 1 : 0) | (out.lengthLearned ? 2 : 0) | (currentValid_ ? 4 : 0) |
         (control_.settings().ropeLengthMm != 0 ? 8 : 0), 0},
    {11, static_cast<int32_t>(out.rcFresh), 0}, {12, static_cast<int32_t>(out.powerEnable), 0},
    {13, static_cast<int32_t>(out.lightWarning), 0}, {14, boundedValue(busErrors()), 0},
    {15, boundedValue(diagnostics_.queue().dropped()), 0}
  };
  const std::size_t batch = static_cast<std::size_t>((index / 2U) % 5U) * 3U;
  return exbus::exData(Manufacturer, Device, values + batch, 3, dest, capacity);
}
}  // namespace winch
