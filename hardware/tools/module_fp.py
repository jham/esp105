#!/usr/bin/env python3
"""Rebuild cn105.pretty/ESP32-C6-MINI-1.kicad_mod from KiCad's library copy:
drop the silkscreen pin-1 triangle and point the 3D model at Espressif's STEP
(cn105.3dshapes/ESP32-C6-MINI-1.step, whose origin is the module's lower-left corner)."""
import re, os, sys
PROJ = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SRC = "/Applications/KiCad/KiCad.app/Contents/SharedSupport/footprints/RF_Module.pretty/ESP32-C6-MINI-1.kicad_mod"
s = open(SRC).read()
def block_at(s, start):
    depth=0; i=start; instr=False
    while i < len(s):
        c=s[i]
        if instr:
            if c=='\\': i+=1
            elif c=='"': instr=False
        elif c=='"': instr=True
        elif c=='(': depth+=1
        elif c==')':
            depth-=1
            if depth==0: return i+1
        i+=1
    raise ValueError
for m in list(re.finditer(r'\(fp_poly', s))[::-1]:
    e = block_at(s, m.start())
    if '(layer "F.SilkS")' in s[m.start():e]:
        a = s.rfind('\n', 0, m.start()); s = s[:a] + s[e:]
i = s.index('(model '); e = block_at(s, i)
s = s[:i] + ('(model "${KIPRJMOD}/cn105.3dshapes/ESP32-C6-MINI-1.step"\n\t\t(offset\n\t\t\t(xyz -6.65 -5.65 0)\n\t\t)\n'
             '\t\t(scale\n\t\t\t(xyz 1 1 1)\n\t\t)\n\t\t(rotate\n\t\t\t(xyz 0 0 0)\n\t\t)\n\t)') + s[e:]
s = s.replace('(descr "', '(descr "Project copy: no silkscreen pin-1 triangle, Espressif STEP model. Original: ', 1)
out = os.path.join(PROJ, "cn105.pretty", "ESP32-C6-MINI-1.kicad_mod")
open(out, "w").write(s); print("wrote", out)
