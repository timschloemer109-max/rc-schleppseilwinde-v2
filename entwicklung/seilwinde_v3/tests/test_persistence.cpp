#include "persistence.hpp"
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace winch;
namespace {
unsigned checks = 0;
#define CHECK(condition) do { ++checks; if (!(condition)) { \
  std::cerr << __FILE__ << ':' << __LINE__ << ": " #condition "\n"; std::exit(1); \
} } while (false)

class Memory : public ByteStorage {
 public:
  std::vector<uint8_t> bytes;
  size_t updates = 0;
  size_t failAt = static_cast<size_t>(-1);
  bool erasePageOnFailure = false;
  explicit Memory(size_t count = 2048) : bytes(count, 0xff) {}
  size_t size() const override { return bytes.size(); }
  uint8_t read(size_t at) const override { CHECK(at < bytes.size()); return bytes[at]; }
  bool update(size_t at, uint8_t value) override {
    CHECK(at < bytes.size());
    if (updates++ == failAt) {
      if (erasePageOnFailure) {
        const size_t base = (at / 1024U) * 1024U;
        for (size_t i = base; i < base + 1024U && i < bytes.size(); ++i) bytes[i] = 0xff;
      }
      return false;
    }
    bytes[at] = value;
    return true;
  }
};

void basicAndInvalid() {
  Memory memory;
  SettingsJournal journal(memory);
  Settings loaded;
  CHECK(!journal.load(loaded));
  CHECK(equalSettings(loaded, Settings::defaults()));
  CHECK(journal.save(loaded, false) == SaveResult::NotAllowed);
  CHECK(memory.updates == 0U);
  CHECK(journal.save(loaded, true) == SaveResult::Saved);
  const size_t once = memory.updates;
  CHECK(journal.save(loaded, true) == SaveResult::Unchanged);
  CHECK(memory.updates == once);
  loaded.fastPermille = 800;
  CHECK(journal.save(loaded, true) == SaveResult::Saved);
  CHECK(journal.activeSlot() == 1);
  CHECK(journal.sequence() == 2U);
  SettingsJournal reboot(memory);
  Settings recovered;
  CHECK(reboot.load(recovered));
  CHECK(equalSettings(loaded, recovered));
  loaded.stallEnabled = 1;
  CHECK(journal.save(loaded, true) == SaveResult::Invalid);
  CHECK(memory.updates == once * 2U);
  Memory small(100);
  SettingsJournal unavailable(small);
  CHECK(!unavailable.load(recovered));
  CHECK(unavailable.save(recovered, true) == SaveResult::StorageError);
  SettingsJournal overlapping(memory, 127);
  CHECK(!overlapping.load(recovered));
  CHECK(overlapping.save(recovered, true) == SaveResult::StorageError);
}

void everyInterruptedWrite() {
  Memory original;
  Settings first = Settings::defaults(); first.learnedPulses = 321;
  SettingsJournal originalJournal(original);
  CHECK(originalJournal.save(first, true) == SaveResult::Saved);
  Settings second = first; second.fastPermille = 800;
  for (bool wholeErasePage : {false, true}) {
    for (size_t cut = 0; cut <= SettingsJournal::RecordSize; ++cut) {
      Memory interrupted = original;
      interrupted.updates = 0;
      interrupted.failAt = cut;
      interrupted.erasePageOnFailure = wholeErasePage;
      SettingsJournal saving(interrupted);
      CHECK(saving.save(second, true) == SaveResult::StorageError);
      SettingsJournal reboot(interrupted);
      Settings recovered;
      CHECK(reboot.load(recovered));
      CHECK(equalSettings(recovered, first));  // Previous slot survives even entire page loss.
    }
  }
  Memory completed = original;
  completed.updates = 0;
  completed.failAt = SettingsJournal::RecordSize + 1U;
  SettingsJournal saving(completed);
  CHECK(saving.save(second, true) == SaveResult::Saved);
  SettingsJournal reboot(completed);
  Settings recovered;
  CHECK(reboot.load(recovered));
  CHECK(equalSettings(recovered, second));

  // Tear a replacement of slot zero as well, after the second slot became latest.
  Settings third = second; third.slowUs = 1300;
  for (size_t cut = 0; cut <= SettingsJournal::RecordSize; ++cut) {
    Memory interrupted = completed;
    interrupted.updates = 0;
    interrupted.failAt = cut;
    interrupted.erasePageOnFailure = true;
    SettingsJournal replacement(interrupted);
    CHECK(replacement.save(third, true) == SaveResult::StorageError);
    SettingsJournal replacementReboot(interrupted);
    CHECK(replacementReboot.load(recovered));
    CHECK(equalSettings(recovered, second));
  }
}

