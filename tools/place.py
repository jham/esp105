#!/usr/bin/env python3
"""Populate cn105.kicad_pcb from the schematic netlist and place parts on a floor plan.

Run with KiCad's bundled Python (it has the pcbnew module):
  /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 tools/place.py

Equivalent to pressing F8 (Update PCB from Schematic) and then dragging every part
into position.  Re-running it moves everything back to the floor plan.
"""
import os, sys, subprocess, xml.etree.ElementTree as ET
import pcbnew

PROJ = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
KC = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
SHARED = "/Applications/KiCad/KiCad.app/Contents/SharedSupport/footprints"
PCB = os.path.join(PROJ, "cn105.kicad_pcb")
SCH = os.path.join(PROJ, "cn105.kicad_sch")
NET = "/tmp/cn105-netlist.xml" if len(sys.argv) < 2 else sys.argv[1]

X0, Y0, BW, BH = 100.0, 100.0, 48.0, 30.0      # must match gen.py
mm = pcbnew.FromMM

# --- floor plan: ref -> (x, y, rotation) in mm relative to the board's top-left corner --------
PLACE = {
    # The module footprint carries Espressif's antenna keep-out: 21.6 mm either side of the
    # module and everything above 5.6 mm below the module origin.  With the module flush with
    # the top edge that means NOTHING else may sit in the top ~6 mm of the board.
    "U1": (21.5, None, 0),          # y computed so the module body sits 0.5 mm inside the top edge
    "J1": (3.5, 15.0, 270),         # CN105 header, pins along the left edge (x/y re-centred below)
    "J2": (None, 15.0, 90),         # USB-C, opening flush with the right edge (x computed)
    # left column (2.6 mm pitch so reference text clears the neighbour): follows the module's
    # left-side pads GND (y 7.5), 3V3 (9.2), EN (13.2) top to bottom
    "C5": (10.5, 8.8, 0), "C4": (10.5, 11.4, 0), "R1": (10.5, 14.0, 0), "C1": (10.5, 16.6, 0),
    "R2": (10.5, 19.2, 0), "R3": (10.5, 21.8, 0),
    # level shifter under the module's bottom-left pads (IO6/IO7)
    "U3": (15.0, 21.2, 0), "R7": (19.5, 20.0, 0), "C6": (19.5, 22.6, 0),
    # power row
    "D2": (8.0, 25.3, 0), "C2": (11.5, 25.3, 0), "U2": (15.5, 25.3, 0), "C3": (19.5, 25.3, 0),
    # USB side
    "R4": (36.0, 9.5, 0), "R5": (36.0, 11.7, 0), "D1": (35.0, 17.0, 0),
    # LED, buttons, debug header along the bottom
    "R6": (31.5, 20.5, 0), "D3": (35.0, 20.5, 0),
    "J3": (24.5, 28.3, 270),
    "SW1": (31.0, 27.0, 0), "SW2": (38.8, 27.0, 0),
    "H1": (3.0, 27.0, 0), "H2": (45.8, 26.0, 0),
}

# --- 1. fresh netlist from the schematic ---------------------------------------------------
subprocess.run([KC, "sch", "export", "netlist", "--format", "kicadxml", "-o", NET, SCH],
               check=True, stderr=subprocess.DEVNULL)
xml = ET.parse(NET).getroot()

# Strip anything a previous run placed (footprints, GND zones, tracks, nets) from the
# file itself, so we always start from the base board gen.py wrote.  Mutating a loaded
# board from Python left dangling zone wrappers, hence the textual approach.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ksym
def is_gnd_zone(n):
    return n[0] == "zone" and any(isinstance(c, list) and c and c[0] == "name" and str(c[1]).startswith("GND_") for c in n)
with open(PCB) as f:
    tree = ksym.parse(f.read())[0]
tree = [n for n in tree if not (isinstance(n, list) and n and (
    n[0] in ("footprint", "segment", "via", "arc") or is_gnd_zone(n) or (n[0] == "net" and str(n[1]) != "0")))]
with open(PCB, "w") as f:
    f.write(ksym.dump(tree) + "\n")
pro_backup = open(os.path.join(PROJ, "cn105.kicad_pro")).read()

board = pcbnew.LoadBoard(PCB)

