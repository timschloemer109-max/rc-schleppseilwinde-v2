/*
  Schleppseilwinde Testbench

  Werkbank-Sketch fuer den ersten Motor- und Sensortest ohne RC-Empfaenger.
  Der Sketch steuert nur den ESC, liest Hall-Impulse und den ACS712 und
  bietet eine serielle Kommandoschnittstelle fuer Codex oder den Serial Monitor.
*/

#include <Servo.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

namespace Pins {
  constexpr uint8_t HALL_SENSOR = 2;
  constexpr uint8_t ESC_SIGNAL = 4;
  constexpr uint8_t LED_STATUS = 13;
  constexpr uint8_t CURRENT_SENSOR = A3;
}

namespace RcPwm {
  constexpr int PWM_MIN = 1000;
  constexpr int PWM_MAX = 2000;
}

namespace Config {
  constexpr unsigned long SERIAL_BAUD = 115200UL;
  constexpr unsigned long ESC_ARM_DELAY_MS = 3000UL;
  constexpr unsigned long TELEMETRY_INTERVAL_MS = 100UL;
  constexpr unsigned long MAX_ACTIVE_RUN_MS = 8000UL;
  constexpr unsigned long STALL_ARM_DELAY_MS = 700UL;
  constexpr unsigned long STALL_NO_PULSE_MS = 300UL;
  constexpr unsigned long HALL_DEBOUNCE_US = 1000UL;

  constexpr int PWM_TEST_MIN = RcPwm::PWM_MIN;
  constexpr int PWM_TEST_MAX = 1450;
  constexpr uint8_t HALL_PULSES_PER_REV = 2;

  constexpr uint8_t CURRENT_ZERO_SAMPLES = 64;
  constexpr float CURRENT_FILTER_ALPHA = 0.18f;
  constexpr float STALL_CURRENT_A = 8.0f;
}

namespace CurrentSense {
  constexpr float ADC_REF_V = 5.0f;
  constexpr float ADC_COUNTS = 1024.0f;
  constexpr float SCALE_FACTOR_V_PER_A = 0.066f;
}

enum class Mode : uint8_t {
  IDLE,
  HOLD,
  SWEEP
};

enum class Fault : uint8_t {
  NONE,
  MAX_RUNTIME,
  STALL
};

struct PulseSnapshot {
  unsigned long pulseCount;
  unsigned long lastPulseMicros;
};

struct SweepState {
  bool active = false;
  int currentUs = RcPwm::PWM_MIN;
  int endUs = RcPwm::PWM_MIN;
  int stepUs = 0;
  unsigned long holdMs = 0;
  unsigned long nextStepMs = 0;
};

Servo g_esc;

volatile unsigned long g_pulseCount = 0;
volatile unsigned long g_lastPulseMicros = 0;
volatile unsigned long g_lastHallEdgeMicros = 0;

float g_currentZeroVoltage = CurrentSense::ADC_REF_V * 0.5f;
float g_filteredCurrentA = 0.0f;

Mode g_mode = Mode::IDLE;
Fault g_fault = Fault::NONE;
bool g_armed = false;
int g_currentPwmUs = RcPwm::PWM_MIN;
unsigned long g_motionStartMs = 0;
unsigned long g_lastTelemetryMs = 0;
unsigned long g_lastTelemetryPulseCount = 0;

SweepState g_sweep;

char g_serialBuffer[80];
size_t g_serialBufferLen = 0;

void hallISR() {
  const unsigned long nowMicros = micros();
  if ((nowMicros - g_lastHallEdgeMicros) < Config::HALL_DEBOUNCE_US) {
    return;
  }

  g_lastHallEdgeMicros = nowMicros;
  g_lastPulseMicros = nowMicros;
  g_pulseCount++;
}

PulseSnapshot snapshotPulses() {
  PulseSnapshot snapshot;
  noInterrupts();
  snapshot.pulseCount = g_pulseCount;
  snapshot.lastPulseMicros = g_lastPulseMicros;
  interrupts();
  return snapshot;
}

float adcToVoltage(int adcValue) {
  return (static_cast<float>(adcValue) * CurrentSense::ADC_REF_V) / CurrentSense::ADC_COUNTS;
}

void calibrateCurrentSensor() {
  float sumVoltage = 0.0f;
  for (uint8_t i = 0; i < Config::CURRENT_ZERO_SAMPLES; ++i) {
    sumVoltage += adcToVoltage(analogRead(Pins::CURRENT_SENSOR));
    delay(2);
  }

  g_currentZeroVoltage = sumVoltage / static_cast<float>(Config::CURRENT_ZERO_SAMPLES);
  g_filteredCurrentA = 0.0f;
}

