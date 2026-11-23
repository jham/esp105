"""Write JLCPCB BOM and placement (CPL) CSVs from the board.

Run with KiCad's bundled Python, from the hardware/ folder:
  /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 tools/export_jlcpcb.py

Every footprint with an LCSC field is included; mounting holes and DNP parts are skipped.
Comment = the LCSC code itself: JLC's matcher searches the Comment text and flags names that
resemble other catalogue entries (TS-1187A-B-A-B vs TS-1187A-BAB) as ambiguous even when the
LCSC column is right, and the code is the one string that matches exactly one part. Coordinates are relative to the board's aux origin
(unset = page origin, which is what the Gerbers use), Y negated as in KiCad's own position files.
"""
import collections, csv, os, re, sys
import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
HW = os.path.dirname(HERE)
OUT = os.path.join(HW, "production")
os.makedirs(OUT, exist_ok=True)

b = pcbnew.LoadBoard(os.path.join(HW, "cn105.kicad_pcb"))
T = pcbnew.ToMM
ao = b.GetDesignSettings().GetAuxOrigin()


def field(fp, name):
    return str(fp.GetFieldText(name)) if fp.HasField(name) else ""


def key(ref):
    m = re.match(r"([A-Za-z-]+)(\d*)", ref)
    return (m.group(1), int(m.group(2) or 0))


rows, skipped = [], []
for fp in b.GetFootprints():
    ref, lcsc = str(fp.GetReference()), field(fp, "LCSC")
    attrs = fp.GetAttributes()
    if not lcsc or attrs & pcbnew.FP_DNP or attrs & pcbnew.FP_EXCLUDE_FROM_BOM or attrs & pcbnew.FP_EXCLUDE_FROM_POS_FILES:
        skipped.append(ref)
        continue
    p = fp.GetPosition()
    if attrs & pcbnew.FP_THROUGH_HOLE:
        # KiCad puts a THT connector's origin on pin 1; JLC wants the centre of the pin pattern,
        # otherwise its preview draws the body shifted by half the connector.
        pads = [pd for pd in fp.Pads() if str(pd.GetNumber())]
        bb = pads[0].GetBoundingBox()
        for pd in pads[1:]:
            bb.Merge(pd.GetBoundingBox())
        p = bb.GetCenter()
    rows.append(dict(ref=ref, comment=lcsc, foot=str(fp.GetFPID().GetLibItemName()),
                     lcsc=lcsc, x=T(p.x - ao.x), y=-T(p.y - ao.y), rot=fp.GetOrientationDegrees(),
                     side="Top" if fp.GetLayer() == pcbnew.F_Cu else "Bottom"))
rows.sort(key=lambda r: key(r["ref"]))

with open(os.path.join(OUT, "cn105-cpl-jlcpcb.csv"), "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
    for r in rows:
        w.writerow([r["ref"], "%.4fmm" % r["x"], "%.4fmm" % r["y"], r["side"], "%.1f" % r["rot"]])

groups = collections.OrderedDict()   # one BOM line per (footprint, LCSC number)
for r in rows:
    groups.setdefault((r["foot"], r["lcsc"]), (r["comment"], []))[1].append(r["ref"])
with open(os.path.join(OUT, "cn105-bom-jlcpcb.csv"), "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["Comment", "Designator", "Footprint", "LCSC Part #"])
    for (foot, lcsc), (c, refs) in groups.items():
        w.writerow([c, ",".join(refs), foot, lcsc])

print("placed %d parts (%s), %d BOM lines, skipped: %s" % (len(rows), dict(collections.Counter(r["side"] for r in rows)), len(groups), ", ".join(skipped)))
