# CN105 ↔ ESP32-C6 adapter — hardware

A small board that plugs into the CN105 port of a Mitsubishi mini-split, is powered by it, and talks
to it over its 5 V serial port using an ESP32-C6 module (Wi-Fi or Thread, chosen in firmware).

**Design state:** schematic and board complete at version 0.4.0, not yet ordered (see the title
block, the `ESP105 v…` silkscreen, and [ORDER_LOG.md](ORDER_LOG.md) for what has been built; the
latest boards made are 0.3.0). What changed since 0.3.0 is under
[Changes from 0.3.0 to 0.4.0](#changes-from-030-to-040). ERC 0, DRC 0 errors, schematic parity 0. Board 41.8 × 37.6 mm, 2 layers, ESP32-C6-WROOM-1-N8 with its antenna
overhanging the top edge, Grove socket for the CN105 cable and USB-C both on the bottom edge, four M2.5 holes. The
LCSC numbers in the BOM were checked against JLCPCB's parts database. Board versions are semver;
the git tag is `v<version>` (the early tags `v0.1` and `v0.2` mean 0.1.0 and 0.2.0). A version is
tagged when it is set, whether or not it has been ordered.

![Top of the board](cn105-board-3d.png)

## What is in this folder

| File | What it is |
|---|---|
| `cn105.kicad_pro` | The project. Double-click this to open KiCad. Contains JLCPCB-friendly design rules and two net classes (Default 0.25 mm tracks, Power 0.5 mm). |
| `cn105.kicad_sch` | The schematic. Complete, ERC 0. Its title block carries the board version and date. |
| `cn105.kicad_pcb` | The board: 41.8 × 37.6 mm, 2 layers, fully routed, GND pours on both layers. DRC 0 errors. Its title block rev/date feed the `ESP105 v${REVISION}` and `${ISSUE_DATE}` silkscreen texts on the back. |
| `cn105.kicad_sym`, `sym-lib-table` | Project symbol library: the ESP32-C6-WROOM-1 symbol (KiCad 10 does not ship one), derived from the MINI-1 symbol with the WROOM pin numbers. |
| `cn105.pretty/`, `fp-lib-table` | Project footprint library: the Grove HY2.0-4P right-angle SMD socket (drawn from the Megastar ZX-HY2.0-4PWT drawing), Espressif's official ESP32-C6-WROOM-1 footprint, and project copies of the HRO USB-C and TS-1187A switch footprints with working 3D models. The JST PA header footprints (CN105 header through 0.2.0) and the ESP32-C6-MINI-1 footprint from 0.1.0 are kept for reference. Don't delete `fp-lib-table`. |
| `cn105.3dshapes/` | 3D models not shipped with KiCad: Espressif's `ESP32-C6-WROOM-1.step` (and the 0.1.0 `ESP32-C6-MINI-1.step`) and plain-box placeholders for the Grove socket and the JST PA headers (from `tools/models.py`). The USB-C uses KiCad's GCT USB4105 model and the switches KiCad's Alps SKRK model, both near-identical parts. |
| `BOM.csv` | Human-readable parts list with LCSC numbers and the reason for each part. The JLCPCB-format BOM is generated, see below. |
| `cn105-board-3d.png` | Render of the board, regenerated with `kicad-cli pcb render` after board changes. |
| `ORDER_LOG.md` | One entry per board order: version, date, tag, fab and assembly options, what changed. |
| `tools/` | `export_jlcpcb.sh` (+ `export_jlcpcb.py`) writes the JLCPCB order files into `production/`; `set_version.py` records a board version and date in both title blocks; `jlc_stock.py` checks a JLCPCB BOM for stock and Standard-only parts; `silk_values.py` switches footprints to print their Value on the silkscreen. `gen.py`, `ksym.py`, `place.py`, `fp.py`, `models.py`, `module_fp.py` generated the original 0.1.0 starter files and **overwrite everything**; see the last section. |

## How the circuit works (read this with the schematic open)

**CN105 interface.** The board carries a 4-pin Grove (HY2.0) socket, reference CN105, and connects
to the heat pump's CN105 port with a CN105-to-Grove cable wired as on
[serin-labs.com/wiring.html](https://serin-labs.com/wiring.html): JST PAP-05V-S on the heat pump
end, Grove HY2.0-4P on ours, four conductors, the 12 V position left empty.

| Grove pin | Wire | CN105 pin | Signal | Used for |
|---|---|---|---|---|
| 1 | black | 2 | GND | Ground |
| 2 | red | 3 | 5 V | Powers the whole board |
| 3 | white | 4 | TX from the heat pump (5 V logic) | Goes through R2/R3 (10k/15k), which divides 5 V to 3.0 V, then into ESP IO7 (UART RX) |
| 4 | yellow | 5 | RX into the heat pump | Driven by U3, a 74AHCT1G125 buffer powered from 5 V, through R13 (100 Ω), so the heat pump gets a real 5 V signal. U3's input is ESP IO6 (UART TX). R6 holds that line high while the ESP is still booting so the heat pump never sees garbage. |

D4, a dual TVS diode at the socket, clamps ESD on pins 3 and 4. The cable is wired straight
through, pin for pin; the UART crossover (heat pump TX into ESP RX
and back) is built into the numbering. Grove pin 1 is at the end marked "1" on the silkscreen,
the left end when you look into the socket opening from the board edge with the latch window up.
CN105 pin 1 (12 V) stays unconnected in the cable. Verify the CN105 pinout on **your** unit with a
meter before plugging anything in.

**ESP32-C6 module.** U1 (ESP32-C6-WROOM-1-N8, 8 MB flash) with its decoupling C4 (22 µF) and C5
(100 nF) at the 3V3 pin, the EN reset circuit (R1, C1, button RESET), the BOOT button on IO9, and
three 0805 status LEDs: green PWR (R7 1k, IO23, net LED_PWR), red HEAT (R8 330 Ω, IO22, net
LED_HEAT) and blue COOL (R9 330 Ω, IO21, net LED_COOL). The resistor values give the three LEDs
similar brightness at 3.3 V (green about 50 mcd, red and blue 25–30 mcd); green and red are JLCPCB
basic parts, the blue is extended because JLCPCB stocks no basic blue 0805. UART1 is on IO6/IO7 for
the heat pump. UART0 goes to J1, a 3-pin header on the back that is not fitted (TX, RX, GND, for a
3.3 V USB-serial adapter); the console is the module's USB-Serial-JTAG over the USB-C port.
Of the strapping pins, IO9 is the BOOT button and both IO9 and IO8 carry 10k pull-ups (R12, R11) so
boot mode never depends on the weak internal pull-ups; the rest are left unconnected. The LEDs,
buttons, USB-C and module print their Value (PWR, HEAT, COOL, RESET, BOOT, USBC, ESP32-C6) on the
silkscreen instead of their reference designator.

**USB-C.** Wired for USB 2.0 only: D+/D- go straight to the C6's native USB pins (IO13/IO12, module
pads 14/13), so no USB-UART chip is needed. The receptacle's two D+ pads and two D- pads are
bridged on the board, as the USB-C spec requires. R4/R5 (5.1k) on CC1/CC2 tell a USB-C charger or
laptop we want 5 V. U4 (SRV05-4, a 4-channel TVS array with pins 1+6 and 3+4 paired on the board)
clamps ESD on D+, D- and VBUS, since the port lives in a damp metal box and gets plugged into by
hand. The VBUS pins are net `5V_USB_IN`; F1, a resettable fuse, feeds `5V_USB` from it.

**Power.** Two 5 V sources feed the `+5V` rail. USB comes in through F1 and then D1, a Schottky. The heat
pump's 5 V comes in through Q1, a P-channel MOSFET: with no USB present R10 holds its gate low and
it conducts with about 30 mV of drop; plugging in USB pulls the gate high through the `5V_USB`
line and switches it off, so USB can never back-feed the heat pump and the heat pump never drives
the USB port. Q1's body diode still lets the heat pump help out if USB sags. D1 is what keeps the
USB line separate from the rail so the gate reads "USB present". D3 (SMAJ5.0A TVS) sits across the
CN105 5 V input, C7 (100 µF) is the bulk reservoir for Wi-Fi transmit peaks, and U2 (AP2112K-3.3,
600 mA) makes 3.3 V with C2/C3 (10 µF) on its input and output. The PWR_FLAG symbols are not real
parts: they tell KiCad's checker that those nets are powered.

## Working on it in KiCad 10

**Opening.** Double-click `cn105.kicad_pro`. The project carries its own symbol and footprint
library tables, so no library setup is needed. Open the Schematic Editor or PCB Editor from the
project manager; `Home` fits the sheet or board in the window. The board sits at x=100, y=100 mm
on the sheet, so if the PCB editor looks empty press `Home`.

**Schematic.** Hover a wire and press `` ` `` (backtick) to highlight its net; `E` opens any item's
properties; `V` edits a value. **Inspect → Electrical Rules Checker → Run ERC** should report 0.
Parts carry `LCSC` and `MPN` fields; the JLCPCB export reads `LCSC`, and `MPN` is for people.

**From schematic to board.** After any schematic change press `F8` in the PCB editor (Update PCB
from Schematic) to push new parts, values and fields onto the board, then place and route what is
new. The board's `Value` field is what prints on the silkscreen for the LEDs, buttons, connectors
and module, so keep those values short.

**Board basics.** Layers tab on the right: `F.Cu`/`B.Cu` copper, `F.SilkS`/`B.SilkS` printing,
`Edge.Cuts` outline, `F.CrtYd` courtyards (the no-overlap zone around each part). White ratsnest
lines are connections not yet routed. `M` moves, `R` rotates, `E` edits, `X` routes a track, `V`
drops a via while routing, `B` refills the ground pours, `Esc` cancels. Hold `Ctrl` while moving to
ignore the grid. Don't press `F` on a part: that flips it to the back and single-sided assembly is
cheaper. `Ctrl+Shift+M` measures; `Space` sets a local origin so the status bar shows distances
from it.

**Rules already set** (File → Board Setup): 0.2 mm clearance, 0.15 mm minimum track, 0.5 mm vias
with 0.3 mm holes, matching JLCPCB's cheap 2-layer process. Net class `Power` covers `+5V`,
`+3V3`, `GND`, `5V_USB`, `5V_USB_IN` and `5V_CN105` and makes those tracks 0.5 mm
automatically; everything else is 0.25 mm.

**Antenna keep-out.** The 6 mm antenna section of the WROOM-1 (the end away from the pins) hangs
over the top edge. No copper, tracks, vias or parts under it on either layer, and Espressif asks
for no metal, including the enclosure, within about 15 mm of that end.

**Placement rules that matter.** C5 and C6 (100 nF) stay tight against the module 3V3 pin and
U3's VCC pin; C2/C3 at U2; C1, C4 and C7 are relaxed. D3 goes next to the Grove socket with a
short ground return. D1 and R10 can sit anywhere on their nets. The USB D+/D- pair runs side by
side; a via or two and a few millimetres of length mismatch are fine at USB full speed. Both
connectors are placed so their openings are flush with the bottom board edge: the Grove footprint
origin is the centre of its pad pattern (JLCPCB's reference point for the part, 3.85 mm in front
of the pin row) and the housing front is 5.45 mm from it (anchor pads stop at 5.15 mm so they keep
the 0.3 mm copper-to-edge clearance with a flush face), the USB-C origin is JLCPCB's reference point for the part (1.57 mm behind the shell centre) and
the shell front is 5.22 mm from it.

**Footprint origins must match JLCPCB's.** JLC places a part at the origin of *its* footprint
(EasyEDA's), which is the centre of the pad pattern for connectors. The CPL reports our footprint
origin, so a project footprint with its origin elsewhere is placed wrong by that offset; the Grove
socket and the USB-C both had this before 2026-11-23. Check a new footprint against
`https://easyeda.com/api/products/<LCSC>/components?version=6.5.42` (pad coordinates relative to
the origin, units of 10 mil) before trusting the placement preview.

