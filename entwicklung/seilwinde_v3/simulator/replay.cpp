#include "application.hpp"
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

// Offline host bridge for the independent Python master. This executes the same
// Application/core code as Nano firmware, with injected inputs/time. It does not
// model electrical UART timing or RA4M1 peripherals, and cannot operate a motor.
namespace {
class Replay {
public:
    Replay() : app_(winch::Settings::defaults()) { app_.reset(0, 0, false); tick(0); }
    bool advance(uint32_t nowUs) {
        if (nowUs < nowUs_) return fail("non-monotonic input time");
        const uint32_t nextMs = nowUs / 1000U;
        while (nowMs_ < nextMs) { ++nowMs_; tick(nowMs_); }
        nowUs_ = nowUs;
        return true;
    }
    bool byte(uint32_t nowUs, uint32_t byte) {
        if (byte > 255 || !advance(nowUs)) return fail("invalid byte/time");
        ++bytes_;
        app_.receive(static_cast<uint8_t>(byte), nowUs, nowUs, nowMs_);
        tick(nowMs_);
        winch::Reply& reply = app_.reply();
        if (reply.length) {
            if (!winch::exbus::validFrameCrc(reply.bytes, reply.length)) return fail("bad response CRC");
            ++responses_;
            reply.length = 0; // Simulate a ready sink, no loop/wait on transport.
        }
        return good_;
    }
    bool inputs(uint32_t nowUs, uint32_t hall, uint32_t end, int32_t currentMa, uint32_t currentValid) {
        if (end > 1 || currentValid > 1 || !advance(nowUs)) return fail("invalid hardware input/time");
        hall_ = hall; end_ = end != 0; currentMa_ = currentMa; currentValid_ = currentValid != 0;
        if (end_ && endAtMs_ == 0) endAtMs_ = nowMs_;
        tick(nowMs_);
        return good_;
    }
    bool finish(bool expectLoss, bool expectEnd) {
        if (!started_) fail("scenario never passed real 3 s arming / neutral / RUN edge");
        if (expectLoss && !lossSeen_) fail("missing expected RC loss under telemetry traffic");
        if (expectLoss && restarted_) fail("RUN return caused automatic restart after RC loss");
        if (!expectLoss && lossSeen_) fail("unexpected RC loss during fresh channel scenario");
        if (expectEnd && (!endAtMs_ || !endStopped_)) fail("end stop was not applied during an active phase");
        if (!responses_) fail("no telemetry/menu replies in scenario");
        if (!app_.diagnostics().queue().dropped()) fail("logger saturation was not exercised");
        std::cout << "{\"passed\":" << (good_ ? "true" : "false")
                  << ",\"bytes\":" << bytes_ << ",\"responses\":" << responses_
                  << ",\"started_ms\":" << startedMs_ << ",\"max_pwm_us\":" << maximumEsc_
                  << ",\"rc_loss_ms\":" << lossMs_ << ",\"restart_after_loss\":" << (restarted_ ? "true" : "false")
                  << ",\"endstop_ms\":" << endAtMs_ << ",\"endstop_command_latency_ms\":" << endLatencyMs_
                  << ",\"bus_errors\":" << app_.busErrors()
                  << ",\"logger_drops\":" << app_.diagnostics().queue().dropped()
                  << ",\"final_pwm_us\":" << app_.controller().outputs().escUs << "}\n";
        return good_;
    }
private:
    winch::Application app_;
    uint32_t nowUs_ = 0, nowMs_ = 0, hall_ = 0;
    int32_t currentMa_ = 0;
    bool end_ = false, currentValid_ = false, good_ = true;
    bool started_ = false, lossSeen_ = false, restarted_ = false, endStopped_ = false;
    uint32_t startedMs_ = 0, lossMs_ = 0, endAtMs_ = 0, endLatencyMs_ = 0;
    uint32_t bytes_ = 0, responses_ = 0;
    uint16_t maximumEsc_ = 1000;
    bool fail(const char* text) {
        std::cerr << "FAIL replay at " << nowUs_ << " us: " << text << '\n';
        good_ = false; return false;
    }
    void tick(uint32_t nowMs) {
        winch::Inputs input;
        input.nowMs = nowMs; input.hallTotal = hall_; input.endSwitch = end_;
        input.currentMilliAmp = currentMa_; input.currentValid = currentValid_;
        const winch::Outputs& out = app_.tick(input);
        if (out.escUs > maximumEsc_) maximumEsc_ = out.escUs;
        if (out.escUs > 1000) {
            if (!started_) { started_ = true; startedMs_ = nowMs; }
            if (lossSeen_) restarted_ = true;
            if (end_) fail("end switch did not override motor command");
        }
        if (!lossSeen_ && out.reason == winch::StopReason::RcLost) {
            lossSeen_ = true; lossMs_ = nowMs;
        }
        if (end_ && out.reason == winch::StopReason::EndSwitch && out.escUs == 1000) {
            if (!endStopped_) endLatencyMs_ = nowMs - endAtMs_;
            endStopped_ = true;
        }
        app_.setTelemetry(currentMa_, currentValid_, 20, hall_);
        winch::DiagnosticsInput diagnostic;
        diagnostic.nowMs = nowMs; diagnostic.hallTotal = hall_;
        diagnostic.currentMilliAmp = currentMa_; diagnostic.currentValid = currentValid_;
        diagnostic.endSwitch = end_; diagnostic.rc = app_.rc();
        diagnostic.busErrors = app_.busErrors();
        app_.diagnostics().tick(app_.controller(), diagnostic);
    }
};
} // namespace

int main(int argc, char** argv) {
    bool loss = false, end = false;
    for (int i = 1; i < argc; ++i) {
        const std::string argument(argv[i]);
        if (argument == "--expect-loss") loss = true;
        else if (argument == "--expect-endstop") end = true;
        else { std::cerr << "unknown option: " << argument << '\n'; return 2; }
    }
    Replay replay;
    char record;
    while (std::cin >> record) {
        uint32_t nowUs;
        if (!(std::cin >> nowUs)) return 2;
        if (record == 'B') {
            uint32_t byte;
            if (!(std::cin >> byte) || !replay.byte(nowUs, byte)) return 1;
        } else if (record == 'I') {
            uint32_t hall, activeEnd, currentValid;
            int32_t currentMa;
            if (!(std::cin >> hall >> activeEnd >> currentMa >> currentValid) ||
                !replay.inputs(nowUs, hall, activeEnd, currentMa, currentValid)) return 1;
        } else if (record == 'T') {
            if (!replay.advance(nowUs)) return 1;
        } else { std::cerr << "unknown record\n"; return 2; }
    }
    return replay.finish(loss, end) ? 0 : 1;
}
