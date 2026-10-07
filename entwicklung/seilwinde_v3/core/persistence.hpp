#ifndef WINCH_V3_PERSISTENCE_HPP
#define WINCH_V3_PERSISTENCE_HPP

#include "settings.hpp"
#include <stddef.h>

namespace winch {

class ByteStorage {
 public:
  virtual ~ByteStorage() {}
  virtual size_t size() const = 0;
  virtual uint8_t read(size_t address) const = 0;
  // False means the write did not complete. Adapter may skip an equal byte.
  virtual bool update(size_t address, uint8_t value) = 0;
};

enum class SaveResult : uint8_t { Saved, Unchanged, NotAllowed, Invalid, StorageError };

class SettingsJournal {
 public:
  static constexpr size_t RecordSize = 128;
  static constexpr size_t PayloadSize = 87;
  static constexpr uint16_t FormatVersion = 1;
  // R4 EEPROM emulation erases a 1024-byte flash block: keep slots in two blocks.
  explicit SettingsJournal(ByteStorage& storage, size_t slotStride = 1024);
  bool load(Settings& settings);  // False => validated safe defaults, never V2 import.
  SaveResult save(const Settings& settings, bool stoppedAndExplicitlyAccepted);
  uint32_t sequence() const { return sequence_; }
  int activeSlot() const { return activeSlot_; }

 private:
  ByteStorage& storage_;
  size_t stride_;
  int activeSlot_ = -1;
  uint32_t sequence_ = 0;
  bool scanned_ = false;
  Settings savedSettings_;
  bool storageFits() const;
  bool readSlot(size_t slot, Settings& settings, uint32_t& sequence) const;
};

// Fixed byte layout independent of compiler padding, bool layout or float format.
void encodeSettings(const Settings& settings, uint8_t* payload);
bool decodeSettings(const uint8_t* payload, Settings& settings);

}  // namespace winch
#endif