**Checks.** `Inspect → Design Rules Checker` with schematic parity on. Expect 0 errors, 0
unconnected, and four `silk_edge_clearance` warnings where the connector and antenna outlines cross
the board edge. A "thermal relief connection incomplete" error means a GND pad is boxed in by
tracks: draw a short GND track from the pad to the nearest GND via. From the shell:

```bash
/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli sch erc --severity-all cn105.kicad_sch
```

```bash
/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli pcb drc --severity-all --schematic-parity --refill-zones cn105.kicad_pcb
```

**Version and date.** Both title blocks hold the board version (semver) and the date it was
set; `python3 tools/set_version.py 0.4.0` sets them with the schematic and PCB editors closed, and
the back silkscreen prints them through `${REVISION}` and `${ISSUE_DATE}`. Setting a version and
ordering boards are separate steps, each with a Claude Code skill in `.claude/skills/`:

- `/bump-version` checks whether the design files changed since the last version tag, proposes
  the next version (PATCH for part or export fixes with the schematic and copper unchanged, MINOR
  for schematic or copper changes, MAJOR on request), sets the title blocks, adds a "Changes from
  … to …" section below, runs the checks, commits and tags `v<version>`.
- `/order` builds a tagged version. If the design changed since the last tag it runs
  `/bump-version` first; if not, it orders the last tagged version again without a new number.
  It then exports the order files, waits for you to place the order, logs it in `ORDER_LOG.md`
  and pushes.

