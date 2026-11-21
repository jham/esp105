#!/usr/bin/env python3
"""Show the Value (not the Reference) on the silkscreen for selected parts.

Run with KiCad's bundled Python:
  /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 tools/silk_values.py [REF ...]

Default targets: U1 SW1 SW2 CN105 USB-C1.  Touches only those footprints' text
fields; tracks, zones and placement are left alone.  Also syncs each target's
Value text from the schematic (what F8 would do).
"""
import os, sys, subprocess, xml.etree.ElementTree as ET
import pcbnew

PROJ = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
KC = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
PCB = os.path.join(PROJ, "cn105.kicad_pcb")
SCH = os.path.join(PROJ, "cn105.kicad_sch")
TARGETS = sys.argv[1:] or ["U1", "SW1", "SW2", "CN105", "USB-C1"]
mm, tomm = pcbnew.FromMM, pcbnew.ToMM

# values from the schematic
net = "/tmp/cn105-netlist-silk.xml"
subprocess.run([KC, "sch", "export", "netlist", "--format", "kicadxml", "-o", net, SCH], check=True, stderr=subprocess.DEVNULL)
sch_value = {c.get("ref"): c.findtext("value") for c in ET.parse(net).getroot().find("components")}

board = pcbnew.LoadBoard(PCB)
edges = board.GetBoardEdgesBoundingBox()
fps = {str(fp.GetReference()): fp for fp in board.GetFootprints()}

def fab_box(fp):
    """body proxy: the courtyard box (iterating GraphicalItems segfaults in KiCad 10's wrapper)"""
    bb = fp.GetCourtyard(pcbnew.F_CrtYd).BBox()
    if bb.GetWidth() == 0:
        bb = fp.GetBoundingBox(False, False)
    return bb

pad_boxes = [p.GetBoundingBox() for fp in board.GetFootprints() for p in fp.Pads()]
body_boxes = {r: fab_box(fp) for r, fp in fps.items()}

def clear(box, me):
    """text box inside the board, not on any pad, not on another part's body"""
    inner = pcbnew.BOX2I(edges.GetPosition(), edges.GetSize()); inner.Inflate(-mm(0.6))
    if not inner.Contains(box):
        return False
    if any(box.Intersects(pb) for pb in pad_boxes):
        return False
    return not any(box.Intersects(bb) for r, bb in body_boxes.items() if r != me)

for ref in TARGETS:
    fp = fps.get(ref)
    if fp is None:
        print(f"{ref}: not on the board, skipped"); continue
    if ref in sch_value and sch_value[ref] != str(fp.GetValue()):
        print(f"{ref}: value {fp.GetValue()!r} -> {sch_value[ref]!r} (from schematic)")
        fp.SetValue(sch_value[ref])
    r, v = fp.Reference(), fp.Value()
    v.SetLayer(pcbnew.F_SilkS)
    v.SetVisible(True)
    v.SetTextSize(pcbnew.VECTOR2I(mm(1.0), mm(1.0)))
    v.SetTextThickness(mm(0.15))
    v.SetMirrored(False)
    v.SetTextAngle(r.GetTextAngle())
    v.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_CENTER)
    v.SetPosition(r.GetPosition())
    r.SetVisible(False)
    # if the text lands on the part's own body or on pads, try spots around the body
    body = body_boxes[ref]
    if not clear(v.GetBoundingBox(), ref) or v.GetBoundingBox().Intersects(body):
        w = v.GetBoundingBox().GetWidth(); h = v.GetBoundingBox().GetHeight()
        cx, cy = body.GetCenter().x, body.GetCenter().y
        cands = [("below", cx, body.GetBottom() + mm(0.6) + h // 2), ("above", cx, body.GetTop() - mm(0.6) - h // 2),
                 ("left", body.GetLeft() - mm(0.6) - w // 2, cy), ("right", body.GetRight() + mm(0.6) + w // 2, cy)]
        placed = False
        for name, x, y in cands:
            v.SetTextAngle(pcbnew.EDA_ANGLE(0, pcbnew.DEGREES_T))
            v.SetPosition(pcbnew.VECTOR2I(int(x), int(y)))
            if clear(v.GetBoundingBox(), ref) and not v.GetBoundingBox().Intersects(body):
                print(f"{ref}: text placed {name} the body"); placed = True; break
        if not placed:
            v.SetPosition(pcbnew.VECTOR2I(int(cx), int(body.GetBottom() + mm(0.6) + h // 2)))
            print(f"{ref}: no clear spot found, left below the body -- move it by hand")
    else:
        print(f"{ref}: text at the old reference position")
    bb = v.GetBoundingBox()
    print(f"   '{fp.GetValue()}' at ({tomm(bb.GetCenter().x)-100:.1f}, {tomm(bb.GetCenter().y)-100:.1f}) mm from the board corner")

pcbnew.SaveBoard(PCB, board)
print("saved", PCB)
