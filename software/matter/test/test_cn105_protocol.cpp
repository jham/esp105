// Host-side tests for the CN105 packet code. Build and run with ./run_tests.sh.
// Copyright 2026 Jeff Hamilton. SPDX-License-Identifier: Apache-2.0

#include "cn105_protocol.h"

#include <cstdio>
#include <cstring>
#include <vector>

using namespace cn105;

static int g_failures = 0;

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                            \
            g_failures++;                                                                          \
        }                                                                                          \
    } while (0)

// Builds a unit-to-us packet with a correct checksum.
static std::vector<uint8_t> make_packet(uint8_t type, std::vector<uint8_t> data)
{
    std::vector<uint8_t> p = {kStart, type, 0x01, 0x30, static_cast<uint8_t>(data.size())};
    p.insert(p.end(), data.begin(), data.end());
    p.push_back(checksum(p.data(), p.size()));
    return p;
}

static void test_connect()
{
    // The handshake every CN105 implementation sends.
    const uint8_t expected[] = {0xFC, 0x5A, 0x01, 0x30, 0x02, 0xCA, 0x01, 0xA8};
    uint8_t out[kRequestLen];
    size_t len = build_connect(out);
    CHECK(len == sizeof(expected));
    CHECK(std::memcmp(out, expected, sizeof(expected)) == 0);
}

static void test_info_request()
{
    uint8_t out[kRequestLen];
    size_t len = build_info_request(InfoCode::kSettings, out);
    CHECK(len == 22);
    CHECK(out[0] == 0xFC && out[1] == 0x42 && out[2] == 0x01 && out[3] == 0x30 && out[4] == 0x10);
    CHECK(out[5] == 0x02);
    for (int i = 6; i < 21; i++) {
        CHECK(out[i] == 0);
    }
    CHECK(out[21] == 0x7B);
}

static void test_set_whole_degrees()
{
    Change c;
    c.set_power = true;
    c.power = true;
    c.set_mode = true;
    c.mode = Mode::kCool;
    c.set_setpoint = true;
    c.setpoint_c = 23.4f; // rounds to 23
    uint8_t out[kRequestLen];
    size_t len = build_set(c, false, out);
    CHECK(len == 22);
    CHECK(out[1] == 0x41 && out[5] == 0x01);
    CHECK(out[6] == 0x07);    // power, mode, setpoint
    CHECK(out[7] == 0x00);
    CHECK(out[8] == 0x01);
    CHECK(out[9] == 0x03);
    CHECK(out[10] == 31 - 23);
    CHECK(out[19] == 0);
    CHECK(out[21] == checksum(out, 21));
}

static void test_set_half_degrees_and_clamp()
{
    Change c;
    c.set_setpoint = true;
    c.setpoint_c = 23.5f;
    uint8_t out[kRequestLen];
    build_set(c, true, out);
    CHECK(out[6] == 0x04);
    CHECK(out[10] == 0);
    CHECK(out[19] == 47 + 128);

    c.setpoint_c = 40.0f;
    build_set(c, true, out);
    CHECK(out[19] == 62 + 128); // clamped to 31

    c.setpoint_c = 5.0f;
    build_set(c, false, out);
    CHECK(out[10] == 31 - 16); // clamped to 16
}

static void test_set_fan_and_vane()
{
    Change c;
    c.set_fan = true;
    c.fan = Fan::kSpeed3;
    c.set_vane = true;
    c.vane = Vane::kSwing;
    uint8_t out[kRequestLen];
    build_set(c, false, out);
    CHECK(out[6] == 0x18);
    CHECK(out[11] == 0x05);
    CHECK(out[12] == 0x07);
}

static void test_merge()
{
    Change a;
    a.set_power = true;
    a.power = true;
    a.set_setpoint = true;
    a.setpoint_c = 20.0f;
    Change b;
    b.set_setpoint = true;
    b.setpoint_c = 24.0f;
    a.merge(b);
    CHECK(a.set_power && a.power);
    CHECK(a.set_setpoint && a.setpoint_c == 24.0f);
    CHECK(!a.set_mode && !a.set_fan && !a.set_vane);
    CHECK(Change().empty());
    CHECK(!a.empty());
}

static void test_fan_speeds()
{
    for (int s = 0; s <= 5; s++) {
        CHECK(fan_to_speed(speed_to_fan(s)) == s);
    }
    CHECK(speed_to_fan(9) == Fan::kSpeed4);
    CHECK(speed_to_fan(-1) == Fan::kAuto);
}

static bool parse_all(Parser &parser, const std::vector<uint8_t> &bytes, int *packets)
{
    bool last = false;
    for (uint8_t b : bytes) {
        last = parser.feed(b);
        if (last) {
            (*packets)++;
        }
    }
    return last;
}