float sampleCurrentA() {
  const float voltage = adcToVoltage(analogRead(Pins::CURRENT_SENSOR));
  const float rawCurrent = (voltage - g_currentZeroVoltage) / CurrentSense::SCALE_FACTOR_V_PER_A;
  g_filteredCurrentA += Config::CURRENT_FILTER_ALPHA * (rawCurrent - g_filteredCurrentA);
  return g_filteredCurrentA;
}

const __FlashStringHelper *modeToFlashString(Mode mode) {
  switch (mode) {
    case Mode::IDLE:
      return F("idle");
    case Mode::HOLD:
      return F("hold");
    case Mode::SWEEP:
      return F("sweep");
  }

  return F("idle");
}

const __FlashStringHelper *faultToFlashString(Fault fault) {
  switch (fault) {
    case Fault::NONE:
      return F("none");
    case Fault::MAX_RUNTIME:
      return F("max_runtime");
    case Fault::STALL:
      return F("stall");
  }

  return F("none");
}

void setEscPwmUs(int pwmUs) {
  g_currentPwmUs = constrain(pwmUs, RcPwm::PWM_MIN, RcPwm::PWM_MAX);
  g_esc.writeMicroseconds(g_currentPwmUs);
}

void updateStatusLed() {
  digitalWrite(Pins::LED_STATUS, (g_fault != Fault::NONE || g_armed) ? HIGH : LOW);
}

void printEvent(const __FlashStringHelper *eventName, const __FlashStringHelper *detail) {
  Serial.print(F("EVENT,"));
  Serial.print(eventName);
  Serial.print(F(","));
  Serial.println(detail);
}

void printEventValue(const __FlashStringHelper *eventName, long value) {
  Serial.print(F("EVENT,"));
  Serial.print(eventName);
  Serial.print(F(","));
  Serial.println(value);
}

void printCsvHeader() {
  Serial.println(F("ms,mode,pwm_us,current_a,pulse_count,delta_pulses,pulses_per_s,rpm,last_hall_ms,fault"));
}

void printTelemetry(bool force = false) {
  const unsigned long nowMs = millis();
  if (!force && (nowMs - g_lastTelemetryMs) < Config::TELEMETRY_INTERVAL_MS) {
    return;
  }

  const unsigned long elapsedMs = force ? (nowMs - g_lastTelemetryMs) : (nowMs - g_lastTelemetryMs);
  const PulseSnapshot pulses = snapshotPulses();
  const float currentA = sampleCurrentA();
  const unsigned long deltaPulses = pulses.pulseCount - g_lastTelemetryPulseCount;
  const float pulsesPerSecond = (elapsedMs > 0UL)
                                    ? (static_cast<float>(deltaPulses) * 1000.0f) /
                                          static_cast<float>(elapsedMs)
                                    : 0.0f;
  const float rpm = (Config::HALL_PULSES_PER_REV > 0U)
                        ? (pulsesPerSecond * 60.0f) /
                              static_cast<float>(Config::HALL_PULSES_PER_REV)
                        : 0.0f;

  long lastHallMs = -1;
  if (pulses.pulseCount > 0UL) {
    const unsigned long lastHallAgeUs = micros() - pulses.lastPulseMicros;
    lastHallMs = static_cast<long>(lastHallAgeUs / 1000UL);
  }

  Serial.print(nowMs);
  Serial.print(F(","));
  Serial.print(modeToFlashString(g_mode));
  Serial.print(F(","));
  Serial.print(g_currentPwmUs);
  Serial.print(F(","));
  Serial.print(currentA, 3);
  Serial.print(F(","));
  Serial.print(pulses.pulseCount);
  Serial.print(F(","));
  Serial.print(deltaPulses);
  Serial.print(F(","));
  Serial.print(pulsesPerSecond, 2);
  Serial.print(F(","));
  Serial.print(rpm, 2);
  Serial.print(F(","));
  Serial.print(lastHallMs);
  Serial.print(F(","));
  Serial.println(faultToFlashString(g_fault));

  g_lastTelemetryMs = nowMs;
  g_lastTelemetryPulseCount = pulses.pulseCount;
}

void resetSweep() {
  g_sweep.active = false;
  g_sweep.currentUs = RcPwm::PWM_MIN;
  g_sweep.endUs = RcPwm::PWM_MIN;
  g_sweep.stepUs = 0;
  g_sweep.holdMs = 0;
  g_sweep.nextStepMs = 0;
}

void stopMotion(bool disarm, Fault fault, const __FlashStringHelper *detail) {
  setEscPwmUs(RcPwm::PWM_MIN);
  g_mode = Mode::IDLE;
  g_fault = fault;
  g_motionStartMs = 0;
  resetSweep();
  if (disarm) {
    g_armed = false;
  }
  updateStatusLed();
  printEvent(F("stop"), detail);
  printTelemetry(true);
}

