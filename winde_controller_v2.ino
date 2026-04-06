/*
  ================================================================
  RC Schleppseilwinde – Controller V2 (Grundversion)
  ---------------------------------------------------------------
  Ziel:
  - Robuste Aufrollsteuerung mit ESC + Motor
  - Endschalter-Abschaltung
  - Stall-/Blockade-Erkennung über Strom + Drehzahlimpulse
  - Lernfunktion für Seillänge (Umdrehungs-/Impulszahl)
  - Umschaltung von schnell auf langsam auf Basis der gelernten Länge
  - Bedienung über 3-Stufen-Schalter am RC-Empfänger

  Bedienlogik:
  - Schalter unten  : RESET / LEARN
      * Fehler quittieren
      * Endschalter quittieren
      * Während dieser Stellung kann das Seil herausgezogen werden
      * Beim Verlassen der Stellung kann die neue Seillänge gelernt werden
  - Schalter Mitte  : STANDBY
      * Motor aus
  - Schalter oben   : RUN
      * Aufrollen starten
      * Danach darf der Schalter wieder in die Mitte

  Hardware (aktuell vorgesehen):
  - Arduino Nano
  - ESC für Bürsten-/Brushless-Antrieb (Servo-PWM 1000..2000 us)
  - ACS712 Stromsensor an A3
  - Hall-Sensor an D2 (Interrupt-Pin) für Trommelimpulse
  - Endschalter an D7 (INPUT_PULLUP)
  - RC-Kanal Geschwindigkeit an D3
  - RC-Kanal 3-Stufen-Auslöser an D5
  - Status-LED an D13

  Wichtige Hinweise:
  - Hall-Sensor liegt absichtlich auf D2 oder alternativ D3, da Interrupt nötig.
  - Endschalter braucht keinen Interrupt und wird im loop() gepollt.
  - Diese Datei ist als solides Fundament gedacht und muss auf echter Hardware
    eingemessen werden (Stromgrenzen, langsame PWM, Zeitkonstanten, etc.).
  ================================================================
*/

#include <Servo.h>
#include <EEPROM.h>

// -----------------------------------------------------------------------------
// Pinbelegung
// -----------------------------------------------------------------------------
namespace Pins {
  constexpr uint8_t END_SWITCH        = 7;   // Endschalter (INPUT_PULLUP)
  constexpr uint8_t LED_STATUS        = 13;  // Status-LED
  constexpr uint8_t RC_SPEED          = 3;   // RC-Kanal: Geschwindigkeit
  constexpr uint8_t RC_TRIGGER        = 5;   // RC-Kanal: 3-Stufen-Schalter
  constexpr uint8_t ESC_SIGNAL        = 4;   // ESC-Signal
  constexpr uint8_t CURRENT_SENSOR    = A3;  // ACS712
  constexpr uint8_t HALL_SENSOR       = 2;   // Hall-Sensor (Interrupt!)
}

// -----------------------------------------------------------------------------
// RC-/ESC-Parameter
// -----------------------------------------------------------------------------
namespace RcPwm {
  constexpr int PWM_MIN              = 1000;
  constexpr int PWM_MAX              = 2000;
  constexpr int PWM_TRIGGER_ACTIVE   = 1800; // Schalter oben
  constexpr int PWM_TRIGGER_RESET    = 1200; // Schalter unten
  constexpr int PWM_SLOW_FIXED       = 1390; // langsame Phase am Ende
  constexpr int PWM_SAFE_FALLBACK    = 1000; // Fallback bei Signalausfall
}

// -----------------------------------------------------------------------------
// Schutz- und Lernparameter
// -----------------------------------------------------------------------------
namespace Config {
  constexpr unsigned long MAX_RUN_TIME_MS         = 20000UL;
  constexpr float         MAX_CURRENT_A           = 25.0f;
  constexpr unsigned long STALL_TIMEOUT_MS        = 200UL;

  constexpr unsigned long LEARN_MIN_DURATION_MS   = 2000UL;
  constexpr unsigned long MIN_LEARN_PULSES        = 20UL;
  constexpr float         LEARN_ALPHA             = 0.20f; // 20% neuer Wert
  constexpr float         SLOW_PHASE_FRACTION     = 0.80f; // ab 80 % langsam
  constexpr float         LEARN_MIN_RELATIVE_OLD  = 0.50f; // mind. 50% alter Wert

  constexpr unsigned long RC_PULSE_TIMEOUT_US     = 25000UL;

  // Debugausgaben
  constexpr bool SERIAL_DEBUG                     = true;
  constexpr unsigned long DEBUG_INTERVAL_MS       = 250UL;
}

