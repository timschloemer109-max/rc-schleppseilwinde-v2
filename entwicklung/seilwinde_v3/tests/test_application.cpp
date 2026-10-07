#include "application.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
using namespace winch;
namespace {
void bytes(Application& app, const uint8_t* data, std::size_t n, uint32_t ms, uint32_t age = 0) {
  const uint32_t now = ms * 1000U;
  for (std::size_t i = 0; i < n; ++i) app.receive(data[i], now - age, now, ms);
}
void channel(Application& app, uint32_t ms, uint16_t trigger, uint32_t age = 0) {
  const uint16_t values[4] = {1600, trigger, 1400, 2000};
  uint8_t data[64]{};
  const std::size_t n = exbus::channels(1, values, 4, false, data, sizeof(data));
  assert(n != 0); bytes(app, data, n, ms, age);
}
void tick(Application& app, uint32_t ms, bool end = false) {
  Inputs in; in.nowMs = ms; in.endSwitch = end; app.tick(in);
}
void ready(Application& app) {
  app.reset(0, 0, false);
  for (uint32_t ms = 0; ms <= 1100; ms += 20) { channel(app, ms, 1500); tick(app, ms); }
  assert(app.controller().outputs().arm == ArmState::Ready);
}
Settings testSettings() {
  Settings s = Settings::defaults(); s.armMs = 1000; s.neutralMs = 50;
  s.modeStableFrames = 1; s.rampUpMs = 0; s.rampDownMs = 0;
  assert(validSettings(s)); return s;
}
void menuKey(Menu& menu, uint8_t keys, uint32_t& ms, Controller& c) {
  menu.buttons(0xf0, ms, c); ms += 400;
  menu.buttons(static_cast<uint8_t>((~static_cast<unsigned>(keys)) & 0xf0U), ms, c);
}
}
int main() {
  {
    Application app(testSettings()); ready(app);
    channel(app, 1120, 2000); tick(app, 1120);
    assert(app.controller().outputs().escUs > 1000);
    // True application, full logger, request storm, foreground menu traffic.
    uint8_t data[64]{};
    for (uint32_t ms = 1121; ms < 1140; ++ms) {
      const std::size_t n = exbus::jetiboxRequest(12, 0xf0, data, sizeof(data));
      bytes(app, data, n, ms); app.reply().length = 0;
      DiagnosticsInput d; d.nowMs = ms * 1000U;
      app.diagnostics().tick(app.controller(), d);
    }
    assert(app.diagnostics().queue().dropped() > 0);
    tick(app, 1140, true);
    assert(app.controller().outputs().escUs == 1000);
    assert(app.controller().outputs().reason == StopReason::EndSwitch);
    channel(app, 1160, 2000); tick(app, 1160, false);
    assert(app.controller().outputs().escUs == 1000);
  }
  {
    Application app(testSettings()); ready(app);
    channel(app, 1120, 2000); tick(app, 1120);
    uint8_t data[64]{};
    for (uint32_t ms = 1140; ms <= 1220; ms += 20) {
      const std::size_t n = exbus::telemetryRequest(17, data, sizeof(data));
      bytes(app, data, n, ms);
      assert(app.reply().length > 0 && app.reply().bytes[3] == 17);
      assert(exbus::validFrameCrc(app.reply().bytes, app.reply().length));
      app.reply().length = 0; tick(app, ms);
    }
    assert(app.controller().outputs().reason == StopReason::RcLost);
    channel(app, 1240, 2000); tick(app, 1240);
    assert(app.controller().outputs().escUs == 1000);
  }
  {
    Application app(testSettings()); ready(app);
    const uint32_t oldSeq = app.rc().sequence;
    channel(app, 1120, 2000, 101000);
    assert(app.rc().sequence == oldSeq);
    // Delayed ISR records use arrival age, never foreground parsing time.
    channel(app, 1140, 2000, 90000); tick(app, 1140);
    assert(app.rc().receivedMs == 1050);
    tick(app, 1150);
    assert(!app.controller().outputs().rcFresh);
  }
  {
    Application app(testSettings());
    const uint32_t ms = 5000000U;
    // micros() epoch wraps independently of millis() after about 71 minutes.
    channel(app, ms, 1500, 4000);
    assert(app.rc().receivedMs == ms - 4U);
    char display[32]; app.menu().render(display, app.controller());
    for (char c : display) assert(c != '\0');
    uint32_t time = 0;
    for (int i = 0; i < 2; ++i) menuKey(app.menu(), 0x10, time, app.controller());
    assert(app.menu().page() == 2);
    for (int i = 0; i < 200; ++i) menuKey(app.menu(), 0x20, time, app.controller());
    assert(app.menu().draft().fastPermille == 990);
    for (int i = 0; i < 200; ++i) menuKey(app.menu(), 0x40, time, app.controller());
    assert(app.menu().draft().fastPermille == 100);
  }
  {
    Application app(testSettings()); ready(app);
    uint32_t time = 1200;
    for (int i = 0; i < 21; ++i) menuKey(app.menu(), 0x10, time, app.controller());
    app.menu().lock(true);
    menuKey(app.menu(), 0x20, time, app.controller());
    assert(!app.takeSaveRequest());
    app.menu().lock(false);
    menuKey(app.menu(), 0x20, time, app.controller());
    assert(app.takeSaveRequest()); assert(!app.takeSaveRequest());
    // Editing a draft and trying Save without Apply must not commit it.
    for (int i = 0; i < 19; ++i) menuKey(app.menu(), 0x80, time, app.controller());
    menuKey(app.menu(), 0x20, time, app.controller());
    for (int i = 0; i < 19; ++i) menuKey(app.menu(), 0x10, time, app.controller());
    menuKey(app.menu(), 0x20, time, app.controller());
    assert(!app.takeSaveRequest());
    menuKey(app.menu(), 0x80, time, app.controller()); // Apply RAM
    const uint16_t previousFast = app.controller().settings().fastPermille;
    app.menu().lock(true);
    menuKey(app.menu(), 0x20, time, app.controller());
    assert(app.controller().settings().fastPermille == previousFast);
    app.menu().lock(false);
    menuKey(app.menu(), 0x20, time, app.controller());
    assert(app.controller().settings().fastPermille != previousFast);
  }
  {
    ByteQueue<8> q; assert(q.push("abcdef", 6));
    uint8_t out[8]{}; assert(q.peek(out, 3) == 3); q.consume(3);
    assert(q.push("12345", 5)); assert(!q.push("x", 1));
    assert(q.peek(out, 8) == 8); assert(std::memcmp(out, "def12345", 8) == 0);
    assert(q.dropped() == 1); q.consume(99); assert(q.size() == 0);
  }
  {
    Diagnostics log; Controller c(testSettings()); DiagnosticsInput d;
    log.tick(c, d); log.tick(c, d);
    d.nowMs = 250; log.tick(c, d);
    uint8_t data[2048]{}; const std::size_t n = log.queue().peek(data, sizeof(data));
    const std::string csv(reinterpret_cast<char*>(data), n);
    assert(csv.find("record_type,") == 0);
    assert(csv.find("record_type,", 1) == std::string::npos);
    assert(csv.find("event,0,") != std::string::npos);
    assert(csv.find("telemetry,250,") != std::string::npos);
    std::size_t start = 0;
    while (start < csv.size()) {
      const std::size_t end = csv.find('\n', start); assert(end != std::string::npos);
      unsigned commas = 0;
      for (std::size_t i = start; i < end; ++i) if (csv[i] == ',') ++commas;
      assert(commas == 33); start = end + 1;
    }
  }
  std::cout << "PASS application: channel-age, telemetry-only failsafe, menu, logger backpressure, endstop, micros wrap\n";
}
