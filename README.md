# esp105 — ESP32-C6 adapter for Mitsubishi CN105

A small board that plugs into the CN105 port of a Mitsubishi mini-split, is powered by it, and
talks to it over its serial port with an ESP32-C6 (Wi-Fi or Thread).

![Top of the board](hardware/cn105-board-3d.png)

| Folder | Contents |
|---|---|
| [`hardware/`](hardware/) | KiCad 10 project: schematic, routed 2-layer PCB, project libraries, BOM with verified LCSC numbers, and the guide for working on it. Start with [hardware/README.md](hardware/README.md). |
| [`software/`](software/) | ESPHome firmware for the board: Home Assistant climate entity over Wi-Fi, status LEDs, build and flashing steps. Start with [software/README.md](software/README.md). |

Open the board in KiCad via `hardware/cn105.kicad_pro`.

## License

The hardware design in [`hardware/`](hardware/) is licensed under the CERN Open Hardware Licence
Version 2 - Permissive; see [hardware/LICENSE](hardware/LICENSE). The firmware in
[`software/`](software/) is licensed under the Apache License 2.0; see
[software/LICENSE](software/LICENSE).
