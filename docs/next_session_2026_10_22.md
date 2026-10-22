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

## Evening 10-22 (card, LEAN, pixels = ATI in all 35 throughout)

| | morning | evening | vs ATI |
|---|---|---|---|
| floodfill | 1.4/s | 2.7/s | 0.34x -> 0.66x |
| getpixel | 1357/s | 1918/s | 0.62x -> 0.86x |
| setpixel | 1443/s | 1696/s | 0.70x -> 0.82x |
| cursor_move | 488/s | 612-690/s | 0.47x -> 0.59-0.66x |
| stretch_dib / stretchblt | 2.9 / 3.6 | 4.2 / 5.6 | 0.32/0.27 -> 0.46/0.41 |
| overall | | | **1.206x ATI** (`a72e18f`) |

- ScanLR and GetPixel read with SCAN_TO_X as ATIM8 does (seg07:0147, 00aa; probe `M8SCANR`: DP_CONFIG
  5210h gives two pixels a word, low byte first). ATIM8's answer is the pixel's own x either way.
- Write-back sends a row equal to the one above as an engine copy (stretch 1.5x).
- Cursor masks live off-screen as bit-planes at (992, 960); a move is 4 engine ops.
- Negative: waiting for free FIFO slots per span (0.89x); stretch read-back skip alone (no change).
- BSOD 0D at 0137:BFF712C5 (KERNEL32) once during an agent restart: count it if it repeats.

## Next, in order

1. **Cursor (0.59-0.66x):** one keyed copy instead of two plane blits for cursors with no inverted
   pixels: source colour compare (92EEh bits 3-5 = 20h, EAEEh key; true takes the background mix; an
   engine reset does not clear 92EEh, so write 0 back). Extended blit as `tools/m8seq/M8SCMP.ASM`. Measure
   the per-move driver time first: USER's share of SetCursorPos is unknown.
2. **Stretch (0.41-0.46x):** trace ATIM8 `seg14`; the write is the cost.
3. **dib_8bpp (0.60x), mono_bmp, text_varied (0.65x):** measure writes and CPU per call first.
4. Polygon (0.75x): per-call fixed cost; ellipse 0.76x, rectangle 0.80x.
5. Then DirectDraw, more colours, 3D (`docs/mach8_unused_silicon_audit.md`).

## Method notes learned today

- The card's time is port writes and engine waits, not our C: measure writes per operation before
  optimising code. Pad diagnostics (a throwaway build writing counters to SCRATCH_PAD_0/1, printed by a
  throwaway M8BENCH) found the polygon fallback in two bed runs.
- Read ATIM8's routine for the exact call GDI makes (Output style 5 = `seg08:0b96`) before designing.
