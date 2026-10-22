# Next session - from 2026-10-22

Supersedes `docs/next_session_2026_10_21.md`. Card log and per-run table: `docs/captures/2026-10-22_card/`.

## State

- Card: `M8OX.DRV` selected (the -ox experiment, not adopted). Master builds the -oi driver; put it back
  with `tools\m8_card_cycle.ps1` before trusting any bench.
- CF restored from the 10-12 image and the 10-21 files put back (session start). `CONFIG.SYS`
  `menudefault=LEAN,5` for driver work (backup `C:\CONFIG.F22`): FULL stalls at `Initing sd120ppd.mpd`
  (LS-120, #46). Set it back to FULL when the driver work ends.
- V9x agent: `allowed_client=` blank in all three INI copies. Two uploads dropped once each; a retry
  worked.
- Card bench: `m8_card_cycle.ps1` now waits for `M8IDLE.EXE` (CPU at most 10 % for 10 s) and benches
  once. Every run today is LEAN against ATI's LEAN runs: like for like.

## Done 10-22 (card, pixels = ATI in all 35 throughout)

| | start | end | vs ATI |
|---|---|---|---|
| fill_hatch | 119/s | 388/s | 0.40x -> **1.30x** |
| polygon | 12.5/s | 54-56/s | 0.17x -> **0.75x** |
| ellipse | 43/s | 62-66/s | 0.59x -> **0.88x** |
| overall | | | 1.062x -> **1.14-1.16x** |

- Hatch and dithered brushes: pattern registers, 3 writes a row; opaque fills copy 8 rows down.
- Polygon/ellipse: GDI hands them over as `OS_POLYSCANLINE`, never as shapes. Dithered spans no longer
  go to the read-back fallback; one set-up per call; one-row spans through SCAN_TO_X (ATIM8
  `seg08:0dcd`); one loop over GDI's span list in `m8hw.c`; last brush remembered.
- Negative results: PATT_LENGTH bits 8/13/14 do nothing on the card (`M8PATT`); clearing GDIINFO polygon
  flags changes nothing; the span-bound trim is neutral; **-ox is neutral on the card (0.998x) and
  slows drag_frame 0.71x**, so CPU in our C is not where the card's time goes.
- 86Box gap: SCAN_TO_X ignores PATT_INDEX, so the bed shows polygon pixels wrong; the card is right.
  Fix in our fork when the bed is next worked on.
- Issues #51 (filesystem), #52 (WINDISK), #53 (GitHub survey leads) opened.
  `docs/mach8_unused_silicon_audit.md` lists every unused lever against the gaps.

## Next, in order

1. **Polygon, the last 0.25x.** 17.9 ms a polygon: ~7 ms port writes, ~0.5 ms FIFO polls; ~10 ms is
   not in our span code (-ox did not move it). Read ATIM8's `Enable` (`seg02:0711`): what it sets in
   GDIINFO `dpPolygonals`, and what DIBENG does with `OS_POLYGON` on ATI's screen device. If GDI's own
   scan converter is the cost, taking polygons ourselves is the lever. Hand assembly of the span loop
   is parked: measure first.
2. **Stretch (0.26-0.32x):** trace ATIM8 `seg14` (StretchBlt) and `seg12` (DibToDevice).
3. **Floodfill (0.34x):** check whether ATIM8 uses destination compare (EEEE); the docs disagree.
4. Smaller gaps: hatch single self-overlapping copy (ATIM8 `seg16:4470`) instead of doubling copies;
   setpixel/getpixel/cursor per-call cost.
5. Then the fun: DirectDraw, more colours, 3D (`docs/mach8_unused_silicon_audit.md`).

## Method notes learned today

- The card's time is port writes and engine waits, not our C: measure writes per operation before
  optimising code. Pad diagnostics (a throwaway build writing counters to SCRATCH_PAD_0/1, printed by a
  throwaway M8BENCH) found the polygon fallback in two bed runs.
- Read ATIM8's routine for the exact call GDI makes (Output style 5 = `seg08:0b96`) before designing.