bool canMove() {
  if (!g_armed) {
    printEvent(F("reject"), F("not_armed"));
    return false;
  }

  if (g_fault != Fault::NONE) {
    printEvent(F("reject"), F("fault_latched"));
    return false;
  }

  return true;
}

bool validatePwm(int pwmUs) {
  return pwmUs >= Config::PWM_TEST_MIN && pwmUs <= Config::PWM_TEST_MAX;
}

void beginMotion(int pwmUs, Mode mode) {
  setEscPwmUs(pwmUs);
  g_mode = mode;
  g_motionStartMs = millis();
  updateStatusLed();
}

void armSystem() {
  g_armed = true;
  g_fault = Fault::NONE;
  updateStatusLed();
  printEvent(F("armed"), F("ok"));
  printTelemetry(true);
}

void handleSetCommand(long pwmValue) {
  const int pwmUs = static_cast<int>(pwmValue);
  if (!validatePwm(pwmUs)) {
    printEvent(F("reject"), F("pwm_range"));
    return;
  }

  if (pwmUs == RcPwm::PWM_MIN) {
    stopMotion(true, Fault::NONE, F("idle"));
    return;
  }

  if (!canMove()) {
    return;
  }

  beginMotion(pwmUs, Mode::HOLD);
  printEventValue(F("set_pwm"), pwmUs);
  printTelemetry(true);
}

void startSweep(int startUs, int endUs, int stepUs, unsigned long holdMs) {
  g_sweep.active = true;
  g_sweep.currentUs = startUs;
  g_sweep.endUs = endUs;
  g_sweep.stepUs = stepUs;
  g_sweep.holdMs = holdMs;
  g_sweep.nextStepMs = millis() + holdMs;
  beginMotion(startUs, Mode::SWEEP);
  printEvent(F("sweep_start"), F("ok"));
  printTelemetry(true);
}

void handleSweepCommand(long startValue, long endValue, long stepValue, long holdValue) {
  const int startUs = static_cast<int>(startValue);
  const int endUs = static_cast<int>(endValue);
  const int rawStepUs = static_cast<int>(stepValue);
  const unsigned long holdMs = static_cast<unsigned long>(holdValue);

  if (!validatePwm(startUs) || !validatePwm(endUs)) {
    printEvent(F("reject"), F("pwm_range"));
    return;
  }

  if (rawStepUs == 0 || holdMs == 0UL) {
    printEvent(F("reject"), F("sweep_args"));
    return;
  }

  if (!canMove()) {
    return;
  }

  int signedStepUs = abs(rawStepUs);
  if (endUs < startUs) {
    signedStepUs = -signedStepUs;
  }

  startSweep(startUs, endUs, signedStepUs, holdMs);
}

void printHelp() {
  Serial.println(F("EVENT,help,ARM|STOP|SET <us>|SWEEP <start> <end> <step> <hold_ms>|STATUS|ZERO|HELP"));
}

void processCommand(char *line) {
  char *token = strtok(line, " ");
  if (token == nullptr) {
    return;
  }

  for (char *p = token; *p != '\0'; ++p) {
    *p = static_cast<char>(toupper(*p));
  }

  if (strcmp(token, "ARM") == 0) {
    armSystem();
    return;
  }

  if (strcmp(token, "STOP") == 0) {
    stopMotion(true, Fault::NONE, F("manual"));
    return;
  }

  if (strcmp(token, "STATUS") == 0) {
    printTelemetry(true);
    return;
  }

  if (strcmp(token, "ZERO") == 0) {
    if (g_currentPwmUs != RcPwm::PWM_MIN) {
      printEvent(F("reject"), F("motor_running"));
      return;
    }

    calibrateCurrentSensor();
    printEvent(F("zero"), F("done"));
    printTelemetry(true);
    return;
  }

  if (strcmp(token, "HELP") == 0) {
    printHelp();
    return;
  }

  if (strcmp(token, "SET") == 0) {
    char *arg = strtok(nullptr, " ");
    if (arg == nullptr) {
      printEvent(F("reject"), F("set_args"));
      return;
    }

    handleSetCommand(strtol(arg, nullptr, 10));
    return;
  }

  if (strcmp(token, "SWEEP") == 0) {
    char *argStart = strtok(nullptr, " ");
    char *argEnd = strtok(nullptr, " ");
    char *argStep = strtok(nullptr, " ");
    char *argHold = strtok(nullptr, " ");
    if (argStart == nullptr || argEnd == nullptr || argStep == nullptr || argHold == nullptr) {
      printEvent(F("reject"), F("sweep_args"));
      return;
    }

    handleSweepCommand(strtol(argStart, nullptr, 10),
                       strtol(argEnd, nullptr, 10),
                       strtol(argStep, nullptr, 10),
                       strtol(argHold, nullptr, 10));
    return;
  }

  printEvent(F("reject"), F("unknown_cmd"));
}

