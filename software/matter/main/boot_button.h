// BOOT button: short press toggles the status LEDs, a 10 second hold factory-resets.
// Copyright 2026 Jeff Hamilton. SPDX-License-Identifier: Apache-2.0
//
// Holding BOOT while the board resets still enters the ROM download mode, as on any ESP32;
// this code only sees presses after boot.

#pragma once

#include <esp_err.h>

namespace boot_button {

esp_err_t init();

} // namespace boot_button
