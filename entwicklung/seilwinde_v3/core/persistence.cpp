#include "persistence.hpp"

namespace winch {
namespace {
constexpr uint32_t MAGIC = 0x30334e57U;  // Little endian "WN30".
constexpr uint8_t COMMITTED = 0xa5U;
constexpr size_t COMMIT_OFFSET = SettingsJournal::RecordSize - 1U;
void put16(uint8_t*& p, uint16_t value) {
  *p++ = static_cast<uint8_t>(value);
  *p++ = static_cast<uint8_t>(value >> 8U);
}
void put32(uint8_t*& p, uint32_t value) {
  put16(p, static_cast<uint16_t>(value));
  put16(p, static_cast<uint16_t>(value >> 16U));
}
uint16_t get16(const uint8_t*& p) {
  const uint16_t low = *p++;
  const uint16_t high = *p++;
  return static_cast<uint16_t>(low | static_cast<uint16_t>(high << 8U));
}
uint32_t get32(const uint8_t*& p) {
  const uint32_t low = get16(p);
  const uint32_t high = get16(p);
  return low | (high << 16U);
}
uint32_t crcByte(uint32_t crc, uint8_t value) {
  crc ^= value;
  for (uint8_t bit = 0; bit < 8U; ++bit)
    crc = (crc >> 1U) ^ ((crc & 1U) ? 0xedb88320U : 0U);
  return crc;
}
uint32_t recordCrc(const uint8_t* record) {
  uint32_t crc = 0xffffffffU;
  for (size_t i = 0; i < COMMIT_OFFSET; ++i)
    if (i < 12U || i >= 16U) crc = crcByte(crc, record[i]);
  return crc ^ 0xffffffffU;
}
bool later(uint32_t a, uint32_t b) {
  const uint32_t difference = a - b;
  return difference != 0U && difference < 0x80000000U;
}
}  // namespace

void encodeSettings(const Settings& s, uint8_t* p) {
#define WINCH_PUT32(field) put32(p, static_cast<uint32_t>(s.field))
#define WINCH_PUT16(field) put16(p, s.field)
#define WINCH_PUT8(field) *p++ = s.field
  WINCH_PUT32(ropeLengthMm); WINCH_PUT32(learnedPulses);
  WINCH_PUT16(fastPermille); WINCH_PUT16(warningPermille);
  WINCH_PUT16(stopUs); WINCH_PUT16(fastMaxUs); WINCH_PUT16(slowUs);
  WINCH_PUT32(maxRunMs); WINCH_PUT32(rcTimeoutMs); WINCH_PUT32(neutralMs);
  WINCH_PUT32(armMs); WINCH_PUT32(powerSettleMs); WINCH_PUT32(rampUpMs);
  WINCH_PUT32(rampDownMs); WINCH_PUT32(stallMaskMs); WINCH_PUT32(stallNoPulseMs);
  WINCH_PUT32(stallMilliAmp); WINCH_PUT32(learnMinMs); WINCH_PUT32(learnMaxMs);
  WINCH_PUT32(learnMinPulses); WINCH_PUT16(learnMinOldPermille);
  WINCH_PUT16(learnMaxOldPermille); WINCH_PUT16(learnAlphaPermille);
  WINCH_PUT16(warningLightUs); WINCH_PUT8(speedChannel); WINCH_PUT8(triggerChannel);
  WINCH_PUT8(lightChannel); WINCH_PUT8(powerChannel); WINCH_PUT8(modeStableFrames);
  WINCH_PUT8(currentCalibrated); WINCH_PUT8(stallEnabled);
  WINCH_PUT8(lightEnabled); WINCH_PUT8(powerEnabled);
#undef WINCH_PUT32
#undef WINCH_PUT16
#undef WINCH_PUT8
}
bool decodeSettings(const uint8_t* p, Settings& s) {
#define WINCH_GET32(field) s.field = get32(p)
#define WINCH_GET16(field) s.field = get16(p)
#define WINCH_GET8(field) s.field = *p++
  WINCH_GET32(ropeLengthMm); WINCH_GET32(learnedPulses);
  WINCH_GET16(fastPermille); WINCH_GET16(warningPermille);
  WINCH_GET16(stopUs); WINCH_GET16(fastMaxUs); WINCH_GET16(slowUs);
  WINCH_GET32(maxRunMs); WINCH_GET32(rcTimeoutMs); WINCH_GET32(neutralMs);
  WINCH_GET32(armMs); WINCH_GET32(powerSettleMs); WINCH_GET32(rampUpMs);
  WINCH_GET32(rampDownMs); WINCH_GET32(stallMaskMs); WINCH_GET32(stallNoPulseMs);
  const uint32_t current = get32(p);
  if (current > 100000U) return false;
  s.stallMilliAmp = static_cast<int32_t>(current);
  WINCH_GET32(learnMinMs); WINCH_GET32(learnMaxMs); WINCH_GET32(learnMinPulses);
  WINCH_GET16(learnMinOldPermille); WINCH_GET16(learnMaxOldPermille);
  WINCH_GET16(learnAlphaPermille); WINCH_GET16(warningLightUs);
  WINCH_GET8(speedChannel); WINCH_GET8(triggerChannel);
  WINCH_GET8(lightChannel); WINCH_GET8(powerChannel); WINCH_GET8(modeStableFrames);
  WINCH_GET8(currentCalibrated); WINCH_GET8(stallEnabled);
  WINCH_GET8(lightEnabled); WINCH_GET8(powerEnabled);
#undef WINCH_GET32
#undef WINCH_GET16
#undef WINCH_GET8
  return validSettings(s);
}

SettingsJournal::SettingsJournal(ByteStorage& storage, size_t slotStride)
    : storage_(storage), stride_(slotStride), savedSettings_(Settings::defaults()) {}
bool SettingsJournal::storageFits() const {
  return stride_ >= RecordSize && storage_.size() >= RecordSize &&
      stride_ <= storage_.size() - RecordSize;
}
bool SettingsJournal::readSlot(size_t slot, Settings& settings, uint32_t& sequence) const {
  uint8_t record[RecordSize];
  const size_t base = slot * stride_;
  for (size_t i = 0; i < RecordSize; ++i) record[i] = storage_.read(base + i);
  if (record[COMMIT_OFFSET] != COMMITTED) return false;
  const uint8_t* p = record;
  if (get32(p) != MAGIC || get16(p) != FormatVersion || get16(p) != PayloadSize)
    return false;
  sequence = get32(p);
  if (get32(p) != recordCrc(record)) return false;
  return decodeSettings(record + 16U, settings);
}
bool SettingsJournal::load(Settings& settings) {
  scanned_ = true;
  activeSlot_ = -1;
  sequence_ = 0;
  settings = Settings::defaults();
  if (!storageFits()) return false;
  Settings a = settings;
  Settings b = settings;
  uint32_t aSequence = 0;
  uint32_t bSequence = 0;
  const bool aGood = readSlot(0, a, aSequence);
  const bool bGood = readSlot(1, b, bSequence);
  if (!aGood && !bGood) return false;
  if (bGood && (!aGood || later(bSequence, aSequence))) {
    activeSlot_ = 1; sequence_ = bSequence; settings = b;
  } else {
    activeSlot_ = 0; sequence_ = aSequence; settings = a;
  }
  savedSettings_ = settings;
  return true;
}
SaveResult SettingsJournal::save(const Settings& settings, bool accepted) {
  if (!accepted) return SaveResult::NotAllowed;
  if (!validSettings(settings)) return SaveResult::Invalid;
  if (!storageFits()) return SaveResult::StorageError;
  if (!scanned_) { Settings previous; load(previous); }
  if (activeSlot_ >= 0 && equalSettings(settings, savedSettings_)) return SaveResult::Unchanged;
  uint8_t record[RecordSize] = {};
  uint8_t* p = record;
  put32(p, MAGIC); put16(p, FormatVersion); put16(p, PayloadSize);
  put32(p, sequence_ + 1U); put32(p, 0);
  encodeSettings(settings, p);
  p = record + 12U;
  put32(p, recordCrc(record));
  record[COMMIT_OFFSET] = COMMITTED;
  const size_t targetSlot = activeSlot_ == 0 ? 1U : 0U;
  const size_t base = targetSlot * stride_;
  // Invalidate first, commit last. The previous slot lives in another erase block.
  if (!storage_.update(base + COMMIT_OFFSET, 0)) return SaveResult::StorageError;
  for (size_t i = 0; i < COMMIT_OFFSET; ++i)
    if (!storage_.update(base + i, record[i])) return SaveResult::StorageError;
  if (!storage_.update(base + COMMIT_OFFSET, COMMITTED)) return SaveResult::StorageError;
  Settings checked;
  uint32_t checkedSequence = 0;
  if (!readSlot(targetSlot, checked, checkedSequence) || !equalSettings(settings, checked))
    return SaveResult::StorageError;
  activeSlot_ = static_cast<int>(targetSlot);
  sequence_ = checkedSequence;
  savedSettings_ = checked;
  return SaveResult::Saved;
}
}  // namespace winch
