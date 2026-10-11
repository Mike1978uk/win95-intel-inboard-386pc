# Mach8 PR - draft for the owner to rewrite (not posted)

Branch: `Mike1978uk/86Box:mach8-pr` (`0d54a50fc`), one commit on upstream `1741108a7`, five files (+1727/-414).

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
| G9 last run on the submitted HEAD | `0d54a50fc`: TEST.COM passes (watched); probe rerun in progress |
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
- ATI TEST.COM passes on the Graphics Ultra model (AT bed, `0d54a50fc`, watched 10-11).
- Register-level probes run on the real card and in 86Box (including an 82-test conformance set built from ATI's Windows 3.1 driver): pixels and read-backs match; SUBSYS_STAT's monitor-ID and vblank bits differ, which depend on the monitor and timing.
- Mach32 and IBM 8514/A: the same probe set before and after. The only differences are the two
  guide-documented fixes above.

Not tested: ATI's Windows drivers on this build (owner check pending); Mach8 cards other than the
Graphics Ultra; 16-bit slot operation.

(Owner: thank anyone who helped in the final text; do not post this draft as written.)

## Blocker found 10-11 evening: TEST.COM fails on the rebased build

- Fork base (`mach8-pr-prep`, `89942be5e`): TEST.COM passes in `vm_5160_now` and in the AT bed `vm_6695_w2`.
- Same Mach8 code on upstream `1741108a7` (`mach8-pr`, and `mach8-prdiag` with the diagnostics back):
  TEST.COM goes black in the AT bed (and in `vm_5160_now`, which also lacks fork-only machine code there).
- Logs `vm_6695_w2/tc_ref.log` (pass) vs `tc_up.log` (black): identical Mach8 traffic until TEST.COM enables the
  8514 side (`4AE8=07` at 06ACh, sub_069Bh). The fork then draws (sub_052Ah); upstream switches straight back
  (`4AE8=02` at 06CCh) without drawing.
- TEST.COM after enabling: DAC check (02EBh=A4, read 02ECh, expect A5), then 92E8h 5555 check; its delays read
  PIT channel 0 directly (sub_072Ch).
- Upstream touched no DAC-index code in vid_svga.c; it did change `pit.h` (+12), `vid_svga.h`, `video.h`.
- Not a PR-code fault in itself (probes pass on the rebased build), but the PR is blocked until explained.
- Next: log the IN values TEST.COM sees between 06ACh and 06CCh on both builds (one short diagnostic), or
  bisect upstream's 648 commits with TEST.COM as the test (owner-watched, ~10 runs).

## Resolved 10-11 late: the missing file was vid_svga.c

TEST.COM went black on the rebased build because the PR carried four files and the fork's fix in
`vid_svga.c` stayed behind: the Graphics Ultra has one DAC (INMOS IMSG176) behind the VGA and 8514/A port
sets, so a DAC index set on 3C7h/3C8h must read back on 02EBh/02ECh. Without it TEST.COM (797Eh) decides
the VGA has a separate pass-through DAC and sets 6AEEh bit 10, blanking the picture. Found by logging the
reads TEST.COM makes after ADVFUNC=7 on both builds (3DAh/02E8h diverged). `vid_svga.c` added (its
MACH8_VGADUMP diagnostic stripped); TEST.COM then passes on `0d54a50fc`.
