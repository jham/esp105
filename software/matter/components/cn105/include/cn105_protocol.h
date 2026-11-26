// CN105 serial protocol: packet encoding and decoding, no hardware access.
// Copyright 2026 Jeff Hamilton. SPDX-License-Identifier: Apache-2.0
//
// Mitsubishi indoor units speak this protocol on the CN105 connector at 2400 baud, 8E1.
// The layout below follows the community reverse engineering (SwiCago/HeatPump,
// echavet/MitsubishiCN105ESPHome); this file is an independent implementation.
//
// Every packet is: 0xFC, type, 0x01, 0x30, data length, data..., checksum
// where checksum = (0xFC - sum of all preceding bytes) & 0xFF.

#pragma once

#include <cstddef>
#include <cstdint>

namespace cn105 {

constexpr uint8_t kStart = 0xFC;
constexpr size_t kHeaderLen = 5;
constexpr size_t kMaxDataLen = 16;
constexpr size_t kMaxPacketLen = kHeaderLen + kMaxDataLen + 1;
// Set and info requests are always 16 data bytes, 22 bytes in all.
constexpr size_t kRequestLen = kHeaderLen + 16 + 1;

// Packet types (byte 1)
enum class PacketType : uint8_t {
    kSet = 0x41,         // we -> unit: change settings
    kInfoRequest = 0x42, // we -> unit: ask for one info block
    kConnect = 0x5A,     // we -> unit: handshake
    kSetAck = 0x61,      // unit -> we
    kInfoResponse = 0x62,// unit -> we
    kConnectAck = 0x7A,  // unit -> we
};

// Info block codes (data byte 0 of an info request/response)
enum class InfoCode : uint8_t {
    kSettings = 0x02,  // power, mode, setpoint, fan, vanes
    kRoomTemp = 0x03,  // room and outside temperature
    kStatus = 0x06,    // compressor frequency, operating flag
};

enum class Mode : uint8_t {
    kHeat = 0x01,
    kDry = 0x02,
    kCool = 0x03,
    kFan = 0x07,
    kAuto = 0x08,
};

// Fan codes as the unit sends them; QUIET is the slowest fixed speed.
enum class Fan : uint8_t {
    kAuto = 0x00,
    kQuiet = 0x01,
    kSpeed1 = 0x02,
    kSpeed2 = 0x03,
    kSpeed3 = 0x05,
    kSpeed4 = 0x06,
};

enum class Vane : uint8_t {
    kAuto = 0x00,
    kPos1 = 0x01,
    kPos2 = 0x02,
    kPos3 = 0x03,
    kPos4 = 0x04,
    kPos5 = 0x05,
    kSwing = 0x07,
};

// Setpoint range the unit accepts, in whole degrees C.
constexpr int kMinSetpointC = 16;
constexpr int kMaxSetpointC = 31;

// Fan as a speed number 0..5: 0 = auto, 1 = quiet, 2..5 = speeds 1..4.
int fan_to_speed(Fan fan);
Fan speed_to_fan(int speed);

// Settings block (info code 0x02)
struct Settings {
    bool power = false;
    Mode mode = Mode::kAuto;
    bool isee = false;           // i-see sensor active (reported as mode + 0x08)
    float setpoint_c = 22.0f;
    bool half_degrees = false;   // unit reports and accepts 0.5 degree steps
    Fan fan = Fan::kAuto;
    Vane vane = Vane::kAuto;
    uint8_t wide_vane = 0;       // raw low nibble; not every unit has one
};

// Room temperature block (info code 0x03)
struct RoomTemp {
    float room_c = 0.0f;
    bool has_outside = false;
    float outside_c = 0.0f;
};

// Status block (info code 0x06)
struct Status {
    uint8_t compressor_hz = 0;
    bool operating = false;
};

// Fields to change in one set packet. Unset fields are left as they are on the unit.
struct Change {
    bool set_power = false;
    bool power = false;
    bool set_mode = false;
    Mode mode = Mode::kAuto;
    bool set_setpoint = false;
    float setpoint_c = 22.0f;
    bool set_fan = false;
    Fan fan = Fan::kAuto;
    bool set_vane = false;
    Vane vane = Vane::kAuto;

    bool empty() const { return !(set_power || set_mode || set_setpoint || set_fan || set_vane); }
    // Merges `other` over this one; fields set in `other` win.
    void merge(const Change &other);
};

uint8_t checksum(const uint8_t *bytes, size_t len);

// Builders return the packet length written to `out` (at least kRequestLen bytes).
size_t build_connect(uint8_t *out);
size_t build_info_request(InfoCode code, uint8_t *out);
// `half_degrees` selects the 0.5 degree setpoint encoding the unit reported.
size_t build_set(const Change &change, bool half_degrees, uint8_t *out);

// Incremental parser: feed bytes as they arrive; returns true when a complete
// packet with a valid checksum is available in packet()/length().
class Parser {
public:
    bool feed(uint8_t byte);
    void reset() { len_ = 0; }
    const uint8_t *packet() const { return buf_; }
    size_t length() const { return len_; }
    PacketType type() const { return static_cast<PacketType>(buf_[1]); }
    const uint8_t *data() const { return buf_ + kHeaderLen; }
    size_t data_length() const { return buf_[4]; }
    uint32_t checksum_errors() const { return checksum_errors_; }

private:
    uint8_t buf_[kMaxPacketLen] = {};
    size_t len_ = 0;
    uint32_t checksum_errors_ = 0;
};

// Decoders take the data part of an info response (data[0] is the info code).
bool decode_settings(const uint8_t *data, size_t len, Settings *out);
bool decode_room_temp(const uint8_t *data, size_t len, RoomTemp *out);
bool decode_status(const uint8_t *data, size_t len, Status *out);

} // namespace cn105
