#!/usr/bin/env python3
"""Generate the cn105 KiCad project: schematic, pcb, project file, fp-lib-table.

Symbols are read from KiCad's own libraries so pin geometry is exact.
All connections are made with short wire stubs + labels / power symbols,
which keeps the generator simple and ERC-clean.
"""
import copy, uuid, json, sys, os, datetime
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ksym
from ksym import Sym, kids, kid, dump

OUT = sys.argv[1] if len(sys.argv) > 1 else "/Users/jham/Desktop/cn105"
PROJECT = "cn105"
TODAY = datetime.date.today().isoformat()

def U():
    return str(uuid.uuid4())

ROOT_UUID = U()
F = lambda v: f"{v:.2f}".rstrip("0").rstrip(".") if abs(v) > 1e-9 else "0"

# --------------------------------------------------------------------------
# Component placement.  (ref, lib, symbol, x, y, rot, value, footprint, extra fields)
# Positions are sheet coordinates in mm; KiCad +y is DOWN.
# --------------------------------------------------------------------------
PARTS = [
    # --- CN105 interface -------------------------------------------------
    ("J1", "Connector_Generic", "Conn_01x05", 50.8, 76.2, 0, "CN105",
     "cn105:JST_PA_B05B-PASK_1x05_P2.00mm_Vertical", {"LCSC": "C160282 (B05B-PASK-1, with boss) - verify", "MPN": "JST B05B-PASK(LF)(SN)"}),
    ("R2", "Device", "R", 71.12, 71.12, 0, "10k", "Resistor_SMD:R_0603_1608Metric", {"LCSC": "C25804"}),
    ("R3", "Device", "R", 71.12, 86.36, 0, "15k", "Resistor_SMD:R_0603_1608Metric", {"LCSC": "C22809"}),
    ("U3", "74xGxx", "74AHCT1G125", 99.06, 76.2, 0, "74AHCT1G125", "Package_TO_SOT_SMD:SOT-23-5",
     {"LCSC": "C7859 (SN74AHCT1G125DBVR) - verify", "MPN": "SN74AHCT1G125DBVR"}),
    ("C6", "Device", "C", 132.08, 71.12, 0, "100nF", "Capacitor_SMD:C_0603_1608Metric", {"LCSC": "C14663"}),
    ("R7", "Device", "R", 78.74, 68.58, 0, "10k", "Resistor_SMD:R_0603_1608Metric", {"LCSC": "C25804"}),
    # --- ESP32-C6 module ------------------------------------------------
    ("U1", "RF_Module", "ESP32-C6-MINI-1", 203.2, 101.6, 0, "ESP32-C6-MINI-1-N4", "RF_Module:ESP32-C6-MINI-1",
     {"LCSC": "C5174297 - verify", "MPN": "ESP32-C6-MINI-1-N4"}),
    ("C4", "Device", "C", 177.8, 66.04, 0, "22uF", "Capacitor_SMD:C_0805_2012Metric", {"LCSC": "C45783"}),
    ("C5", "Device", "C", 213.36, 66.04, 0, "100nF", "Capacitor_SMD:C_0603_1608Metric", {"LCSC": "C14663"}),
    ("R1", "Device", "R", 142.24, 71.12, 0, "10k", "Resistor_SMD:R_0603_1608Metric", {"LCSC": "C25804"}),
    ("C1", "Device", "C", 142.24, 88.9, 0, "1uF", "Capacitor_SMD:C_0603_1608Metric", {"LCSC": "C15849"}),
    ("SW1", "Switch", "SW_Push", 157.48, 80.01, 0, "RESET", "Button_Switch_SMD:SW_Push_1P1T_XKB_TS-1187A", {"LCSC": "C318884"}),
    ("SW2", "Switch", "SW_Push", 157.48, 104.14, 0, "BOOT", "Button_Switch_SMD:SW_Push_1P1T_XKB_TS-1187A", {"LCSC": "C318884"}),
    ("R6", "Device", "R", 142.24, 121.92, 0, "1k", "Resistor_SMD:R_0603_1608Metric", {"LCSC": "C21190"}),
    ("D3", "Device", "LED", 152.4, 134.62, 180, "LED", "LED_SMD:LED_0603_1608Metric", {"LCSC": "C2286 (blue) - any 0603 LED"}),
    # --- USB-C ---------------------------------------------------------------
    ("J2", "Connector", "USB_C_Receptacle_USB2.0_16P", 50.8, 139.7, 0, "USB-C",
     "Connector_USB:USB_C_Receptacle_HRO_TYPE-C-31-M-12", {"LCSC": "C165948", "MPN": "HRO TYPE-C-31-M-12"}),
    ("R4", "Device", "R", 83.82, 134.62, 0, "5.1k", "Resistor_SMD:R_0603_1608Metric", {"LCSC": "C23186"}),
    ("R5", "Device", "R", 91.44, 134.62, 0, "5.1k", "Resistor_SMD:R_0603_1608Metric", {"LCSC": "C23186"}),
    ("D1", "Diode", "1N5819WS", 88.9, 116.84, 0, "1N5819WS", "Diode_SMD:D_SOD-323", {"LCSC": "C191023"}),
    # --- Power ---------------------------------------------------------------
    ("D2", "Diode", "1N5819WS", 119.38, 116.84, 0, "1N5819WS", "Diode_SMD:D_SOD-323", {"LCSC": "C191023"}),
    ("U2", "Regulator_Linear", "AP2112K-3.3", 147.32, 134.62, 0, "AP2112K-3.3", "Package_TO_SOT_SMD:SOT-23-5",
     {"LCSC": "C51118", "MPN": "AP2112K-3.3TRG1"}),
    ("C2", "Device", "C", 124.46, 134.62, 0, "10uF", "Capacitor_SMD:C_0603_1608Metric", {"LCSC": "C19702"}),
    ("C3", "Device", "C", 177.8, 134.62, 0, "10uF", "Capacitor_SMD:C_0603_1608Metric", {"LCSC": "C19702"}),
    # --- Debug / mechanical ---------------------------------------------------
    ("J3", "Connector_Generic", "Conn_01x03", 238.76, 139.7, 0, "DEBUG_UART0",
     "Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical", {}),
    ("H1", "Mechanical", "MountingHole", 254.0, 139.7, 0, "M2.5", "MountingHole:MountingHole_2.7mm_M2.5", {}),
    ("H2", "Mechanical", "MountingHole", 264.16, 139.7, 0, "M2.5", "MountingHole:MountingHole_2.7mm_M2.5", {}),
    # --- Power flags (PWR_FLAG tells ERC a passive-fed net really is powered) ----
    ("#FLG01", "power", "PWR_FLAG", 99.06, 134.62, 180, "PWR_FLAG", "", {}),   # +5V
    ("#FLG02", "power", "PWR_FLAG", 99.06, 147.32, 0, "PWR_FLAG", "", {}),      # GND
    ("#FLG03", "power", "PWR_FLAG", 109.22, 116.84, 180, "PWR_FLAG", "", {}),    # VBUS
    ("#FLG04", "power", "PWR_FLAG", 128.27, 111.76, 0, "PWR_FLAG", "", {}),     # 5V_CN105
]

