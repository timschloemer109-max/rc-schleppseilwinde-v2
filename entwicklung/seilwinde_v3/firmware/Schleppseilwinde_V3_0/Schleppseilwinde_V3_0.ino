/* RC-Schleppseilwinde V3.0 -- Nano R4, core 1.6.0.
 * Build via ../../build.py arduino (stages canonical portable sources).
 * Default profile is inhibited. Bench qualification is a build-time step.
 * No upload, motor command or run permission is implied by a successful build.
 */
#include <Arduino.h>
#include <new>
#include <cstdio>
#include <tusb.h>
#include "board_config.hpp"
#include "r4_exbus_uart.hpp"
#include "r4_esc_pwm.hpp"
#include "r4_current.hpp"
#include "r4_flash_storage.hpp"
#include "disabled_adapters.hpp"
#include "src/winch/application.hpp"
#include "src/winch/persistence.hpp"

static_assert(!board::PowerAdapterQualified, "Implement and review a real power driver first");
static_assert(!board::LightAdapterQualified, "Implement and review the selected uniLIGHT interface first");

namespace {
R4ExBusUart bus;
R4EscPwm esc;
R4Current current;
R4FlashStorage flash;
winch::SettingsJournal journal(flash, 1024);
DisabledPowerDriver power;
DisabledUniLight light;
alignas(winch::Application) uint8_t applicationMemory[sizeof(winch::Application)];
winch::Application* application = nullptr;
volatile uint32_t hallCount = 0, lastHallEdgeUs = 0;
bool busHealthy = false, flashHealthy = false, maintenance = false, latchedHardwareFault = false;
uint32_t lastBusErrors = 0, loopMaxUs = 0, rateMs = 0, rateHall = 0, pulseRate = 0;
uint16_t actualEscUs = 1000;
uint32_t lastAdcDebugMs = 0;

bool endSwitchActive() {
  const bool low = digitalRead(board::EndSwitch) == LOW;
  return board::EndSwitchActiveLow ? low : !low;
}
void hallIsr() {
  const uint32_t now = micros();
  if (static_cast<uint32_t>(now - lastHallEdgeUs) >= board::HallDebounceUs) {
    lastHallEdgeUs = now; ++hallCount;
  }
}
struct Hall { uint32_t count, lastUs; };
Hall snapshotHall() {
  const uint32_t primask = __get_PRIMASK(); __disable_irq();
  const Hall result{hallCount, lastHallEdgeUs};
  __set_PRIMASK(primask); return result;
}
bool inhibited() {
  return !board::MotorBenchQualified || !busHealthy || !esc.healthy() ||
      maintenance || latchedHardwareFault ||
      (board::CurrentCalibrationQualified && !current.healthy());
}
void outputs(const winch::Outputs& output) {
  actualEscUs = inhibited() || endSwitchActive() ? 1000 : output.escUs;
  if (actualEscUs == 1000) esc.stopNow();
  else esc.writeMicroseconds(actualEscUs);
  power.request(!inhibited() && output.powerEnable);
  light.request(output.lightUs, output.lightWarning);
  digitalWrite(board::Led, inhibited() || output.state == winch::State::Fault ? HIGH : LOW);
}
void safetyTick(uint32_t nowMs, const Hall& hall) {
  if (maintenance) { esc.stopNow(); power.off(); actualEscUs = 1000; return; }
  winch::Inputs input;
  input.nowMs = nowMs; input.hallTotal = hall.count; input.endSwitch = endSwitchActive();
  input.currentValid = current.valid(); input.currentMilliAmp = current.milliAmp();
  input.powerDriverHealthy = power.healthy();
  outputs(application->tick(input));
}
void serviceSave(uint32_t nowMs, const Hall& hall) {
  flash.service(nowMs); // One asynchronous stage advance; never wait for FCU.
  if (maintenance) {
    if (flash.error()) {
      latchedHardwareFault = true; maintenance = false;
      application->menu().saved(false);
      application->menu().lock(false);
      application->reset(nowMs, hall.count, endSwitchActive());
    } else if (!flash.busy()) {
      maintenance = false; application->controller().acknowledgeSaved();
      application->menu().saved(true);
      application->menu().lock(false);
      // Full stop-PWM arming and fresh neutral/new RUN after every flash save.
      application->reset(nowMs, hall.count, endSwitchActive());
    }
  }
  if (!application->takeSaveRequest()) return;
  if (!flashHealthy || maintenance || latchedHardwareFault || !application->controller().canSave()) {
    application->menu().saved(false); return;
  }
  esc.stopNow(); power.off(); actualEscUs = 1000;
  const winch::SaveResult result = journal.save(application->controller().settings(), true);
  if (result == winch::SaveResult::Unchanged) {
    application->controller().acknowledgeSaved(); application->menu().saved(true);
    application->reset(nowMs, hall.count, endSwitchActive()); return;
  }
  if (result != winch::SaveResult::Saved || !flash.startCommit(nowMs)) {
    latchedHardwareFault = true; application->menu().saved(false); return;
  }
  maintenance = true;
  application->menu().lock(true);
  application->reset(nowMs, hall.count, endSwitchActive());
}
void serviceUsb() {
  if (!tud_cdc_connected()) return;
  uint8_t bytes[board::UsbBudget];
  uint32_t room = tud_cdc_write_available();
  if (room > board::UsbBudget) room = board::UsbBudget;
  const size_t count = application->diagnostics().queue().peek(bytes, room);
  if (count == 0) return;
  // Direct TinyUSB bounded enqueue; SerialUSB.write() contains a wait loop.
  const uint32_t sent = tud_cdc_write(bytes, static_cast<uint32_t>(count));
  application->diagnostics().queue().consume(sent);
  tud_cdc_write_flush(); // Single endpoint submission, no transport wait.
}
}  // namespace

