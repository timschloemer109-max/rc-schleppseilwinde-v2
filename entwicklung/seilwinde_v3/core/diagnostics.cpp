#include "diagnostics.hpp"
#include <cstdio>
#include <cstring>
namespace winch {
const char* Diagnostics::header() {
  return "record_type,arduino_ms,run_state,stop_reason,esc_arm_state,rc_mode,rc_speed_us,rc_speed_valid,rc_trigger_us,rc_trigger_valid,esc_output_us,current_a,hall_pulse_count,delta_pulses,pulses_per_s,last_hall_age_ms,rope_pulses_avg,end_switch_active,end_switch_latched,fault_latched,learning_active,event_name,event_value,schema_version,position_valid,remaining_mm,remaining_percent,power_enabled,light_warning,log_dropped,bus_errors,loop_max_us,actual_esc_output_us,hardware_inhibited\n";
}
Diagnostics::Diagnostics() {
  const char* h = header();
  queue_.push(h, std::strlen(h));
}
void Diagnostics::tick(const Controller& controller, const DiagnosticsInput& in) {
  const Outputs& out = controller.outputs();
  const bool event = boot_ || out.events != lastEvents_;
  if (!event && static_cast<uint32_t>(in.nowMs - lastMs_) < 250U) return;
  const uint32_t delta = in.hallTotal - lastHall_;
  const uint32_t elapsed = in.nowMs - lastMs_;
  const uint32_t rate = elapsed == 0 ? 0 : static_cast<uint32_t>((static_cast<uint64_t>(delta) * 1000U) / elapsed);
  char row[512]{};
  char amps[32]{};
  if (in.currentValid) {
    const int64_t signedValue = in.currentMilliAmp;
    const uint64_t magnitude = static_cast<uint64_t>(signedValue < 0 ? -signedValue : signedValue);
    std::snprintf(amps, sizeof(amps), "%s%lu.%03lu", signedValue < 0 ? "-" : "",
                  static_cast<unsigned long>(magnitude / 1000U), static_cast<unsigned long>(magnitude % 1000U));
  }
  const int n = std::snprintf(row, sizeof(row),
    "%s,%lu,%s,%s,%u,%u,%u,%u,%u,%u,%u,%s,%lu,%lu,%lu,%lu,%lu,%u,%u,%u,%u,%s,%lu,3.0,%u,%lu,%u.%u,%u,%u,%lu,%lu,%lu,%u,%u\n",
    event ? "event" : "telemetry", static_cast<unsigned long>(in.nowMs), stateName(out.state), reasonName(out.reason),
    static_cast<unsigned>(out.arm), static_cast<unsigned>(in.rc.mode), static_cast<unsigned>(in.rc.speedUs),
    static_cast<unsigned>(out.rcFresh), static_cast<unsigned>(in.triggerUs), static_cast<unsigned>(out.rcFresh),
    static_cast<unsigned>(out.escUs), amps, static_cast<unsigned long>(in.hallTotal), static_cast<unsigned long>(delta),
    static_cast<unsigned long>(rate), static_cast<unsigned long>(in.lastHallAgeMs),
    static_cast<unsigned long>(controller.settings().learnedPulses), static_cast<unsigned>(in.endSwitch),
    static_cast<unsigned>(out.endLatched), static_cast<unsigned>(out.state == State::Fault),
    static_cast<unsigned>(out.state == State::Learning), boot_ ? "boot" : (event ? "state_change" : ""),
    static_cast<unsigned long>(out.events), static_cast<unsigned>(out.positionKnown),
    static_cast<unsigned long>(out.estimatedRemainingMm), static_cast<unsigned>(out.remainingPermille / 10U),
    static_cast<unsigned>(out.remainingPermille % 10U), static_cast<unsigned>(out.powerEnable),
    static_cast<unsigned>(out.lightWarning), static_cast<unsigned long>(queue_.dropped()),
    static_cast<unsigned long>(in.busErrors), static_cast<unsigned long>(in.loopMaxUs),
    static_cast<unsigned>(in.actualEscUs), static_cast<unsigned>(in.hardwareInhibited));
  if (n > 0 && static_cast<std::size_t>(n) < sizeof(row)) queue_.push(row, static_cast<std::size_t>(n));
  lastMs_ = in.nowMs; lastHall_ = in.hallTotal; lastEvents_ = out.events; boot_ = false;
}
}  // namespace winch