# --------------------------------------------------------------------------
# Connections: (ref, pin number) -> spec
#   "NC"                      no-connect flag on the pin
#   ("L", name)               2.54 mm stub + local label
#   ("P", net, h, v)          power symbol; h = horizontal run (signed, 0 = none),
#                             v = vertical run (signed, +down).  Omit for the
#                             default (straight stub in the pin's direction).
#   ("W", [(x,y),...])        explicit polyline from the pin end
# Pins not listed on multi-pin parts get an NC flag automatically, except
# pins whose electrical type is no_connect (left bare, like KiCad expects).
# --------------------------------------------------------------------------
CONN = {
    # J1 CN105: 1=12V 2=GND 3=5V 4=TX(heat pump -> us) 5=RX(us -> heat pump)
    ("J1", "1"): "NC",
    ("J1", "2"): ("P", "GND", -5.08, -5.08),
    ("J1", "3"): ("L", "5V_CN105"),
    ("J1", "4"): ("L", "HP_TX_5V"),
    ("J1", "5"): ("L", "HP_RX_5V"),
    # divider R2 (top) / R3 (bottom); series junction has the ESP_RX label
    ("R2", "1"): ("L", "HP_TX_5V"),
    ("R2", "2"): ("W", [(71.12, 78.74)]),
    ("R3", "1"): ("W", [(71.12, 78.74)]),
    ("R3", "2"): ("P", "GND"),
    # buffer
    ("U3", "1"): ("P", "GND"),          # ~OE tied low = always enabled
    ("U3", "2"): ("W", [(78.74, 76.2)]),
    ("R7", "1"): ("P", "+3V3"),
    ("R7", "2"): ("W", [(78.74, 76.2)]),
    ("U3", "3"): ("P", "GND"),
    ("U3", "4"): ("L", "HP_RX_5V"),
    ("U3", "5"): ("P", "+5V"),
    ("C6", "1"): ("P", "+5V"),
    ("C6", "2"): ("P", "GND"),
    # module
    ("U1", "3"): ("P", "+3V3"),
    ("U1", "1"): ("P", "GND"),
    ("U1", "8"): ("L", "EN"),
    ("U1", "15"): ("L", "ESP_TX"),      # IO6 = UART1 TX (not a strapping pin)
    ("U1", "16"): ("L", "ESP_RX"),      # IO7 = UART1 RX
    ("U1", "23"): ("L", "IO9_BOOT"),
    ("U1", "17"): ("L", "USB_D-"),      # IO12
    ("U1", "18"): ("L", "USB_D+"),      # IO13
    ("U1", "24"): ("L", "LED"),         # IO18
    ("U1", "31"): ("L", "TXD0"),
    ("U1", "30"): ("L", "RXD0"),
    ("C4", "1"): ("P", "+3V3"), ("C4", "2"): ("P", "GND"),
    ("C5", "1"): ("P", "+3V3"), ("C5", "2"): ("P", "GND"),
    # EN pull-up / RC / reset button
    ("R1", "1"): ("P", "+3V3"),
    ("R1", "2"): ("W", [(142.24, 80.01)]),
    ("C1", "1"): ("W", [(142.24, 80.01)]),
    ("C1", "2"): ("P", "GND"),
    ("SW1", "1"): ("W", [(147.32, 80.01), (142.24, 80.01)]),
    ("SW1", "2"): ("P", "GND", 5.08, 5.08),
    # boot button
    ("SW2", "1"): ("L", "IO9_BOOT"),
    ("SW2", "2"): ("P", "GND", 5.08, 5.08),
    # LED
    ("R6", "1"): ("L", "LED"),
    ("R6", "2"): ("W", [(142.24, 134.62), (148.59, 134.62)]),   # ends on D3 anode
    ("D3", "2"): ("W", []),                                    # anode: wired above
    ("D3", "1"): ("P", "GND", 5.08, 2.54),                     # cathode
    # USB-C
    ("J2", "A4"): ("P", "VBUS", 5.08, -2.54),
    ("J2", "A5"): ("L", "CC1"),
    ("J2", "B5"): ("L", "CC2"),
    ("J2", "B7"): ("L", "USB_D-"),
    ("J2", "A7"): ("W", [(68.58, 137.16), (68.58, 139.7)]),
    ("J2", "A6"): ("L", "USB_D+"),
    ("J2", "B6"): ("W", [(68.58, 144.78), (68.58, 142.24)]),
    ("J2", "A8"): "NC", ("J2", "B8"): "NC",
    ("J2", "A1"): ("P", "GND"),
    ("J2", "SH"): ("P", "GND"),
    ("R4", "1"): ("L", "CC1"), ("R4", "2"): ("P", "GND"),
    ("R5", "1"): ("L", "CC2"), ("R5", "2"): ("P", "GND"),
    ("D1", "1"): ("P", "+5V", -5.08, -2.54),    # K -> +5V rail
    ("D1", "2"): ("P", "VBUS", 5.08, -2.54),    # A <- USB
    # power
    ("D2", "1"): ("P", "+5V", -5.08, -2.54),    # K -> +5V rail
    ("D2", "2"): ("W", [(130.81, 116.84)]),     # A <- CN105 5V (label below)
    ("U2", "1"): ("P", "+5V", -5.08, -2.54),    # VIN
    ("U2", "3"): ("W", [(137.16, 134.62), (137.16, 132.08)]),  # EN tied to VIN stub
    ("U2", "2"): ("P", "GND"),
    ("U2", "5"): ("P", "+3V3", 5.08, -2.54),    # VOUT
    ("C2", "1"): ("P", "+5V"), ("C2", "2"): ("P", "GND"),
    ("C3", "1"): ("P", "+3V3"), ("C3", "2"): ("P", "GND"),
    # debug header
    ("J3", "1"): ("L", "TXD0"),
    ("J3", "2"): ("L", "RXD0"),
    ("J3", "3"): ("P", "GND", -5.08, 2.54),
    # power flags
    ("#FLG01", "1"): ("P", "+5V"),
    ("#FLG02", "1"): ("P", "GND"),
    ("#FLG03", "1"): ("P", "VBUS"),
    ("#FLG04", "1"): ("W", [(128.27, 116.84)]),
}

