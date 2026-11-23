# Order log

One entry per board order, newest first. Board versions are semver; the git tag is `v<version>`
(tags `v0.1` and `v0.2` predate the three-part numbering and mean 0.1.0 and 0.2.0). The order files
for any entry can be regenerated from its commit with `sh tools/export_jlcpcb.sh`.

## 0.2.0 — 2026-11-23

- Tag `v0.2`, commit 1176abf. Ordered from JLCPCB on 2026-11-23.
- Fab: 2 layers, 1.6 mm, 41.8 × 37.6 mm. Assembly: Economic, top side, 24 BOM lines / 33 parts.
- Changes since 0.1.0: module changed to ESP32-C6-WROOM-1-N8 (Economic-eligible, 8 MB) and the
  board re-laid out around it (antenna overhanging the top edge, both connectors on the bottom
  edge, four M2.5 holes). U4 → SRV05-4, D3 → SMAJ5.0A. COOL LED now blue (KT-0805B); LED resistors
  rebalanced (R7 1k, R8/R9 330 Ω). JLC BOM Comment column is the LCSC code.
- Extended parts: U1, U2, U3, CN105, USB-C1, COOL1; D3 and U4 preferred (no fee).
- JLCPCB order number, quantity and finish: not recorded.

## 0.1.0 — 2026-11-21

- Tag `v0.1`, commit 138649b. Ordered from JLCPCB on 2026-11-21.
- Fab: 2 layers, 1.6 mm, 48 × 30 mm. Assembly: Standard (every ESP32-C6-MINI-1 variant is
  Standard-only at JLCPCB), top side, 24 BOM lines / 33 parts.
- Design: ESP32-C6-MINI-1-H8, side-entry JST PA CN105 header, HRO USB-C, AP2112K 3.3 V regulator,
  SN74AHCT1G125 TX level shifter, AO3401A power switch, three status LEDs.
- JLCPCB order number, quantity and finish: not recorded.
