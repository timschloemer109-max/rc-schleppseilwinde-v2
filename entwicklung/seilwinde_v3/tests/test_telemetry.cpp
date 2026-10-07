#include "application.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>

namespace {
// Read responses using an independent numeric decoder and CRC polynomial
// division, never the DUT's encoder as an expected-data generator.
uint8_t referenceCrc8(const uint8_t* bytes, std::size_t count) {
    uint16_t remainder = 0;
    for (std::size_t i = 0; i <= count; ++i) {
        // Divide the message polynomial with the final eight zero bits appended.
        remainder = static_cast<uint16_t>((remainder << 8) | (i < count ? bytes[i] : 0));
        for (int bit = 15; bit >= 8; --bit) {
            if ((remainder & (uint16_t{1} << bit)) != 0) {
                remainder ^= static_cast<uint16_t>(uint16_t{0x107} << (bit - 8));
            }
        }
    }
    return static_cast<uint8_t>(remainder);
}
uint16_t referenceCrc16(const uint8_t* bytes, std::size_t count) {
    uint16_t crc = 0;
    for (std::size_t i = 0; i < count; ++i) {
        uint8_t byte = static_cast<uint8_t>(bytes[i] ^ static_cast<uint8_t>(crc));
        byte ^= static_cast<uint8_t>(byte << 4);
        crc = static_cast<uint16_t>(((static_cast<uint16_t>(byte) << 8) | (crc >> 8)) ^
                                   (byte >> 4) ^ (static_cast<uint16_t>(byte) << 3));
    }
    return crc;
}

void receive(winch::Application& app, const uint8_t* bytes, std::size_t count,
             uint32_t nowUs, uint32_t delayUs = 0) {
    for (std::size_t i = 0; i < count; ++i) {
        app.receive(bytes[i], nowUs - delayUs, nowUs, nowUs / 1000U);
    }
}
void channels(winch::Application& app, uint32_t nowMs, uint16_t trigger,
              uint32_t hall, bool end) {
    const uint16_t values[] = {1500, trigger};
    uint8_t packet[12]{};
    const std::size_t count = winch::exbus::channels(0x55, values, 2, false, packet, sizeof(packet));
    assert(count == sizeof(packet));
    receive(app, packet, count, nowMs * 1000U);
    winch::Inputs input;
    input.nowMs = nowMs; input.hallTotal = hall; input.endSwitch = end;
    app.tick(input);
}
void request(winch::Application& app, uint8_t id, uint32_t nowUs) {
    uint8_t packet[8]{};
    assert(winch::exbus::telemetryRequest(id, packet, sizeof(packet)) == 8);
    receive(app, packet, 8, nowUs);
}
void validateBus(const winch::Reply& reply, uint8_t packetId, uint8_t blockId) {
    assert(reply.length >= 8 && reply.length <= 40);
    const uint8_t* bytes = reply.bytes;
    assert(bytes[0] == 0x3b && bytes[1] == 1 && bytes[2] == reply.length);
    assert(bytes[3] == packetId && bytes[4] == blockId && bytes[5] == reply.length - 8);
    const uint16_t expected = static_cast<uint16_t>(bytes[reply.length - 2]) |
        static_cast<uint16_t>(static_cast<uint16_t>(bytes[reply.length - 1]) << 8);
    assert(referenceCrc16(bytes, reply.length - 2) == expected);
}
void validateEx(const winch::Reply& reply) {
    const uint8_t* ex = reply.bytes + 6;
    const std::size_t length = reply.bytes[5];
    assert(length >= 8 && length <= 28 && ex[0] == 0x9f);
    assert((ex[1] & 63u) == length - 2 && ex[2] == 0 && ex[3] == 0xa4);
    assert(ex[4] == 0 && ex[5] == 3 && ex[6] == 0);
    assert(referenceCrc8(ex + 1, length - 2) == ex[length - 1]);
}
struct Reading { int32_t raw = 0; uint8_t decimals = 0; unsigned count = 0; };
void decodeData(const winch::Reply& reply, Reading (&readings)[16]) {
    const uint8_t* ex = reply.bytes + 6;
    const std::size_t length = reply.bytes[5];
    std::size_t offset = 7;
    unsigned count = 0;
    while (offset < length - 1) {
        const uint8_t type = static_cast<uint8_t>(ex[offset] & 15u);
        const uint8_t id = static_cast<uint8_t>(ex[offset++] >> 4);
        assert(id >= 1 && id <= 15);
        const std::size_t width = type == 0 ? 1u : type == 1 ? 2u : type == 4 ? 3u : type == 8 ? 4u : 0u;
        assert(width && offset + width < length);
        uint32_t magnitude = 0;
        for (std::size_t byte = 0; byte < width; ++byte) {
            const uint8_t value = byte == width - 1 ? static_cast<uint8_t>(ex[offset + byte] & 31u) : ex[offset + byte];
            magnitude |= static_cast<uint32_t>(value) << (byte * 8);
        }
        const uint8_t last = ex[offset + width - 1];
        readings[id].raw = (last & 128u) ? -static_cast<int32_t>(magnitude) : static_cast<int32_t>(magnitude);
        readings[id].decimals = static_cast<uint8_t>((last >> 5) & 3u);
        ++readings[id].count;
        offset += width; ++count;
    }
    assert(offset == length - 1 && count == 3);
}

void carousel() {
    winch::Settings settings = winch::Settings::defaults();
    settings.ropeLengthMm = 100000; settings.armMs = 1000;
    settings.neutralMs = 50; settings.modeStableFrames = 1;
    settings.learnMinMs = 1000;
    winch::Application app(settings);
    app.reset(0, 0, true);
    for (uint32_t ms = 0; ms <= 1100; ms += 20) channels(app, ms, 1500, 0, true);
    channels(app, 1120, 1000, 0, true);
    assert(app.controller().outputs().state == winch::State::Learning);
    for (uint32_t ms = 1140; ms < 2120; ms += 20) channels(app, ms, 1000, (ms - 1120) / 50U, false);
    channels(app, 2120, 1500, 20, false);
    assert(app.controller().outputs().learnAccepted);
    assert(app.controller().outputs().estimatedRemainingMm == 100000);
    app.setTelemetry(-12340, true, 2, 123);
    const char* const labels[] = {"Winch V3", "Rope remain", "Remain", "State", "Current", "Hall pulses",
        "Pulse rate", "Drum RPM", "ESC PWM", "Stop reason", "Validity bits", "RC fresh", "Power", "Light warn", "Bus errors", "Log drops"};
    const char* const units[] = {"", "m", "%", "", "A", "", "Hz", "rpm", "us", "", "", "", "", "", "", ""};
    Reading readings[16]{};
    unsigned descriptors[16]{};
    for (uint32_t index = 0; index < 320; ++index) {
        const uint32_t nowMs = 2140 + index * 5;
        channels(app, nowMs, 1500, 20, false);
        const uint8_t id = static_cast<uint8_t>(index);
        request(app, id, nowMs * 1000U);
        const winch::Reply& reply = app.reply();
        validateBus(reply, id, 0x3a); validateEx(reply);
        const uint8_t* ex = reply.bytes + 6;
        const uint8_t type = static_cast<uint8_t>(ex[1] >> 6);
        if (type == 0) {
            const uint8_t descriptor = ex[7];
            const std::size_t labelLength = ex[8] >> 3;
            const std::size_t unitLength = ex[8] & 7u;
            assert(descriptor < 16 && reply.bytes[5] == labelLength + unitLength + 10);
            assert(labelLength == std::strlen(labels[descriptor]) && unitLength == std::strlen(units[descriptor]));
            assert(std::memcmp(ex + 9, labels[descriptor], labelLength) == 0);
            assert(std::memcmp(ex + 9 + labelLength, units[descriptor], unitLength) == 0);
            ++descriptors[descriptor];
        } else {
            assert(type == 1);
            decodeData(reply, readings);
        }
        app.reply().length = 0;
    }
    const int32_t expected[] = {0, 1000, 1000, 0, -1234, 123, 2, 60, 1000, 0, 15, 1, 0, 0, 0, 0};
    const uint8_t decimals[] = {0, 1, 1, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    for (std::size_t id = 1; id < 16; ++id) {
        assert(readings[id].count >= 30 && readings[id].raw == expected[id] && readings[id].decimals == decimals[id]);
    }
    for (unsigned count : descriptors) assert(count == 10);
    // Unknown scale/current and saturation are visible as validity/data fields.
    settings = winch::Settings::defaults(); settings.learnedPulses = 100;
    winch::Application unknown(settings); unknown.reset(0, 0, true);
    unknown.setTelemetry(999999, false, 100000, 0xffffffffu);
    Reading second[16]{};
    for (uint32_t index = 0; index < 20; ++index) {
        request(unknown, static_cast<uint8_t>(index), index * 5000U);
        validateBus(unknown.reply(), static_cast<uint8_t>(index), 0x3a); validateEx(unknown.reply());
        if ((unknown.reply().bytes[7] >> 6) == 1) decodeData(unknown.reply(), second);
        unknown.reply().length = 0;
    }
    assert(second[1].raw == 0 && second[4].raw == 0 && second[10].raw == 3);
    assert(second[5].raw == 2097151 && second[7].raw == 2097151);
}

void displayAndStaleRequests() {
    winch::Application app(winch::Settings::defaults());
    char text[32]{};
    app.menu().render(text, app.controller());
    const char expected[33] = "V3 status       idle RC old     ";
    static_assert(sizeof(expected) == 33, "32-char screen plus sentinel");
    assert(std::memcmp(text, expected, 32) == 0);
    uint8_t packet[9]{};
    assert(winch::exbus::jetiboxRequest(0x77, 0xe0, packet, sizeof(packet)) == 9); // RIGHT.
    receive(app, packet, 9, 100000, 2001);
    assert(app.menu().page() == 0 && app.reply().length == 0);
    receive(app, packet, 9, 110000);
    assert(app.menu().page() == 1);
    validateBus(app.reply(), 0x77, 0x3b);
    assert(app.reply().length == 40);
    const char row1[17] = "Rope metres     ";
    const char row2[17] = "0               ";
    assert(std::memcmp(app.reply().bytes + 6, row1, 16) == 0);
    assert(std::memcmp(app.reply().bytes + 22, row2, 16) == 0);
    for (std::size_t i = 6; i < 38; ++i) assert(app.reply().bytes[i] >= 32 && app.reply().bytes[i] < 127);
    app.reply().length = 0;
    // Old key presses cannot edit the draft, even after debounce/repeat time.
    assert(winch::exbus::jetiboxRequest(0x78, 0xd0, packet, sizeof(packet)) == 9); // UP.
    receive(app, packet, 9, 510000, 10000);
    assert(app.menu().draft().ropeLengthMm == 0 && app.reply().length == 0);
    receive(app, packet, 9, 520000);
    assert(app.menu().draft().ropeLengthMm == 1000);
}
} // namespace

int main() {
    carousel(); displayAndStaleRequests();
    std::puts("PASS independent telemetry: 15 IDs, 16 descriptors, signed/scaled values, saturation, 320-request wrap, 32-char padded JETIBOX, stale menu keys");
}
