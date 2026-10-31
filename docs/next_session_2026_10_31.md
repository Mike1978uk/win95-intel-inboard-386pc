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
