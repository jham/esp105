// CN105 heat pump link: UART task that keeps the connection up, polls the unit and
// sends requested changes.
// Copyright 2026 Jeff Hamilton. SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include <esp_err.h>

#include "cn105_protocol.h"

namespace cn105 {

struct State {
    bool connected = false;
    bool have_settings = false;
    Settings settings;
    bool have_room = false;
    RoomTemp room;
    bool have_status = false;
    Status status;
};

struct Config {
    int uart_num = 1;
    int tx_pin = 6;
    int rx_pin = 7;
    // One poll round reads settings, room temperature and status.
    uint32_t poll_interval_ms = 2000;
    // Delay before the first handshake, so a USB log client can attach.
    uint32_t start_delay_ms = 2000;
};

// Called from the CN105 task whenever State changes. Keep it short; do not block.
using StateCallback = void (*)(const State &state, void *ctx);

esp_err_t start(const Config &config, StateCallback callback, void *ctx);

// Queues a change. Changes made before the task sends them are merged, so the
// latest value of each field wins. Safe to call from any task.
void request(const Change &change);

// Snapshot of the last known state.
State get_state();

} // namespace cn105
