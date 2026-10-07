#pragma once
#include <Arduino.h>
#include <analog.h>
#include "board_config.hpp"

// One FSP status poll per loop, no analogRead() conversion spin loop.
class R4Current {
 public:
  bool begin() {
    const int32_t index = digitalPinToAnalogPin(board::Current);
    const auto configs = getPinCfgs(index, PIN_CFG_REQ_ADC);
    if (configs[0] == 0 || IS_ADC1(configs[0])) return false;
    channel_ = static_cast<adc_channel_t>(GET_CHANNEL(configs[0]));
    // RA4M1 supports 14-bit ADC; select real 12-bit conversion explicitly so
    // counts/reference math matches 0..4095, not merely Arduino display scaling.
    adc_.cfg.resolution = ADC_RESOLUTION_12_BIT;
    adc_.channel_cfg.scan_mask = 1UL << static_cast<unsigned>(channel_);
    pinPeripheral(digitalPinToBspPin(index), static_cast<uint32_t>(IOPORT_CFG_ANALOG_ENABLE));
    healthy_ = R_ADC_Open(&adc_.ctrl, &adc_.cfg) == FSP_SUCCESS &&
        R_ADC_ScanCfg(&adc_.ctrl, &adc_.channel_cfg) == FSP_SUCCESS;
    return healthy_;
  }
  void tick(uint32_t nowMs) {
    if (!healthy_) return;
    if (pending_) {
      adc_status_t status{};
      if (R_ADC_StatusGet(&adc_.ctrl, &status) != FSP_SUCCESS) { healthy_ = false; return; }
      if (status.state == ADC_STATE_SCAN_IN_PROGRESS) {
        if (static_cast<uint32_t>(nowMs - startedMs_) > 10U) {
          R_ADC_ScanStop(&adc_.ctrl); pending_ = false; healthy_ = false;
        }
        return;
      }
      uint16_t sample = 0;
      if (R_ADC_Read(&adc_.ctrl, channel_, &sample) != FSP_SUCCESS) { healthy_ = false; return; }
      raw_ = sample; pending_ = false;
      if (zeroCount_ < board::CurrentZeroSamples) {
        zeroSum_ += sample; ++zeroCount_;
        if (zeroCount_ == board::CurrentZeroSamples) zero_ = zeroSum_ / board::CurrentZeroSamples;
      } else if (board::CurrentSensitivityMvPerAmp != 0) {
        const int32_t mA = convert(sample, zero_, board::CurrentSensitivityMvPerAmp);
        filteredMa_ += (mA - filteredMa_) / 4;
      }
    }
    if (static_cast<uint32_t>(nowMs - startedMs_) >= board::CurrentSampleMs) {
      pending_ = R_ADC_ScanStart(&adc_.ctrl) == FSP_SUCCESS;
      if (!pending_) healthy_ = false;
      startedMs_ = nowMs;
    }
  }
  bool valid() const { return healthy_ && zeroCount_ == board::CurrentZeroSamples &&
    board::CurrentCalibrationQualified && board::CurrentSensitivityMvPerAmp != 0; }
  int32_t milliAmp() const { return filteredMa_; }
  uint16_t raw() const { return raw_; }
  bool healthy() const { return healthy_; }
 private:
  static int32_t convert(uint16_t sample, uint32_t zero, uint16_t sensitivity) {
    if (sensitivity == 0) return 0;
    const int64_t counts = static_cast<int32_t>(sample) - static_cast<int32_t>(zero);
    return static_cast<int32_t>(counts * board::AdcReferenceMv * 1000 /
      (static_cast<int64_t>(board::AdcMaximum) * sensitivity));
  }
  ADC_Container adc_{0, nullptr};
  adc_channel_t channel_ = ADC_CHANNEL_0;
  bool healthy_ = false, pending_ = false;
  uint8_t zeroCount_ = 0;
  uint32_t zeroSum_ = 0, zero_ = 0, startedMs_ = 0;
  uint16_t raw_ = 0;
  int32_t filteredMa_ = 0;
};
