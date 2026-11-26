---
name: bump-version
description: Set a new semver board version for the hardware/ KiCad project. Checks whether the design changed since the last version tag, proposes the next version, records it in the KiCad title blocks (which drive the version/date silkscreen), documents the changes in hardware/README.md, runs the checks, commits and tags v<version>. Independent of ordering; the /order skill runs it first when the design changed. Use when the user says "bump the version", "new board version", "set the board version", "tag a board version" or "/bump-version".
---

# Board version bump

Everything happens in `hardware/`. A version names a board **design**, not an order. Every
version gets an annotated tag `v<version>` on the commit whose title blocks carry it, at the time
the version is set. A version can exist without ever being ordered, and an unchanged version can
be ordered more than once; orders are logged in `ORDER_LOG.md`, not here.

Boards are identified by semver only (`0.4.0`, tag `v0.4.0`); never call a board "rev A",
"rev B" or similar, in files, commits or replies. Tags `v0.1` and `v0.2` predate the three-part
numbering and mean 0.1.0 and 0.2.0.

Standing rules that apply throughout:

- Never write a KiCad file while its editor is open. A lock file `hardware/~cn105.kicad_sch.lck`
  or `hardware/~cn105.kicad_pcb.lck` means open. Ask the user to close that editor (not just
  save) and check again. The project manager's `~cn105.kicad_pro.lck` alone does not block,
  since nothing here writes the project file.
- The user reviews any change before it is committed. Show the diff summary and wait for a yes.
- Push only if the user asks, or when the `/order` skill reaches its push step.

## 1. Did the design change since the last version?

The design files are the ones that decide what JLCPCB builds:

```bash
LAST=$(git describe --tags --abbrev=0 --match 'v*') && echo "$LAST" && git diff --stat "$LAST" -- hardware/cn105.kicad_sch hardware/cn105.kicad_pcb hardware/cn105.kicad_pro hardware/cn105.kicad_sym hardware/cn105.pretty hardware/fp-lib-table hardware/sym-lib-table hardware/tools/export_jlcpcb.sh hardware/tools/export_jlcpcb.py && git status --short -- hardware/cn105.pretty
```

`git diff <tag>` compares the tag with the working tree, so uncommitted edits count too.
Documentation, renders, `BOM.csv` (human-readable; the JLCPCB BOM comes from the schematic's
`LCSC` fields), `ORDER_LOG.md` and the 3D models do not count.

- **No changes:** the design is still `$LAST`. Say so and stop; there is nothing to bump. (When
  `/order` called this skill, it continues with that version.)
- **Changes:** continue. If the working tree also holds unrelated edits (firmware, other docs),
  ask whether they belong in the version commit or should be committed separately first.

## 2. Propose the version and confirm it

Bump rule (semver, from the last tag read as MAJOR.MINOR.PATCH):

- PATCH when `cn105.kicad_pcb` and `cn105.kicad_sch` are unchanged apart from the title block:
  a part substitution with the same footprint and nets, a footprint-library or export-script fix
  such as placement-file rotations, a project-rule tweak.
- MINOR when the schematic or the copper changed (any circuit, placement, routing or silkscreen
  change on the board).
- MAJOR only if the user asks for it (incompatible connector, pinout or form-factor change).

Tell the user the proposed version, the bump reason in one sentence, and today's date, and ask
them to confirm or give another version. Do not continue until they answer. Use
`AskUserQuestion` when available, otherwise ask in the reply and stop.

## 3. Record the version in the design files

With the schematic and PCB editors closed:

```bash
cd hardware && python3 tools/set_version.py <version> <YYYY-MM-DD>
```

This sets `(rev ...)` and `(date ...)` in the title blocks of `cn105.kicad_pcb` and
`cn105.kicad_sch`; the date is the day the version was set, not an order date. The board's
silkscreen texts are `ESP105 v${REVISION}` and `${ISSUE_DATE}`, so they print the new values
without anyone editing the layout; kicad-cli resolves the variables when it plots. The script also
rewrites a hand-typed `ESP105 v…` or date text into the variable form if it finds one. If it
reports that no board text uses the variables, tell the user and continue; adding the silkscreen
text is a layout change for them to make.

## 4. Document the version

In `hardware/README.md`:

- **Design state** paragraph at the top: the version number, whether it has been ordered (the
  latest boards made are in `ORDER_LOG.md`), and anything the new version changed in that
  paragraph (board size, module, connector placement).
- Add a `## Changes from <previous> to <version>` section directly above the previous one
  (newest first; older sections stay). Split it into **Circuit** and **Layout**: new or changed
  parts with their LCSC numbers and why, renamed nets, moved parts, routing changes, silkscreen
  changes, and what stayed the same (outline, holes, connectors, design rules). Get the facts
  from the diff against the last tag, not from memory: compare footprint positions, net names and
  track/via counts between `git show $LAST:hardware/cn105.kicad_pcb` and the working copy.
- Fix the circuit description and placement notes where the new version made them wrong.

If the board changed, re-render the picture the README shows:

```bash
cd hardware && /Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli pcb render --side top --width 1568 --height 984 -o cn105-board-3d.png cn105.kicad_pcb
```

Update `TODO.md` where it mentions the version or work this version finished.

## 5. Checks

```bash
cd hardware && /Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli sch erc --format json --severity-all -o /tmp/erc.json cn105.kicad_sch && /Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli pcb drc --format json --severity-all --schematic-parity --refill-zones -o /tmp/drc.json cn105.kicad_pcb
```

Required: ERC 0, DRC 0 errors, 0 unconnected, 0 parity issues. Four `silk_edge_clearance`
warnings where the USB-C and the module outlines cross the board edge are expected. Anything
else: stop, report it, and let the user fix the board; then rerun from this step.

## 6. Commit and tag

Show `git status --short` and a one-line summary of each changed file, and ask the user to
confirm the commit. On yes, stage the design files, the README, the render and TODO.md (plus
anything else the user agreed to include) and commit:

```bash
git commit -F - <<'EOF'
Board <version>: <one-line summary of what changed>

<a few lines: circuit changes, layout changes, check results>

<the Co-Authored-By line this session's attribution instructions give>
EOF
```

Then tag that commit:

```bash
git tag -a v<version> -m "Board <version>: <one-line summary>"
```

Never move or recreate an existing tag; if a fix is needed after tagging, that is a new version.
Report the commit, the tag, and that nothing is pushed unless the user asked for a push.
