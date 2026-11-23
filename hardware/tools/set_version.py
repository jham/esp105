#!/usr/bin/env python3
"""Record a board version and date in the KiCad files.

    python3 tools/set_version.py 0.3.0 [2026-11-23]

Run from the hardware/ folder with the KiCad editors CLOSED (the script refuses to touch a file
that has a ~*.lck lock next to it). It is plain text surgery, no pcbnew needed.

What it sets:
- cn105.kicad_pcb title block: (rev "0.3.0") and (date "2026-11-23"). Any board text containing
  ${REVISION} or ${ISSUE_DATE} picks these up: the version/date silkscreen on the board is written
  as "ESP105 v${REVISION}" and "${ISSUE_DATE}", and kicad-cli resolves the variables when plotting
  Gerbers (verified: the plotted silkscreen is byte-identical to literal text).
- cn105.kicad_sch title block: same rev and date, so the schematic sheet frame matches.
- One-time migration: a board text written as a literal "ESP105 v<something>" or as a bare
  YYYY-MM-DD date is rewritten to the ${...} form, so a hand-typed version never goes stale.

Versions are semver (MAJOR.MINOR.PATCH), no "v" prefix here; the git tag is v<version>.
"""
import datetime
import os
import re
import sys

PCB = "cn105.kicad_pcb"
SCH = "cn105.kicad_sch"
SEMVER = re.compile(r"^\d+\.\d+\.\d+$")


def die(msg):
    sys.exit("set_version: " + msg)


def title_block(text, rev, date):
    """Return text with the title_block rev/date set (creating the block after (paper ...) if absent)."""
    m = re.search(r"\n\t\(title_block\n(.*?)\n\t\)\n", text, re.S)
    if m:
        body = m.group(1)
        if re.search(r'\n?\t\t\(rev "[^"]*"\)', body):
            body = re.sub(r'\t\t\(rev "[^"]*"\)', '\t\t(rev "%s")' % rev, body)
        else:
            body += '\n\t\t(rev "%s")' % rev
        if re.search(r'\t\t\(date "[^"]*"\)', body):
            body = re.sub(r'\t\t\(date "[^"]*"\)', '\t\t(date "%s")' % date, body)
        else:
            body += '\n\t\t(date "%s")' % date
        return text[: m.start(1)] + body + text[m.end(1):]
    paper = re.search(r'\n\t\(paper "[^"]*"\)\n', text)
    if not paper:
        die("no (paper ...) line to anchor a new title_block on")
    block = '\t(title_block\n\t\t(date "%s")\n\t\t(rev "%s")\n\t)\n' % (date, rev)
    return text[: paper.end()] + block + text[paper.end():]


def current(text, key):
    m = re.search(r'\n\t\(title_block\n.*?\t\t\(%s "([^"]*)"\).*?\n\t\)\n' % key, text, re.S)
    return m.group(1) if m else None


def main():
    if len(sys.argv) not in (2, 3):
        die("usage: set_version.py MAJOR.MINOR.PATCH [YYYY-MM-DD]")
    version = sys.argv[1].lstrip("v")
    if not SEMVER.match(version):
        die("version must be semver like 0.3.0 (got %r)" % sys.argv[1])
    date = sys.argv[2] if len(sys.argv) == 3 else datetime.date.today().isoformat()
    if not re.match(r"^\d{4}-\d{2}-\d{2}$", date):
        die("date must be YYYY-MM-DD")
    for f in (PCB, SCH):
        if not os.path.exists(f):
            die("run from the hardware/ folder; %s not found" % f)
        if os.path.exists("~" + f + ".lck"):
            die("%s is open in KiCad (lock file present); close the editor and rerun" % f)

    pcb = open(PCB).read()
    old_date = current(pcb, "date")
    pcb = title_block(pcb, version, date)
    # One-time migration of hand-typed silkscreen to the variable form.
    pcb, n1 = re.subn(r'\(gr_text "ESP105 v(?!\$\{)[^"]*"', '(gr_text "ESP105 v${REVISION}"', pcb)
    # A board text that is just a date (the previous title-block date, or any hand-typed
    # YYYY-MM-DD) becomes ${ISSUE_DATE}.
    pcb, n2 = re.subn(r'\(gr_text "(?:%s|\d{4}-\d{2}-\d{2})"' % (re.escape(old_date) if old_date else "(?!)"),
                      '(gr_text "${ISSUE_DATE}"', pcb)
    open(PCB, "w").write(pcb)

    sch = open(SCH).read()
    sch = title_block(sch, version, date)
    open(SCH, "w").write(sch)

    has_rev = "${REVISION}" in pcb
    has_date = "${ISSUE_DATE}" in pcb
    print("title block set to rev %s, date %s in %s and %s" % (version, date, PCB, SCH))
    if n1 or n2:
        print("migrated %d version text(s) and %d date text(s) on the board to ${...} form" % (n1, n2))
    if not has_rev or not has_date:
        print("note: the board has no text using %s; add a silkscreen text such as "
              '"ESP105 v${REVISION}" / "${ISSUE_DATE}" if you want it printed'
              % " / ".join(v for v, ok in (("${REVISION}", has_rev), ("${ISSUE_DATE}", has_date)) if not ok))


if __name__ == "__main__":
    main()