// -----------------------------------------------------------------------------
// ACS712-Kalibrierung (aus bisherigem Projektstand übernommen)
// -----------------------------------------------------------------------------
namespace CurrentSense {
  constexpr float VREF_V         = 6.03f;
  constexpr float ADC_COUNTS     = 1024.0f;
  constexpr float ADC_CORR       = 1.22f;
  constexpr float SCALE_FACTOR   = 0.066f; // ACS712 30A-Version

  constexpr float RES_ADC_V      = VREF_V / ADC_COUNTS / ADC_CORR;
  constexpr float ZERO_POINT_V   = VREF_V / 2.0f;
  constexpr uint8_t SAMPLES      = 10;
}

// -----------------------------------------------------------------------------
// EEPROM-Adressen
// -----------------------------------------------------------------------------
namespace EepromMap {
  constexpr int ADDR_ROPE_PULSES_AVG = 0; // float
  constexpr int ADDR_MAGIC           = 16; // uint16_t
  constexpr uint16_t MAGIC_VALUE     = 0x57D2;
}

// -----------------------------------------------------------------------------
// Zustände und Modi
// -----------------------------------------------------------------------------
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
  MAX_RUNTIME
};

// -----------------------------------------------------------------------------
// Globale Variablen
// -----------------------------------------------------------------------------
Servo esc;

volatile unsigned long g_pulseCount = 0;
volatile unsigned long g_lastPulseMillis = 0;

RunState   g_runState       = RunState::IDLE;
RcMode     g_lastRcMode     = RcMode::STANDBY;
StopReason g_stopReason     = StopReason::NONE;

bool g_endSwitchLatched     = false;
bool g_overcurrentFault     = false;
bool g_learningActive       = false;

unsigned long g_runStartMillis   = 0;
unsigned long g_pulsesAtRunStart = 0;
unsigned long g_learnStartPulses = 0;
unsigned long g_learnStartMillis = 0;
unsigned long g_lastDebugMillis  = 0;

float g_ropePulsesAvg = 0.0f;

// -----------------------------------------------------------------------------
// Interrupt-Service-Routine
// -----------------------------------------------------------------------------
void hallISR() {
  g_pulseCount++;
  g_lastPulseMillis = millis();
}

// -----------------------------------------------------------------------------
// Hilfsfunktionen
// -----------------------------------------------------------------------------
void debugPrint(const __FlashStringHelper* msg) {
  if (Config::SERIAL_DEBUG) {
    Serial.println(msg);
  }
}

