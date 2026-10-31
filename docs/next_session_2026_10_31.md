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

## Answered 10-31: GDI bitmap RAM is cached (card_pc31)

1 KB hashed on a second pass: a bitmap's bits 245-249 us, the driver's own data 247-248 us (both
stamp-inclusive), bitmaps at 4 KB pages 394-527 (~1.6-2.1 MB), driver data page 266: all above 1 MB,
all cached under CMLR F0. No Inboard finding. The lookup is slow because it hashes every call
(~0.16 us a byte cached, plus first-touch fills). First probe build (M8PCACHE) read 2 KB past a 1 KB
icon's selector limit: GPF at 0001:b2bd in bmp_icon (photos error_1-3), fixed by bounding the read.

Next for mem_to_mem: remember a lookup's result per source bitmap and rectangle until that bitmap is
touched (m8_px_touch) or deleted (RealizeObject) - the invalidation the proxies already rely on.

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
- **mem_to_mem, measured** (card_pm1/2, card_pn1/2; profile slots 39 = m8_px_m2m, 40 = copies, 41-44 =
  M8_PX_NO_ reasons): no card copy fails (41-44 = 0; the comparison doc's 30-80% failure is wrong).
  ~60% of calls return before counting (no proxies) and are RAM copies, ~1.1-1.2 ms a call in all.
  A card copy costs **~2.3-2.5 ms** a call - more than a RAM copy, which is all ATIM8 does. Next:
  a RAM copy when the source has no proxy, deciding what to do with the destination's proxy (a
  write-back is ~14 ms; drag_frame's 7x depends on proxy-to-proxy copies).
- `0d002e4` pixel write-through (owner: hardware over software): bmp_changing 8.15x ATI; 1.772x overall.
- **mem_to_mem split** (card_po1/2): every card copy's source is RAM (s37 = 0); `m8_bc_lookup` alone
  costs **~2.0 ms** a call (s38). Next (owner agreed): RAM source -> RAM copy as ATIM8, keeping the
  destination's proxy only when it pays (unfilled: drop; filled: write back once after N RAM copies).
- XFree86 sweep committed (`7d000cf`, `docs/xfree86_and_all_tests_sweep.md`): lines_dash is also below ATI
  (0.79-0.92); XFree86/ATIM8 wait for FIFO room where ours drains to empty (m8_fifo_drain).
- **Disproved (card_mm1-4, power-cycled after boot stall #4):** copying every RAM source in RAM
  gave mem_to_mem only +10% (0.92x ATI) and dropped **drag_frame 146 -> 21/s**: drag_frame's copies
  come from RAM through the bitmap cache, not proxy to proxy. Reverted. A mem_to_mem fix must keep
  the cache path for sources that recur (drag_frame) and skip it for one-off ones - e.g. the cache's
  seen-twice rule before paying the ~2 ms lookup, or a cheaper key than the full hash.
- Also no gain (card_bl1-4, reverted): copying the rows out only on a cache store: mem_to_mem +4%,
  drag_frame -9%, both in noise. The ~2 ms m8_bc_lookup is not the row copy. Next: time its parts
  (m8_bits_row + hash per row, the entries scan, seen-twice) in the profile build before any change.
- **m8_bc_lookup measured** (card_pb1/2, profile slots 35/36): the row loop (m8_bits_row + copy +
  hash) is ~1,750 us for mem_to_mem's 40x96 and ~520 us for draw_icon's 32x32 - ~18 us a row, not
  per byte; the entries scan is ~40 us. drag_frame copies from proxies (s37 ~210, lookups 1-3).
  Not the cause (card_br1-4, reverted): bottom-up rows taking m8_bits_row's selector path. Next:
  one call hashing all rows (asm walking the rows itself, one selector load), measured on the card.
- **Boot stall #5** right after #4 (owner: wallpaper + cursor again); a second power cycle cleared it.
  Two consecutive stalls: investigate before the next long card session (BOOTLOG, Explorer with ATIM8).
- (done) XFree86 + all-35 sweep agent was running at the limit: check for `docs/xfree86_and_all_tests_sweep.md`.
- Owner decided bmp_changing: write-through, done.
- (old) Owner decision owed: bmp_changing - keep the card copy current on SetPixel (doc item 2) changes
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
