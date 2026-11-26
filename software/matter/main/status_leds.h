// PWR, HEAT and COOL status LEDs.
// Copyright 2026 Jeff Hamilton. SPDX-License-Identifier: Apache-2.0
//
// PWR:  solid once the board is commissioned and attached to a Thread network; fast blink
//       while a commissioning window is open; slow blink otherwise.
// HEAT: full while heating, dim in HEAT mode while idle.
// COOL: full while cooling or drying, dim in COOL or DRY mode while idle.
// AUTO with the unit idle dims both. All off when the heat pump is off.
// A short press of BOOT turns all of them off and on again; the choice is kept in NVS.

#pragma once

#include <esp_err.h>

namespace status_leds {

enum class Network {
    kNotCommissioned,
    kCommissioning,
    kConnected,
    kDisconnected,
};

esp_err_t init();

void set_network(Network network);
// Brightness 0..1 for the HEAT and COOL LEDs.
void set_climate(float heat, float cool);
// Blinks all LEDs together for `seconds` (Matter Identify); 0 stops it.
void identify(int seconds);
// All LEDs on solid while the BOOT button is held long enough for a factory reset.
void factory_reset_warning(bool on);

bool enabled();
void set_enabled(bool enabled);
void toggle_enabled();

} // namespace status_leds
