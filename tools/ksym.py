"""Minimal KiCad s-expression parser + symbol library reader."""
import re, sys
LIBDIR = "/Applications/KiCad/KiCad.app/Contents/SharedSupport/symbols/"

class Sym(str):
    """bare symbol token (unquoted)"""

def parse(text):
    tok = re.compile(r'\s*(?:(\()|(\))|"((?:[^"\\]|\\.)*)"|([^\s()"]+))', re.S)
    pos, stack, cur = 0, [], []
    while pos < len(text):
        m = tok.match(text, pos)
        if not m: break
        pos = m.end()
        if m.group(1):
            stack.append(cur); cur = []
        elif m.group(2):
            done = cur; cur = stack.pop(); cur.append(done)
        elif m.group(3) is not None:
            cur.append(m.group(3).replace('\\"', '"'))
        elif m.group(4):
            cur.append(Sym(m.group(4)))
    return cur

def dump(node, ind=0):
    if not isinstance(node, list):
        if isinstance(node, Sym): return str(node)
        return '"' + node.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n') + '"'
    simple = all(not isinstance(c, list) for c in node)
    if simple:
        return "(" + " ".join(dump(c) for c in node) + ")"
    out = "(" + " ".join(dump(c) for c in node if not isinstance(c, list))
    for c in node:
        if isinstance(c, list):
            out += "\n" + "\t" * (ind + 1) + dump(c, ind + 1)
    return out + "\n" + "\t" * ind + ")"

_cache = {}
def lib(name):
    if name not in _cache:
        with open(LIBDIR + name + ".kicad_sym") as f:
            _cache[name] = parse(f.read())[0]
    return _cache[name]

def find_symbol(libname, symname):
    for n in lib(libname):
        if isinstance(n, list) and n and n[0] == "symbol" and n[1] == symname:
            return n
    raise KeyError(f"{libname}:{symname}")

def kids(node, key):
    return [c for c in node if isinstance(c, list) and c and c[0] == key]

def kid(node, key):
    k = kids(node, key); return k[0] if k else None

def flatten(libname, symname):
    """Return a self-contained copy of the symbol (resolving 'extends')."""
    import copy
    s = copy.deepcopy(find_symbol(libname, symname))
    ext = kid(s, "extends")
    if ext:
        parent = flatten(libname, ext[1])
        props = {p[1]: p for p in kids(s, "property")}
        out = [c for c in parent if not (isinstance(c, list) and c and c[0] in ("property",))]
        out[1] = symname
        # rename units Parent_x_y -> Child_x_y
        for c in out:
            if isinstance(c, list) and c and c[0] == "symbol":
                c[1] = symname + c[1][len(ext[1]):]
        # merge properties: child overrides parent
        pprops = {p[1]: p for p in kids(parent, "property")}
        pprops.update(props)
        idx = 2
        for p in pprops.values():
            out.insert(idx, p); idx += 1
        return out
    return s

def pins(sym):
    """[(number, name, type, x, y, angle)] over all units (unit 0 and 1)."""
    res = []
    for unit in kids(sym, "symbol"):
        for p in kids(unit, "pin"):
            at = kid(p, "at"); name = kid(p, "name"); num = kid(p, "number")
            res.append((num[1], name[1], str(p[1]), float(at[1]), float(at[2]), float(at[3]) if len(at) > 3 else 0.0))
    return res

if __name__ == "__main__":
    for ln, sn in [a.split(":") for a in sys.argv[1:]]:
        s = flatten(ln, sn)
        fp = kid(s, "property")  # placeholder
        fpv = [p for p in kids(s, "property") if p[1] == "Footprint"]
        print(f"=== {ln}:{sn}  footprint={fpv[0][2] if fpv else '?'}")
        for num, name, typ, x, y, a in sorted(pins(s), key=lambda t: (t[5], t[4], t[3])):
            print(f"  {num:>4} {name:<12} {typ:<14} at=({x:7.2f},{y:7.2f}) ang={a:g}")
