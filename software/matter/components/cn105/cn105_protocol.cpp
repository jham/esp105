// CN105 serial protocol: packet encoding and decoding, no hardware access.
// Copyright 2026 Jeff Hamilton. SPDX-License-Identifier: Apache-2.0

#include "cn105_protocol.h"

#include <cmath>
#include <cstring>

namespace cn105 {

namespace {

constexpr uint8_t kHeader[kHeaderLen - 1] = {kStart, 0x00, 0x01, 0x30};

// Set packet control flags
constexpr uint8_t kFlagPower = 0x01;
constexpr uint8_t kFlagMode = 0x02;
constexpr uint8_t kFlagSetpoint = 0x04;
constexpr uint8_t kFlagFan = 0x08;
constexpr uint8_t kFlagVane = 0x10;

size_t start_packet(PacketType type, uint8_t data_len, uint8_t *out)
{
    std::memcpy(out, kHeader, sizeof(kHeader));
    out[1] = static_cast<uint8_t>(type);
    out[4] = data_len;
    std::memset(out + kHeaderLen, 0, data_len);
    return kHeaderLen + data_len;
}

size_t finish_packet(uint8_t *out, size_t len)
{
    out[len] = checksum(out, len);
    return len + 1;
}

float clamp_setpoint(float c)
{
    if (c < kMinSetpointC) {
        return kMinSetpointC;
    }
    if (c > kMaxSetpointC) {
        return kMaxSetpointC;
    }
    return c;
}

bool valid_mode(uint8_t raw)
{
    switch (static_cast<Mode>(raw)) {
    case Mode::kHeat:
    case Mode::kDry:
    case Mode::kCool:
    case Mode::kFan:
    case Mode::kAuto:
        return true;
    }
    return false;
}

} // namespace

int fan_to_speed(Fan fan)
{
    switch (fan) {
    case Fan::kAuto:
        return 0;
    case Fan::kQuiet:
        return 1;
    case Fan::kSpeed1:
        return 2;
    case Fan::kSpeed2:
        return 3;
    case Fan::kSpeed3:
        return 4;
    case Fan::kSpeed4:
        return 5;
    }
    return 0;
}

Fan speed_to_fan(int speed)
{
    switch (speed) {
    case 1:
        return Fan::kQuiet;
    case 2:
        return Fan::kSpeed1;
    case 3:
        return Fan::kSpeed2;
    case 4:
        return Fan::kSpeed3;
    case 5:
        return Fan::kSpeed4;
    default:
        return speed > 5 ? Fan::kSpeed4 : Fan::kAuto;
    }
}

void Change::merge(const Change &other)
{
    if (other.set_power) {
        set_power = true;
        power = other.power;
    }
    if (other.set_mode) {
        set_mode = true;
        mode = other.mode;
    }
    if (other.set_setpoint) {
        set_setpoint = true;
        setpoint_c = other.setpoint_c;
    }
    if (other.set_fan) {
        set_fan = true;
        fan = other.fan;
    }
    if (other.set_vane) {
        set_vane = true;
        vane = other.vane;
    }
}

uint8_t checksum(const uint8_t *bytes, size_t len)
{
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += bytes[i];
    }
    return static_cast<uint8_t>(kStart - sum);
}

size_t build_connect(uint8_t *out)
{
    size_t len = start_packet(PacketType::kConnect, 2, out);
    out[5] = 0xCA;
    out[6] = 0x01;
    return finish_packet(out, len);
}

size_t build_info_request(InfoCode code, uint8_t *out)
{
    size_t len = start_packet(PacketType::kInfoRequest, 16, out);
    out[5] = static_cast<uint8_t>(code);
    return finish_packet(out, len);
}

