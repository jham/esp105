# To do

Open work as of 2026-11-26. Board 0.3.0 (tag `v0.3.0`) is ordered and not yet received.

## 1. Uncommitted work on `main`

These four files are changed but not committed:

- `hardware/cn105.kicad_sch`
- `hardware/cn105.kicad_pro`
- `hardware/BOM.csv`
- `hardware/README.md`

They hold the next board version's schematic changes:

- R13, D4, F1 and J1 (described under "Next board version" in `hardware/README.md`).
- The `USBC_5V` net. It replaces the old VBUS net on the USB-C pins and U4, and it is in the `Power` net class.

ERC passes with 0 errors and 0 warnings. Commit these together with the layout in section 2, after reviewing that layout in KiCad.

## 2. Lay out the new parts (hands-on, in the PCB editor)

1. Open `hardware/cn105.kicad_pcb` and press `F8` (Tools → Update PCB from Schematic) to bring in R13, D4, F1 and J1.
2. Place them:
   - **R13 (100 Ω):** between U3's output (pin 4) and Grove pin 4.
   - **D4 (ESD5V0AP, SOT-23):** right at the Grove socket, on the connector side of R13 and R2. Give it a short ground return shared with D3.
   - **F1 (PPTC, 0805):** near the USB-C connector. It sits between the `USBC_5V` and `5V_USB` nets.
   - **J1 (UART0 header, not fitted):** anywhere convenient. Label TX, RX and GND on the silkscreen.
3. Route them. `USBC_5V` tracks get 0.5 mm automatically from the `Power` net class.
4. Run DRC with the schematic parity check. Until the parts are placed, parity reports them as missing.
5. Review the layout, then commit and push.

## 3. Before the next order

- **Version number:** this is a schematic and copper change, so it is a MINOR bump (0.4.0 unless you choose otherwise). Use the `/order` skill (`.claude/skills/order/SKILL.md`).
- **Rotations in the placement file:** JLCPCB's engineers had to correct 10 parts on the 0.3.0 order: Q1, U3, U4, PWR1, COOL1, HEAT1, C5, R6, BOOT1 and RESET1.
  - JLCPCB's zero rotation is 180° off from KiCad's for these packages:
    - SOT-23 (Q1)
    - SOT-23-5 (U3)
    - SOT-23-6 (U4)
    - KENTO 0805 LEDs (`LED0805-R-RD`)
  - The NationStar red LED (`LED0805-RD`) numbers its pads the same way KiCad does.
  - `hardware/tools/export_jlcpcb.py` applies no rotation corrections yet. Add them, check each part against EasyEDA's pad numbering, and include the new D4 (also SOT-23) and F1.
  - EasyEDA footprint data: `https://easyeda.com/api/products/<LCSC>/components?version=6.5.42`. It needs a browser User-Agent. Pads are the `PAD~` lines in `dataStr.shape`, in units of 10 mil.
- **Extended parts:** D4 and F1 are extended parts at JLCPCB, so each adds a loading fee. Check stock with `hardware/tools/jlc_stock.py`.
- **J1:** it is marked DNP, so it stays out of the BOM and placement file. Check that it does.

## 4. When the 0.3.0 boards arrive

- Work through the bring-up checklist in `hardware/README.md`. Step 4 checks the Serin cable's orientation: cavity 2 (black) must beep to GND, and cavity 3 (red) must beep to Q1 pin 3. Do this before connecting to the heat pump.
- **Grove pin 1 orientation is unconfirmed.** It follows the Serin page's numbering, while M5Stack's NanoC6 schematic numbers the socket the other way round. If step 4 shows it mirrored:
  - the fix is a re-pinned cable, not a new board;
  - correct the footprint before the next order.
- Record what you find:
  - Doc-only notes that apply to the boards as built go on branch `release/v0.3.0` (from tag `v0.3.0`) and on `main`. Commit on `main`, then cherry-pick to the release branch. The branch can't be called `v0.3.0`, because that clashes with the tag.
  - Design fixes go on `main` only.
- Flash `software/esp105.yaml` and check the firmware on real hardware:
  - UART1 on GPIO6/7 at 2400 8E1
  - the PWR, HEAT and COOL LEDs
  - BOOT short-press toggling the LEDs
  - OTA
  - the web UI
- Check whether Wi-Fi transmit peaks reset the board when it runs from the heat pump's 5 V. C7 is there to prevent that.

## 5. Later / optional

- Thread firmware is still an open question; the hardware supports both Wi-Fi and Thread.
- Conformal-coat the assembled board (mask the two connectors) before installing it next to the coil.
- Stale "JST PA" wording was left in deliberately:
  - `hardware/fp-lib-table`'s library description
  - the old generator scripts `hardware/tools/fp.py`, `gen.py` and `models.py`

  The generators are history only. Never run them: they overwrite the hand-edited design.

## Working rules (so they survive the move)

- Commit and push only when asked. Any PCB layout change gets a manual review in KiCad first.
- You do the placement and routing; Claude edits only the schematic, BOM and docs.
- Don't edit a KiCad file while its editor has it open. Check for lock files first: `find hardware -maxdepth 1 -name '*.lck'`.
- Board versions are semver (tags `v<version>`), never "rev A/B".
- Never write JLCPCB order numbers anywhere in this public repo.
- Every power net name includes its voltage (`USBC_5V`, `5V_USB`, `5V_CN105`, `+5V`, `+3V3`), never a bare `VBUS`.
- `software/secrets.yaml` is git-ignored and is never committed. The template is `secrets.example.yaml`.
- Licenses: CERN-OHL-P-2.0 for hardware, Apache-2.0 for software.

## Relocation notes

- On 2026-11-26 the repo moved from `~/Desktop/cn105` to `~/src/mitsubishi/esp105`. Sibling repos for related hardware go next to it.
- Claude's memory for this project is stored per folder path. It now lives at the parent workspace's entry, `~/.claude/projects/-Users-jham-src-mitsubishi/memory/`, so start sessions in `~/src/mitsubishi` to pick it up.
- `hardware/production/` (the order files) is git-ignored, so it won't come with a fresh clone. Regenerate it from a tag with `hardware/tools/export_jlcpcb.sh`.
- Your PCB edits discarded on 2026-11-25 are backed up only in a temporary folder that will not survive: `/private/tmp/claude-502/-Users-jham-Desktop-cn105/8aeec4d0-c675-404b-9398-c04ec4bf28e0/scratchpad/pcb-backup-20261125-143736/` (the board file and a diff). Copy it somewhere if you want it.