# Free-standing items
EXTRA_WIRES = [
    ("cn105", (71.12, 78.74), (73.66, 78.74)),        # divider mid -> label stub
]
EXTRA_LABELS = [
    ("cn105", "ESP_RX", 73.66, 78.74, 0),
    ("cn105", "ESP_TX", 78.74, 76.2, 180),
    ("mcu", "EN", 147.32, 80.01, 90),
    ("pwr", "5V_CN105", 130.81, 116.84, 0),
]
EXTRA_JUNCTIONS = [
    ("cn105", 71.12, 78.74), ("mcu", 142.24, 80.01), ("usb", 68.58, 139.7), ("usb", 68.58, 142.24),
    ("pwr", 137.16, 132.08), ("pwr", 128.27, 116.84),
]
NOTES = [
    ("cn105", 38.1, 45.72, 2.0, "CN105 INTERFACE"),
    ("cn105", 38.1, 50.8, 1.27, "CN105 (JST PA, 2.0 mm): 1 = 12 V (unused)   2 = GND   3 = 5 V   4 = TX from heat pump (5 V)   5 = RX into heat pump\n"
                               "Verify this pinout on YOUR unit with a meter before plugging anything in."),
    ("cn105", 38.1, 101.6, 1.27, "R2/R3 divide 5 V down to 3.0 V\nfor the ESP32-C6 RX pin (max 3.6 V)."),
    ("cn105", 88.9, 101.6, 1.27, "U3 is powered from 5 V: 3.3 V in -> 5 V out\nso the heat pump sees clean 5 V logic.\n~OE tied low = always enabled.\nR7 keeps TX idle-high while the ESP boots."),
    ("mcu", 127.0, 45.72, 2.0, "ESP32-C6 MODULE"),
    ("mcu", 127.0, 50.8, 1.27, "UART1 on IO6/IO7 talks to the heat pump. UART0 (TXD0/RXD0) stays free for a debug console on J3.\n"
                              "IO4, IO5, IO8, IO9, IO15 are boot-strapping pins: only IO9 is used, as the BOOT button. IO12/IO13 are the native USB pins."),
    ("usb", 38.1, 96.52, 2.0, "USB-C (programming + bench power)"),
    ("usb", 38.1, 101.6, 1.27, "5.1k on CC1 and CC2 tells a USB-C source we are a 5 V device.\nD1 and D2 OR the two 5 V sources so USB and the heat pump never back-feed each other."),
    ("pwr", 99.06, 160.02, 2.0, "3.3 V REGULATOR"),
    ("pwr", 99.06, 165.1, 1.27, "AP2112K-3.3: 600 mA, 0.25 V dropout. Wi-Fi TX bursts are the worst case.\nPWR_FLAGs tell ERC that +5V, VBUS, 5V_CN105 and GND are really powered."),
    ("dbg", 226.06, 152.4, 1.27, "J3: UART0 console (3.3 V!)\nH1/H2: M2.5 mounting holes"),
]

