# Mach8 PR - draft for the owner to rewrite (not posted)

Branch: `Mike1978uk/86Box:mach8-pr` (`6e9978efd`), one commit on upstream `1741108a7`.

## Gate status (repo-hygiene section 10), 2026-10-11

| Gate | Result |
|---|---|
| G1 no debug left on | pass: no pclog, getenv, ENABLE_*_LOG or #if 0/1 in the added lines |
| G2 absent means unchanged | Mach32 and IBM 8514/A share the changed code: both regressed against the branch point (A6) - differences only the colour-compare fix and mix 1Fh, both per the guides. A config with no 8514-class card: not run (the changed files build only into those devices) |
| G3 Settings attach | not applicable: no new device, bus or option |
| G4 working device, stranger's driver | not run: needs ATI's own Windows 3.1/95 driver on the Graphics Ultra - owner (A5) |
| G5 comments re-read | done: probe names, tool paths and repo docs removed; every note is a guide page, ATI TEST.COM or "measured on a real card" |
| G6 claims reproducible | in the text below: TEST.COM ships with the card; no claim depends on this machine |
| G7 builds on current master | pass: applies with --3way, builds, no warnings in the four files |
| G8 minimal diff | 1708/410 lines; 35 lines change only whitespace - to check before submitting |
| G9 last run on the submitted HEAD | pass: Mach32 35/35 and Graphics Ultra probes identical on `6e9978efd`'s build |
| G10 owner by hand | open |

## Draft text

Title: ATI Graphics Ultra (Mach8): accelerator behaviour measured on a real card

ATI's own diagnostic, TEST.COM from the 8514/Ultra and Graphics Ultra utilities, failed its drawing
tests on the emulated Graphics Ultra. This changes the Mach8 accelerator model so that TEST.COM passes,
and every change was checked against a real Graphics Ultra (113-11504-002, 1 MB, 8-bit slot) by
running the same register sequences on the card and in 86Box and comparing the pixels read back.

What changes, Graphics Ultra only unless stated:
- 8514/A colour compare: a TRUE compare protects the pixel, and planes outside WRT_MASK take no part
  (Richter and Smith; Mach32 guide). Applies to all 8514-class cards.
- Mix 1Fh saturates as the guide's table gives it. Applies to all 8514-class cards.
- Host-data byte order, command 3/4 shapes, polygon fill, short strokes, LINEDRAW pre-clip, scissor
  reads, pattern fills, read-back flags, CRT shadow sets, CONFIG_STATUS_1, LINE_SYNC - each comment
  states the rule and its source.

Tested:
- TEST.COM passes on the Graphics Ultra model. [TO VERIFY on `6e9978efd` - last run was the working branch, 10-08]
- A set of register-level probes, run on the real card and in 86Box: results byte-identical.
- Mach32 and IBM 8514/A: the same probe set before and after. The only differences are the two
  guide-documented fixes above.

Not tested: ATI's Windows drivers on this build (owner check pending); Mach8 cards other than the
Graphics Ultra; 16-bit slot operation.

(Owner: thank anyone who helped in the final text; do not post this draft as written.)