void corruptAndOldFormat() {
  Memory pristine;
  Settings first = Settings::defaults(); first.learnedPulses = 321;
  Settings second = first; second.fastPermille = 800;
  SettingsJournal journal(pristine);
  CHECK(journal.save(first, true) == SaveResult::Saved);
  CHECK(journal.save(second, true) == SaveResult::Saved);
  for (size_t i = 0; i < SettingsJournal::RecordSize; ++i) {
    Memory corrupt = pristine;
    corrupt.bytes[1024U + i] ^= 1U;
    SettingsJournal reboot(corrupt);
    Settings recovered;
    CHECK(reboot.load(recovered));
    CHECK(equalSettings(recovered, first));
  }
  Memory old;
  // A V2 float/magic-shaped image must not be silently treated as V3 settings.
  old.bytes[0] = 0x00; old.bytes[1] = 0x80; old.bytes[2] = 0xa0; old.bytes[3] = 0x43;
  old.bytes[16] = 0xd2; old.bytes[17] = 0x57;
  SettingsJournal unsupported(old);
  Settings defaults;
  CHECK(!unsupported.load(defaults));
  CHECK(defaults.learnedPulses == 0U);
  Memory damaged = pristine;
  damaged.bytes[127] = 0;
  damaged.bytes[1024U + 127U] = 0;
  SettingsJournal neither(damaged);
  CHECK(!neither.load(defaults));
  CHECK(equalSettings(defaults, Settings::defaults()));
}

void fixedSerialization() {
  Settings s = Settings::defaults();
  s.ropeLengthMm = 123456;
  s.learnedPulses = 654321;
  uint8_t guarded[SettingsJournal::PayloadSize + 2U] = {};
  guarded[0] = 0x61; guarded[SettingsJournal::PayloadSize + 1U] = 0x62;
  encodeSettings(s, guarded + 1U);
  CHECK(guarded[0] == 0x61U);
  CHECK(guarded[SettingsJournal::PayloadSize + 1U] == 0x62U);
  CHECK(guarded[1] == 0x40U);  // Independent little endian 123456 = 0x0001e240.
  CHECK(guarded[2] == 0xe2U); CHECK(guarded[3] == 0x01U); CHECK(guarded[4] == 0x00U);
  Settings restored;
  CHECK(decodeSettings(guarded + 1U, restored));
  CHECK(equalSettings(s, restored));
  guarded[9] = 0xff; guarded[10] = 0xff;  // Fast fraction, outside valid limits.
  CHECK(!decodeSettings(guarded + 1U, restored));
}

void sequenceRolloverGoldenFixture() {
  // Independent Python 3 struct/zlib fixture, NOT the production encoder:
  // payload = struct.pack('<II5H9Ii3I4H9B',100000,321,870,200,1000,2000,
  // 1280,60000,100,250,3000,500,500,200,400,200,0,2000,120000,20,
  // 700,1500,200,0,0,1,2,3,3,0,0,0,0)
  // r = header(magic=0x30334e57,version=1,len=87,seq=0xffffffff)+payload+
  // bytes(24)+b'\xa5'; CRC=zlib.crc32(r[:12]+r[16:127]) == 0xef241997.
  // Second record sequence=0, fastPermille=800, CRC=0x7cd28ab4.
  const uint8_t oldRecord[128] = {
    0x57,0x4e,0x33,0x30,0x01,0x00,0x57,0x00,0xff,0xff,0xff,0xff,0x97,0x19,0x24,0xef,
    0xa0,0x86,0x01,0x00,0x41,0x01,0x00,0x00,0x66,0x03,0xc8,0x00,0xe8,0x03,0xd0,0x07,
    0x00,0x05,0x60,0xea,0x00,0x00,0x64,0x00,0x00,0x00,0xfa,0x00,0x00,0x00,0xb8,0x0b,
    0x00,0x00,0xf4,0x01,0x00,0x00,0xf4,0x01,0x00,0x00,0xc8,0x00,0x00,0x00,0x90,0x01,
    0x00,0x00,0xc8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xd0,0x07,0x00,0x00,0xc0,0xd4,
    0x01,0x00,0x14,0x00,0x00,0x00,0xbc,0x02,0xdc,0x05,0xc8,0x00,0x00,0x00,0x00,0x01,
    0x02,0x03,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xa5
  };
  Memory memory;
  for (size_t i = 0; i < 128U; ++i) {
    memory.bytes[i] = oldRecord[i]; memory.bytes[1024U + i] = oldRecord[i];
  }
  for (size_t i = 8; i < 12U; ++i) memory.bytes[1024U + i] = 0;
  memory.bytes[1024U + 12U] = 0xb4; memory.bytes[1024U + 13U] = 0x8a;
  memory.bytes[1024U + 14U] = 0xd2; memory.bytes[1024U + 15U] = 0x7c;
  memory.bytes[1024U + 24U] = 0x20; memory.bytes[1024U + 25U] = 0x03;
  SettingsJournal journal(memory);
  Settings restored;
  CHECK(journal.load(restored));
  CHECK(journal.sequence() == 0U);
  CHECK(journal.activeSlot() == 1);
  CHECK(restored.fastPermille == 800U);
  CHECK(restored.ropeLengthMm == 100000U);
  CHECK(restored.learnedPulses == 321U);
  restored.fastPermille = 810;
  CHECK(journal.save(restored, true) == SaveResult::Saved);
  CHECK(journal.sequence() == 1U);
  SettingsJournal reboot(memory);
  CHECK(reboot.load(restored));
  CHECK(reboot.sequence() == 1U);
  CHECK(restored.fastPermille == 810U);
}
}  // namespace

int main() {
  basicAndInvalid(); everyInterruptedWrite(); corruptAndOldFormat(); fixedSerialization();
  sequenceRolloverGoldenFixture();
  std::cout << "PASS persistence: " << checks
            << " checks; CRC, schema, every tear point, erase-block isolation, wear\n";
  return 0;
}
