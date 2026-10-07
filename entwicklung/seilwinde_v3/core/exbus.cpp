#include "exbus.h"

#include <cstring>

namespace winch { namespace exbus {
namespace {
bool masterHeader(uint8_t byte) { return byte == 0x3d || byte == 0x3e; }

void appendCrc(uint8_t* out, std::size_t length) {
    const uint16_t crc = crc16(out, length - 2);
    out[length - 2] = static_cast<uint8_t>(crc);
    out[length - 1] = static_cast<uint8_t>(crc >> 8);
}

std::size_t packet(uint8_t header, uint8_t packetId, uint8_t blockId,
                   const uint8_t* data, std::size_t dataLength,
                   uint8_t* out, std::size_t capacity) {
    const std::size_t length = dataLength + 8;
    if (!out || dataLength > MaxFrameBytes - 8 || capacity < length ||
        (dataLength && !data)) return 0;
    out[0] = header; out[1] = 0x01;
    out[2] = static_cast<uint8_t>(length); out[3] = packetId;
    out[4] = blockId; out[5] = static_cast<uint8_t>(dataLength);
    if (dataLength) std::memcpy(out + 6, data, dataLength);
    appendCrc(out, length);
    return length;
}

void exHeader(uint16_t manufacturerId, uint16_t deviceId, uint8_t* out) {
    out[0] = 0x9f; // Public EX marker, 0x7e is omitted in EX Bus.
    out[2] = static_cast<uint8_t>(manufacturerId);
    out[3] = static_cast<uint8_t>(manufacturerId >> 8);
    out[4] = static_cast<uint8_t>(deviceId);
    out[5] = static_cast<uint8_t>(deviceId >> 8);
    out[6] = 0;
}

std::size_t boundedTextLength(const char* text, std::size_t maximum) {
    if (!text) return maximum + 1;
    for (std::size_t i = 0; i <= maximum; ++i) {
        if (!text[i]) return i;
    }
    return maximum + 1;
}
} // namespace

uint16_t crc16(const uint8_t* bytes, std::size_t length) {
    uint16_t crc = 0;
    // Reflected CCITT, polynomial 0x1021 (reflected 0x8408), initial zero.
    // Not CRC-CCITT-FALSE; no final complement.
    for (std::size_t i = 0; i < length; ++i) {
        crc ^= bytes[i];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = static_cast<uint16_t>((crc >> 1) ^ ((crc & 1u) ? 0x8408u : 0u));
        }
    }
    return crc;
}

uint8_t crc8(const uint8_t* bytes, std::size_t length) {
    uint8_t crc = 0;
    for (std::size_t i = 0; i < length; ++i) {
        crc ^= bytes[i];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = static_cast<uint8_t>((static_cast<uint32_t>(crc) << 1) ^
                                     ((crc & 0x80u) ? 0x07u : 0u));
        }
    }
    return crc;
}

bool validFrameCrc(const uint8_t* bytes, std::size_t length) {
    if (!bytes || length < 8 || length > MaxFrameBytes || bytes[2] != length) return false;
    const uint16_t expected = static_cast<uint16_t>(bytes[length - 2]) |
                              static_cast<uint16_t>(static_cast<uint16_t>(bytes[length - 1]) << 8);
    return crc16(bytes, length - 2) == expected;
}

void Parser::reset() { used_ = 0; expected_ = 0; }

void Parser::expire(uint32_t nowUs) {
    if (used_ && (static_cast<uint32_t>(nowUs - lastUs_) > InterByteTimeoutUs ||
                  static_cast<uint32_t>(nowUs - startUs_) > FrameTimeoutUs)) {
        ++stats_.timeouts;
        reset();
    }
}

void Parser::retainPossibleHeader() {
    // An invalid outer frame may have swallowed a real header. Scan at most 63
    // bytes and retain a plausible suffix; a complete candidate must have good CRC.
    for (std::size_t offset = 1; offset < used_; ++offset) {
        if (!masterHeader(buffer_[offset])) continue;
        const std::size_t suffix = used_ - offset;
        if (suffix > 1 && buffer_[offset + 1] != 0x01 && buffer_[offset + 1] != 0x03) continue;
        std::size_t expected = 0;
        if (suffix > 2) {
            expected = buffer_[offset + 2];
            if (expected < 8 || expected > MaxFrameBytes || expected < suffix) continue;
            if (expected == suffix && !validFrameCrc(buffer_ + offset, suffix)) continue;
        }
        std::memmove(buffer_, buffer_ + offset, suffix);
        used_ = suffix;
        expected_ = expected;
        // Keep original frame start: noise cannot extend a partial frame forever.
        return;
    }
    reset();
}