## Production files (JLCPCB)

```bash
sh tools/export_jlcpcb.sh
```

Needs KiCad in /Applications. It writes `production/` (git-ignored): `cn105-gerbers.zip` (Gerbers
for F.Cu, B.Cu, both masks, both silkscreens, F.Paste, Edge.Cuts, plus Excellon PTH/NPTH drills),
`cn105-bom-jlcpcb.csv` and `cn105-cpl-jlcpcb.csv` in JLC's column format, built from the `LCSC`
field on every part. The BOM's Comment column is the LCSC code, so JLC's matcher cannot confuse a
part with a similarly named one. Mounting holes are skipped. Through-hole parts are reported at the
centre of their pin pattern, which is what JLC's preview expects (the 0.2.0 CN105 header needed
this; the Grove socket is SMD).

```bash
python3 tools/jlc_stock.py production/cn105-bom-jlcpcb.csv
```

checks every line against JLCPCB's database and flags missing or out-of-stock parts, parts that
force Standard assembly, and which extended parts carry a loading fee.

## Firmware

Two firmwares live in [`../software/`](../software/): ESPHome over Wi-Fi in `software/wifi/` and
Matter over Thread in `software/matter/`, each with build and flashing steps. The pins both rely
on: UART TX GPIO6 and RX GPIO7, LEDs PWR GPIO23 / HEAT GPIO22 / COOL GPIO21 (high is
on), BOOT on GPIO9, console on the USB-C port.