POWER_NETS = {"GND": "down", "+3V3": "up", "+5V": "up", "VBUS": "up"}

# Section offsets: everything above was laid out tightly; these spread the
# sections across the A3 sheet.
OFF = {"cn105": (0, 0), "mcu": (120.65, 0), "usb": (0, 25.4), "pwr": (63.5, 38.1), "dbg": (91.44, 76.2)}
GROUP_OF = {
    "J1": "cn105", "R7": "cn105", "R2": "cn105", "R3": "cn105", "U3": "cn105", "C6": "cn105",
    "U1": "mcu", "C4": "mcu", "C5": "mcu", "R1": "mcu", "C1": "mcu", "SW1": "mcu", "SW2": "mcu", "R6": "mcu", "D3": "mcu",
    "J2": "usb", "R4": "usb", "R5": "usb", "D1": "usb", "#FLG03": "usb",
    "D2": "pwr", "U2": "pwr", "C2": "pwr", "C3": "pwr", "#FLG01": "pwr", "#FLG02": "pwr", "#FLG04": "pwr",
    "J3": "dbg", "H1": "dbg", "H2": "dbg",
}
TEXT_OVERRIDE = {
    "U1": {"Reference": (-2.54, -24.13, "right"), "Value": (2.54, 27.94, "left")},
    "U2": {"Reference": (8.89, 3.81, "left"), "Value": (8.89, 6.35, "left")},
    "U3": {"Reference": (2.54, 10.16, "left"), "Value": (2.54, 12.7, "left")},
    "J3": {"Reference": (-1.27, -5.08, "left"), "Value": (2.54, 6.35, "left")},
    "D1": {"Reference": (0, -2.54, None), "Value": (0, 2.79, None)},
    "D2": {"Reference": (0, -2.54, None), "Value": (0, 2.79, None)},
    "D3": {"Reference": (0, -2.54, None), "Value": (0, 2.79, None)},
}
def off(ref):
    return OFF[GROUP_OF[ref]]

# --------------------------------------------------------------------------
sym_cache = {}
def libsym(libname, name):
    key = (libname, name)
    if key not in sym_cache:
        sym_cache[key] = ksym.flatten(libname, name)
    return sym_cache[key]

def pin_table(sym):
    out = {}
    for num, name, typ, x, y, a in ksym.pins(sym):
        out.setdefault(num, (name, typ, x, y, a))
    return out

def xform(sx, sy, rot, px, py):
    """lib coords (y up) -> sheet coords (y down), rot 0 or 180."""
    if rot == 0:
        return sx + px, sy - py
    return sx - px, sy + py

def stub_dir(angle):
    """unit vector pointing AWAY from the symbol body for a pin with this angle."""
    a = angle % 360
    return {0: (-1, 0), 180: (1, 0), 90: (0, 1), 270: (0, -1)}[int(a)]

lib_symbols = []
seen_libsyms = set()
items = []       # list of s-expr nodes (wires, labels, junctions, no_connects, symbols, text)
pwr_counter = [0]

def add_libsym(libname, name):
    full = f"{libname}:{name}"
    if full in seen_libsyms:
        return
    seen_libsyms.add(full)
    s = copy.deepcopy(libsym(libname, name))
    s[1] = full
    lib_symbols.append(s)

def effects(hide=False, justify=None, size=1.27):
    e = [Sym("effects"), [Sym("font"), [Sym("size"), Sym(F(size)), Sym(F(size))]]]
    if justify:
        e.append([Sym("justify")] + [Sym(j) for j in justify.split()])
    if hide:
        e.append([Sym("hide"), Sym("yes")])
    return e

def prop(name, value, x, y, angle=0, hide=False, justify=None):
    return [Sym("property"), name, value, [Sym("at"), Sym(F(x)), Sym(F(y)), Sym(F(angle))], effects(hide, justify)]

