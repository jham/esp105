# Wi-Fi firmware (ESPHome)

ESPHome firmware for the esp105 board, version 0.3.0 (the Grove-socket board). It talks to the
heat pump through the [MitsubishiCN105ESPHome](https://github.com/echavet/MitsubishiCN105ESPHome)
component and shows up in Home Assistant as a climate entity, with a password-protected local web
page for use without Home Assistant. For Thread and other Matter controllers, use the firmware in
[`../matter/`](../matter/) instead.

Run the commands on this page from this folder, `software/wifi/`.

| File | What it is |
|---|---|
| `esp105.yaml` | The device configuration. Pin map, heat pump settings, LEDs, button, diagnostics. |
| `secrets.example.yaml` | Template for `secrets.yaml`, which holds the Wi-Fi credentials and keys and is git-ignored. |
| `nanoc6.yaml` | Test build for an M5Stack NanoC6 with a simulated heat pump; see "Testing without a heat pump". |
| `components/cn105_sim/` | The simulated heat pump used by `nanoc6.yaml`. |

The configuration is pinned to ESPHome 2025.11 or later and to the CN105 component release
2026.11.1. It validates and builds with ESPHome 2026.11.0.

## What the board does

**Heat pump.** UART1 at 2400 baud, 8E1, TX on GPIO6 and RX on GPIO7. Home Assistant gets the
climate entity (mode, target temperature, fan, vertical swing) plus a vertical vane select. Sensors
that not every indoor unit reports, such as compressor frequency, outside temperature, input power,
energy, i-see and the horizontal vane, are present but disabled by default; enable the ones your
unit supports in Home Assistant.

**LEDs.**

| LED | Meaning |
|---|---|
| PWR (green) | On when Wi-Fi is connected, blinking while it is not. |
| HEAT (red) | Full brightness while heating, dim in HEAT mode while the unit is idle. |
| COOL (blue) | Full brightness while cooling or drying, dim in COOL or DRY mode while idle. |
| HEAT and COOL dim | AUTO mode with the unit idle. |
| All off | Heat pump off, or the LEDs switched off. |

The "Status LEDs" switch in Home Assistant turns all three off, for a unit in a bedroom. A short
press of the BOOT button toggles the same switch on the board. The setting survives a reboot.

**Console.** Logs go to the USB-C port through the module's built-in USB-Serial-JTAG, so nothing
is ever printed on the heat pump's serial line.

**Diagnostics.** Wi-Fi signal, uptime, module temperature, ESPHome version and IP address, plus
Restart and Restart in safe mode buttons.

## First install

You need ESPHome on your computer: the Home Assistant add-on, or the command-line tool.

```bash
python3 -m venv .venv
```

```bash
.venv/bin/pip install esphome
```

1. Copy `secrets.example.yaml` to `secrets.yaml` and fill it in. Generate the API key with the
   command below; it also encrypts firmware updates, so there is no separate OTA password.

   ```bash
   openssl rand -base64 32
   ```

2. If you have more than one board, change `name` and `friendly_name` at the top of
   `esp105.yaml` for each, or copy the file per unit.
3. **Unplug the board from the heat pump.** USB and the heat pump must never be connected at the
   same time unless the computer runs on battery; see the [hardware README](../../hardware/README.md).
4. Plug the board into the computer by USB-C and flash it:

   ```bash
   .venv/bin/esphome run esp105.yaml
   ```

   Choose the USB serial port when asked. The ESP32-C6 enters download mode by itself over USB. If
   the upload cannot connect, hold BOOT, tap RESET, release BOOT and try again.

   If the board last ran the Matter firmware, erase the flash before this step, so none of its
   settings or Thread credentials are left behind:

   ```bash
   .venv/bin/esptool --chip esp32c6 erase-flash
   ```
5. Watch the log until the PWR LED goes solid and the log shows Wi-Fi connected. The heat pump
   handshake will time out on the bench; that is expected without a heat pump.
6. Unplug USB, connect the Grove cable to the heat pump, and Home Assistant should discover the
   device. Adopt it with the API key from `secrets.yaml`.

Later updates go over the network:

```bash
.venv/bin/esphome run esp105.yaml --device esp105.local
```

## Testing without a heat pump

`nanoc6.yaml` builds this firmware for an [M5Stack NanoC6](https://docs.m5stack.com/en/core/M5NanoC6),
a small ESP32-C6 module, and adds a simulated indoor unit. Everything runs on the NanoC6 with
nothing plugged in but USB, so you can try the whole Wi-Fi side and the CN105 code before a board
or a heat pump is at hand.

What differs from the esp105 build:

| | esp105 board | NanoC6 test build |
|---|---|---|
| Flash | 8 MB | 4 MB |
| Heat pump UART | GPIO6 / GPIO7 to the Grove socket | GPIO1 / GPIO2, looped inside the chip to the simulator |
| PWR LED | Green, GPIO23 | Blue, GPIO7 |
| HEAT / COOL LEDs | Red GPIO22, blue GPIO21 | The RGB LED: red for HEAT, blue for COOL, purple for both dim |
| BOOT button | GPIO9 | GPIO9, the button on the NanoC6 |
| Device name | `esp105` | `esp105-nanoc6`, so it never clashes with a real board |

**Never plug the NanoC6's Grove port into a heat pump.** Its pins are 3.3 V with no level
shifting, and the CN105 port carries 5 V signals and 12 V. Leave the Grove port empty: the
simulator uses its two signal pins internally.

The simulator answers the handshake and every request the CN105 component makes, and it applies
the changes it is sent. It also runs a crude room model. While the unit heats or cools, the room
moves 0.5 °C towards the setpoint every 20 seconds, and the unit reports itself as operating. Once
the room reaches the setpoint the unit goes idle, and the room drifts back to 21 °C. It starts
switched off, in COOL, at 22 °C.

To flash it, set up `secrets.yaml` as for the real board, plug in the NanoC6 and run:

```bash
.venv/bin/esphome run nanoc6.yaml
```

Things to check:

1. **Wi-Fi.** The blue LED blinks until Wi-Fi connects, then stays on. Put a wrong password in
   `secrets.yaml` to see the fallback access point and its setup page.
2. **Heat pump link.** The log shows `cn105_sim: Handshake (0x5A)`, then the CN105 component
   reporting the unit's settings.
3. **Home Assistant.** Adopt `esp105-nanoc6` and open the "Heat pump" climate entity. Turn it on
   in HEAT at 24 °C. The log shows the simulator receive the change, then `Now heating`. The RGB
   LED turns full red, and the room temperature climbs 0.5 °C every 20 seconds. At 24 °C the
   unit goes idle and the LED dims.
4. **Other controls.** Try COOL, DRY, AUTO and FAN_ONLY, the fan speeds, swing and the vertical
   vane select. Each change shows up as a `cn105_sim: Set:` line.
5. **Local web page, OTA and buttons.** Open `http://esp105-nanoc6.local`, update over the
   network with `--device esp105-nanoc6.local`, and press the NanoC6's button to turn the LEDs
   off and on.

When you are done, erase the NanoC6 or flash it with something else; it shows up in Home
Assistant as its own device.

## Using a room temperature sensor

The heat pump can regulate on a Home Assistant temperature sensor instead of its own. Add this to
`esp105.yaml`, with your sensor's entity id:

```yaml
sensor:
  - platform: homeassistant
    id: room_temperature
    entity_id: sensor.living_room_temperature
    internal: true

climate:
  - platform: cn105
    # ...existing settings...
    remote_temperature_source:
      sensor_id: room_temperature
```

Merge the `climate` lines into the existing climate block rather than adding a second one. The
existing `remote_temperature_timeout: 30min` makes the unit fall back to its own sensor if Home
Assistant stops sending values.

## Thread

This configuration uses Wi-Fi only. For Thread, use the Matter firmware in
[`../matter/`](../matter/).

## Troubleshooting

- **No response from the heat pump.** Raise the CN105 log level to DEBUG in the `logger` block,
  then read `READ` and `WRITE` lines with `esphome logs esp105.yaml`. Check the Grove cable's black
  wire lands on the socket pin marked 1.
- **Resets when Wi-Fi transmits.** The heat pump's 5 V supply is the usual suspect; see the
  [hardware README](../../hardware/README.md).

## License

Copyright 2026 Jeff Hamilton.

Licensed under the Apache License, Version 2.0 (the "License"); you may not use these files except
in compliance with the License. You may obtain a copy of the License in [LICENSE](../LICENSE) or at
<https://www.apache.org/licenses/LICENSE-2.0>. Unless required by applicable law or agreed to in
writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific
language governing permissions and limitations under the License.

The MitsubishiCN105ESPHome component the firmware pulls in at build time is a separate project
under its own BSD-2-Clause license.