## Bring-up checklist

1. Power from USB only. Measure +3V3 on C3: expect 3.25–3.35 V. The module should enumerate as a
   USB serial/JTAG device on the Mac (`ls /dev/cu.usb*`).
2. Flash the firmware from `software/`. The PWR LED blinks until Wi-Fi joins, then stays on.
3. Measure Grove pin 4 relative to pin 1: ~5 V (USB-powered it'll read ~4.6 V). That's UART idle.
4. Check the cable's orientation. Unplug USB, plug the Serin cable into the board's Grove socket
   with its CN105 end loose, and use the meter's continuity mode, poking a probe (or a bit of wire)
   into the cavities of the CN105 plug. Cavity 1 (the ▲ end) is empty.
   - Cavity 2 (black) beeps to the USB-C shell, which is GND.
   - Cavity 3 (red) beeps to Q1 pin 3, the lone pin on one side of the SOT-23 (the `5V_CN105`
     net), and does **not** beep to the USB-C shell.

   If black does not reach GND, the socket's pin 1 is mirrored relative to the cable; do not
   connect it to the heat pump (see the Grove note under "Things I assumed").
5. Only now plug into the heat pump. Confirm pin 3 is 5 V and pin 2 is ground **with the cable
   unplugged from the board** first, meter on the heat pump side.
6. **Do not have USB and the heat pump connected at the same time** unless the USB host is
   battery powered or you use a USB isolator. USB ties the heat pump's control ground to the
   PC's earth through this board, and the two are not guaranteed to agree. Flash on the bench,
   then install; use OTA afterwards.

## Changes from 0.3.0 to 0.4.0

0.4.0 is laid out and passes ERC, DRC and schematic parity, but it has not been ordered. The 0.3.0
boards (tag `v0.3.0`) have none of these changes, and none of them is needed for 0.3.0 to work.
The board outline, mounting holes, connectors, module position and design rules are unchanged.

### Circuit

- **R13, 100 Ω in series with U3's output** (0603, UNI-ROYAL 0603WAF1000T5E, LCSC C22775, JLCPCB
  basic). U3 pin 4 is now net `CN105_RX_BUF`, and R13 joins it to `CN105_RX_5V` at Grove pin 4.
  The 74AHCT1G125 is only rated for about 2 kV human-body ESD; R13 limits current into it from a
  static discharge, a short or a miswired cable, and costs nothing at 2400 baud.