bool Parser::decode(uint32_t nowUs, Frame& result) const {
    Frame frame{};
    frame.packetId = buffer_[3];
    frame.busReleased = buffer_[1] == 0x01;
    frame.receivedUs = nowUs;
    std::size_t offset = 4;
    bool known = false;
    while (offset < used_ - 2) {
        if (offset + 2 > used_ - 2) return false;
        const uint8_t id = buffer_[offset];
        const std::size_t length = buffer_[offset + 1];
        offset += 2;
        if (length > used_ - 2 - offset) return false;
        if (id == 0x31) {
            if (known || buffer_[0] != 0x3e || length == 0 ||
                (length & 1u) || length > MaxChannels * 2) return false;
            frame.kind = Kind::Channels;
            frame.channelCount = static_cast<uint8_t>(length / 2);
            for (std::size_t i = 0; i < frame.channelCount; ++i) {
                const uint16_t raw = static_cast<uint16_t>(buffer_[offset + 2 * i]) |
                    static_cast<uint16_t>(static_cast<uint16_t>(buffer_[offset + 2 * i + 1]) << 8);
                frame.channelsUs[i] = static_cast<uint16_t>(raw >> 3);
            }
            known = true;
        } else if (id == 0x3a) {
            if (known || buffer_[0] != 0x3d || !frame.busReleased || length != 0) return false;
            frame.kind = Kind::TelemetryRequest;
            known = true;
        } else if (id == 0x3b) {
            if (known || buffer_[0] != 0x3d || !frame.busReleased || length != 1 ||
                (buffer_[offset] & 0x0fu)) return false;
            frame.kind = Kind::JetiboxRequest;
            frame.buttons = buffer_[offset];
            known = true;
        }
        offset += length;
    }
    if (offset != used_ - 2) return false;
    result = frame;
    return true;
}

bool Parser::feed(uint8_t byte, uint32_t nowUs, Frame& result) {
    expire(nowUs);
    if (!used_) {
        if (!masterHeader(byte)) return false;
        buffer_[0] = byte; used_ = 1;
        startUs_ = nowUs; lastUs_ = nowUs;
        return false;
    }
    lastUs_ = nowUs;
    if (used_ == 1 && byte != 0x01 && byte != 0x03) {
        ++stats_.formatErrors;
        reset();
        if (masterHeader(byte)) {
            buffer_[0] = byte; used_ = 1; startUs_ = nowUs;
        }
        return false;
    }
    buffer_[used_++] = byte;
    if (used_ == 3) {
        expected_ = byte;
        if (expected_ < 8 || expected_ > MaxFrameBytes) {
            ++stats_.formatErrors;
            retainPossibleHeader();
            return false;
        }
    }
    if (!expected_ || used_ < expected_) return false;
    if (validFrameCrc(buffer_, used_)) {
        if (decode(nowUs, result)) {
            ++stats_.accepted;
            reset();
            return true;
        }
        ++stats_.formatErrors;
    } else {
        ++stats_.crcErrors;
    }
    retainPossibleHeader();
    // Only one extra bounded candidate inspection; no unbounded scan/retry loop.
    if (expected_ && used_ == expected_ && validFrameCrc(buffer_, used_) && decode(nowUs, result)) {
        ++stats_.accepted;
        reset();
        return true;
    }
    return false;
}

std::size_t channels(uint8_t packetId, const uint16_t* valuesUs, std::size_t count,
                     bool busReleased, uint8_t* out, std::size_t capacity) {
    if (!out || !valuesUs || !count || count > MaxChannels || capacity < 8 + 2 * count) return 0;
    uint8_t data[MaxChannels * 2]{};
    for (std::size_t i = 0; i < count; ++i) {
        if (valuesUs[i] > 8191) return 0;
        const uint16_t raw = static_cast<uint16_t>(valuesUs[i] << 3);
        data[2 * i] = static_cast<uint8_t>(raw);
        data[2 * i + 1] = static_cast<uint8_t>(raw >> 8);
    }
    const std::size_t length = packet(0x3e, packetId, 0x31, data, count * 2, out, capacity);
    out[1] = busReleased ? 0x01 : 0x03;
    appendCrc(out, length);
    return length;
}

