#include "application.hpp"
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

// Host-only pipe helper. All channels/menu/telemetry enter the real EX Bus
// parser. It has no UART, GPIO, firmware adapter or physical motor path.
namespace {
class LiveCore {
 public:
  LiveCore() : app_(winch::Settings::defaults()) { app_.reset(0, 0, false); tick(); }
  bool step(uint32_t targetUs, uint32_t hall, unsigned end,
            const std::string& channels, const std::string& menu, const std::string& telemetry) {
    if (end > 1 || targetUs < nowUs_ || targetUs - nowUs_ > 1000000U) return false;
    if (!advance(targetUs)) return false;
    hall_ = hall; end_ = end != 0; tick(); // End stop precedes any new bus data.
    std::string menuReply, telemetryReply;
    if (!packet(channels, nullptr) || !packet(menu, &menuReply) ||
        !packet(telemetry, &telemetryReply)) return false;
    tick();
    const winch::Outputs& out = app_.controller().outputs();
    std::cout << "{\"now_ms\":" << nowMs_ << ",\"now_us\":" << nowUs_
      << ",\"state\":\"" << winch::stateName(out.state) << "\",\"esc_us\":" << out.escUs
      << ",\"reason\":\"" << winch::reasonName(out.reason) << "\",\"rc_fresh\":" << out.rcFresh
      << ",\"arm\":" << static_cast<unsigned>(out.arm) << ",\"end_latched\":" << out.endLatched
      << ",\"remaining_pulses\":" << out.remainingPulses << ",\"hall\":" << hall_
      << ",\"bus_errors\":" << app_.busErrors() << ",\"rc_sequence\":" << app_.rc().sequence
      << ",\"menu_reply\":\"" << menuReply << "\",\"telemetry_reply\":\"" << telemetryReply << "\"}\n";
    std::cout.flush();
    return true;
  }
 private:
  winch::Application app_;
  uint32_t nowUs_ = 0, nowMs_ = 0, hall_ = 0;
  bool end_ = false;
  void tick() {
    winch::Inputs input; input.nowMs = nowMs_; input.hallTotal = hall_; input.endSwitch = end_;
    app_.tick(input); app_.setTelemetry(0, false, 0, hall_);
  }
  bool advance(uint32_t targetUs) {
    if (targetUs < nowUs_) return false;
    while (nowMs_ < targetUs / 1000U) { ++nowMs_; tick(); }
    nowUs_ = targetUs; app_.expire(nowUs_); return true;
  }
  static int nibble(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
  }
  bool packet(const std::string& hex, std::string* response) {
    if (hex == "-") return true;
    if (hex.empty() || hex.size() % 2U != 0 || hex.size() > 128U) return false;
    for (std::size_t i = 0; i < hex.size(); i += 2U) {
      const int high = nibble(hex[i]), low = nibble(hex[i + 1U]);
      if (high < 0 || low < 0 || !advance(nowUs_ + 40U)) return false;
      app_.receive(static_cast<uint8_t>(high * 16 + low), nowUs_, nowUs_, nowMs_);
      tick();
    }
    winch::Reply& reply = app_.reply();
    if (reply.length != 0) {
      if (!response || !winch::exbus::validFrameCrc(reply.bytes, reply.length)) return false;
      std::ostringstream result;
      result << std::hex << std::setfill('0');
      for (std::size_t i = 0; i < reply.length; ++i)
        result << std::setw(2) << static_cast<unsigned>(reply.bytes[i]);
      *response = result.str();
      const uint32_t transmissionUs = static_cast<uint32_t>(reply.length) * 40U;
      reply.length = 0;
      if (!advance(nowUs_ + transmissionUs + 200U)) return false;
    } else if (response) return false;
    return advance(nowUs_ + 200U);
  }
};
}  // namespace

int main() {
  LiveCore core;
  std::string line;
  while (std::getline(std::cin, line)) {
    if (line.size() > 512U) return 2;
    std::istringstream row(line);
    std::string command, channelHex, menuHex, telemetryHex, extra;
    uint32_t targetUs = 0, hall = 0; unsigned end = 0;
    if (!(row >> command >> targetUs >> hall >> end >> channelHex >> menuHex >> telemetryHex) ||
        command != "STEP" || (row >> extra) ||
        !core.step(targetUs, hall, end, channelHex, menuHex, telemetryHex)) {
      std::cerr << "Invalid/non-monotonic bounded host STEP\n"; return 2;
    }
  }
  return 0;
}