static void test_parse_settings()
{
    // Settings: power on, HEAT, index 9 (22 C), fan speed 2, vane swing, half-degree 21.5 C
    auto p = make_packet(0x62, {0x02, 0x00, 0x00, 0x01, 0x01, 0x09, 0x03, 0x07, 0x00, 0x00, 0x03, 0xAB,
                                0x00, 0x00, 0x00, 0x00});
    Parser parser;
    int packets = 0;
    CHECK(parse_all(parser, p, &packets));
    CHECK(packets == 1);
    CHECK(parser.type() == PacketType::kInfoResponse);
    CHECK(parser.data_length() == 16);
    Settings s;
    CHECK(decode_settings(parser.data(), parser.data_length(), &s));
    CHECK(s.power);
    CHECK(s.mode == Mode::kHeat);
    CHECK(!s.isee);
    CHECK(s.half_degrees);
    CHECK(s.setpoint_c == 21.5f);
    CHECK(s.fan == Fan::kSpeed2);
    CHECK(s.vane == Vane::kSwing);
    CHECK(s.wide_vane == 3);

    // Whole-degree unit with i-see on (mode reported as COOL + 0x08)
    auto q = make_packet(0x62, {0x02, 0x00, 0x00, 0x00, 0x0B, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0x00, 0x00});
    packets = 0;
    CHECK(parse_all(parser, q, &packets));
    CHECK(decode_settings(parser.data(), parser.data_length(), &s));
    CHECK(!s.power);
    CHECK(s.isee);
    CHECK(s.mode == Mode::kCool);
    CHECK(!s.half_degrees);
    CHECK(s.setpoint_c == 24.0f);

    // Unknown mode byte is rejected rather than guessed
    auto r = make_packet(0x62, {0x02, 0x00, 0x00, 0x01, 0x05, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0x00, 0x00});
    packets = 0;
    CHECK(parse_all(parser, r, &packets));
    CHECK(!decode_settings(parser.data(), parser.data_length(), &s));
}

static void test_parse_room_and_status()
{
    Parser parser;
    int packets = 0;
    auto room = make_packet(0x62, {0x03, 0x00, 0x00, 0x0B, 0x00, 0x8A, 0xAD, 0x00, 0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00});
    CHECK(parse_all(parser, room, &packets));
    RoomTemp r;
    CHECK(decode_room_temp(parser.data(), parser.data_length(), &r));
    CHECK(r.room_c == 22.5f);
    CHECK(r.has_outside && r.outside_c == 5.0f);

    auto room_old = make_packet(0x62, {0x03, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                       0x00, 0x00, 0x00, 0x00, 0x00});
    CHECK(parse_all(parser, room_old, &packets));
    CHECK(decode_room_temp(parser.data(), parser.data_length(), &r));
    CHECK(r.room_c == 21.0f);
    CHECK(!r.has_outside);

    auto status = make_packet(0x62, {0x06, 0x00, 0x00, 0x2A, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                     0x00, 0x00, 0x00, 0x00, 0x00});
    CHECK(parse_all(parser, status, &packets));
    Status st;
    CHECK(decode_status(parser.data(), parser.data_length(), &st));
    CHECK(st.compressor_hz == 42);
    CHECK(st.operating);
    CHECK(!decode_settings(parser.data(), parser.data_length(), nullptr));
}

static void test_parser_resync()
{
    Parser parser;
    int packets = 0;
    auto ack = make_packet(0x7A, {0x00});
    std::vector<uint8_t> stream = {0x00, 0x13, 0xFC, 0x99, 0xFC}; // noise, then a bogus header
    stream.insert(stream.end(), ack.begin(), ack.end());
    CHECK(parse_all(parser, stream, &packets));
    CHECK(packets == 1);
    CHECK(parser.type() == PacketType::kConnectAck);

    // A corrupted checksum is dropped and counted, and the next good packet still parses
    auto bad = make_packet(0x61, {0x00});
    bad.back() ^= 0xFF;
    packets = 0;
    CHECK(!parse_all(parser, bad, &packets));
    CHECK(parser.checksum_errors() == 1);
    CHECK(parse_all(parser, ack, &packets));
    CHECK(packets == 1);

    // Back-to-back packets both come out
    std::vector<uint8_t> two = ack;
    two.insert(two.end(), ack.begin(), ack.end());
    packets = 0;
    parse_all(parser, two, &packets);
    CHECK(packets == 2);
}

int main()
{
    test_connect();
    test_info_request();
    test_set_whole_degrees();
    test_set_half_degrees_and_clamp();
    test_set_fan_and_vane();
    test_merge();
    test_fan_speeds();
    test_parse_settings();
    test_parse_room_and_status();
    test_parser_resync();
    if (g_failures) {
        std::printf("%d check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("All CN105 protocol tests passed\n");
    return 0;
}