void setup() {
  power.off();
  pinMode(board::Hall, INPUT_PULLUP);
  pinMode(board::EndSwitch, INPUT_PULLUP);
  pinMode(board::Led, OUTPUT);
  if (!esc.begin(board::Esc)) latchedHardwareFault = true;
  esc.stopNow();
  flashHealthy = flash.begin();
  winch::Settings settings;
  journal.load(settings);
  // Capability gates are never enabled by a stored or remotely edited record.
  if (!board::PowerAdapterQualified) settings.powerEnabled = 0;
  if (!board::LightAdapterQualified) settings.lightEnabled = 0;
  if (!board::CurrentCalibrationQualified) { settings.currentCalibrated = 0; settings.stallEnabled = 0; }
  application = new (applicationMemory) winch::Application(settings);
  const uint32_t nowMs = millis();
  lastHallEdgeUs = micros();
  application->reset(nowMs, 0, endSwitchActive());
  attachInterrupt(digitalPinToInterrupt(board::Hall), hallIsr, FALLING);
  current.begin();
  busHealthy = bus.begin(board::BusBaud);
  Serial.begin(115200); // USB only; no wait for enumeration or DTR.
  rateMs = nowMs;
}

void loop() {
  const uint32_t startedUs = micros();
  uint32_t nowMs = millis();
  Hall hall = snapshotHall();
  safetyTick(nowMs, hall);
  current.tick(nowMs);
  bus.service(micros());
  const uint32_t errors = bus.errors() + bus.overflows();
  if (errors != lastBusErrors) { application->transportError(); lastBusErrors = errors; }
  R4ExBusUart::Byte byte;
  for (uint8_t i = 0; i < board::RxBudget; ++i) {
    if (static_cast<uint32_t>(micros() - startedUs) >= board::BusBudgetUs || !bus.pop(byte)) break;
    application->receive(byte.value, byte.receivedUs, micros(), millis());
    // Recheck the physical end switch while consuming a traffic burst.
    if (endSwitchActive()) esc.stopNow();
  }
  application->expire(micros());
  nowMs = millis(); hall = snapshotHall();
  safetyTick(nowMs, hall);
  winch::Reply& reply = application->reply();
  if (reply.length != 0) {
    const uint32_t ageUs = micros() - reply.requestedUs;
    if (busHealthy && bus.available() == 0 && winch::exbus::fitsResponseWindow(reply.length, bus.baud(), ageUs))
      bus.trySend(reply.bytes, reply.length);
    // Never send a late reply or wait for a busy UART; drop this one request.
    reply.length = 0;
  }
  serviceSave(nowMs, hall);
  const uint32_t rateElapsed = nowMs - rateMs;
  if (rateElapsed >= 250U) {
    pulseRate = static_cast<uint32_t>((static_cast<uint64_t>(hall.count - rateHall) * 1000U) / rateElapsed);
    rateMs = nowMs; rateHall = hall.count;
  }
  application->setTelemetry(current.milliAmp(), current.valid(), pulseRate, hall.count);
  winch::DiagnosticsInput diagnostic;
  diagnostic.nowMs = nowMs; diagnostic.hallTotal = hall.count;
  diagnostic.currentMilliAmp = current.milliAmp(); diagnostic.currentValid = current.valid();
  diagnostic.endSwitch = endSwitchActive(); diagnostic.lastHallAgeMs = (micros() - hall.lastUs) / 1000U;
  diagnostic.rc = application->rc(); diagnostic.triggerUs = application->triggerUs();
  diagnostic.busErrors = application->busErrors(); diagnostic.loopMaxUs = loopMaxUs;
  diagnostic.actualEscUs = actualEscUs; diagnostic.hardwareInhibited = inhibited();
  application->diagnostics().tick(application->controller(), diagnostic);
  if (static_cast<uint32_t>(nowMs - lastAdcDebugMs) >= 1000U) {
    char line[64];
    const int n = std::snprintf(line, sizeof(line), "#adc_raw,%u,healthy,%u,calibrated,%u\n",
      static_cast<unsigned>(current.raw()), static_cast<unsigned>(current.healthy()),
      static_cast<unsigned>(current.valid()));
    if (n > 0 && static_cast<size_t>(n) < sizeof(line))
      application->diagnostics().queue().push(line, static_cast<size_t>(n));
    lastAdcDebugMs = nowMs;
  }
  serviceUsb();
  if (endSwitchActive()) esc.stopNow();
  const uint32_t elapsedUs = micros() - startedUs;
  if (elapsedUs > loopMaxUs) loopMaxUs = elapsedUs;
}
