/*
  RC Schleppseilwinde V2.0

  Erster hardwaretauglicher Stand auf Basis des V2-Entwurfs:
  - State Machine statt Zeitketten
  - Hall-Impulse per Interrupt
  - Blockade nur aus Strom + fehlenden Hall-Pulsen
  - Lernfunktion fuer Seillaenge mit EEPROM
  - pulsgesteuerte Umschaltung FAST -> SLOW
  - sichere Behandlung von RC-Signalverlust

  Hinweis:
  Die RC-Kanaele werden in diesem Stand noch mit pulseIn() gelesen.
  Das ist fuer die erste Hardware-Inbetriebnahme ok, aber spaeter ein
  guter Kandidat fuer eine interruptbasierte Verbesserung.

  Hardware-Referenz:
  - Hall-Sensor KY-003 mit A3144
  - 2 Magnete an der Trommel
  - ESC z. B. Turnigy Plush 30A
*/

#include <EEPROM.h>
#include <Servo.h>
#include <math.h>

namespace Pins {
  constexpr uint8_t HALL_SENSOR = 2;
  constexpr uint8_t RC_SPEED = 3;
  constexpr uint8_t ESC_SIGNAL = 4;
  constexpr uint8_t RC_TRIGGER = 5;
  constexpr uint8_t END_SWITCH = 7;
  constexpr uint8_t LED_STATUS = 13;
  constexpr uint8_t CURRENT_SENSOR = A3;
}

namespace RcPwm {
  constexpr int PWM_MIN = 1000;
  constexpr int PWM_MAX = 2000;
  constexpr int PWM_TRIGGER_RUN = 1800;
  constexpr int PWM_TRIGGER_RESET = 1200;
  constexpr int PWM_SLOW_FIXED = 1390;
  constexpr int PWM_SPEED_FALLBACK = 1500;
}

namespace Config {
  constexpr bool SERIAL_DEBUG = true;
  constexpr unsigned long DEBUG_INTERVAL_MS = 250UL;
  constexpr uint8_t HALL_PULSES_PER_REV = 2;

  constexpr bool END_SWITCH_ACTIVE_LOW = true;
  constexpr unsigned long MAX_RUN_TIME_MS = 20000UL;
  constexpr float MAX_CURRENT_A = 25.0f;
  constexpr unsigned long STALL_TIMEOUT_MS = 200UL;
  constexpr unsigned long STALL_ARM_DELAY_MS = 400UL;

  constexpr unsigned long LEARN_MIN_DURATION_MS = 2000UL;
  constexpr unsigned long LEARN_MIN_PULSES = 20UL;
  constexpr float LEARN_ALPHA = 0.20f;
  constexpr float LEARN_MIN_RELATIVE_OLD = 0.50f;
  constexpr float SLOW_PHASE_FRACTION = 0.80f;

  constexpr unsigned long RC_PULSE_TIMEOUT_US = 25000UL;
  constexpr unsigned long HALL_DEBOUNCE_US = 1000UL;

  constexpr uint8_t CURRENT_ZERO_SAMPLES = 64;
  constexpr float CURRENT_FILTER_ALPHA = 0.18f;
}

namespace CurrentSense {
  constexpr float ADC_REF_V = 5.0f;
  constexpr float ADC_COUNTS = 1024.0f;
  constexpr float SCALE_FACTOR_V_PER_A = 0.066f;
}

namespace EepromMap {
  constexpr int ADDR_ROPE_PULSES_AVG = 0;
  constexpr int ADDR_MAGIC = 16;
  constexpr uint16_t MAGIC_VALUE = 0x57D2;
}

enum class RunState : uint8_t {
  IDLE,
  RUN_FAST,
  RUN_SLOW,
  FAULT
};

enum class RcMode : uint8_t {
  STANDBY,
  RESET_LEARN,
  RUN
};

enum class StopReason : uint8_t {
  NONE,
  END_SWITCH,
  OVERCURRENT_STALL,
  MAX_RUNTIME,
  RC_SIGNAL_LOSS
};

struct PwmSample {
  int valueUs;
  bool valid;
};

struct PulseSnapshot {
  unsigned long pulseCount;
  unsigned long lastPulseMicros;
};

Servo g_esc;

volatile unsigned long g_pulseCount = 0;
volatile unsigned long g_lastPulseMicros = 0;
volatile unsigned long g_lastHallEdgeMicros = 0;

RunState g_runState = RunState::IDLE;
RcMode g_lastRcMode = RcMode::STANDBY;
StopReason g_stopReason = StopReason::NONE;

