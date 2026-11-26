// Pin map for the esp105 board, version 0.3.0. Matches ../../wifi/esp105.yaml and the schematic.
// Copyright 2026 Jeff Hamilton. SPDX-License-Identifier: Apache-2.0

#pragma once

namespace board {

// Heat pump UART (2400 baud, 8E1)
//   GPIO6 TX -> U3 buffer (5 V) -> Grove pin 4 -> heat pump RX (CN105 pin 5)
//   GPIO7 RX <- R2/R3 divider   <- Grove pin 3 <- heat pump TX (CN105 pin 4)
constexpr int kHeatPumpUart = 1;
constexpr int kHeatPumpTx = 6;
constexpr int kHeatPumpRx = 7;

// Status LEDs, high = on
constexpr int kLedPwr = 23;  // green
constexpr int kLedHeat = 22; // red
constexpr int kLedCool = 21; // blue

// BOOT button: R12 10k pull-up, pressed = low. Also the boot strapping pin.
constexpr int kBootButton = 9;

} // namespace board