std::size_t telemetryRequest(uint8_t packetId, uint8_t* out, std::size_t capacity) {
    return packet(0x3d, packetId, 0x3a, nullptr, 0, out, capacity);
}
std::size_t jetiboxRequest(uint8_t packetId, uint8_t buttons, uint8_t* out, std::size_t capacity) {
    if (buttons & 0x0fu) return 0;
    return packet(0x3d, packetId, 0x3b, &buttons, 1, out, capacity);
}
std::size_t telemetryResponse(uint8_t packetId, const uint8_t* ex, std::size_t exLength,
                             uint8_t* out, std::size_t capacity) {
    if (exLength && !validExCrc(ex, exLength)) return 0;
    return packet(0x3b, packetId, 0x3a, ex, exLength, out, capacity);
}
std::size_t jetiboxResponse(uint8_t packetId, const char text[32], uint8_t* out, std::size_t capacity) {
    if (!text) return 0;
    return packet(0x3b, packetId, 0x3b, reinterpret_cast<const uint8_t*>(text), 32, out, capacity);
}

std::size_t exData(uint16_t manufacturerId, uint16_t deviceId,
                   const ExValue* values, std::size_t count, uint8_t* out, std::size_t capacity) {
    if (!out || !values || !count || count > 15 || capacity < 8) return 0;
    uint8_t candidate[MaxExBytes]{};
    exHeader(manufacturerId, deviceId, candidate);
    std::size_t offset = 7;
    uint16_t usedIds = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const ExValue value = values[i];
        if (!value.id || value.id > 15 || value.decimals > 3 ||
            (usedIds & (uint16_t{1} << value.id))) return 0;
        usedIds |= static_cast<uint16_t>(uint16_t{1} << value.id);
        // int64 intermediate safely handles INT32_MIN before validation.
        const uint32_t magnitude = static_cast<uint32_t>(value.value < 0 ?
            -static_cast<int64_t>(value.value) : static_cast<int64_t>(value.value));
        if (magnitude > 536870911u) return 0;
        std::size_t width = 1;
        uint8_t type = 0;
        if (magnitude > 31u) { width = 2; type = 1; }
        if (magnitude > 8191u) { width = 3; type = 4; }
        if (magnitude > 2097151u) { width = 4; type = 8; }
        if (offset + 1 + width + 1 > MaxExBytes) return 0;
        candidate[offset++] = static_cast<uint8_t>((value.id << 4) | type);
        for (std::size_t byte = 0; byte < width; ++byte) {
            candidate[offset++] = static_cast<uint8_t>(magnitude >> (8 * byte));
        }
        candidate[offset - 1] |= static_cast<uint8_t>((value.value < 0 ? 0x80u : 0u) |
                                                     (static_cast<uint32_t>(value.decimals) << 5));
    }
    const std::size_t length = offset + 1;
    if (capacity < length) return 0;
    candidate[1] = static_cast<uint8_t>(0x40u | (length - 2));
    candidate[offset] = crc8(candidate + 1, offset - 1);
    std::memcpy(out, candidate, length);
    return length;
}

std::size_t exText(uint16_t manufacturerId, uint16_t deviceId, uint8_t id,
                   const char* label, const char* unit, uint8_t* out, std::size_t capacity) {
    const std::size_t labelLength = boundedTextLength(label, 18);
    const std::size_t unitLength = boundedTextLength(unit, 7);
    const std::size_t length = 10 + labelLength + unitLength;
    if (!out || !labelLength || labelLength > 18 || unitLength > 7 ||
        length > MaxExBytes || capacity < length) return 0;
    exHeader(manufacturerId, deviceId, out);
    out[1] = static_cast<uint8_t>(length - 2);
    out[7] = id;
    out[8] = static_cast<uint8_t>((labelLength << 3) | unitLength);
    std::memcpy(out + 9, label, labelLength);
    if (unitLength) std::memcpy(out + 9 + labelLength, unit, unitLength);
    out[length - 1] = crc8(out + 1, length - 2);
    return length;
}

bool validExCrc(const uint8_t* ex, std::size_t length) {
    if (!ex || length < 8 || length > MaxExBytes || (ex[0] & 0x0fu) != 0x0fu ||
        (ex[1] & 0x3fu) != length - 2) return false;
    return crc8(ex + 1, length - 2) == ex[length - 1];
}

bool fitsResponseWindow(std::size_t bytes, uint32_t baud, uint32_t elapsedUs, uint32_t guardUs) {
    if (!bytes || bytes > MaxFrameBytes || (baud != 125000 && baud != 250000) ||
        elapsedUs >= ResponseWindowUs || guardUs >= ResponseWindowUs) return false;
    const uint32_t wireUs = static_cast<uint32_t>((bytes * 10000000u + baud - 1u) / baud);
    return wireUs + elapsedUs + guardUs <= ResponseWindowUs;
}

} } // namespace winch::exbus
