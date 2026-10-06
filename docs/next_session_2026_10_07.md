# Next session - 2026-10-07 (Mach8 TEST.COM bisect)

Goal unchanged (owner, 2026-10-06): the 86Box bed must pass ATI TEST.COM 100% before any driver work.

## Where it stands

- TEST.COM in the bed: Register, FIFO, RAMDAC, Video RAM pass. TS1 fails (14216 27DB 0016), TS2 fails (14204 0602 0004).
- TS1 bisect (M8SEQ `M8S4`, reference `docs/captures/2026-10-06_m8seq/5160_M8SEQ4.BIN`): ops 0-133 of 172 match the
  card; 39 of 173 checkpoints differ. Every fix and its evidence: `docs/ati_test_com_notes.md`.
- Diagnostic tree `86box_3c509b`: all committed, latest `03b909952`. Main repo: committed. Nothing pushed.

## Next, in order

1. **Op 134** (TS1 ops 133-140): one-row blits after writes to 92EEh and EAEEh, Mach8-only registers 86Box ignores.
   Read first: `WIN31ACC.EXE` (ATIMACH8\V1 and GFXULTRA) references EAEE at file offsets 0x247e and 0xe4fb.
   Then, if needed, M8ROW3 at checkpoints 133-137 (edit K_FIRST/K_LAST/AREA_Y; read area must cover y 4Eh-52h,
   x 74h-B4h - AREA_X is 20h, so move it).
2. Continue TS1 to 172, then point the same tooling at TS2's table.
3. Open, parked: DP_CONFIG bit 11 with source 6 (CA11h) - see the notes.

## Method that worked today

Read the Mach32 guide (`references/ati_mach32/guide.txt`) first; work the arithmetic from the fold where one op
differs; measure on the 5160 (M8ROW3 / small probes over COMrade) only where the guide is silent. After
`file_write` over COMrade, confirm the size with `DIR` before running - a timed-out write ran a truncated
M8FG7.COM once. Heredoc `\n` in python edits gets mangled: write edit scripts to files.