- **D4, ESD5V0AP dual TVS on Grove pins 3 and 4** (SOT-23, ElecSuper ESD5V0AP, LCSC C5199224,
  extended). Pins 1 and 2 go to `CN105_TX_5V` and `CN105_RX_5V`, pin 3 (the common anode) to GND.
  5 V working voltage, 6 V breakdown, ±30 kV contact; the 3.3 V diodes the M5Stack NanoC6 uses
  would conduct on our 5 V signals.
- **J1, UART0 header, not fitted** (`PinHeader_1x03_P2.54mm_Vertical`, marked DNP so JLCPCB leaves
  it empty): pin 1 TXD0 (module pad 25, GPIO16, net `UART0_TX`), pin 2 RXD0 (pad 24, GPIO17,
  `UART0_RX`), pin 3 GND. With a 3.3 V USB-serial adapter it gives the ROM boot messages, and
  serial flashing with BOOT held, if firmware breaks USB-Serial-JTAG or the USB-C connector is
  damaged. It does not get around the ground warning in step 6 of the bring-up checklist, since a
  USB-serial adapter ties the grounds together just as USB-C does.
- **F1, resettable fuse on USB VBUS** (0805, BHFUSE BSMD0805-075-6V, LCSC C883110, extended; 6 V,
  0.75 A hold, 1.5 A trip). The USB-C VBUS pins and U4's VBUS clamp are now net `5V_USB_IN`
  (renamed from `USBC_5V`, and in the `Power` net class), and F1 feeds `5V_USB` from it. It
  protects the computer's USB port from a fault on this board, such as a solder bridge or a failed
  TVS or capacitor; most USB hosts already limit current, so it is the least important of the
  four. Its resistance (70–385 mΩ) adds up to about 0.1 V of drop at Wi-Fi peaks on top of D1,
  which still leaves the AP2112K enough headroom.

D4 and F1 are extended parts, each adding JLCPCB's extended-part loading fee to an order. The
firmware needs no change: no ESP pin moved.

### Layout