void processSerial() {
  while (Serial.available() > 0) {
    const char incoming = static_cast<char>(Serial.read());
    if (incoming == '\r') {
      continue;
    }

    if (incoming == '\n') {
      g_serialBuffer[g_serialBufferLen] = '\0';
      processCommand(g_serialBuffer);
      g_serialBufferLen = 0;
      continue;
    }

    if (g_serialBufferLen < (sizeof(g_serialBuffer) - 1U)) {
      g_serialBuffer[g_serialBufferLen++] = incoming;
    } else {
      g_serialBufferLen = 0;
      printEvent(F("reject"), F("line_too_long"));
    }
  }
}

void updateSweep() {
  if (!g_sweep.active || g_mode != Mode::SWEEP) {
    return;
  }

  const unsigned long nowMs = millis();
  if (nowMs < g_sweep.nextStepMs) {
    return;
  }

  if (g_sweep.currentUs == g_sweep.endUs) {
    stopMotion(true, Fault::NONE, F("sweep_complete"));
    return;
  }

  int nextUs = g_sweep.currentUs + g_sweep.stepUs;
  if ((g_sweep.stepUs > 0 && nextUs > g_sweep.endUs) ||
      (g_sweep.stepUs < 0 && nextUs < g_sweep.endUs)) {
    nextUs = g_sweep.endUs;
  }

  g_sweep.currentUs = nextUs;
  setEscPwmUs(nextUs);
  g_sweep.nextStepMs = nowMs + g_sweep.holdMs;
  printEventValue(F("sweep_pwm"), nextUs);
  printTelemetry(true);
}

void enforceSafety() {
  if (g_mode == Mode::IDLE || g_currentPwmUs <= RcPwm::PWM_MIN) {
    return;
  }

  const unsigned long nowMs = millis();
  if ((nowMs - g_motionStartMs) >= Config::MAX_ACTIVE_RUN_MS) {
    stopMotion(true, Fault::MAX_RUNTIME, F("max_runtime"));
    return;
  }

  if ((nowMs - g_motionStartMs) < Config::STALL_ARM_DELAY_MS) {
    return;
  }

  const float currentA = sampleCurrentA();
  if (fabs(currentA) < Config::STALL_CURRENT_A) {
    return;
  }

  const PulseSnapshot pulses = snapshotPulses();
  if (pulses.pulseCount == 0UL) {
    stopMotion(true, Fault::STALL, F("stall_no_pulses"));
    return;
  }

  const unsigned long lastHallAgeMs = (micros() - pulses.lastPulseMicros) / 1000UL;
  if (lastHallAgeMs >= Config::STALL_NO_PULSE_MS) {
    stopMotion(true, Fault::STALL, F("stall_timeout"));
  }
}

void setup() {
  pinMode(Pins::HALL_SENSOR, INPUT_PULLUP);
  pinMode(Pins::LED_STATUS, OUTPUT);
  digitalWrite(Pins::LED_STATUS, LOW);

  Serial.begin(Config::SERIAL_BAUD);
  g_esc.attach(Pins::ESC_SIGNAL, RcPwm::PWM_MIN, RcPwm::PWM_MAX);
  setEscPwmUs(RcPwm::PWM_MIN);

  const unsigned long nowMicros = micros();
  g_lastPulseMicros = nowMicros;
  g_lastHallEdgeMicros = nowMicros;
  attachInterrupt(digitalPinToInterrupt(Pins::HALL_SENSOR), hallISR, FALLING);

  calibrateCurrentSensor();
  resetSweep();

  delay(Config::ESC_ARM_DELAY_MS);

  Serial.println(F("Schleppseilwinde Testbench startet"));
  Serial.print(F("INFO,pwm_range,"));
  Serial.print(Config::PWM_TEST_MIN);
  Serial.print(F(","));
  Serial.println(Config::PWM_TEST_MAX);
  Serial.print(F("INFO,current_zero_v,"));
  Serial.println(g_currentZeroVoltage, 3);
  Serial.print(F("INFO,hall_pulses_per_rev,"));
  Serial.println(Config::HALL_PULSES_PER_REV);
  printCsvHeader();
  printHelp();
  printTelemetry(true);
}

void loop() {
  processSerial();
  updateSweep();
  enforceSafety();
  printTelemetry();
  updateStatusLed();
}