size_t build_set(const Change &change, bool half_degrees, uint8_t *out)
{
    size_t len = start_packet(PacketType::kSet, 16, out);
    out[5] = 0x01; // "settings" set command
    uint8_t flags = 0;
    if (change.set_power) {
        flags |= kFlagPower;
        out[8] = change.power ? 0x01 : 0x00;
    }
    if (change.set_mode) {
        flags |= kFlagMode;
        out[9] = static_cast<uint8_t>(change.mode);
    }
    if (change.set_setpoint) {
        flags |= kFlagSetpoint;
        float c = clamp_setpoint(change.setpoint_c);
        if (half_degrees) {
            out[19] = static_cast<uint8_t>(std::lround(c * 2.0f) + 128);
        } else {
            out[10] = static_cast<uint8_t>(kMaxSetpointC - std::lround(c));
        }
    }
    if (change.set_fan) {
        flags |= kFlagFan;
        out[11] = static_cast<uint8_t>(change.fan);
    }
    if (change.set_vane) {
        flags |= kFlagVane;
        out[12] = static_cast<uint8_t>(change.vane);
    }
    out[6] = flags;
    return finish_packet(out, len);
}

bool Parser::feed(uint8_t byte)
{
    // A complete packet was returned last time; start over.
    if (len_ > kHeaderLen && len_ == kHeaderLen + buf_[4] + 1) {
        len_ = 0;
    }
    if (len_ == 0 && byte != kStart) {
        return false;
    }
    buf_[len_++] = byte;

    if (len_ == kHeaderLen) {
        if (buf_[2] != 0x01 || buf_[3] != 0x30 || buf_[4] > kMaxDataLen) {
            // Not a header after all. Resynchronise on the next start byte in what we have.
            size_t i = 1;
            while (i < len_ && buf_[i] != kStart) {
                i++;
            }
            std::memmove(buf_, buf_ + i, len_ - i);
            len_ -= i;
        }
        return false;
    }
    if (len_ > kHeaderLen && len_ == kHeaderLen + buf_[4] + 1) {
        if (checksum(buf_, len_ - 1) == buf_[len_ - 1]) {
            return true;
        }
        checksum_errors_++;
        len_ = 0;
    }
    return false;
}

bool decode_settings(const uint8_t *data, size_t len, Settings *out)
{
    if (len < 12 || data[0] != static_cast<uint8_t>(InfoCode::kSettings)) {
        return false;
    }
    Settings s;
    s.power = data[3] != 0;
    uint8_t mode = data[4];
    if (mode > 0x08) {
        s.isee = true;
        mode -= 0x08;
    }
    if (!valid_mode(mode)) {
        return false;
    }
    s.mode = static_cast<Mode>(mode);
    // data[11] is the half-degree setpoint; 0 or 0x80 means the unit does not use it.
    if (data[11] != 0 && data[11] != 0x80) {
        s.half_degrees = true;
        s.setpoint_c = (data[11] - 128) / 2.0f;
    } else if (data[5] <= kMaxSetpointC - kMinSetpointC) {
        s.setpoint_c = static_cast<float>(kMaxSetpointC - data[5]);
    } else {
        return false;
    }
    s.fan = static_cast<Fan>(data[6]);
    s.vane = static_cast<Vane>(data[7]);
    s.wide_vane = data[10] & 0x0F;
    *out = s;
    return true;
}

bool decode_room_temp(const uint8_t *data, size_t len, RoomTemp *out)
{
    if (len < 7 || data[0] != static_cast<uint8_t>(InfoCode::kRoomTemp)) {
        return false;
    }
    RoomTemp r;
    if (data[6] != 0) {
        r.room_c = (data[6] - 128) / 2.0f;
    } else {
        r.room_c = static_cast<float>(data[3] + 10);
    }
    if (data[5] > 1) {
        r.has_outside = true;
        r.outside_c = (data[5] - 128) / 2.0f;
    }
    *out = r;
    return true;
}

bool decode_status(const uint8_t *data, size_t len, Status *out)
{
    if (len < 5 || data[0] != static_cast<uint8_t>(InfoCode::kStatus)) {
        return false;
    }
    out->compressor_hz = data[3];
    out->operating = data[4] != 0;
    return true;
}

} // namespace cn105
