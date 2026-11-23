---
name: order
description: Place a board order at JLCPCB for the hardware/ KiCad project. Proposes the next semver board version, records it in the KiCad title blocks (which drive the version/date silkscreen), runs the checks and the JLCPCB export, waits for the user to submit the order, then logs it in ORDER_LOG.md, commits to main and tags v<version>. Use when the user says "order", "place an order", "new board order", "prepare a JLCPCB order" or "/order".
---

# Board order (JLCPCB)

Everything happens in `hardware/`. Boards are identified by semver only (`0.3.0`, tag `v0.3.0`);
never call a board "rev A", "rev B" or similar, in files, commits or replies. Tags `v0.1` and
`v0.2` predate the three-part numbering and mean 0.1.0 and 0.2.0.

Standing rules that apply throughout:

- Never write a KiCad file while its editor is open. A lock file `hardware/~cn105.kicad_*.lck`
  means open. Ask the user to close the editor (not just save) and check again.
- The user reviews any change before it is committed. Show the diff summary and wait for a yes.
- Commit and push only at the step that says so, and only after the user confirms.

## 1. Preconditions

```bash
cd hardware && git status --short && find . -maxdepth 1 -name "*.lck"
```

- The working tree should contain only the files this order will change. If there are unrelated
  uncommitted edits, ask whether to include them in the order commit or commit them first.
- No lock files for step 2. The export in step 3 only reads the files and may run with the
  editors open, but the board on disk must be saved; ask the user to save if unsure.

## 2. Propose the version and confirm it

Find the latest tag and what changed since it:

```bash
git describe --tags --abbrev=0 && git diff --stat $(git describe --tags --abbrev=0) -- hardware/
```

Bump rule (semver, from the latest tag read as MAJOR.MINOR.PATCH):

- PATCH when `cn105.kicad_pcb` and `cn105.kicad_sch` are unchanged since the tag (a re-order,
  or BOM/ordering-only changes such as a part substitution with the same footprint and nets).
- MINOR when the schematic or the copper changed (any layout or circuit change).
- MAJOR only if the user asks for it (incompatible connector, pinout or form-factor change).

Tell the user the proposed version, the bump reason in one sentence, and today's date, and ask
them to confirm or give another version. Do not continue until they answer. Use
`AskUserQuestion` when available, otherwise ask in the reply and stop.

## 3. Record the version in the design files

With the editors closed:

```bash
cd hardware && python3 tools/set_version.py <version> <YYYY-MM-DD>
```

This sets `(rev ...)` and `(date ...)` in the title blocks of `cn105.kicad_pcb` and
`cn105.kicad_sch`. The board's silkscreen texts are `ESP105 v${REVISION}` and `${ISSUE_DATE}`,
so they print the new values without anyone editing the layout; kicad-cli resolves the variables
when it plots. The script also rewrites a hand-typed `ESP105 v…` or date text into the variable
form if it finds one. If it reports that no board text uses the variables, tell the user and
continue; adding the silkscreen text is a layout change for them to make.

Then update the version number in the **Design state** paragraph at the top of
`hardware/README.md` ("complete at version <version>") and fix anything in that paragraph the
new version changed (board size, module, connector placement). The README does not describe
orders; that is what `ORDER_LOG.md` is for.

## 4. Checks and export

```bash
cd hardware && sh tools/export_jlcpcb.sh
```

Before or right after the export, run the checks against the saved files and require all clean:

```bash
cd hardware && /Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli sch erc --format json --severity-all -o /tmp/erc.json cn105.kicad_sch && /Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli pcb drc --format json --severity-all --schematic-parity --refill-zones -o /tmp/drc.json cn105.kicad_pcb
```

Required: ERC 0, DRC 0 errors, 0 unconnected, 0 parity issues. Six `silk_edge_clearance`
warnings on the connectors and antenna are expected. Anything else: stop, report it, and let the
user fix the board; then rerun from this step.

Then check the parts:

```bash
cd hardware && python3 tools/jlc_stock.py production/cn105-bom-jlcpcb.csv
```

Every line must be found with stock; a `type=2` part forces Standard assembly, which the user
must know before ordering. Report the extended parts (loading fee) and the preferred ones (none).

Finally confirm the Edge.Cuts extent matches the board size in the README, and send the user the
three upload files with `SendUserFile` (`production/cn105-gerbers.zip`,
`production/cn105-bom-jlcpcb.csv`, `production/cn105-cpl-jlcpcb.csv`).

## 5. The user places the order

Tell the user the upload order: Gerbers zip on the PCB step; then PCB Assembly (Economic unless a
part is Standard-only), top side, BOM and CPL on the assembly step; check the parts page for
matches; check the placement preview for connector positions and polarised-part rotations.

Ask the user to confirm once the order is submitted, and ask for the quantity, the PCB colour
and finish, and the assembly service used. Never ask for or record the JLCPCB order number: the
repository is public. Wait for the answer. If they
say they did not order, stop here, leave the files changed but uncommitted, and say so.

## 6. Log the order

Add an entry at the top of `hardware/ORDER_LOG.md` (newest first) in the existing format:

```
## <version> — <date>

- Tag `v<version>`, commit <filled in after the commit>. Ordered from JLCPCB on <date>.
- Fab: 2 layers, 1.6 mm, <W × H> mm, <colour>, <finish>. Assembly: <Economic|Standard>, top
  side, <n> BOM lines / <n> parts, <quantity> boards.
- Changes since <previous version>: <what changed, in design terms>.
- Extended parts: <list>; preferred (no fee): <list>.
```

Leave the commit hash as "see tag" if it must be written before committing; the tag identifies
the commit.

## 7. Commit, tag, push

Show `git status --short` and a one-line summary of each changed file, and ask the user to
confirm the commit. On yes:

```bash
git add -A hardware && git commit -F - <<'EOF'
Order <version> from JLCPCB (<date>)

<two to four lines: what this version changes, assembly service, anything notable>

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>
EOF
```

Then, as separate commands (the push may need the SSH agent to approve the key; if it fails
with an agent error, ask the user to approve it and retry):

```bash
git tag -a v<version> -m "Board <version>: ordered from JLCPCB <date>" && git push origin main && git push origin v<version>
```

Confirm with `git status -sb` (no "ahead") and `git tag -n1 | tail -1`, and report the commit,
the tag and what the order log now says.
