#include "exbus.h"
#include "../simulator/reference_telegrams.h"

#include <cstdio>
#include <cstring>
#include <limits>

namespace bus = winch::exbus;
namespace {
unsigned checks = 0;
unsigned failures = 0;
#define CHECK(expression) do { ++checks; if (!(expression)) { \
    std::printf("FAIL line %d: %s\n", __LINE__, #expression); ++failures; } } while (false)

// Independent official algebraic CRC, not the DUT implementation. Used only to
// construct malformed/combined packet tests; fixed golden vectors remain authority.
uint16_t referenceCrc(const uint8_t* data, std::size_t length) {
    uint16_t crc = 0;
    for (std::size_t i = 0; i < length; ++i) {
        uint8_t value = static_cast<uint8_t>(data[i] ^ static_cast<uint8_t>(crc));
        value ^= static_cast<uint8_t>(value << 4);
        crc = static_cast<uint16_t>((static_cast<uint16_t>(value) << 8) |
                                   static_cast<uint16_t>(crc >> 8));
        crc ^= static_cast<uint16_t>((value >> 4) ^ (static_cast<uint16_t>(value) << 3));
    }
    return crc;
}

void finish(uint8_t* packet, std::size_t length) {
    packet[2] = static_cast<uint8_t>(length);
    const uint16_t crc = referenceCrc(packet, length - 2);
    packet[length - 2] = static_cast<uint8_t>(crc);
    packet[length - 1] = static_cast<uint8_t>(crc >> 8);
}

unsigned feed(bus::Parser& parser, const uint8_t* data, std::size_t length,
              uint32_t& nowUs, bus::Frame& frame, uint32_t interval = 80) {
    unsigned completed = 0;
    for (std::size_t i = 0; i < length; ++i) {
        if (parser.feed(data[i], nowUs, frame)) ++completed;
        CHECK(parser.bufferedBytes() <= bus::MaxFrameBytes);
        nowUs += interval;
    }
    return completed;
}

template<std::size_t N> unsigned feed(bus::Parser& parser, const uint8_t (&data)[N],
                                     uint32_t& nowUs, bus::Frame& frame, uint32_t interval = 80) {
    return feed(parser, data, N, nowUs, frame, interval);
}

void goldenPackets() {
    CHECK(bus::crc16(reference::official_channels, sizeof(reference::official_channels) - 2) == 0xe24f);
    CHECK(bus::crc16(reference::official_telemetry_request, 6) == 0x8198);
    CHECK(bus::crc16(reference::official_jetibox_request, 7) == 0x24a3);
    CHECK(bus::validFrameCrc(reference::official_telemetry_response, sizeof(reference::official_telemetry_response)));
    CHECK(bus::validFrameCrc(reference::official_jetibox_response, sizeof(reference::official_jetibox_response)));
    CHECK(bus::validExCrc(reference::official_ex_data, sizeof(reference::official_ex_data)));
    CHECK(bus::validExCrc(reference::official_ex_text, sizeof(reference::official_ex_text)));
    bus::Parser parser;
    bus::Frame frame;
    uint32_t now = 0;
    CHECK(feed(parser, reference::official_channels, now, frame) == 1);
    CHECK(frame.kind == bus::Kind::Channels && frame.packetId == 6 && !frame.busReleased);
    CHECK(frame.channelCount == 16);
    for (uint16_t channel : frame.channelsUs) {
        // 8066 raw / 8 = 1008.25 us, integer control input deliberately floors.
        CHECK(channel == 1008 || channel == 0);
    }
    const uint32_t channelsReceived = frame.receivedUs;
    CHECK(feed(parser, reference::official_telemetry_request, now, frame) == 1);
    CHECK(frame.kind == bus::Kind::TelemetryRequest && frame.packetId == 6 && frame.busReleased);
    CHECK(frame.channelCount == 0 && frame.receivedUs > channelsReceived);
    CHECK(feed(parser, reference::official_jetibox_request, now, frame) == 1);
    CHECK(frame.kind == bus::Kind::JetiboxRequest && frame.packetId == 0x88 && frame.buttons == 0xf0);
    CHECK(parser.statistics().accepted == 3);
}

void goldenEncoders() {
    uint8_t bytes[bus::MaxFrameBytes]{};
    const uint16_t values[] = {1000, 1500, 2000, 1250};
    CHECK(bus::channels(0x42, values, 4, false, bytes, sizeof(bytes)) == sizeof(reference::independent_channels));
    CHECK(std::memcmp(bytes, reference::independent_channels, sizeof(reference::independent_channels)) == 0);
    CHECK(bus::telemetryRequest(6, bytes, sizeof(bytes)) == 8);
    CHECK(std::memcmp(bytes, reference::official_telemetry_request, 8) == 0);
    CHECK(bus::jetiboxRequest(0x88, 0xf0, bytes, sizeof(bytes)) == 9);
    CHECK(std::memcmp(bytes, reference::official_jetibox_request, 9) == 0);
    CHECK(bus::jetiboxResponse(0x88, reinterpret_cast<const char*>(reference::official_jetibox_response + 6),
                             bytes, sizeof(bytes)) == 40);
    CHECK(std::memcmp(bytes, reference::official_jetibox_response, 40) == 0);
    CHECK(bus::telemetryResponse(8, reference::official_telemetry_response + 6, 24, bytes, sizeof(bytes)) == 32);
    CHECK(std::memcmp(bytes, reference::official_telemetry_response, 32) == 0);
    const bus::ExValue ex[] = {{1, 12345, 3}, {2, -1234, 2}, {3, 1000, 0},
                             {4, 31, 3}, {5, -536870911, 0}};
    uint8_t inner[bus::MaxExBytes]{};
    const std::size_t innerLength = bus::exData(0xa400, 0x0300, ex, 5, inner, sizeof(inner));
    CHECK(innerLength == sizeof(reference::independent_ex_data));
    CHECK(std::memcmp(inner, reference::independent_ex_data, innerLength) == 0);
    CHECK(bus::telemetryResponse(0x42, inner, innerLength, bytes, sizeof(bytes)) == sizeof(reference::independent_telemetry_response));
    CHECK(std::memcmp(bytes, reference::independent_telemetry_response, sizeof(reference::independent_telemetry_response)) == 0);
    CHECK(bus::exText(0xa8a1, 0x555d, 2, "Temp.", "\xb0" "C", inner, sizeof(inner)) == sizeof(reference::official_ex_text));
    CHECK(std::memcmp(inner, reference::official_ex_text, sizeof(reference::official_ex_text)) == 0);
    CHECK(bus::exText(0xa400, 0x0300, 1, "Remain est", "m", inner, sizeof(inner)) == sizeof(reference::independent_ex_text));
    CHECK(std::memcmp(inner, reference::independent_ex_text, sizeof(reference::independent_ex_text)) == 0);
}

void corruptionAndStructure() {
    uint8_t bytes[bus::MaxFrameBytes]{};
    bus::Parser parser;
    bus::Frame frame;
    uint32_t now = 0;
    std::memcpy(bytes, reference::independent_channels, sizeof(reference::independent_channels));
    bytes[8] ^= 1;
    CHECK(feed(parser, bytes, sizeof(reference::independent_channels), now, frame) == 0);
    CHECK(parser.statistics().crcErrors == 1);
    now += 7000;
    CHECK(feed(parser, reference::independent_channels, now, frame) == 1);
    const uint8_t noise[] = {0xff, 0x00, 0x3e, 0x02, 0x3d, 0x01, 0xff, 0x00};
    CHECK(feed(parser, noise, now, frame) == 0);
    CHECK(feed(parser, reference::official_telemetry_request, now, frame) == 1);
    // CRC-valid invalid channel length, subblock overflow, zero channels.
    const uint8_t badLengths[] = {1, 3, 48, 50, 255, 0};
    for (uint8_t length : badLengths) {
        std::memcpy(bytes, reference::independent_channels, sizeof(reference::independent_channels));
        bytes[5] = length;
        finish(bytes, sizeof(reference::independent_channels));
        now += 7000;
        CHECK(feed(parser, bytes, sizeof(reference::independent_channels), now, frame) == 0);
    }
    // Request without bus release, wrong buttons low nibble, wrong request sublength.
    std::memcpy(bytes, reference::official_telemetry_request, 8);
    bytes[1] = 3; finish(bytes, 8); now += 7000;
    CHECK(feed(parser, bytes, 8, now, frame) == 0);
    std::memcpy(bytes, reference::official_jetibox_request, 9);
    bytes[6] = 0xf1; finish(bytes, 9); now += 7000;
    CHECK(feed(parser, bytes, 9, now, frame) == 0);
    bytes[6] = 0xf0; bytes[5] = 0; finish(bytes, 9); now += 7000;
    CHECK(feed(parser, bytes, 9, now, frame) == 0);
    // Unknown block is bounded/skipped; channels in a second block still decoded.
    const uint8_t multi[] = {0x3e, 3, 19, 1, 0x55, 1, 0xaa, 0x31, 8,
                            0x40, 0x1f, 0xe0, 0x2e, 0x80, 0x3e, 0x10, 0x27, 0, 0};
    std::memcpy(bytes, multi, sizeof(multi)); finish(bytes, sizeof(multi)); now += 7000;
    CHECK(feed(parser, bytes, sizeof(multi), now, frame) == 1);
    CHECK(frame.kind == bus::Kind::Channels && frame.channelCount == 4 && frame.channelsUs[1] == 1500);
    // Ambiguous duplicate channel blocks rejected.
    const uint8_t duplicate[] = {0x3e, 3, 14, 1, 0x31, 2, 0x40, 0x1f, 0x31, 2, 0x40, 0x1f, 0, 0};
    std::memcpy(bytes, duplicate, sizeof(duplicate)); finish(bytes, sizeof(duplicate)); now += 7000;
    CHECK(feed(parser, bytes, sizeof(duplicate), now, frame) == 0);
    // Unknown proprietary packet yields Other, never fresh channels.
    const uint8_t unknown[] = {0x3d, 1, 9, 2, 0x55, 1, 0xaa, 0, 0};
    std::memcpy(bytes, unknown, sizeof(unknown)); finish(bytes, sizeof(unknown)); now += 7000;
    CHECK(feed(parser, bytes, sizeof(unknown), now, frame) == 1);
    CHECK(frame.kind == bus::Kind::Other && frame.channelCount == 0);
    // A malformed outer length swallowing a complete valid request can recover.
    std::memset(bytes, 0, sizeof(bytes));
    bytes[0] = 0x3e; bytes[1] = 3; bytes[2] = 64;
    std::memcpy(bytes + 56, reference::official_telemetry_request, 8); now += 7000;
    CHECK(feed(parser, bytes, sizeof(bytes), now, frame) == 1);
    CHECK(frame.kind == bus::Kind::TelemetryRequest);
}

void fragmentsTimeoutAndRollover() {
    const uint32_t intervals[] = {40u, 80u, 960u};
    for (uint32_t interval : intervals) {
        bus::Parser parser;
        bus::Frame frame;
        uint32_t now = 0xfffffff0u;
        // Short requests remain below 6 ms even with a near-timeout byte gap.
        const unsigned expected = interval == 960 ? 0u : 1u;
        CHECK(feed(parser, reference::official_telemetry_request, now, frame, interval) == expected);
        if (expected) CHECK(frame.packetId == 6 && frame.receivedUs == static_cast<uint32_t>(0xfffffff0u + 7 * interval));
    }
    bus::Parser parser;
    bus::Frame frame;
    uint32_t now = 0;
    CHECK(feed(parser, reference::official_channels, 10, now, frame) == 0);
    now += bus::InterByteTimeoutUs + 1;
    parser.expire(now);
    CHECK(parser.bufferedBytes() == 0 && parser.statistics().timeouts == 1);
    CHECK(feed(parser, reference::official_channels + 10, 30, now, frame) == 0);
    now += 7000;
    CHECK(feed(parser, reference::official_channels, now, frame) == 1);
    // Total deadline cannot be prolonged by short-but-continuous byte fragments.
    parser.reset(); now += 7000;
    CHECK(feed(parser, reference::official_channels, now, frame, 160) == 0);
    CHECK(parser.statistics().timeouts >= 2);
    now += 7000;
    CHECK(feed(parser, reference::official_channels, now, frame, 40) == 1);
    CHECK(feed(parser, reference::official_telemetry_request, now, frame, 40) == 1);
}

void limitsAndAdmission() {
    uint8_t bytes[bus::MaxFrameBytes]{};
    uint16_t values[25]{};
    for (uint16_t& value : values) value = 1500;
    CHECK(bus::channels(1, values, 24, true, bytes, sizeof(bytes)) == 56);
    CHECK(bus::validFrameCrc(bytes, 56));
    CHECK(bus::channels(1, values, 25, true, bytes, sizeof(bytes)) == 0);
    CHECK(bus::channels(1, values, 0, true, bytes, sizeof(bytes)) == 0);
    CHECK(bus::channels(1, values, 2, true, bytes, 11) == 0);
    CHECK(bus::channels(1, values, 2, true, nullptr, sizeof(bytes)) == 0);
    values[0] = 8192;
    CHECK(bus::channels(1, values, 2, true, bytes, sizeof(bytes)) == 0);
    CHECK(bus::jetiboxRequest(1, 0xff, bytes, sizeof(bytes)) == 0);
    CHECK(bus::telemetryRequest(1, bytes, 7) == 0);
    CHECK(bus::telemetryResponse(1, nullptr, 0, bytes, sizeof(bytes)) == 8);
    CHECK(bus::telemetryResponse(1, nullptr, 1, bytes, sizeof(bytes)) == 0);
    CHECK(bus::jetiboxResponse(1, nullptr, bytes, sizeof(bytes)) == 0);
    bus::ExValue value{1, std::numeric_limits<int32_t>::min(), 0};
    CHECK(bus::exData(0xa400, 1, &value, 1, bytes, sizeof(bytes)) == 0);
    value.value = std::numeric_limits<int32_t>::max();
    CHECK(bus::exData(0xa400, 1, &value, 1, bytes, sizeof(bytes)) == 0);
    value.value = 0; value.id = 0;
    CHECK(bus::exData(0xa400, 1, &value, 1, bytes, sizeof(bytes)) == 0);
    value.id = 16;
    CHECK(bus::exData(0xa400, 1, &value, 1, bytes, sizeof(bytes)) == 0);
    value.id = 1; value.decimals = 4;
    CHECK(bus::exData(0xa400, 1, &value, 1, bytes, sizeof(bytes)) == 0);
    const bus::ExValue duplicate[] = {{1, 100, 0}, {1, 200, 0}};
    CHECK(bus::exData(0xa400, 1, duplicate, 2, bytes, sizeof(bytes)) == 0);
    const bus::ExValue single[] = {{1, 0, 0}};
    CHECK(bus::exData(0xa400, 1, single, 1, bytes, 9) == 0);
    CHECK(bus::exData(0xa400, 1, nullptr, 1, bytes, sizeof(bytes)) == 0);
    CHECK(bus::exText(0xa400, 1, 1, "", "", bytes, sizeof(bytes)) == 0);
    CHECK(bus::exText(0xa400, 1, 1, "Too long a label here", "", bytes, sizeof(bytes)) == 0);
    CHECK(bus::exText(0xa400, 1, 1, "Label", "Too long", bytes, sizeof(bytes)) == 0);
    CHECK(bus::exText(0xa400, 1, 1, "Label", "", bytes, 10) == 0);
    CHECK(bus::fitsResponseWindow(40, 125000, 600));
    CHECK(!bus::fitsResponseWindow(40, 125000, 601));
    CHECK(bus::fitsResponseWindow(40, 250000, 2200));
    CHECK(!bus::fitsResponseWindow(64, 125000, 0));
    CHECK(!bus::fitsResponseWindow(40, 115200, 0));
    CHECK(!bus::fitsResponseWindow(40, 125000, 0xffffffffu));
    CHECK(!bus::fitsResponseWindow(0, 125000, 0));
    CHECK(!bus::fitsResponseWindow(65, 125000, 0));
    CHECK(!bus::validFrameCrc(nullptr, 8));
    CHECK(!bus::validExCrc(nullptr, 8));
}

uint32_t randomWord(uint32_t& state) {
    state ^= state << 13; state ^= state >> 17; state ^= state << 5;
    return state;
}

void seededFuzz() {
    bus::Parser parser;
    bus::Frame frame;
    uint32_t now = 0xffff0000u;
    uint32_t seed = 0x5345494cu;
    for (unsigned iteration = 0; iteration < 100000; ++iteration) {
        const uint32_t word = randomWord(seed);
        now += word & 127u;
        const bool complete = parser.feed(static_cast<uint8_t>(word >> 8), now, frame);
        CHECK(parser.bufferedBytes() <= bus::MaxFrameBytes);
        if (complete) CHECK(frame.channelCount <= bus::MaxChannels);
        if ((iteration % 257u) == 0) {
            now += 7000;
            CHECK(feed(parser, reference::independent_channels, now, frame) == 1);
            CHECK(frame.kind == bus::Kind::Channels && frame.channelsUs[0] == 1000 && frame.channelsUs[2] == 2000);
        }
    }
    // Exhaustive single-bit corruption of one golden channel frame.
    for (std::size_t bit = 0; bit < sizeof(reference::independent_channels) * 8; ++bit) {
        uint8_t damaged[sizeof(reference::independent_channels)]{};
        std::memcpy(damaged, reference::independent_channels, sizeof(damaged));
        damaged[bit / 8] ^= static_cast<uint8_t>(1u << (bit % 8));
        now += 7000; parser.reset();
        CHECK(feed(parser, damaged, sizeof(damaged), now, frame) == 0);
    }
    std::printf("PASS fixed-seed fuzz: 100000 bytes, seed=0x5345494c, 128 single-bit mutations\n");
}
} // namespace

int main() {
    goldenPackets(); goldenEncoders(); corruptionAndStructure();
    fragmentsTimeoutAndRollover(); limitsAndAdmission(); seededFuzz();
    std::printf("%s EX Bus: %u checks, %u failures\n", failures ? "FAIL" : "PASS", checks, failures);
    return failures ? 1 : 0;
}
