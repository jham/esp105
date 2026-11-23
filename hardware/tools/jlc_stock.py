#!/usr/bin/env python3
"""Check every line of the JLCPCB BOM against JLCPCB's parts database.

    python3 tools/jlc_stock.py production/cn105-bom-jlcpcb.csv

Prints, per LCSC code: library type (base/expand), preferred flag, product type, stock and the
manufacturer part name. Flags to look for before ordering:
- NOT FOUND or stock 0: pick another part.
- type=2: JLCPCB assembles the part under its Standard service only (Economic is refused).
- expand without pref=True: the part costs the extended-part loading fee.
"""
import csv
import json
import sys
import urllib.request

URL = "https://jlcpcb.com/api/overseas-pcb-order/v1/shoppingCart/smtGood/selectSmtComponentList"


def lookup(code):
    body = json.dumps({"keyword": code, "currentPage": 1, "pageSize": 5}).encode()
    req = urllib.request.Request(URL, data=body, headers={"Content-Type": "application/json",
                                                           "User-Agent": "Mozilla/5.0"})
    data = json.load(urllib.request.urlopen(req, timeout=30))
    rows = ((data.get("data") or {}).get("componentPageInfo") or {}).get("list") or []
    for row in rows:
        if row.get("componentCode") == code:
            return row
    return None


def main():
    if len(sys.argv) != 2:
        sys.exit("usage: jlc_stock.py BOM.csv (JLC format with an 'LCSC Part #' column)")
    problems = 0
    with open(sys.argv[1], newline="") as f:
        for row in csv.DictReader(f):
            code = row["LCSC Part #"].strip()
            des = row["Designator"]
            try:
                x = lookup(code)
            except Exception as e:  # network, JSON
                print("%-10s ERROR %s  [%s]" % (code, e, des))
                problems += 1
                continue
            if not x:
                print("%-10s NOT FOUND  [%s]" % (code, des))
                problems += 1
                continue
            stock = x.get("stockCount") or 0
            flag = ""
            if stock == 0:
                flag = "  <-- OUT OF STOCK"
                problems += 1
            elif x.get("componentProductType") == 2:
                flag = "  <-- Standard assembly only"
                problems += 1
            print("%-10s %-6s pref=%-5s type=%s stock=%8s  %s [%s]%s" % (
                code, x.get("componentLibraryType"), x.get("preferredComponentFlag"),
                x.get("componentProductType"), stock, x.get("componentModelEn"), des, flag))
    print("%d problem line(s)" % problems)
    sys.exit(1 if problems else 0)


if __name__ == "__main__":
    main()