def place_symbol(ref, libname, name, sx, sy, rot, value, footprint, fields):
    if ref in GROUP_OF:
        sx, sy = sx + off(ref)[0], sy + off(ref)[1]
    add_libsym(libname, name)
    s = libsym(libname, name)
    node = [Sym("symbol"), [Sym("lib_id"), f"{libname}:{name}"],
            [Sym("at"), Sym(F(sx)), Sym(F(sy)), Sym(F(rot))], [Sym("unit"), Sym("1")],
            [Sym("exclude_from_sim"), Sym("no")], [Sym("in_bom"), Sym("yes")],
            [Sym("on_board"), Sym("yes")], [Sym("dnp"), Sym("no")], [Sym("uuid"), U()]]
    libprops = {p[1]: p for p in kids(s, "property")}
    is_power = ref.startswith("#")
    def lp(pname):
        p = libprops.get(pname)
        if p is None:
            return (0.0, 0.0, 0.0, None)
        at = kid(p, "at")
        eff = kid(p, "effects")
        just = None
        if eff is not None and kid(eff, "justify") is not None:
            just = " ".join(str(j) for j in kid(eff, "justify")[1:])
        return float(at[1]), float(at[2]), float(at[3]) if len(at) > 3 else 0.0, just
    # Reference / Value positions: use the library's intent, transformed
    for pname, pval, hide in (("Reference", ref, is_power), ("Value", value, False),
                              ("Footprint", footprint, True), ("Datasheet", "", True),
                              ("Description", "", True)):
        px, py, pa, just = lp(pname)
        if ref in TEXT_OVERRIDE and pname in TEXT_OVERRIDE[ref]:
            dx, dy, just = TEXT_OVERRIDE[ref][pname]
            node.append(prop(pname, pval, sx + dx, sy + dy, 0, hide, just))
        elif libname == "Device" or libname == "Diode":
            # small passives: put ref/value to the right of the body
            if pname == "Reference":
                x, y, just = (sx + 2.54, sy - 1.27, "left") if rot == 0 else (sx + 2.54, sy - 1.27, "left")
            elif pname == "Value":
                x, y, just = (sx + 2.54, sy + 1.27, "left")
            else:
                x, y = sx, sy
            node.append(prop(pname, pval, x, y, 0, hide, just))
        else:
            x, y = xform(sx, sy, rot, px, py)
            node.append(prop(pname, pval, x, y, 0 if rot == 0 else pa, hide, just))
    for k, v in fields.items():
        node.append(prop(k, v, sx, sy, 0, True, None))
    for num in pin_table(s):
        node.append([Sym("pin"), num, [Sym("uuid"), U()]])
    node.append([Sym("instances"), [Sym("project"), PROJECT,
                 [Sym("path"), "/" + ROOT_UUID, [Sym("reference"), ref], [Sym("unit"), Sym("1")]]]])
    items.append(node)

def wire(p, q):
    if abs(p[0]-q[0]) < 1e-6 and abs(p[1]-q[1]) < 1e-6:
        return
    items.append([Sym("wire"), [Sym("pts"), [Sym("xy"), Sym(F(p[0])), Sym(F(p[1]))], [Sym("xy"), Sym(F(q[0])), Sym(F(q[1]))]],
                  [Sym("stroke"), [Sym("width"), Sym("0")], [Sym("type"), Sym("default")]], [Sym("uuid"), U()]])

def label(name, x, y, angle):
    just = {0: "left bottom", 90: "left bottom", 180: "right bottom", 270: "right bottom"}[angle]
    items.append([Sym("label"), name, [Sym("at"), Sym(F(x)), Sym(F(y)), Sym(F(angle))],
                  effects(False, just), [Sym("uuid"), U()]])

def junction(x, y):
    items.append([Sym("junction"), [Sym("at"), Sym(F(x)), Sym(F(y))], [Sym("diameter"), Sym("0")],
                  [Sym("color"), Sym("0"), Sym("0"), Sym("0"), Sym("0")], [Sym("uuid"), U()]])

def no_connect(x, y):
    items.append([Sym("no_connect"), [Sym("at"), Sym(F(x)), Sym(F(y))], [Sym("uuid"), U()]])

def text(x, y, size, s):
    items.append([Sym("text"), s, [Sym("exclude_from_sim"), Sym("no")],
                  [Sym("at"), Sym(F(x)), Sym(F(y)), Sym("0")], effects(False, "left bottom", size), [Sym("uuid"), U()]])

def power_symbol(net, x, y, rot):
    pwr_counter[0] += 1
    ref = f"#PWR{pwr_counter[0]:02d}"
    place_symbol(ref, "power", net, x, y, rot, net, "", {})

def label_angle(d):
    return {(-1, 0): 180, (1, 0): 0, (0, -1): 90, (0, 1): 270}[d]

