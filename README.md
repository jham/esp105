# esp105 — ESP32-C6 adapter for Mitsubishi CN105

A small board that plugs into the CN105 port of a Mitsubishi mini-split, is powered by it, and
talks to it over its serial port with an ESP32-C6 (Wi-Fi or Thread).

![Top of the board](hardware/cn105-board-3d.png)

| Folder | Contents |
|---|---|
| [`hardware/`](hardware/) | KiCad 10 project: schematic, routed 2-layer PCB, project libraries, BOM with verified LCSC numbers, and the guide for working on it. Start with [hardware/README.md](hardware/README.md). |
| [`software/`](software/) | Firmware (ESPHome configuration) — to come. |

Open the board in KiCad via `hardware/cn105.kicad_pro`.

## License

Copyright 2026 Jeff Hamilton. Licensed under the [Solderpad Hardware License v2.1](https://solderpad.org/licenses/SHL-2.1/),
an Apache 2.0 wraparound for hardware designs (SPDX: `Apache-2.0 WITH SHL-2.1`). It covers the
schematic, board, libraries, BOM and firmware alike; at your option you may treat the whole work as
plain Apache 2.0. See [LICENSE](LICENSE).
