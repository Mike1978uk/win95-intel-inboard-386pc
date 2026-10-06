# Next session - 2026-10-06

Supersedes `next_session_2026_10_05.md` (still the record of the OFFLINE boot entry, SHADRAM v4 and the
three Inboard PRs, all merged).

## Done 2026-10-06

- **Sources:** VBEMP (NT only, closed - nothing here); velocity9x re-read for Andrew's VGA/8514 switching
  idea - live mode switching is same-depth only, GPL-3.0, Windows 98SE only. vmdisp9x (MIT, claims Win95)
  is the fallback source for the switching code. `docs/mach8_text_and_driver_plan.md` "Sources reviewed".
- **Owner messaged Michael Dale (velocity9x) on Reddit:** asked whether a Mach8 family on his framework
  under GPL-3.0 is welcome, and whether it runs on Windows 95. Await the reply before choosing the
  driver's framework and licence.
- **Andrew's #49 point (VGA framebuffer for DirectDraw, "memory twice as fast as I/O"):** answered by the
  2026-09-20 measurement - the Mach8's VGA memory is the slowest window on the machine (4.07-4.26 us/B
  reads vs 3.82 us/B word port writes). Writes into A0000h are still unmeasured. **Reply to Andrew owed.**
- **86Box Mach8 model (diagnostic tree `86box_3c509b`, local):** TEST.COM now passes Register, FIFO,
  RAMDAC and **Video RAM** in the bed; the ATI ROM's own self-test is back to `Testing........Ok` (the
  RAM Addressing draw broke when FIFO-test writes were queued - `78eeb22d5`). Test Sequence 1 bisected with
  the new **M8SEQ** against the real card: four more fixes, first difference moved from command 0 to
  command 16. Details and the commit table: `docs/ati_test_com_notes.md`.

## On the CF now

`D:\M8SEQ\` - `M8SEQ.COM`, `TEST.COM` (copy) and the real card's `M8SEQ.BIN` (also kept in
`docs/captures/2026-10-06_m8seq/`). Nothing else changed on the card.

## In the bed (`vm_magnaram_off/card.img`)

`AUTOEXEC.BAT` runs `C:\M8SEQ.COM` before `C:\TEST.COM`; the previous one is `C:\AUTOEXEC.M8B`.
Bed cycle script (scratch): launch, wait for M8SEQ's VGA restore, stop, extract, diff with
`tools/m8seq/m8seq_diff.py docs/captures/2026-10-06_m8seq/5160_M8SEQ.BIN <bed>.BIN TEST.COM`.

## Next, in order

1. **Polygon-boundary lines** (TS1 command 16 on): a probe that draws one polygon line at a time into a
   cleared area and reads the area back directly (no fold), run on the 5160 and in the bed. Needs one DOS
   boot. Evidence so far in the notes.
2. Then continue the M8SEQ loop in the bed; Test Sequence 2 (tables `65E0`, `64AC`, `5A10`) after TS1.
3. **M8TIME** (text-path timing on the 5160: ATI-style per-character writes vs string expand vs glyph
   cache, plus writes into A0000h) - not written yet.
4. Owed: reply to Andrew on #49 (with thanks); #49 status block still says "2026-10-05: measured and
   planned, not started" - draft a replacement for approval.

## Noted, not fixed

- `tools/movewin.ps1`: `$w` (the process) and the `$W` parameter are the same variable in PowerShell, so
  it fails. Not changed.
- The real-hardware `comrade` MCP server failed to connect at session start; the 5160 runs were by hand.