void loadConfigFromEeprom() {
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

void saveConfigToEeprom() {
  EEPROM.put(EepromMap::ADDR_ROPE_PULSES_AVG, g_ropePulsesAvg);
  EEPROM.put(EepromMap::ADDR_MAGIC, EepromMap::MAGIC_VALUE);
}

int readPwmSafe(uint8_t pin, int fallbackUs) {
  const unsigned long pulse = pulseIn(pin, HIGH, Config::RC_PULSE_TIMEOUT_US);
  if (pulse < 900UL || pulse > 2100UL) {
    return fallbackUs;
  }
  return static_cast<int>(pulse);
}

RcMode decodeRcMode(int triggerPwmUs) {
  if (triggerPwmUs <= RcPwm::PWM_TRIGGER_RESET) {
    return RcMode::RESET_LEARN;
  }
  if (triggerPwmUs >= RcPwm::PWM_TRIGGER_ACTIVE) {
    return RcMode::RUN;
  }
  return RcMode::STANDBY;
}

float readCurrentA() {
  float vOut = 0.0f;
  for (uint8_t i = 0; i < CurrentSense::SAMPLES; ++i) {
    vOut += (CurrentSense::RES_ADC_V * analogRead(Pins::CURRENT_SENSOR));
    delay(1);
  }
  vOut /= static_cast<float>(CurrentSense::SAMPLES);
  return (vOut - CurrentSense::ZERO_POINT_V) / CurrentSense::SCALE_FACTOR;
}

void updateRopeLengthAverage(unsigned long newPulses) {
  if (newPulses < Config::MIN_LEARN_PULSES) {
    return;
  }

  if (g_ropePulsesAvg <= 0.0f) {
    g_ropePulsesAvg = static_cast<float>(newPulses);
  } else {
    g_ropePulsesAvg = (1.0f - Config::LEARN_ALPHA) * g_ropePulsesAvg
                    + Config::LEARN_ALPHA * static_cast<float>(newPulses);
  }

  saveConfigToEeprom();
}

void resetFaultsAndLatch() {
  g_overcurrentFault = false;
  g_endSwitchLatched = false;
  g_stopReason       = StopReason::NONE;
  g_runState         = RunState::IDLE;
}

void stopWithReason(StopReason reason, bool latchFault) {
  g_runState   = (latchFault ? RunState::FAULT : RunState::IDLE);
  g_stopReason = reason;

  if (reason == StopReason::END_SWITCH) {
    g_endSwitchLatched = true;
  }
  if (reason == StopReason::OVERCURRENT_STALL || reason == StopReason::MAX_RUNTIME) {
    g_overcurrentFault = true;
  }
}

int computeEscOutputUs(int speedPwmUs) {
  switch (g_runState) {
    case RunState::IDLE:
    case RunState::FAULT:
      return RcPwm::PWM_MIN;

    case RunState::RUN_FAST:
      return constrain(speedPwmUs, RcPwm::PWM_MIN, RcPwm::PWM_MAX);

    case RunState::RUN_SLOW:
      return RcPwm::PWM_SLOW_FIXED;
  }

  return RcPwm::PWM_MIN;
}

void printDebug(RcMode rcMode, int speedPwmUs, int triggerPwmUs, float currentA) {
  if (!Config::SERIAL_DEBUG) return;
  if (millis() - g_lastDebugMillis < Config::DEBUG_INTERVAL_MS) return;
  g_lastDebugMillis = millis();

  unsigned long pulseCountSnapshot;
  unsigned long lastPulseSnapshot;

  noInterrupts();
  pulseCountSnapshot = g_pulseCount;
  lastPulseSnapshot  = g_lastPulseMillis;
  interrupts();

  Serial.print(F("RC mode="));
  Serial.print(static_cast<int>(rcMode));
  Serial.print(F(" state="));
  Serial.print(static_cast<int>(g_runState));
  Serial.print(F(" trig="));
  Serial.print(triggerPwmUs);
  Serial.print(F(" speed="));
  Serial.print(speedPwmUs);
  Serial.print(F(" I="));
  Serial.print(currentA, 2);
  Serial.print(F("A pulses="));
  Serial.print(pulseCountSnapshot);
  Serial.print(F(" ropeAvg="));
  Serial.print(g_ropePulsesAvg, 1);
  Serial.print(F(" lastPulseAgo="));
  Serial.print(millis() - lastPulseSnapshot);
  Serial.print(F("ms stopReason="));
  Serial.println(static_cast<int>(g_stopReason));
}

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------
void setup() {
  pinMode(Pins::END_SWITCH, INPUT_PULLUP);
  pinMode(Pins::LED_STATUS, OUTPUT);
  pinMode(Pins::HALL_SENSOR, INPUT_PULLUP);

  esc.attach(Pins::ESC_SIGNAL, RcPwm::PWM_MIN, RcPwm::PWM_MAX);
  esc.writeMicroseconds(RcPwm::PWM_MIN);

  if (Config::SERIAL_DEBUG) {
    Serial.begin(115200);
    delay(50);
    Serial.println(F("RC Schleppseilwinde V2 startet..."));
  }

  loadConfigFromEeprom();

  g_lastPulseMillis = millis();
  attachInterrupt(digitalPinToInterrupt(Pins::HALL_SENSOR), hallISR, FALLING);

  digitalWrite(Pins::LED_STATUS, LOW);

  if (Config::SERIAL_DEBUG) {
    Serial.print(F("Gelernte Seillaenge (Pulse): "));
    Serial.println(g_ropePulsesAvg, 1);
  }
}

// -----------------------------------------------------------------------------
// Hauptloop
// -----------------------------------------------------------------------------
void loop() {
  const int speedPwmUs   = readPwmSafe(Pins::RC_SPEED,   RcPwm::PWM_SLOW_FIXED);
  const int triggerPwmUs = readPwmSafe(Pins::RC_TRIGGER, RcPwm::PWM_SAFE_FALLBACK);
  const RcMode rcMode    = decodeRcMode(triggerPwmUs);

  // Endschalter abfragen und sofort stoppen
  if (digitalRead(Pins::END_SWITCH) == LOW) {
    stopWithReason(StopReason::END_SWITCH, false);
  }

  // RESET / LEARN betreten
  if (rcMode == RcMode::RESET_LEARN) {
    if (g_lastRcMode != RcMode::RESET_LEARN) {
      resetFaultsAndLatch();
      g_learningActive   = true;
      g_learnStartPulses = g_pulseCount;
      g_learnStartMillis = millis();
      debugPrint(F("RESET/LEARN gestartet"));
    }
  } else {
    // RESET / LEARN verlassen -> ggf. Seillaenge uebernehmen
    if (g_lastRcMode == RcMode::RESET_LEARN && g_learningActive) {
      const unsigned long newPulses = g_pulseCount - g_learnStartPulses;
      const unsigned long duration  = millis() - g_learnStartMillis;

      bool durationOk = (duration >= Config::LEARN_MIN_DURATION_MS);
      bool pulsesOk   = (newPulses >= Config::MIN_LEARN_PULSES);
      bool relativeOk = true;

      if (g_ropePulsesAvg > 0.0f) {
        const float minAllowed = g_ropePulsesAvg * Config::LEARN_MIN_RELATIVE_OLD;
        if (static_cast<float>(newPulses) < minAllowed) {
          relativeOk = false;
        }
      }

      if (durationOk && pulsesOk && relativeOk) {
        updateRopeLengthAverage(newPulses);
        if (Config::SERIAL_DEBUG) {
          Serial.print(F("Neue Seillaenge gelernt: "));
          Serial.print(newPulses);
          Serial.print(F(" Pulse, Mittelwert jetzt: "));
          Serial.println(g_ropePulsesAvg, 1);
        }
      } else {
        if (Config::SERIAL_DEBUG) {
          Serial.print(F("Lernen verworfen: pulses="));
          Serial.print(newPulses);
          Serial.print(F(" duration="));
          Serial.print(duration);
          Serial.print(F("ms durationOk="));
          Serial.print(durationOk);
          Serial.print(F(" pulsesOk="));
          Serial.print(pulsesOk);
          Serial.print(F(" relativeOk="));
          Serial.println(relativeOk);
        }
      }

      g_learningActive = false;
    }
  }

  // RUN starten (nur wenn kein Fehler und kein Endschalter-Latch aktiv)
  if (rcMode == RcMode::RUN &&
      g_runState == RunState::IDLE &&
      !g_overcurrentFault &&
      !g_endSwitchLatched) {

    g_runStartMillis   = millis();
    g_pulsesAtRunStart = g_pulseCount;
    g_runState         = RunState::RUN_FAST;
    debugPrint(F("RUN gestartet (FAST)"));
  }

  // Max. Laufzeit ueberwachen
  if ((g_runState == RunState::RUN_FAST || g_runState == RunState::RUN_SLOW) &&
      (millis() - g_runStartMillis > Config::MAX_RUN_TIME_MS)) {
    stopWithReason(StopReason::MAX_RUNTIME, true);
    debugPrint(F("Stop: max. Laufzeit ueberschritten"));
  }

  // Umschalten auf langsam anhand gelernter Seillaenge
  if (g_runState == RunState::RUN_FAST && g_ropePulsesAvg > 0.0f) {
    const unsigned long runPulses = g_pulseCount - g_pulsesAtRunStart;
    const unsigned long slowThreshold =
        static_cast<unsigned long>(g_ropePulsesAvg * Config::SLOW_PHASE_FRACTION);

    if (runPulses >= slowThreshold) {
      g_runState = RunState::RUN_SLOW;
      debugPrint(F("Wechsel auf SLOW"));
    }
  }

  // Stall-Erkennung = hoher Strom + keine Drehimpulse fuer gewisse Zeit
  const float currentA = readCurrentA();
  if (g_runState == RunState::RUN_FAST || g_runState == RunState::RUN_SLOW) {
    if (currentA >= Config::MAX_CURRENT_A) {
      unsigned long lastPulseSnapshot;
      noInterrupts();
      lastPulseSnapshot = g_lastPulseMillis;
      interrupts();

      const unsigned long msSincePulse = millis() - lastPulseSnapshot;
      if (msSincePulse > Config::STALL_TIMEOUT_MS) {
        stopWithReason(StopReason::OVERCURRENT_STALL, true);
        debugPrint(F("Stop: Ueberstrom + fehlende Hall-Impulse (Stall)"));
      }
    }
  }

  // ESC ansteuern
  const int escOutputUs = computeEscOutputUs(speedPwmUs);
  esc.writeMicroseconds(escOutputUs);

  // LED-Logik
  digitalWrite(Pins::LED_STATUS, (g_overcurrentFault || g_endSwitchLatched) ? HIGH : LOW);

  // Debug
  printDebug(rcMode, speedPwmUs, triggerPwmUs, currentA);

  // letzten RC-Modus merken
  g_lastRcMode = rcMode;
}
