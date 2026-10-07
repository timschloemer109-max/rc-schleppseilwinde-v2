#pragma once
#include <Arduino.h>
namespace board {
constexpr uint8_t Hall = 2;
constexpr uint8_t EndSwitch = 7;
constexpr uint8_t Esc = 4;
constexpr uint8_t Current = A3;
constexpr uint8_t Led = LED_BUILTIN;
constexpr bool EndSwitchActiveLow = true;
#ifndef WINCH_BUS_BAUD
#define WINCH_BUS_BAUD 125000
#endif
constexpr uint32_t BusBaud = WINCH_BUS_BAUD;
static_assert(BusBaud == 125000 || BusBaud == 250000, "EX Bus baud must be 125000 or 250000");
constexpr uint32_t HallDebounceUs = 1000;
constexpr uint16_t AdcMaximum = 4095;
constexpr uint32_t AdcReferenceMv = 5000;
constexpr uint32_t CurrentSampleMs = 5;
constexpr uint8_t CurrentZeroSamples = 64;
constexpr uint8_t RxBudget = 64;
constexpr uint16_t BusBudgetUs = 350;
constexpr uint8_t UsbBudget = 32;
// No guessed GPIO mapping for the unqualified power stage or uniLIGHT module.
constexpr bool PowerAdapterQualified = false;
constexpr bool LightAdapterQualified = false;
constexpr bool CurrentCalibrationQualified = false;
// Explicit build-time integration step; never a remotely editable menu item.
constexpr bool MotorBenchQualified = false;
constexpr uint16_t CurrentSensitivityMvPerAmp = 0; // ACS712 variant unknown.
}  // namespace board