def connect_pin(ref, sx, sy, rot, num, pin, spec):
    sx, sy = sx + off(ref)[0], sy + off(ref)[1]
    name, typ, px, py, pa = pin
    x, y = xform(sx, sy, rot, px, py)
    ang = (pa + rot) % 360
    d = stub_dir(ang)
    if spec == "NC":
        no_connect(x, y)
        return
    kind = spec[0]
    if kind == "L":
        e = (x + 2.54 * d[0], y + 2.54 * d[1])
        wire((x, y), e)
        label(spec[1], e[0], e[1], label_angle(d))
    elif kind == "W":
        p = (x, y)
        dx, dy = off(ref)
        for q in spec[1]:
            q = (q[0] + dx, q[1] + dy)
            wire(p, q); p = q
    elif kind == "P":
        net = spec[1]
        natural = POWER_NETS[net]
        if len(spec) == 2:
            e = (x + 2.54 * d[0], y + 2.54 * d[1])
            wire((x, y), e)
            if d[0] != 0:
                raise ValueError(f"{ref}.{num}: horizontal pin needs explicit h,v for power net {net}")
            going = "down" if d[1] > 0 else "up"
            power_symbol(net, e[0], e[1], 0 if going == natural else 180)
        else:
            h, v = spec[2], spec[3]
            p1 = (x + h, y)
            wire((x, y), p1)
            p2 = (p1[0], p1[1] + v)
            wire(p1, p2)
            going = "down" if v > 0 else "up"
            power_symbol(net, p2[0], p2[1], 0 if going == natural else 180)

# --------------------------------------------------------------------------
for ref, libname, name, sx, sy, rot, value, fp, fields in PARTS:
    place_symbol(ref, libname, name, sx, sy, rot, value, fp, fields)
    s = libsym(libname, name)
    for num, pin in pin_table(s).items():
        spec = CONN.get((ref, num))
        if spec is None:
            if pin[1] == "no_connect":
                continue
            # stacked pins share a position with a connected pin -> already wired
            shared = any(CONN.get((ref, n2)) is not None and p2[2:5] == pin[2:5] for n2, p2 in pin_table(s).items())
            if shared:
                continue
            spec = "NC"
        connect_pin(ref, sx, sy, rot, num, pin, spec)

for g, p, q in EXTRA_WIRES:
    dx, dy = OFF[g]; wire((p[0]+dx, p[1]+dy), (q[0]+dx, q[1]+dy))
for g, n, x, y, a in EXTRA_LABELS:
    dx, dy = OFF[g]; label(n, x+dx, y+dy, a)
for g, x, y in EXTRA_JUNCTIONS:
    dx, dy = OFF[g]; junction(x+dx, y+dy)
for g, x, y, size, txt in NOTES:
    dx, dy = OFF[g]; text(x+dx, y+dy, size, txt)

sch = [Sym("kicad_sch"), [Sym("version"), Sym("20250114")], [Sym("generator"), "eeschema"],
       [Sym("generator_version"), "9.0"], [Sym("uuid"), ROOT_UUID], [Sym("paper"), "A3"],
       [Sym("title_block"), [Sym("title"), "Mitsubishi CN105 <-> ESP32-C6 adapter"], [Sym("date"), TODAY],
        [Sym("rev"), "A"], [Sym("comment"), Sym("1"), "Starter schematic. Verify the CN105 pinout on your unit before use."]],
       [Sym("lib_symbols")] + lib_symbols] + items + [
       [Sym("sheet_instances"), [Sym("path"), "/", [Sym("page"), "1"]]],
       [Sym("embedded_fonts"), Sym("no")]]

os.makedirs(OUT, exist_ok=True)
with open(os.path.join(OUT, f"{PROJECT}.kicad_sch"), "w") as f:
    f.write(dump(sch) + "\n")

# --------------------------------------------------------------------------
# PCB: outline, antenna keepout, a few notes.  Footprints come in via
# "Update PCB from Schematic" (F8) which the user does by hand.
# --------------------------------------------------------------------------
BW, BH = 48.0, 30.0           # board size mm
X0, Y0 = 100.0, 100.0         # top-left corner on the sheet
LAYERS = [
    (0, "F.Cu", "signal"), (31, "B.Cu", "signal"),
    (32, "B.Adhes", "user", "B.Adhesive"), (33, "F.Adhes", "user", "F.Adhesive"),
    (34, "B.Paste", "user"), (35, "F.Paste", "user"),
    (36, "B.SilkS", "user", "B.Silkscreen"), (37, "F.SilkS", "user", "F.Silkscreen"),
    (38, "B.Mask", "user"), (39, "F.Mask", "user"),
    (40, "Dwgs.User", "user", "User.Drawings"), (41, "Cmts.User", "user", "User.Comments"),
    (42, "Eco1.User", "user", "User.Eco1"), (43, "Eco2.User", "user", "User.Eco2"),
    (44, "Edge.Cuts", "user"), (45, "Margin", "user"),
    (46, "B.CrtYd", "user", "B.Courtyard"), (47, "F.CrtYd", "user", "F.Courtyard"),
    (48, "B.Fab", "user"), (49, "F.Fab", "user"),
]
def layer_node(l):
    n = [Sym("layers")]
    for L in LAYERS:
        e = [Sym(str(L[0])), L[1], Sym(L[2])]
        if len(L) > 3:
            e.append(L[3])
        n.append(e)
    return n

def gr_line(a, b, layer="Edge.Cuts", w=0.1):
    return [Sym("gr_line"), [Sym("start"), Sym(F(a[0])), Sym(F(a[1]))], [Sym("end"), Sym(F(b[0])), Sym(F(b[1]))],
            [Sym("stroke"), [Sym("width"), Sym(F(w))], [Sym("type"), Sym("default")]], [Sym("layer"), layer], [Sym("uuid"), U()]]

