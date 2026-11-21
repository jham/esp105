"""Generate the JST PA 5-pin header footprints into cn105.pretty.
Top entry  B05B-PASK(-1): housing 12 x 5.3, pin row 3.1 from the front face, 2.2 from the back; bosses 1.7 behind the pins.
Side entry S05B-PASK-2 : housing 12 x 8.2, opening at the front; pins 1.0 from the back; bosses 2.1 in front of the pins.
Dimensions from the JST ePA-F drawing. +y in the footprint is the side that faces the board edge on this board.
"""
import uuid, os, sys
OUT = sys.argv[1]
N, P = 5, 2.0
A = (N - 1) * P
X0, X1 = -2.0, A + 2.0
U = lambda: str(uuid.uuid4())

def make(name, Y0, Y1, boss_y, descr, model):
    L = []
    def line(a, b, layer, w):
        L.append(f'  (fp_line (start {a[0]:g} {a[1]:g}) (end {b[0]:g} {b[1]:g}) (stroke (width {w}) (type solid)) (layer "{layer}") (uuid "{U()}"))')
    def rect(x0, y0, x1, y1, layer, w):
        for a, b in (((x0,y0),(x1,y0)), ((x1,y0),(x1,y1)), ((x1,y1),(x0,y1)), ((x0,y1),(x0,y0))):
            line(a, b, layer, w)
    line((X0+1, Y0), (X1, Y0), "F.Fab", 0.1); line((X1, Y0), (X1, Y1), "F.Fab", 0.1)
    line((X1, Y1), (X0, Y1), "F.Fab", 0.1); line((X0, Y1), (X0, Y0+1), "F.Fab", 0.1); line((X0, Y0+1), (X0+1, Y0), "F.Fab", 0.1)
    s = 0.11
    rect(X0-s, Y0-s, X1+s, Y1+s, "F.SilkS", 0.12)
    # pin-1 arrow off the END of the housing, pointing at pin 1 along the row
    line((X0-1.3, -0.6), (X0-0.7, 0.0), "F.SilkS", 0.12); line((X0-0.7, 0.0), (X0-1.3, 0.6), "F.SilkS", 0.12); line((X0-1.3, 0.6), (X0-1.3, -0.6), "F.SilkS", 0.12)
    c = 0.5
    rect(X0-c, Y0-c, X1+c, Y1+c, "F.CrtYd", 0.05)
    pads = []
    for i in range(N):
        shape = "roundrect" if i == 0 else "circle"
        extra = " (roundrect_rratio 0.25)" if i == 0 else ""
        pads.append(f'  (pad "{i+1}" thru_hole {shape} (at {i*P:g} 0) (size 1.3 1.3) (drill 0.75) (layers "*.Cu" "*.Mask") (remove_unused_layers no){extra} (uuid "{U()}"))')
    for bx in (-1.5, A + 1.5):
        pads.append(f'  (pad "" np_thru_hole circle (at {bx:g} {boss_y:g}) (size 1.1 1.1) (drill 1.1) (layers "*.Cu" "*.Mask") (uuid "{U()}"))')
    fp = f'''(footprint "{name}"
  (version 20241229)
  (generator "pcbnew")
  (generator_version "9.0")
  (layer "F.Cu")
  (descr "{descr}")
  (tags "connector JST PA CN105")
  (property "Reference" "REF**" (at {A/2:g} {Y0-1.1:g} 0) (layer "F.SilkS") (uuid "{U()}") (effects (font (size 1 1) (thickness 0.15))))
  (property "Value" "{name}" (at {A/2:g} {Y1+1.1:g} 0) (layer "F.Fab") (uuid "{U()}") (effects (font (size 1 1) (thickness 0.15))))
  (property "Datasheet" "https://www.jst-mfg.com/product/pdf/eng/ePA-F.pdf" (at 0 0 0) (layer "F.Fab") (hide yes) (uuid "{U()}") (effects (font (size 1.27 1.27) (thickness 0.15))))
  (property "Description" "" (at 0 0 0) (layer "F.Fab") (hide yes) (uuid "{U()}") (effects (font (size 1.27 1.27) (thickness 0.15))))
  (attr through_hole)
{chr(10).join(L)}
  (fp_text user "${{REFERENCE}}" (at {A/2:g} {(Y0+Y1)/2:g} 0) (layer "F.Fab") (uuid "{U()}") (effects (font (size 0.8 0.8) (thickness 0.12))))
  (fp_text user "1" (at -1.0 -1.0 0) (layer "F.SilkS") (uuid "{U()}") (effects (font (size 0.8 0.8) (thickness 0.12))))
{chr(10).join(pads)}
  (model "${{KIPRJMOD}}/cn105.3dshapes/{model}.wrl" (offset (xyz 0 0 0)) (scale (xyz 1 1 1)) (rotate (xyz 0 0 0)))
  (embedded_fonts no)
)
'''
    os.makedirs(os.path.join(OUT, "cn105.pretty"), exist_ok=True)
    with open(os.path.join(OUT, "cn105.pretty", name + ".kicad_mod"), "w") as f:
        f.write(fp)
    print("wrote footprint", name)

make("JST_PA_B05B-PASK_1x05_P2.00mm_Vertical", -3.1, 2.2, 1.7,
     "JST PA top-entry through-hole header B05B-PASK / B05B-PASK-1 (with boss), 2.0 mm pitch, 5 circuits. Mates with PAP-05V-S (Mitsubishi CN105 cable). Holes 0.7 mm, boss holes 1.1 mm.",
     "JST_PA_B05B-PASK_1x05_P2.00mm_Vertical")
make("JST_PA_S05B-PASK-2_1x05_P2.00mm_Horizontal", -1.0, 7.2, 2.1,
     "JST PA side-entry (right-angle) through-hole header S05B-PASK-2, 2.0 mm pitch, 5 circuits; cable enters from the +y side. Mates with PAP-05V-S (Mitsubishi CN105 cable). Holes 0.7 mm, boss holes 1.1 mm.",
     "JST_PA_S05B-PASK-2_1x05_P2.00mm_Horizontal")
