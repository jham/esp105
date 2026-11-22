#!/bin/sh
# Produce everything JLCPCB needs in hardware/production/ (git-ignored):
#   cn105-gerbers.zip        Gerbers + Excellon drills (upload as the PCB file)
#   cn105-bom-jlcpcb.csv     assembly BOM
#   cn105-cpl-jlcpcb.csv     pick-and-place positions
set -e
cd "$(dirname "$0")/.."
KC=/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli
PY=/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3
rm -rf production && mkdir -p production/gerbers
$KC pcb export gerbers --layers "F.Cu,B.Cu,F.Paste,F.SilkS,B.SilkS,F.Mask,B.Mask,Edge.Cuts" \
    --no-x2 --no-netlist --subtract-soldermask --use-drill-file-origin -o production/gerbers/ cn105.kicad_pcb
$KC pcb export drill --format excellon --excellon-units mm --excellon-zeros-format decimal --excellon-separate-th \
    --drill-origin plot --generate-map --map-format gerberx2 -o production/gerbers/ cn105.kicad_pcb
(cd production && zip -q -r cn105-gerbers.zip gerbers)
$PY tools/export_jlcpcb.py
ls -l production