# --- 2. nets -------------------------------------------------------------------------------
nets = {}
for n in xml.find("nets"):
    name = n.get("name")
    item = pcbnew.NETINFO_ITEM(board, name)
    board.Add(item)
    nets[name] = item
pad_net = {}
for n in xml.find("nets"):
    for node in n.findall("node"):
        pad_net[(node.get("ref"), node.get("pin"))] = n.get("name")

# --- 3. footprints --------------------------------------------------------------------------
def lib_path(nick):
    return os.path.join(PROJ, "cn105.pretty") if nick == "cn105" else os.path.join(SHARED, nick + ".pretty")

placed = {}
for comp in xml.find("components"):
    ref = comp.get("ref")
    value = comp.findtext("value")
    libnick, fpname = comp.findtext("footprint").split(":")
    sheetpath = comp.find("sheetpath").get("tstamps")
    tstamp = comp.findtext("tstamps")
    fp = pcbnew.FootprintLoad(lib_path(libnick), fpname)
    if fp is None:
        sys.exit(f"footprint not found: {libnick}:{fpname}")
    fp.SetFPID(pcbnew.LIB_ID(libnick, fpname))     # keep "Lib:Name" so F8 sees a match
    fp.SetReference(ref)
    fp.SetValue(value)
    fp.SetPath(pcbnew.KIID_PATH(sheetpath.rstrip("/") + "/" + tstamp))
    fp.SetExcludedFromBOM(False)                     # match the symbol (mounting holes default to excluded)
    fp.SetField("Datasheet", comp.findtext("datasheet") or "")
    fp.SetField("Description", "")
    for f in comp.find("fields") or []:
        if f.get("name") in ("LCSC", "MPN"):
            fp.SetField(f.get("name"), f.text or "")
            for fld in fp.GetFields():
                if fld.GetName() == f.get("name"):
                    fld.SetVisible(False)
    for pad in fp.Pads():
        net = pad_net.get((ref, pad.GetNumber()))
        if net:
            pad.SetNet(nets[net])
    board.Add(fp)
    placed[ref] = fp

# --- 4. placement --------------------------------------------------------------------------
def fab_bbox(fp):
    """(left, top, right, bottom) in mm of the F.Fab outline, relative to the footprint origin."""
    xs, ys = [], []
    for g in fp.GraphicalItems():
        if g.GetLayerName() == "F.Fab" and g.GetClass() == "PCB_SHAPE":
            b = g.GetBoundingBox()
            xs += [b.GetLeft(), b.GetRight()]; ys += [b.GetTop(), b.GetBottom()]
    return tuple(pcbnew.ToMM(v) for v in (min(xs), min(ys), max(xs), max(ys)))

def put(ref, x, y, rot):
    fp = placed[ref]
    fp.SetOrientationDegrees(rot)
    fp.SetPosition(pcbnew.VECTOR2I(mm(X0 + x), mm(Y0 + y)))

# module: antenna end (−y in the footprint) overhangs the top edge by 1 mm
u1 = placed["U1"]
u1.SetOrientationDegrees(0)
u1.SetPosition(pcbnew.VECTOR2I(0, 0))
mod_l, mod_top, mod_r, mod_bot = fab_bbox(u1)
assert 12 < mod_r - mod_l < 15 and 15 < mod_bot - mod_top < 18, "unexpected module outline"
u1_y = 0.5 - mod_top          # body flush with the top edge (0.5 mm in), antenna end up
put("U1", PLACE["U1"][0], u1_y, 0)
print(f"U1 body {mod_r-mod_l:.1f} x {mod_bot-mod_top:.1f} mm, centre y = {u1_y:.2f}, body spans y {u1_y+mod_top:.1f}..{u1_y+mod_bot:.1f}")

# USB-C: rotate so the opening (+y in the footprint) points to +x, then push flush to the edge
j2 = placed["J2"]
put("J2", 30.0, PLACE["J2"][1], PLACE["J2"][2])
bb = j2.GetBoundingBox(False, False)
right = pcbnew.ToMM(bb.GetRight()) - X0
pads_x = max(pcbnew.ToMM(p.GetPosition().x) for p in j2.Pads()) - X0
if pads_x > right - 1.0:          # pads ended up at the edge side -> opening is the other way
    put("J2", 30.0, PLACE["J2"][1], 270)
    bb = j2.GetBoundingBox(False, False); right = pcbnew.ToMM(bb.GetRight()) - X0