bool g_endSwitchLatched = false;
bool g_faultLatched = false;
bool g_learningActive = false;

unsigned long g_runStartMillis = 0;
unsigned long g_pulsesAtRunStart = 0;
unsigned long g_learnStartPulses = 0;
unsigned long g_learnStartMillis = 0;
unsigned long g_lastDebugMillis = 0;

float g_ropePulsesAvg = 0.0f;
float g_currentZeroVoltage = CurrentSense::ADC_REF_V * 0.5f;
float g_filteredCurrentA = 0.0f;
int g_lastValidSpeedPwmUs = RcPwm::PWM_SPEED_FALLBACK;

void hallISR() {
  const unsigned long nowMicros = micros();
  if ((nowMicros - g_lastHallEdgeMicros) < Config::HALL_DEBOUNCE_US) {
    return;
  }

  g_lastHallEdgeMicros = nowMicros;
  g_lastPulseMicros = nowMicros;
  g_pulseCount++;
}

bool isEndSwitchActive() {
  const int raw = digitalRead(Pins::END_SWITCH);
  return Config::END_SWITCH_ACTIVE_LOW ? (raw == LOW) : (raw == HIGH);
}

void debugPrint(const __FlashStringHelper *msg) {
  if (Config::SERIAL_DEBUG) {
    Serial.println(msg);
  }
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

void loadPersistentData() {
  uint16_t magic = 0;
  EEPROM.get(EepromMap::ADDR_MAGIC, magic);

  if (magic == EepromMap::MAGIC_VALUE) {
    EEPROM.get(EepromMap::ADDR_ROPE_PULSES_AVG, g_ropePulsesAvg);
    if (!(g_ropePulsesAvg >= 0.0f && g_ropePulsesAvg < 1000000.0f)) {
      g_ropePulsesAvg = 0.0f;
    }
  } else {
    g_ropePulsesAvg = 0.0f;
  }
}

void savePersistentData() {
  EEPROM.put(EepromMap::ADDR_ROPE_PULSES_AVG, g_ropePulsesAvg);
  EEPROM.put(EepromMap::ADDR_MAGIC, EepromMap::MAGIC_VALUE);
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

PwmSample readPwmSample(uint8_t pin, int fallbackUs) {
  const unsigned long pulse = pulseIn(pin, HIGH, Config::RC_PULSE_TIMEOUT_US);
  if (pulse < 900UL || pulse > 2100UL) {
    return {fallbackUs, false};
  }

  return {static_cast<int>(pulse), true};
}

RcMode decodeRcMode(const PwmSample &triggerSample) {
  if (!triggerSample.valid) {
    return RcMode::STANDBY;
  }

  if (triggerSample.valueUs <= RcPwm::PWM_TRIGGER_RESET) {
    return RcMode::RESET_LEARN;
  }

  if (triggerSample.valueUs >= RcPwm::PWM_TRIGGER_RUN) {
    return RcMode::RUN;
  }

  return RcMode::STANDBY;
}

float sampleCurrentA() {
  const float voltage = adcToVoltage(analogRead(Pins::CURRENT_SENSOR));
  const float rawCurrent = (voltage - g_currentZeroVoltage) / CurrentSense::SCALE_FACTOR_V_PER_A;

  g_filteredCurrentA += Config::CURRENT_FILTER_ALPHA * (rawCurrent - g_filteredCurrentA);
  return g_filteredCurrentA;
}

void updateRopeLengthAverage(unsigned long learnedPulses) {
  if (learnedPulses < Config::LEARN_MIN_PULSES) {
    return;
  }

  if (g_ropePulsesAvg <= 0.0f) {
    g_ropePulsesAvg = static_cast<float>(learnedPulses);
  } else {
    g_ropePulsesAvg =
        ((1.0f - Config::LEARN_ALPHA) * g_ropePulsesAvg) +
        (Config::LEARN_ALPHA * static_cast<float>(learnedPulses));
  }

  savePersistentData();
}

void resetFaults() {
  g_faultLatched = false;
  g_endSwitchLatched = false;
  g_stopReason = StopReason::NONE;
  g_runState = RunState::IDLE;
}

void stopWithReason(StopReason reason, bool latchFault) {
  g_stopReason = reason;
  g_runState = latchFault ? RunState::FAULT : RunState::IDLE;

  if (reason == StopReason::END_SWITCH) {
    g_endSwitchLatched = true;
  }

  if (latchFault) {
    g_faultLatched = true;
  }
}

void beginLearning() {
  resetFaults();
  g_learningActive = true;
  g_learnStartPulses = snapshotPulses().pulseCount;
  g_learnStartMillis = millis();
  debugPrint(F("RESET/LEARN gestartet"));
}

void finishLearning() {
  if (!g_learningActive) {
    return;
  }

  const PulseSnapshot pulses = snapshotPulses();
  const unsigned long learnedPulses = pulses.pulseCount - g_learnStartPulses;
  const unsigned long durationMs = millis() - g_learnStartMillis;

  const bool durationOk = (durationMs >= Config::LEARN_MIN_DURATION_MS);
  const bool pulsesOk = (learnedPulses >= Config::LEARN_MIN_PULSES);

  bool relativeOk = true;
  if (g_ropePulsesAvg > 0.0f) {
    const float minimumAllowed = g_ropePulsesAvg * Config::LEARN_MIN_RELATIVE_OLD;
    relativeOk = static_cast<float>(learnedPulses) >= minimumAllowed;
  }

  if (durationOk && pulsesOk && relativeOk) {
    updateRopeLengthAverage(learnedPulses);
    if (Config::SERIAL_DEBUG) {
      Serial.print(F("Lernwert uebernommen: "));
      Serial.print(learnedPulses);
      Serial.print(F(" Pulse, Mittelwert: "));
      Serial.println(g_ropePulsesAvg, 1);
    }
  } else if (Config::SERIAL_DEBUG) {
    Serial.print(F("Lernwert verworfen. pulses="));
    Serial.print(learnedPulses);
    Serial.print(F(" durationMs="));
    Serial.print(durationMs);
    Serial.print(F(" durationOk="));
    Serial.print(durationOk);
    Serial.print(F(" pulsesOk="));
    Serial.print(pulsesOk);
    Serial.print(F(" relativeOk="));
    Serial.println(relativeOk);
  }

  g_learningActive = false;
}

void startRun() {
  const PulseSnapshot pulses = snapshotPulses();
  g_runStartMillis = millis();
  g_pulsesAtRunStart = pulses.pulseCount;
  g_runState = RunState::RUN_FAST;
  g_stopReason = StopReason::NONE;
  debugPrint(F("RUN gestartet"));
}

void handleRcSignalFault(bool speedValid, bool triggerValid, RcMode rcMode) {
  const bool running = (g_runState == RunState::RUN_FAST || g_runState == RunState::RUN_SLOW);
  const bool startRequested = (g_runState == RunState::IDLE && rcMode == RcMode::RUN);

  if ((running || startRequested) && (!speedValid || !triggerValid)) {
    stopWithReason(StopReason::RC_SIGNAL_LOSS, true);
    debugPrint(F("Stop: RC-Signal ungueltig"));
  }
}

void updateRunState(float currentA) {
  if (!(g_runState == RunState::RUN_FAST || g_runState == RunState::RUN_SLOW)) {
    return;
  }

  const unsigned long nowMs = millis();
  const PulseSnapshot pulses = snapshotPulses();

  if ((nowMs - g_runStartMillis) > Config::MAX_RUN_TIME_MS) {
    stopWithReason(StopReason::MAX_RUNTIME, true);
    debugPrint(F("Stop: max Laufzeit"));
    return;
  }

  if (g_runState == RunState::RUN_FAST && g_ropePulsesAvg > 0.0f) {
    const unsigned long runPulses = pulses.pulseCount - g_pulsesAtRunStart;
    const unsigned long slowThreshold =
        static_cast<unsigned long>(g_ropePulsesAvg * Config::SLOW_PHASE_FRACTION);

    if (runPulses >= slowThreshold) {
      g_runState = RunState::RUN_SLOW;
      debugPrint(F("Wechsel auf SLOW"));
    }
  }

  if ((nowMs - g_runStartMillis) < Config::STALL_ARM_DELAY_MS) {
    return;
  }

  if (fabsf(currentA) < Config::MAX_CURRENT_A) {
    return;
  }

  const unsigned long timeSincePulseMs = (micros() - pulses.lastPulseMicros) / 1000UL;
  if (timeSincePulseMs > Config::STALL_TIMEOUT_MS) {
    stopWithReason(StopReason::OVERCURRENT_STALL, true);
    debugPrint(F("Stop: Stall erkannt"));
  }
}

int computeEscOutputUs() {
  switch (g_runState) {
    case RunState::IDLE:
    case RunState::FAULT:
      return RcPwm::PWM_MIN;

    case RunState::RUN_FAST:
      return constrain(g_lastValidSpeedPwmUs, RcPwm::PWM_MIN, RcPwm::PWM_MAX);

    case RunState::RUN_SLOW:
      return RcPwm::PWM_SLOW_FIXED;
  }

  return RcPwm::PWM_MIN;
}

void printDebug(const PwmSample &speedSample,
                const PwmSample &triggerSample,
                RcMode rcMode,
                float currentA) {
  if (!Config::SERIAL_DEBUG) {
    return;
  }

  if ((millis() - g_lastDebugMillis) < Config::DEBUG_INTERVAL_MS) {
    return;
  }

  g_lastDebugMillis = millis();
  const PulseSnapshot pulses = snapshotPulses();

  Serial.print(F("rcMode="));
  Serial.print(static_cast<int>(rcMode));
  Serial.print(F(" state="));
  Serial.print(static_cast<int>(g_runState));
  Serial.print(F(" speed="));
  Serial.print(speedSample.valueUs);
  Serial.print(F(" speedOk="));
  Serial.print(speedSample.valid);
  Serial.print(F(" trig="));
  Serial.print(triggerSample.valueUs);
  Serial.print(F(" trigOk="));
  Serial.print(triggerSample.valid);
  Serial.print(F(" I="));
  Serial.print(currentA, 2);
  Serial.print(F("A pulses="));
  Serial.print(pulses.pulseCount);
  Serial.print(F(" ropeAvg="));
  Serial.print(g_ropePulsesAvg, 1);
  Serial.print(F(" endLatch="));
  Serial.print(g_endSwitchLatched);
  Serial.print(F(" faultLatch="));
  Serial.print(g_faultLatched);
  Serial.print(F(" reason="));
  Serial.println(static_cast<int>(g_stopReason));
}

void setup() {
  pinMode(Pins::HALL_SENSOR, INPUT_PULLUP);
  pinMode(Pins::RC_SPEED, INPUT);
  pinMode(Pins::RC_TRIGGER, INPUT);
  pinMode(Pins::END_SWITCH, INPUT_PULLUP);
  pinMode(Pins::LED_STATUS, OUTPUT);

  g_esc.attach(Pins::ESC_SIGNAL, RcPwm::PWM_MIN, RcPwm::PWM_MAX);
  g_esc.writeMicroseconds(RcPwm::PWM_MIN);

  if (Config::SERIAL_DEBUG) {
    Serial.begin(115200);
    delay(50);
    Serial.println(F("Schleppseilwinde V2.0 startet"));
  }

  loadPersistentData();
  calibrateCurrentSensor();

  const unsigned long nowMicros = micros();
  g_lastPulseMicros = nowMicros;
  g_lastHallEdgeMicros = nowMicros;

  attachInterrupt(digitalPinToInterrupt(Pins::HALL_SENSOR), hallISR, FALLING);

  if (Config::SERIAL_DEBUG) {
    Serial.print(F("Rope avg pulses: "));
    Serial.println(g_ropePulsesAvg, 1);
    Serial.print(F("Current zero voltage: "));
    Serial.println(g_currentZeroVoltage, 3);
  }
}

void loop() {
  const PwmSample speedSample = readPwmSample(Pins::RC_SPEED, g_lastValidSpeedPwmUs);
  const PwmSample triggerSample = readPwmSample(Pins::RC_TRIGGER, RcPwm::PWM_MIN);
  const RcMode rcMode = decodeRcMode(triggerSample);

  if (speedSample.valid) {
    g_lastValidSpeedPwmUs = speedSample.valueUs;
  }

  const float currentA = sampleCurrentA();

  if (rcMode == RcMode::RESET_LEARN) {
    if (g_lastRcMode != RcMode::RESET_LEARN) {
      beginLearning();
    }
  } else if (g_lastRcMode == RcMode::RESET_LEARN) {
    finishLearning();
  }

  if (rcMode != RcMode::RESET_LEARN && isEndSwitchActive()) {
    stopWithReason(StopReason::END_SWITCH, false);
  }

  handleRcSignalFault(speedSample.valid, triggerSample.valid, rcMode);

  if (rcMode == RcMode::RUN &&
      g_runState == RunState::IDLE &&
      !g_faultLatched &&
      !g_endSwitchLatched &&
      !isEndSwitchActive() &&
      speedSample.valid &&
      triggerSample.valid) {
    startRun();
  }

  updateRunState(currentA);

  const int escOutputUs = computeEscOutputUs();
  g_esc.writeMicroseconds(escOutputUs);

  digitalWrite(Pins::LED_STATUS, (g_faultLatched || g_endSwitchLatched) ? HIGH : LOW);

  printDebug(speedSample, triggerSample, rcMode, currentA);
  g_lastRcMode = rcMode;
}
