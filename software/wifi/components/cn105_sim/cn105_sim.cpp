// A fake Mitsubishi indoor unit on a UART, for testing the CN105 firmware without a heat pump.
// Copyright 2026 Jeff Hamilton. SPDX-License-Identifier: Apache-2.0

#include "cn105_sim.h"

#include <cmath>
#include <cstring>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::cn105_sim {

static const char *const TAG = "cn105_sim";

static constexpr uint8_t kStart = 0xFC;

// Packet types
static constexpr uint8_t kSet = 0x41;
static constexpr uint8_t kInfoRequest = 0x42;
static constexpr uint8_t kConnect = 0x5A;
static constexpr uint8_t kConnectInstaller = 0x5B;
static constexpr uint8_t kSetAck = 0x61;
static constexpr uint8_t kInfoResponse = 0x62;

// Modes
static constexpr uint8_t kHeat = 0x01;
static constexpr uint8_t kDry = 0x02;
static constexpr uint8_t kCool = 0x03;
static constexpr uint8_t kFan = 0x07;
static constexpr uint8_t kAuto = 0x08;

static uint8_t checksum(const uint8_t *bytes, size_t len) {
  uint8_t sum = 0;
  for (size_t i = 0; i < len; i++)
    sum += bytes[i];
  return static_cast<uint8_t>(kStart - sum);
}

static const char *mode_name(uint8_t mode) {
  switch (mode) {
    case kHeat:
      return "HEAT";
    case kDry:
      return "DRY";
    case kCool:
      return "COOL";
    case kFan:
      return "FAN";
    case kAuto:
      return "AUTO";
    default:
      return "?";
  }
}

static uint8_t clamp_u8(long v, long lo, long hi) { return static_cast<uint8_t>(v < lo ? lo : (v > hi ? hi : v)); }

void Cn105Sim::setup() {
  this->room_c_ = this->ambient_c_;
  this->last_step_ms_ = millis();
  ESP_LOGI(TAG, "Simulated indoor unit ready: OFF, COOL, %.1f C setpoint, room %.1f C", this->setpoint_c_,
           this->room_c_);
}

void Cn105Sim::dump_config() {
  ESP_LOGCONFIG(TAG,
                "CN105 heat pump simulator:\n"
                "  Room temperature at rest: %.1f C\n"
                "  Outside temperature: %.1f C\n"
                "  Half-degree setpoints: %s\n"
                "  Room update interval: %" PRIu32 " ms",
                this->ambient_c_, this->outside_c_, YESNO(this->half_degrees_), this->update_interval_ms_);
}

void Cn105Sim::loop() {
  uint8_t byte;
  while (this->available() && this->read_byte(&byte)) {
    if (this->feed_(byte))
      this->handle_packet_();
  }
  const uint32_t now = millis();
  if (now - this->last_step_ms_ >= this->update_interval_ms_) {
    if (this->heating_ || this->cooling_) {
      this->runtime_ms_ += now - this->last_step_ms_;
      this->runtime_minutes_ += this->runtime_ms_ / 60000;
      this->runtime_ms_ %= 60000;
    }
    this->last_step_ms_ = now;
    this->step_room_();
  }
}

bool Cn105Sim::feed_(uint8_t byte) {
  if (this->len_ > kHeaderLen && this->len_ == kHeaderLen + this->buf_[4] + 1)
    this->len_ = 0;  // previous packet done
  if (this->len_ == 0 && byte != kStart)
    return false;
  this->buf_[this->len_++] = byte;
  if (this->len_ == kHeaderLen && (this->buf_[2] != 0x01 || this->buf_[3] != 0x30 || this->buf_[4] > 16)) {
    this->len_ = (byte == kStart) ? 1 : 0;
    if (this->len_)
      this->buf_[0] = kStart;
    return false;
  }
  if (this->len_ > kHeaderLen && this->len_ == kHeaderLen + this->buf_[4] + 1) {
    if (checksum(this->buf_, this->len_ - 1) == this->buf_[this->len_ - 1])
      return true;
    this->checksum_errors_++;
    ESP_LOGW(TAG, "Dropped a packet with a bad checksum (%" PRIu32 " so far)", this->checksum_errors_);
    this->len_ = 0;
  }
  return false;
}

void Cn105Sim::reply_(uint8_t type, const uint8_t *data, size_t len) {
  uint8_t out[kMaxPacket];
  out[0] = kStart;
  out[1] = type;
  out[2] = 0x01;
  out[3] = 0x30;
  out[4] = static_cast<uint8_t>(len);
  std::memcpy(out + kHeaderLen, data, len);
  out[kHeaderLen + len] = checksum(out, kHeaderLen + len);
  this->write_array(out, kHeaderLen + len + 1);
}

void Cn105Sim::handle_packet_() {
  this->packets_++;
  const uint8_t type = this->buf_[1];
  const uint8_t *data = this->buf_ + kHeaderLen;
  uint8_t reply[16] = {};

  switch (type) {
    case kConnect:
    case kConnectInstaller:
      ESP_LOGI(TAG, "Handshake (0x%02X)", type);
      reply[0] = 0x00;
      this->reply_(type + 0x20, reply, 1);  // 0x7A / 0x7B
      break;

    case kInfoRequest:
      ESP_LOGV(TAG, "Info request 0x%02X", data[0]);
      this->fill_info_(data[0], reply);
      this->reply_(kInfoResponse, reply, sizeof(reply));
      break;

    case kSet:
      if (data[0] == 0x01) {
        this->apply_set_(data);
      } else if (data[0] == 0x07) {
        ESP_LOGI(TAG, "Remote temperature command: %02X %02X %02X (not simulated)", data[1], data[2], data[3]);
      } else {
        ESP_LOGI(TAG, "Set command 0x%02X acknowledged (not simulated)", data[0]);
      }
      this->reply_(kSetAck, reply, sizeof(reply));
      break;

    default:
      ESP_LOGW(TAG, "Ignoring packet type 0x%02X", type);
      break;
  }
}

