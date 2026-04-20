/*
  ESP32 RC PWM Logger

  Diagnosesketch fuer die Stoersuche an der Schleppseilwinde.
  Startkonfiguration:
  - ein aktiver Messkanal
  - passives Mitschneiden des Arduino-Ausgangs D4 -> ESC
  - CSV-Ausgabe ueber USB-Serial

  CSV:
  time_us,channel,pulse_us,period_us,state
*/

#include <Arduino.h>

namespace Config {
  constexpr uint32_t SERIAL_BAUD = 115200;
  constexpr uint32_t TIMEOUT_US = 50000UL;
  constexpr uint32_t MIN_VALID_US = 800UL;
  constexpr uint32_t MAX_VALID_US = 2200UL;
  constexpr uint32_t DIAG_INTERVAL_MS = 1000UL;
}

struct ChannelConfig {
  const char *name;
  uint8_t pin;
};

struct ChannelRuntime {
  volatile uint32_t lastRiseUs;
  volatile uint32_t lastPeriodUs;
  volatile uint32_t lastPulseUs;
  volatile bool pulseReady;
  volatile bool timeoutReported;
};

struct Channel {
  ChannelConfig config;
  ChannelRuntime runtime;
};

// Fuer spaetere Erweiterung auf mehrere Kanaele weitere Eintraege ergaenzen.
Channel g_channels[] = {
    {{"D4", 18}, {0UL, 0UL, 0UL, false, false}},
};

constexpr size_t CHANNEL_COUNT = sizeof(g_channels) / sizeof(g_channels[0]);

portMUX_TYPE g_channelMux = portMUX_INITIALIZER_UNLOCKED;
unsigned long g_lastDiagMillis = 0UL;

void IRAM_ATTR handleEdge(Channel *channel) {
  const uint32_t nowUs = micros();
  const bool level = digitalRead(channel->config.pin);

  portENTER_CRITICAL_ISR(&g_channelMux);

  if (level) {
    if (channel->runtime.lastRiseUs != 0UL) {
      channel->runtime.lastPeriodUs = nowUs - channel->runtime.lastRiseUs;
    }
    channel->runtime.lastRiseUs = nowUs;
    channel->runtime.timeoutReported = false;
  } else if (channel->runtime.lastRiseUs != 0UL) {
    channel->runtime.lastPulseUs = nowUs - channel->runtime.lastRiseUs;
    channel->runtime.pulseReady = true;
  }

  portEXIT_CRITICAL_ISR(&g_channelMux);
}

void IRAM_ATTR isrChannel0() {
  handleEdge(&g_channels[0]);
}

using ChannelIsr = void (*)();

ChannelIsr g_channelIsrs[] = {
    isrChannel0,
};

static_assert(CHANNEL_COUNT == (sizeof(g_channelIsrs) / sizeof(g_channelIsrs[0])),
              "ISR table size must match channel count");

const char *classifyPulse(uint32_t pulseUs) {
  if (pulseUs < Config::MIN_VALID_US || pulseUs > Config::MAX_VALID_US) {
    return "OUT_OF_RANGE";
  }
  return "OK";
}

void printHeader() {
  Serial.println("time_us,channel,pulse_us,period_us,state");
}

void printDiagLine() {
  Serial.print("# diag,ms=");
  Serial.print(millis());
  for (size_t i = 0; i < CHANNEL_COUNT; ++i) {
    Serial.print(",channel=");
    Serial.print(g_channels[i].config.name);
    Serial.print(",pin=");
    Serial.print(g_channels[i].config.pin);
    Serial.print(",level=");
    Serial.print(digitalRead(g_channels[i].config.pin));
  }
  Serial.println();
}

void emitPulseIfReady(Channel *channel) {
  uint32_t pulseUs = 0UL;
  uint32_t periodUs = 0UL;
  bool ready = false;

  portENTER_CRITICAL(&g_channelMux);
  ready = channel->runtime.pulseReady;
  if (ready) {
    pulseUs = channel->runtime.lastPulseUs;
    periodUs = channel->runtime.lastPeriodUs;
    channel->runtime.pulseReady = false;
  }
  portEXIT_CRITICAL(&g_channelMux);

  if (!ready) {
    return;
  }

  Serial.print(micros());
  Serial.print(",");
  Serial.print(channel->config.name);
  Serial.print(",");
  Serial.print(pulseUs);
  Serial.print(",");
  Serial.print(periodUs);
  Serial.print(",");
  Serial.println(classifyPulse(pulseUs));
}

void emitTimeoutIfNeeded(Channel *channel) {
  const uint32_t nowUs = micros();
  uint32_t lastRiseUs = 0UL;
  bool alreadyReported = false;

  portENTER_CRITICAL(&g_channelMux);
  lastRiseUs = channel->runtime.lastRiseUs;
  alreadyReported = channel->runtime.timeoutReported;
  portEXIT_CRITICAL(&g_channelMux);

  if (lastRiseUs == 0UL) {
    return;
  }

  if (alreadyReported || (nowUs - lastRiseUs) <= Config::TIMEOUT_US) {
    return;
  }

  portENTER_CRITICAL(&g_channelMux);
  channel->runtime.timeoutReported = true;
  portEXIT_CRITICAL(&g_channelMux);

  Serial.print(nowUs);
  Serial.print(",");
  Serial.print(channel->config.name);
  Serial.print(",0,");
  Serial.print(nowUs - lastRiseUs);
  Serial.print(",");
  Serial.println("TIMEOUT");
}

void setup() {
  Serial.begin(Config::SERIAL_BAUD);
  delay(300);
  Serial.println("# esp32_rc_pwm_logger boot");
  printHeader();

  for (size_t i = 0; i < CHANNEL_COUNT; ++i) {
    pinMode(g_channels[i].config.pin, INPUT);
    attachInterrupt(digitalPinToInterrupt(g_channels[i].config.pin), g_channelIsrs[i], CHANGE);
  }

  printDiagLine();
}

void loop() {
  for (size_t i = 0; i < CHANNEL_COUNT; ++i) {
    emitPulseIfReady(&g_channels[i]);
  }

  for (size_t i = 0; i < CHANNEL_COUNT; ++i) {
    emitTimeoutIfNeeded(&g_channels[i]);
  }

  if ((millis() - g_lastDiagMillis) >= Config::DIAG_INTERVAL_MS) {
    g_lastDiagMillis = millis();
    printDiagLine();
  }

  delay(1);
}