put("J2", 30.0 + (BW - right), PLACE["J2"][1], j2.GetOrientationDegrees())

for ref, (x, y, rot) in PLACE.items():
    if ref in ("U1", "J2"):
        continue
    put(ref, x, y, rot)

# CN105 header: make sure the back face sits inside the left edge, pins 1..5 bottom to top
j1 = placed["J1"]
cy = j1.GetCourtyard(pcbnew.F_CrtYd).BBox()
j1.Move(pcbnew.VECTOR2I(mm(X0 + 0.6) - cy.GetLeft(), mm(Y0 + PLACE["J1"][1]) - cy.GetCenter().y))
cy = j1.GetCourtyard(pcbnew.F_CrtYd).BBox()
print(f"J1 courtyard x {pcbnew.ToMM(cy.GetLeft())-X0:.1f}..{pcbnew.ToMM(cy.GetRight())-X0:.1f} y {pcbnew.ToMM(cy.GetTop())-Y0:.1f}..{pcbnew.ToMM(cy.GetBottom())-Y0:.1f}; pin1 at y {pcbnew.ToMM(j1.FindPadByNumber('1').GetPosition().y)-Y0:.1f}")
bb = j2.GetBoundingBox(False, False)
print(f"J2 bbox x {pcbnew.ToMM(bb.GetLeft())-X0:.1f}..{pcbnew.ToMM(bb.GetRight())-X0:.1f} y {pcbnew.ToMM(bb.GetTop())-Y0:.1f}..{pcbnew.ToMM(bb.GetBottom())-Y0:.1f} rot {j2.GetOrientationDegrees()}")
# USB-C ground/shell pads: solid zone connection (they are at the board edge and need the copper)
for pad in j2.Pads():
    if pad.GetNetname() == "GND":
        pad.SetLocalZoneConnection(pcbnew.ZONE_CONNECTION_FULL)

# module reference: out of the way, to the right of the body
u1.Reference().SetPosition(pcbnew.VECTOR2I(mm(X0 + 31.0), mm(Y0 + 8.0)))

# hide value text clutter on tiny parts, keep references
for ref, fp in placed.items():
    fp.Value().SetVisible(False)
    if ref.startswith("H"):
        fp.Reference().SetVisible(False)

# --- 5. antenna rule area + GND pours ------------------------------------------------------
# The module footprint's own keep-out ends 5.6 mm above its origin; mirror that in the
# board-level rule area (full width, so the intent is obvious) and keep the GND pours below it.
keep_b = u1_y - 5.6
for z in board.Zones():
    if z.GetZoneName() == "antenna_keepout":
        o = z.Outline(); o.RemoveAllContours(); o.NewOutline()
        for (x, y) in [(-0.1, -0.1), (BW + 0.1, -0.1), (BW + 0.1, keep_b), (-0.1, keep_b)]:
            o.Append(mm(X0 + x), mm(Y0 + y))

gnd = nets["GND"]
pour_top = keep_b + 0.4
for layer, name in ((pcbnew.F_Cu, "GND_top"), (pcbnew.B_Cu, "GND_bottom")):
    z = pcbnew.ZONE(board)
    z.SetLayer(layer)
    z.SetNet(gnd)
    z.SetZoneName(name)
    z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
    z.SetMinThickness(mm(0.25))
    z.SetLocalClearance(mm(0.3))
    z.SetThermalReliefGap(mm(0.5))
    z.SetThermalReliefSpokeWidth(mm(0.5))
    o = z.Outline(); o.NewOutline()
    for (x, y) in [(0, pour_top), (BW, pour_top), (BW, BH), (0, BH)]:
        o.Append(mm(X0 + x), mm(Y0 + y))
    board.Add(z)

pcbnew.ZONE_FILLER(board).Fill(board.Zones())
pcbnew.SaveBoard(PCB, board)
if open(os.path.join(PROJ, "cn105.kicad_pro")).read() != pro_backup:
    open(os.path.join(PROJ, "cn105.kicad_pro"), "w").write(pro_backup)
    print("restored cn105.kicad_pro")
print(f"placed {len(placed)} footprints, {len(nets)} nets; saved {PCB}")