void Cn105Sim::fill_info_(uint8_t code, uint8_t *d) {
  d[0] = code;
  switch (code) {
    case 0x02: {  // settings
      d[3] = this->power_ ? 0x01 : 0x00;
      d[4] = this->mode_;
      d[5] = clamp_u8(31 - std::lround(this->setpoint_c_), 0, 15);
      d[6] = this->fan_;
      d[7] = this->vane_;
      d[10] = this->wide_vane_;
      d[11] = this->half_degrees_ ? static_cast<uint8_t>(std::lround(this->setpoint_c_ * 2) + 128) : 0x00;
      break;
    }
    case 0x03: {  // room temperature
      d[3] = clamp_u8(std::lround(this->room_c_) - 10, 0, 31);
      d[5] = static_cast<uint8_t>(std::lround(this->outside_c_ * 2) + 128);
      d[6] = this->half_degrees_ ? static_cast<uint8_t>(std::lround(this->room_c_ * 2) + 128) : 0x00;
      d[11] = (this->runtime_minutes_ >> 16) & 0xFF;
      d[12] = (this->runtime_minutes_ >> 8) & 0xFF;
      d[13] = this->runtime_minutes_ & 0xFF;
      break;
    }
    case 0x06: {  // status
      const bool operating = this->heating_ || this->cooling_;
      float gap = std::fabs(this->setpoint_c_ - this->room_c_);
      d[3] = operating ? clamp_u8(std::lround(20 + gap * 15), 20, 90) : 0;  // compressor Hz
      d[4] = operating ? 0x01 : 0x00;
      break;
    }
    case 0x09: {  // standby / stage
      d[3] = this->power_ ? 0x00 : 0x10;                            // sub mode: NORMAL or OFF
      d[4] = (this->heating_ || this->cooling_) ? 0x03 : 0x00;      // stage: MEDIUM or IDLE
      if (this->power_ && this->mode_ == kAuto)
        d[5] = this->heating_ ? 0x02 : (this->cooling_ ? 0x01 : 0x00);  // AUTO_HEAT / AUTO_COOL
      break;
    }
    default:
      // Timers (0x05), errors (0x04), options (0x42), functions (0x20/0x22): all zero,
      // which reads as "none" or "not supported".
      break;
  }
}

void Cn105Sim::apply_set_(const uint8_t *d) {
  const uint8_t flags = d[1];
  const uint8_t flags2 = d[2];
  if (flags & 0x01)
    this->power_ = d[3] != 0;
  if (flags & 0x02) {
    switch (d[4]) {
      case kHeat:
      case kDry:
      case kCool:
      case kFan:
      case kAuto:
        this->mode_ = d[4];
        break;
      default:
        ESP_LOGW(TAG, "Unknown mode 0x%02X ignored", d[4]);
    }
  }
  if (flags & 0x04) {
    float c = d[14] != 0 ? (d[14] - 128) / 2.0f : 31.0f - d[5];
    if (!this->half_degrees_)
      c = std::round(c);
    this->setpoint_c_ = c < 16 ? 16 : (c > 31 ? 31 : c);
  }
  if (flags & 0x08)
    this->fan_ = d[6];
  if (flags & 0x10)
    this->vane_ = d[7];
  if (flags2 & 0x01)
    this->wide_vane_ = d[13] & 0x0F;

  this->update_operating_();
  ESP_LOGI(TAG, "Set: %s %s %.1f C, fan 0x%02X, vane 0x%02X, wide vane 0x%02X (room %.1f C)",
           this->power_ ? "ON" : "OFF", mode_name(this->mode_), this->setpoint_c_, this->fan_, this->vane_,
           this->wide_vane_, this->room_c_);
}

void Cn105Sim::update_operating_() {
  const bool was_heating = this->heating_, was_cooling = this->cooling_;
  this->heating_ = this->cooling_ = false;
  if (this->power_) {
    const float sp = this->setpoint_c_, room = this->room_c_;
    switch (this->mode_) {
      case kHeat:
        this->heating_ = room < sp;
        break;
      case kCool:
      case kDry:
        this->cooling_ = room > sp;
        break;
      case kAuto:
        // A half-degree band either side, and keep going until the setpoint is reached.
        this->heating_ = room < sp - 0.5f || (was_heating && room < sp);
        this->cooling_ = room > sp + 0.5f || (was_cooling && room > sp);
        break;
      default:
        break;
    }
  }
  if (this->heating_ != was_heating || this->cooling_ != was_cooling) {
    ESP_LOGI(TAG, "Now %s (room %.1f C, setpoint %.1f C)",
             this->heating_ ? "heating" : (this->cooling_ ? "cooling" : "idle"), this->room_c_, this->setpoint_c_);
  }
}

void Cn105Sim::step_room_() {
  if (this->heating_) {
    this->room_c_ += 0.5f;
  } else if (this->cooling_) {
    this->room_c_ -= 0.5f;
  } else if (this->room_c_ < this->ambient_c_) {
    this->room_c_ += 0.5f;
  } else if (this->room_c_ > this->ambient_c_) {
    this->room_c_ -= 0.5f;
  }
  this->update_operating_();
}

}  // namespace esphome::cn105_sim
