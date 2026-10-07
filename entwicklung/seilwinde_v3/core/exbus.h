#pragma once

#include <cstddef>
#include <cstdint>

// Own bounded implementation of the public JETI EX Bus 1.21 / EX 1.07 formats.
// Transport, hardware half duplex, channel-age failsafe and menu policy are external.
namespace winch { namespace exbus {

constexpr std::size_t MaxFrameBytes = 64;
constexpr std::size_t MaxChannels = 24;
constexpr std::size_t MaxExBytes = 28; // EX max 29 including omitted 0x7e separator.
constexpr uint32_t InterByteTimeoutUs = 1000;
constexpr uint32_t FrameTimeoutUs = 6000;
constexpr uint32_t ResponseWindowUs = 4000;

enum class Kind : uint8_t { Channels, TelemetryRequest, JetiboxRequest, Other };

struct Frame {
    Kind kind = Kind::Other;
    uint8_t packetId = 0;
    bool busReleased = false;
    uint32_t receivedUs = 0;
    uint16_t channelsUs[MaxChannels]{};
    uint8_t channelCount = 0;
    uint8_t buttons = 0xf0; // Active low: L=0x80, D=0x40, U=0x20, R=0x10.
};

struct Statistics {
    uint32_t accepted = 0;
    uint32_t crcErrors = 0;
    uint32_t formatErrors = 0;
    uint32_t timeouts = 0;
};

uint16_t crc16(const uint8_t* bytes, std::size_t length);
uint8_t crc8(const uint8_t* bytes, std::size_t length);
bool validFrameCrc(const uint8_t* bytes, std::size_t length);

class Parser {
public:
    // Consume exactly one byte; true means a CRC/structure-checked frame in result.
    // One bounded resynchronisation scan follows bad CRC, no heap allocation.
    bool feed(uint8_t byte, uint32_t nowUs, Frame& result);
    void expire(uint32_t nowUs);
    void reset();
    const Statistics& statistics() const { return stats_; }
    std::size_t bufferedBytes() const { return used_; }
private:
    bool decode(uint32_t nowUs, Frame& result) const;
    void retainPossibleHeader();
    uint8_t buffer_[MaxFrameBytes]{};
    std::size_t used_ = 0;
    std::size_t expected_ = 0;
    uint32_t startUs_ = 0;
    uint32_t lastUs_ = 0;
    Statistics stats_{};
};

// All encoders return zero on invalid input or insufficient capacity. Request IDs
// are copied into the response; caller alone decides when transmission is allowed.
std::size_t channels(uint8_t packetId, const uint16_t* valuesUs, std::size_t count,
                     bool busReleased, uint8_t* out, std::size_t capacity);
std::size_t telemetryRequest(uint8_t packetId, uint8_t* out, std::size_t capacity);
std::size_t jetiboxRequest(uint8_t packetId, uint8_t buttons,
                          uint8_t* out, std::size_t capacity);
std::size_t telemetryResponse(uint8_t packetId, const uint8_t* ex, std::size_t exLength,
                             uint8_t* out, std::size_t capacity);
std::size_t jetiboxResponse(uint8_t packetId, const char text[32],
                           uint8_t* out, std::size_t capacity);

struct ExValue { uint8_t id; int32_t value; uint8_t decimals; };
// Integer values use the EX signed-magnitude representation. Caller supplies raw
// decimal-scaled integers, e.g. 123, decimals=1 means 12.3. IDs 1..15, unique.
std::size_t exData(uint16_t manufacturerId, uint16_t deviceId,
                   const ExValue* values, std::size_t count,
                   uint8_t* out, std::size_t capacity);
// Text descriptor including device name at id 0. Combined label+unit <=18 bytes.
std::size_t exText(uint16_t manufacturerId, uint16_t deviceId, uint8_t id,
                   const char* label, const char* unit,
                   uint8_t* out, std::size_t capacity);
bool validExCrc(const uint8_t* ex, std::size_t length);

// Conservative admission check: reserve a guard before the minimum 4 ms end.
// Includes all 10 UART bits per byte and a caller-specified transport guard.
bool fitsResponseWindow(std::size_t bytes, uint32_t baud, uint32_t elapsedUs,
                        uint32_t guardUs = 200);

} } // namespace winch::exbus
