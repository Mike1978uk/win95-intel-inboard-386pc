# Next session - from 2026-10-31

Supersedes `docs/next_session_2026_10_24.md` (its items 0, 2-4, 6b, 8 still stand; listed below).
Card log: `docs/captures/2026-10-31_card/m8_card_bench.txt`. Gate unchanged: >= ATIM8 in every test.

## State

- Card: `M8PIC4.DRV` (profile build) selected; next card cycle installs its own driver.
  master `6cd03ab` = 1.597x ATI over four runs (card_if1,2 + card_ifb1,2), pixels = ATI in all 35.
- `tools\m8_card_cycle.ps1` now always waits for M8IDLE and prints `idle:` (with -BenchOnly too).
  Read it before the numbers; bench twice.
- Profile build: ExtTextOut parts in styles 56-61 (key, DIB_ExtTextOutExt, bits lookup, mono
  expansion, plane blit, replay); BitBlt steps 50-55 (mono_blt, px_restore, upload+restore,
  px_touch, bc_draw, content path). One stamp (~83 us) per slot to subtract.

## Done 10-31

1. **Text key** (`43aefd5`): GDI passes TEXTXFORM bytes that change every call; the key hashed all
   19, so text_sys/same never hit. Now keyed on bold/italic/underline/strikeout (DDK STRBLT.ASM
   reads ftWeight only). text_sys 2.05x, text_same 2.23x, text_gui 2.15x ATI.
2. **Bits hash from a key's second sight** (`16f7498`): text_varied +1-5% (noise band).
3. **draw_icon** (`6cd03ab`): DrawIcon rewrites its scratch bitmap with the same bits, so the colour
   half was re-uploaded (2.9 ms) every call. A small bitmap (<= 4 KB) that just lost its proxy tries
   the bitmap cache first; eviction and deletion do not count as a rewrite. 0.84 -> 1.39x ATI.
4. Disproved: skipping the bits hash on a first-sight key with the old key (text_sys/same -25%).

## Late 10-31 (owner away)

- `b4020c3` the caches' hash in assembly (`hash.asm`, same hash): 1.620x ATI, pixels = ATI in all 35.
- **Boot stall #3**: restart into `M8PSET` (a profile build of master + timers) - ping answers, port
  9869 refused, as on 10-23. ~21 driver restarts today, one stall. Card needs a manual restart.
- Polygon split (profile, card_ps1): per Output call ~1,985 us = walk ~1,465 (32 spans, ~46 us a
  span, one drain per 8 spans) + **~440 us of set-up** x 6.2 calls = 2.7 ms a polygon, which is the
  whole gap to ATI. The set-up timers (s46 fill_begin_fast, s47 brush_colours, s48 scan_bound)
  are in the profile build but were never run (the stall). Suspect s48: the bench parks the cursor
  in the clip, so every call walks all records for a bound.
- **Branch `spans-cacheline` (`07db4d6`)**: span walk rewritten by cache lines (common path in seven
  aligned lines, ~10 refetched a record vs ~15-17). Bed: pixels = earlier bed run in all 35. Not yet
  on the card. Next: card-bench it twice against master; merge only if polygon/ellipse gain and
  nothing drops. Then run M8PSET's set-up timers.

## Evening 10-31: 1.652x ATI (29e3f3b, four runs), pixels = ATI in all 35

- Boot stall #3 on screen: wallpaper and cursor, no taskbar (as 10-23); a power cycle cleared it.
- `6b02fde` span walk by cache lines: polygon +7%. `863906f` rows before the box for the cursor
  test (scan_bound was ~190 us a polygon call): polygon 0.95x, ellipse 1.03x.
- `29e3f3b` DIB rows translated four pixels a step in `xlat.asm`, a band per `rep outsw`:
  dib_8bpp 0.83 -> 1.13x, stretch_dib +20%, stretchblt +14%.
- ATIM8 per-test comparison: `docs/atim8_vs_ours_lagging_tests.md` (polygon: ATI writes the clip into
  the hardware scissors once a call and does no software clipping; 6 port accesses a span).
- **First bench runs after a reboot read low** in some tests (dstinvert 1416/1610, then 1758/1787 on
  the same boot) although M8IDLE passed: judge a change on the later runs, or bench four.
- Still below (four runs): mem_to_mem 0.79, cursor_move 0.89, text_varied 0.90, bmp_changing 0.93,
  polygon 0.95; dstinvert ~0.98 on later runs. mem_to_mem outcome counters (slots 39-44) on the card.
- Owner decision owed: bmp_changing - keep the card copy current on SetPixel (doc item 2) changes
  what the test measures ("what a cache cannot help").

## Below ATI (four-run mean, 6cd03ab)

text_varied 0.89, dib_8bpp 0.82, polygon 0.83, mem_to_mem 0.75 (swings 0.66-0.94), bmp_changing
0.93, dstinvert 0.95; fill_small, setpixel, cursor_move, ellipse in the 0.87-0.98 noise band.

## Next, in order

1. **text_varied (0.89)**: per call ~128 us key hash + ~440 us DIB layout/fill + ~630 us mono
   expansion. Key hash in assembly (dword steps, no spills: `wdis` shows 13 instructions a word);
   ATIM8's layout cost is the same as ours, so the expansion and set-up are what is left.
2. **spans.asm** (polygon, ellipse) by the cache-line model - handoff 10-24 item 6b.
3. **dib_8bpp, short paths** in assembly (fill/copy/pixel).
4. **mem_to_mem**: RAM copy vs card path, measured (10-24 item 4).
5. **bmp_icon after the first bench run**: ~620/s vs ~720/s (still 2.26x ATI). The profile shows
   the same path and no more driver time; suspect code layout (cache lines). Check with item 2's work.
6. Still open from 10-24: boot stall (item 0), the L1 on/off `rep insw` timing owed to Andrew (item 8).

## Retrospective check (owner, 10-31: apply each win across all 35)

- Volatile bytes in a cache key: only three keys exist. Text key fixed; bitmap cache hashes
  visible pixels only; the mono/string cache hashes whole rows incl. pad bits (widths not a
  multiple of 16) - on no lagging test.
- Re-upload of a rewritten-but-identical bitmap: s52 (uploads) is frequent only in draw_icon and
  bmp_changing (content really changes, 6.8 KB, outside the 4 KB limit).