- **New parts.** D4 sits about 3 mm from Grove pins 3 and 4, on the connector side of R2 and R13,
  with a ground via about 1 mm from its ground pin. R13 is next to U3. F1 is on the bottom edge
  between the USB-C and Grove sockets. J1 is on the back at the right edge, with TX, RX and GND
  printed on the back silkscreen.
- **Moved to make room.** U3 and C6 moved 2.5 mm right, and R2 and R3 moved next to D4. C7 (100 µF)
  moved from beside U3 to beside U2 and Q1, where it holds up the regulator input during Wi-Fi
  peaks. Q1, D1 and R10 shifted to fit around it. C2, C3, D3, R4, R5, R6, R11 and U2 moved by about a
  millimetre or less.
- **Routing.** USB power now runs from the VBUS pins on the bottom layer to F1, and U4's VBUS
  clamp is on that side of the fuse. A bottom-layer track carries `+5V` across to U3 and C6. UART0
  runs to J1 with two vias per signal. The board went from 179 track segments and 41 vias to 216
  and 43; all power nets are still 0.5 mm.
- **Silkscreen.** Every front label is now 0.8 mm high with a 0.1 mm stroke (most were 1.0 mm with
  0.15 mm), except RESET and BOOT, which are 1.2 mm and bold.

## Things I assumed that you should double-check

- CN105 pin order (12V, GND, 5V, TX, RX) and which end is pin 1 on your unit's socket.
- The Grove socket footprint was drawn from the Megastar ZX-HY2.0-4PWT drawing. Pin 1 was taken
  to be the left end with the opening toward you and the latch window up, the JST PH convention
  the HY2.0 family copies and the numbering the Serin page uses (pin 1 = black GND). M5Stack's
  NanoC6 schematic numbers the same socket the other way (GND = pin 4), and Megastar's drawing
  numbers no pins at all, so this is unconfirmed until bring-up step 4 passes. If it is mirrored,
  the heat pump's GND lands on U3's output, its 5 V on the R2/R3 divider, its TX on the
  `5V_CN105` input and its RX on GND: the board gets no power and nothing works. The fix is a
  cable with the Grove end re-pinned in reverse order, not a new board.
- U3 must be an **AHCT** part. AHC or LVC in the same package will not translate 3.3 V → 5 V reliably.
- The heat pump's 5 V pin can supply a few hundred mA on most units. C7 (100 µF on +5V near Q1 and U2)
  is there to ride through Wi-Fi transmit peaks; if you still see resets during Wi-Fi activity,
  the supply is the suspect.
- The CN105 5 V input has a TVS (D3, SMAJ5.0A) that clamps at about 9 V. It protects against a
  mis-wired cable; it is not a substitute for checking the pinout, since a sustained 12 V would
  eventually cook it.
- It lives next to a cold coil: conformal-coat the assembled board (mask the two connectors),
  mount it above the drip path, and point the USB-C opening down or cap it.
- Thread firmware is still the open question; the hardware supports both.

## The generator scripts

`tools/gen.py`, `ksym.py`, `fp.py`, `models.py`, `module_fp.py` and `place.py` produced the
original 0.1.0 starter project (ESP32-C6-MINI-1, 48 × 30 mm, generic J1/J2 names) and are kept for
history. The current schematic and board were edited by hand from there. **Do not run them:**
`gen.py` rewrites the schematic, project file and board, and `place.py` throws away all
footprints, tracks and pours. Nothing they produce matches the current design.

## License

Copyright 2026 Jeff Hamilton.

This source describes Open Hardware and is licensed under the
[CERN Open Hardware Licence Version 2 - Permissive](https://ohwr.org/cern_ohl_p_v2.txt)
(SPDX: `CERN-OHL-P-2.0`). You may redistribute and modify this documentation and make products
using it under the terms of that licence. It is distributed WITHOUT ANY EXPRESS OR IMPLIED
WARRANTY, INCLUDING OF MERCHANTABILITY, SATISFACTORY QUALITY AND FITNESS FOR A PARTICULAR PURPOSE.
See [LICENSE](LICENSE) for the full text and conditions.
