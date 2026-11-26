# Software

Two firmwares for the esp105 board, version 0.3.0 (the Grove-socket board). Flash one or the
other; the board is the same, and nothing on it has to change.

| Firmware | Network | Works with | Status |
|---|---|---|---|
| [`wifi/`](wifi/) | Wi-Fi, ESPHome | Home Assistant, plus a local web page | In use. See [wifi/README.md](wifi/README.md). |
| [`matter/`](matter/) | Thread, Matter | Apple Home, Google Home, Home Assistant, any Matter controller with a Thread border router | Builds, not yet tried on hardware. See [matter/README.md](matter/README.md). |

Both use the same pins: the heat pump on UART1 (TX GPIO6, RX GPIO7, 2400 baud 8E1), the PWR, HEAT
and COOL LEDs on GPIO23, GPIO22 and GPIO21, and the BOOT button on GPIO9. Both log to the USB-C
port. When switching from one firmware to the other, erase the flash first so no old settings are
left behind; each README says how.

**Unplug the board from the heat pump before connecting USB**, whichever firmware you flash. USB
and the heat pump must never be connected at the same time unless the computer runs on battery;
see the [hardware README](../hardware/README.md).

## License

Copyright 2026 Jeff Hamilton. Both firmwares are licensed under the Apache License, Version 2.0;
see [LICENSE](LICENSE). The MitsubishiCN105ESPHome component the Wi-Fi firmware pulls in at build
time is a separate project under its own BSD-2-Clause license.
