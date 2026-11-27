// A fake Mitsubishi indoor unit on a UART, for testing the CN105 firmware without a heat pump.
// Copyright 2026 Jeff Hamilton. SPDX-License-Identifier: Apache-2.0
//
// It answers the handshake, the info requests the CN105 component polls (settings, room
// temperature, status, standby and the rest with empty blocks) and set commands, and runs a
// crude room model: while the unit heats or cools, the room moves 0.5 degrees towards the
// setpoint every update interval; otherwise it drifts back to `room_temperature`.

#pragma once

#include <cstddef>
#include <cstdint>

#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

namespace esphome::cn105_sim {

class Cn105Sim : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_room_temperature(float c) { this->ambient_c_ = c; }
  void set_outside_temperature(float c) { this->outside_c_ = c; }
  void set_half_degrees(bool v) { this->half_degrees_ = v; }
  void set_update_interval(uint32_t ms) { this->update_interval_ms_ = ms; }

 protected:
  static constexpr size_t kHeaderLen = 5;
  static constexpr size_t kMaxPacket = kHeaderLen + 16 + 1;

  bool feed_(uint8_t byte);
  void handle_packet_();
  void reply_(uint8_t type, const uint8_t *data, size_t len);
  void fill_info_(uint8_t code, uint8_t *data);
  void apply_set_(const uint8_t *data);
  void step_room_();
  void update_operating_();

  // Receive buffer
  uint8_t buf_[kMaxPacket]{};
  size_t len_{0};

  // Unit settings
  bool power_{false};
  uint8_t mode_{0x03};  // COOL
  float setpoint_c_{22.0f};
  uint8_t fan_{0x00};   // AUTO
  uint8_t vane_{0x00};  // AUTO
  uint8_t wide_vane_{0x03};

  // Room model
  float ambient_c_{21.0f};
  float room_c_{21.0f};
  float outside_c_{8.0f};
  bool half_degrees_{true};
  uint32_t update_interval_ms_{20000};
  uint32_t last_step_ms_{0};
  bool heating_{false};
  bool cooling_{false};
  uint32_t runtime_minutes_{0};
  uint32_t runtime_ms_{0};

  // Counters for dump_config / logs
  uint32_t packets_{0};
  uint32_t checksum_errors_{0};
};

}  // namespace esphome::cn105_sim
