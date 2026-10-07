#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace winch {
// Single producer/consumer in loop(), never from an ISR. Whole records are
// accepted or dropped; a partially sent CSV row is never overwritten.
template <std::size_t Capacity> class ByteQueue {
 public:
  bool push(const char* bytes, std::size_t count) {
    if (count > Capacity - used_) { ++dropped_; return false; }
    for (std::size_t i = 0; i < count; ++i) {
      data_[(head_ + used_ + i) % Capacity] = static_cast<uint8_t>(bytes[i]);
    }
    used_ += count;
    return true;
  }
  // Copy bounded chunks to a caller-owned buffer; consume ONLY bytes accepted
  // by the nonblocking transport. USB disconnect can discard pending rows.
  std::size_t peek(uint8_t* dest, std::size_t maximum) const {
    const std::size_t count = maximum < used_ ? maximum : used_;
    for (std::size_t i = 0; i < count; ++i) dest[i] = data_[(head_ + i) % Capacity];
    return count;
  }
  void consume(std::size_t count) {
    if (count > used_) count = used_;
    head_ = (head_ + count) % Capacity;
    used_ -= count;
  }
  void clear() { head_ = 0; used_ = 0; }
  std::size_t size() const { return used_; }
  uint32_t dropped() const { return dropped_; }
 private:
  uint8_t data_[Capacity]{};
  std::size_t head_ = 0;
  std::size_t used_ = 0;
  uint32_t dropped_ = 0;
};
}  // namespace winch
