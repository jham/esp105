---
name: order
description: Place a board order at JLCPCB for the hardware/ KiCad project. First makes sure the design is a tagged version, running the /bump-version skill when the design changed since the last version tag, then runs the checks and the JLCPCB export for that version, waits for the user to submit the order, logs it in ORDER_LOG.md, commits to main and pushes the commit and the version tag. Use when the user says "order", "place an order", "new board order", "prepare a JLCPCB order" or "/order".
---

# Board order (JLCPCB)

Everything happens in `hardware/`. An order builds one tagged board version. Versions are set by
the `bump-version` skill (`.claude/skills/bump-version/SKILL.md`), not here: this skill only calls
it when the design changed since the last version tag. An unchanged version can be ordered again
without a new version number, and a version can exist that was never ordered.

Boards are identified by semver only (`0.4.0`, tag `v0.4.0`); never call a board "rev A",
"rev B" or similar, in files, commits or replies. Tags `v0.1` and `v0.2` predate the three-part
numbering and mean 0.1.0 and 0.2.0.

Standing rules that apply throughout:

- Never write a KiCad file while its editor is open (see the bump-version skill for which lock
  files block). This skill itself writes no KiCad file; only a version bump does.
- The user reviews any change before it is committed. Show the diff summary and wait for a yes.
- Commit and push only at the step that says so, and only after the user confirms.

## 1. Preconditions

```bash
cd hardware && git status --short && find . -maxdepth 1 -name "*.lck"
```

- If there are uncommitted edits outside the design files (firmware, docs), ask whether to commit
  them first or leave them out of the order. The order commit holds only `ORDER_LOG.md`.
- The export in step 3 only reads the files and may run with the editors open, but the board on
  disk must be saved; ask the user to save if unsure.

## 2. Make sure the design is a tagged version

Run step 1 of the bump-version skill (the design-change check against the last version tag).

- **The design changed since the last tag** (committed or not): follow the bump-version skill from
  its step 2 to the end, including its commit and tag, before going on. The order then builds the
  new version.
- **No changes:** the order builds the last tagged version as it is. Tell the user which version
  that is and, from `ORDER_LOG.md`, whether it has been ordered before. Do not bump.

Either way, finish this step with the version to order in hand (`<version>`, tag `v<version>`),
and confirm the design files on disk match the tag:

```bash
git diff --quiet v<version> -- hardware/cn105.kicad_sch hardware/cn105.kicad_pcb hardware/cn105.kicad_pro hardware/cn105.kicad_sym hardware/cn105.pretty hardware/fp-lib-table hardware/sym-lib-table hardware/tools/export_jlcpcb.sh hardware/tools/export_jlcpcb.py && echo "matches v<version>"
```

## 3. Checks and export

```bash
cd hardware && sh tools/export_jlcpcb.sh
```

Before or right after the export, run the checks against the saved files and require all clean:

```bash
cd hardware && /Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli sch erc --format json --severity-all -o /tmp/erc.json cn105.kicad_sch && /Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli pcb drc --format json --severity-all --schematic-parity --refill-zones -o /tmp/drc.json cn105.kicad_pcb
```

Required: ERC 0, DRC 0 errors, 0 unconnected, 0 parity issues. Four `silk_edge_clearance`
warnings where the USB-C and the module outlines cross the board edge are expected. Anything
else: stop, report it, and let the user fix the board. A fix changes the design, so it means a
new version: rerun from step 2.

Then check the parts:

```bash
cd hardware && python3 tools/jlc_stock.py production/cn105-bom-jlcpcb.csv
```

Every line must be found with stock; a `type=2` part forces Standard assembly, which the user
must know before ordering. Report the extended parts (loading fee) and the preferred ones (none).

Finally confirm the Edge.Cuts extent matches the board size in the README, and send the user the
three upload files with `SendUserFile` (`production/cn105-gerbers.zip`,
`production/cn105-bom-jlcpcb.csv`, `production/cn105-cpl-jlcpcb.csv`).

## 4. The user places the order

Tell the user the upload order: Gerbers zip on the PCB step; then PCB Assembly (Economic unless a
part is Standard-only), top side, BOM and CPL on the assembly step; check the parts page for
matches; check the placement preview for connector positions and polarised-part rotations.

Ask the user to confirm once the order is submitted, and ask for the quantity, the PCB colour
and finish, and the assembly service used. Never ask for or record the JLCPCB order number: the
repository is public. Wait for the answer. If they say they did not order, stop here; any
version bump from step 2 stays committed and tagged, and nothing else changed.

## 5. Log the order

Add an entry at the top of `hardware/ORDER_LOG.md` (newest first) in the existing format:

```
## <version> — <order date>

- Tag `v<version>`, commit see tag. Ordered from JLCPCB on <order date>.
- Fab: 2 layers, 1.6 mm, <W × H> mm, <colour>, <finish>. Assembly: <Economic|Standard>, top
  side, <n> BOM lines / <n> parts, <quantity> boards.
- Changes since <previously ordered version>: <what changed, in design terms; the README's
  "Changes from … to …" sections cover each version in between>. For a repeat order of the same
  version, say "Repeat order of <version>; no design changes."
- Extended parts: <list>; preferred (no fee): <list>.
```

The heading date is the order date; the title-block date is the day the version was set, and the
two can differ.

## 6. Commit and push

Show `git status --short` and a one-line summary of each changed file, and ask the user to
confirm the commit. On yes:

```bash
git add hardware/ORDER_LOG.md && git commit -F - <<'EOF'
Order <version> from JLCPCB (<order date>)

<one or two lines: quantity, assembly service, anything notable>

<the Co-Authored-By line this session's attribution instructions give>
EOF
```

Do not create or move a tag here; `v<version>` already exists from the version bump. Then, as
separate commands (the push may need the SSH agent to approve the key; if it fails with an agent
error, ask the user to approve it and retry):

```bash
git push origin main && git push origin v<version>
```

Pushing a tag that is already on the remote is a no-op. Confirm with `git status -sb` (no
"ahead") and `git ls-remote --tags origin v<version>`, and report the commit, the tag and what the
order log now says.