def gr_text(s, x, y, layer="Cmts.User", size=1.0):
    return [Sym("gr_text"), s, [Sym("at"), Sym(F(x)), Sym(F(y)), Sym("0")], [Sym("layer"), layer], [Sym("uuid"), U()],
            [Sym("effects"), [Sym("font"), [Sym("size"), Sym(F(size)), Sym(F(size))], [Sym("thickness"), Sym("0.15")]],
             [Sym("justify"), Sym("left")]]]

corners = [(X0, Y0), (X0 + BW, Y0), (X0 + BW, Y0 + BH), (X0, Y0 + BH)]
pcb = [Sym("kicad_pcb"), [Sym("version"), Sym("20241229")], [Sym("generator"), "pcbnew"], [Sym("generator_version"), "9.0"],
       [Sym("general"), [Sym("thickness"), Sym("1.6")], [Sym("legacy_teardrops"), Sym("no")]],
       [Sym("paper"), "A4"], layer_node(None),
       [Sym("setup"), [Sym("pad_to_mask_clearance"), Sym("0")], [Sym("allow_soldermask_bridges_in_footprints"), Sym("no")],
        [Sym("pcbplotparams"), [Sym("layerselection"), Sym("0x00000000_00000000_55555555_5755f5ff")],
         [Sym("plot_on_all_layers_selection"), Sym("0x00000000_00000000_00000000_00000000")],
         [Sym("disableapertmacros"), Sym("no")], [Sym("usegerberextensions"), Sym("no")], [Sym("usegerberattributes"), Sym("yes")],
         [Sym("usegerberadvancedattributes"), Sym("yes")], [Sym("creategerberjobfile"), Sym("yes")],
         [Sym("dashed_line_dash_ratio"), Sym("12")], [Sym("dashed_line_gap_ratio"), Sym("3")], [Sym("svgprecision"), Sym("4")],
         [Sym("plotframeref"), Sym("no")], [Sym("mode"), Sym("1")], [Sym("useauxorigin"), Sym("no")],
         [Sym("hpglpennumber"), Sym("1")], [Sym("hpglpenspeed"), Sym("20")], [Sym("hpglpendiameter"), Sym("15")],
         [Sym("pdf_front_fp_property_popups"), Sym("yes")], [Sym("pdf_back_fp_property_popups"), Sym("yes")],
         [Sym("pdf_metadata"), Sym("yes")], [Sym("pdf_single_document"), Sym("no")], [Sym("dxfpolygonmode"), Sym("yes")],
         [Sym("dxfimperialunits"), Sym("yes")], [Sym("dxfusepcbnewfont"), Sym("yes")], [Sym("psnegative"), Sym("no")],
         [Sym("psa4output"), Sym("no")], [Sym("plot_black_and_white"), Sym("yes")], [Sym("sketchpadsonfab"), Sym("no")],
         [Sym("plotpadnumbers"), Sym("no")], [Sym("hidednponfab"), Sym("no")], [Sym("sketchdnponfab"), Sym("yes")],
         [Sym("crossoutdnponfab"), Sym("yes")], [Sym("subtractmaskfromsilk"), Sym("no")], [Sym("outputformat"), Sym("1")],
         [Sym("mirror"), Sym("no")], [Sym("drillshape"), Sym("1")], [Sym("scaleselection"), Sym("1")], [Sym("outputdirectory"), "gerbers/"]]],
       [Sym("net"), Sym("0"), ""]]
for i in range(4):
    pcb.append(gr_line(corners[i], corners[(i + 1) % 4]))
# antenna keep-out: the module's antenna end should hang over this strip at the top edge
KX0, KX1 = X0 + 12.0, X0 + 30.0
KY0, KY1 = Y0 - 0.1, Y0 + 6.5
pcb.append([Sym("zone"), [Sym("net"), Sym("0")], [Sym("net_name"), ""], [Sym("layers"), Sym("F&B.Cu")], [Sym("uuid"), U()],
            [Sym("name"), "antenna_keepout"], [Sym("hatch"), Sym("edge"), Sym("0.5")], [Sym("priority"), Sym("0")],
            [Sym("connect_pads"), [Sym("clearance"), Sym("0")]], [Sym("min_thickness"), Sym("0.25")],
            [Sym("filled_areas_thickness"), Sym("no")],
            [Sym("keepout"), [Sym("tracks"), Sym("not_allowed")], [Sym("vias"), Sym("not_allowed")], [Sym("pads"), Sym("allowed")],
             [Sym("copperpour"), Sym("not_allowed")], [Sym("footprints"), Sym("allowed")]],
            [Sym("fill"), [Sym("thermal_gap"), Sym("0.5")], [Sym("thermal_bridge_width"), Sym("0.5")]],
            [Sym("polygon"), [Sym("pts"), [Sym("xy"), Sym(F(KX0)), Sym(F(KY0))], [Sym("xy"), Sym(F(KX1)), Sym(F(KY0))],
                              [Sym("xy"), Sym(F(KX1)), Sym(F(KY1))], [Sym("xy"), Sym(F(KX0)), Sym(F(KY1))]]]])
