# Matter-over-Thread firmware

An alternative to the ESPHome Wi-Fi firmware in [`../wifi/`](../wifi/), for board 0.3.0. It makes the heat
pump a Matter **Room Air Conditioner** on a Thread network, so it works with Apple Home, Google
Home, Home Assistant or any other Matter controller that has a Thread border router. It uses no
Wi-Fi.

Flash one firmware or the other. The board is the same, and nothing on it has to change.

**Status: builds, not yet tried on a board or a heat pump.** The heat pump protocol code has host
tests. The Matter side, commissioning and the heat pump link still need checking on real
hardware; see "Not done yet" below.

## What it does

**Heat pump.** UART1 at 2400 baud, 8E1, TX on GPIO6 and RX on GPIO7, the same as the ESPHome
build. The CN105 code in `components/cn105/` is written for this project. It reads the unit's
settings, room temperature and operating status every 2 seconds and sends changes as soon as a
controller makes them.

**In Matter**, one endpoint carries:

| Matter | Heat pump |
|---|---|
| On/Off | Power. |
| Thermostat system mode | Heat, Cool, Dry, Fan only or Auto. Off when the unit is off. |
| Heating setpoint | The setpoint in HEAT. |
| Cooling setpoint | The setpoint in COOL and DRY. |
| Heat/cool range in Auto | The unit has one setpoint, so it gets the middle of the range. A change made on the unit's own remote moves the range and keeps its width. |
| Local temperature | The unit's room temperature. |
| Outdoor temperature | The outside temperature, on units that report it. |
| Running state | Heating, cooling or fan, from the unit's operating flag. In Auto it is a guess from the room temperature, because the unit does not say which way it is working. |
| Fan control | Auto plus 5 speeds: 1 is QUIET, 2 to 5 are the unit's speeds 1 to 4. Fan mode Off turns the unit off. |
| Fan rocking (up/down) | Vertical vane swing. Turning rocking off puts the vane back on AUTO. |

Setpoints are limited to the unit's 16 to 31 °C. Units that report half degrees get half
degrees; the others are rounded to whole degrees.

**LEDs.**

| LED | Meaning |
|---|---|
| PWR (green) | Solid once commissioned and on the Thread network. Fast blink while open for commissioning. Slow blink when not commissioned or not attached. |
| HEAT (red) | Full brightness while heating, dim in HEAT mode while idle. |
| COOL (blue) | Full brightness while cooling or drying, dim in COOL or DRY mode while idle. |
| HEAT and COOL dim | AUTO mode with the unit idle. |
| All blinking together | A controller asked the board to identify itself. |

**BOOT button.** A short press turns all LEDs off, or back on; the setting survives a reboot. Hold
it for 10 seconds and all three LEDs light up: letting go then factory-resets the board, which
removes it from every Matter controller.

**Console.** Logs and the Matter shell go to the USB-C port, as in the ESPHome build.

## Building

You need ESP-IDF 6.0.2. The Matter SDK (esp-matter 1.6.1) is downloaded by the build itself into
`managed_components/` and pinned by `dependencies.lock`. These steps install ESP-IDF in `~/esp`:

```bash
mkdir -p ~/esp
```

```bash
git clone --depth 1 --branch v6.0.2 --recursive --shallow-submodules https://github.com/espressif/esp-idf.git ~/esp/esp-idf-v6.0.2
```

```bash
~/esp/esp-idf-v6.0.2/install.sh esp32c6
```

ESP-IDF's installer wants Python 3.10 to 3.13. If your default `python3` is newer, put a
3.13 first in your `PATH` for the install step.

Then, in each new terminal, from this folder:

```bash
. ~/esp/esp-idf-v6.0.2/export.sh
```

```bash
idf.py build
```

The first build compiles the whole Matter SDK and takes several minutes. `idf.py set-target` is
not needed; `sdkconfig.defaults` selects the ESP32-C6 and 8 MB of flash. If you change
`sdkconfig.defaults`, delete `sdkconfig` so the build picks up the change.

## Flashing

1. **Unplug the board from the heat pump.** USB and the heat pump must never be connected at the
   same time unless the computer runs on battery; see the hardware README.
2. Plug the board into the computer by USB-C, then flash and watch the log:

   ```bash
   idf.py flash monitor
   ```

   The ESP32-C6 enters download mode by itself over USB. If it cannot connect, hold BOOT, tap
   RESET, release BOOT and try again. Leave the monitor with `Ctrl-]`.

The first time, or when switching from the ESPHome firmware, erase the flash first so no
old settings are left behind:

```bash
idf.py erase-flash flash monitor
```

## Commissioning

You need a Thread border router on your network: a HomePod mini or Apple TV 4K, a Google Nest
Hub (2nd gen) or Nest Wifi Pro, or Home Assistant with a Thread radio.

1. Power the board. Over USB is fine for this step.
2. Add it in your Matter app as a new accessory. The board uses Matter's test setup code:

   ```text
   Manual code: 34970112332
   QR payload:  MT:Y.K9042C00KA0648G00
   ```

   The log prints the code and a link to the QR code at boot, under `SetupQRCode`.
3. Your app will warn that the accessory is not certified. That is expected: the firmware uses
   Matter's test vendor ID and test certificates. Choose to add it anyway.
4. Once it is on the network, the PWR LED goes solid. Then move the board to the heat pump.

Every board built from this code has the same setup code. Commission one at a time.

If commissioning fails partway, hold BOOT for 10 seconds, release it, and start again.

## Tests

The CN105 packet code builds and runs on the computer, with no board:

```bash
test/run_tests.sh
```

## Files

| File | What it is |
|---|---|
| `main/app_main.cpp` | Start-up: Matter node, Thread radio, LEDs, button, heat pump link. |
| `main/heat_pump_device.cpp` | The Matter endpoint and the mapping in the table above. |
| `main/status_leds.cpp`, `main/boot_button.cpp` | The LEDs and the BOOT button. |
| `main/board.h` | Pin map, matching `../wifi/esp105.yaml`. |
| `components/cn105/` | The CN105 protocol (`cn105_protocol.*`, no hardware access) and the UART task (`cn105.*`). |
| `test/` | Host tests for the protocol code. |
| `sdkconfig.defaults`, `partitions.csv` | Build settings and the 8 MB flash layout with two firmware slots. |

## Not done yet

- **Nothing has run on a board yet.** Check, in this order: boot and LEDs, BLE commissioning,
  Thread attach, then the heat pump link with a real unit.
- **The CN105 byte layout** comes from the community's reverse engineering. The handshake and
  request packets match other implementations, but the decoders have only been tested against
  packets built by the tests. Compare the `cn105` log against the ESPHome build on the same unit.
- **Over-the-air updates.** The OTA requestor is enabled, but no update has been tried. It needs a
  Matter OTA provider on the network.
- **Production identity.** Vendor and product names read `TEST_VENDOR` and `TEST_PRODUCT`, and the
  setup code is shared by every board. Unique setup codes need a factory partition made with
  Espressif's `esp-matter-mfg-tool`.
- **Room temperature from another sensor**, the vane positions other than swing, the horizontal
  vane, and the extra sensors the ESPHome build has (compressor frequency, power, energy) are not
  exposed in Matter yet.