pcb.append(gr_text("ANTENNA KEEPOUT: no copper, no traces, module antenna end here, pointing off the board edge", KX0, KY0 - 1.5, "Cmts.User", 0.8))
pcb.append(gr_text(f"Board {BW:g} x {BH:g} mm. Suggested: J1 (CN105) left edge, J2 (USB-C) right edge, U1 antenna at top edge.", X0, Y0 + BH + 2.0))
pcb.append(gr_text("Press F8 in this editor (Update PCB from Schematic) to import the footprints.", X0, Y0 + BH + 4.0))
pcb.append([Sym("embedded_fonts"), Sym("no")])
with open(os.path.join(OUT, f"{PROJECT}.kicad_pcb"), "w") as f:
    f.write(dump(pcb) + "\n")

# --------------------------------------------------------------------------
# Project file: JLCPCB-friendly design rules + net classes.
# --------------------------------------------------------------------------
def netclass(name, clearance, track, via_d, via_drill, color="rgba(0, 0, 0, 0.000)", wire_w=6):
    return {"name": name, "bus_width": 12, "clearance": clearance, "diff_pair_gap": 0.25, "diff_pair_via_gap": 0.25,
            "diff_pair_width": 0.2, "line_style": 0, "microvia_diameter": 0.3, "microvia_drill": 0.1,
            "pcb_color": color, "schematic_color": color, "track_width": track, "via_diameter": via_d,
            "via_drill": via_drill, "wire_width": wire_w}

pro = {
    "board": {
        "3dviewports": [],
        "design_settings": {
            "defaults": {
                "board_outline_line_width": 0.1, "copper_line_width": 0.2, "copper_text_size_h": 1.5, "copper_text_size_v": 1.5,
                "copper_text_thickness": 0.3, "other_line_width": 0.15, "silk_line_width": 0.15, "silk_text_size_h": 1.0,
                "silk_text_size_v": 1.0, "silk_text_thickness": 0.15,
            },
            "diff_pair_dimensions": [],
            "drc_exclusions": [],
            "rules": {
                "min_clearance": 0.2, "min_connection": 0.0, "min_copper_edge_clearance": 0.3, "min_hole_clearance": 0.25,
                "min_hole_to_hole": 0.5, "min_microvia_diameter": 0.2, "min_microvia_drill": 0.1, "min_resolved_spokes": 2,
                "min_silk_clearance": 0.0, "min_text_height": 0.8, "min_text_thickness": 0.08, "min_through_hole_diameter": 0.3,
                "min_track_width": 0.15, "min_via_annular_width": 0.13, "min_via_diameter": 0.5,
                "solder_mask_to_copper_clearance": 0.0, "use_height_for_length_calcs": True,
            },
            "track_widths": [0.0, 0.25, 0.4, 0.6, 0.8],
            "via_dimensions": [{"diameter": 0.0, "drill": 0.0}, {"diameter": 0.6, "drill": 0.3}, {"diameter": 0.8, "drill": 0.4}],
        },
        "ipc2581": {"dist": "", "distpn": "", "internal_id": "", "mfg": "", "mpn": ""},
        "layer_presets": [],
        "viewports": [],
    },
    "boards": [],
    "cvpcb": {"equivalence_files": []},
    "libraries": {"pinned_footprint_libs": [], "pinned_symbol_libs": []},
    "meta": {"filename": f"{PROJECT}.kicad_pro", "version": 3},
    "net_settings": {
        "classes": [
            netclass("Default", 0.2, 0.25, 0.6, 0.3),
            netclass("Power", 0.2, 0.5, 0.8, 0.4, "rgba(255, 0, 0, 0.6)", 10),
        ],
        "meta": {"version": 4},
        "net_colors": None,
        "netclass_assignments": None,
        "netclass_patterns": [
            {"netclass": "Power", "pattern": "+5V"}, {"netclass": "Power", "pattern": "+3V3"},
            {"netclass": "Power", "pattern": "GND"}, {"netclass": "Power", "pattern": "VBUS"},
            {"netclass": "Power", "pattern": "5V_CN105"},
        ],
    },
    "pcbnew": {"last_paths": {"gencad": "", "idf": "", "netlist": "", "plot": "gerbers/", "pos_files": "", "specctra_dsn": "", "step": "", "svg": "", "vrml": ""},
               "page_layout_descr_file": ""},
    "schematic": {"legacy_lib_dir": "", "legacy_lib_list": []},
    "sheets": [[ROOT_UUID, "Root"]],
    "text_variables": {},
}
with open(os.path.join(OUT, f"{PROJECT}.kicad_pro"), "w") as f:
    json.dump(pro, f, indent=2)

with open(os.path.join(OUT, "fp-lib-table"), "w") as f:
    f.write('(fp_lib_table\n  (version 7)\n  (lib (name "cn105") (type "KiCad") (uri "${KIPRJMOD}/cn105.pretty") (options "") (descr "Project footprints (JST PA header)"))\n)\n')

print("wrote", OUT)
