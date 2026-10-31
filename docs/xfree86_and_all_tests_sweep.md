# XFree86's Mach8 server against ours and ATIM8: the lagging tests, then all 35

Static reading, 2026-10-31. Nothing here was built or run. XFree86 3.3.6: `references/xfree86_336_mach8/`
(`mach8/`, `ibm8514/`, `cache/`). Ours: master `cdd5a8a` (`drivers/mach8_display/src`). ATIM8: taken from
`docs/atim8_vs_ours_lagging_tests.md` and `docs/atim8_disassembly.md`, not re-read.
Ratios (ATI / ours, ms/op) are `card_ati31` against `card_xl1,xl2,xlb1,xlb2` (10-31, `29e3f3b`) in
`docs/captures/2026-10-31_card/m8_card_bench.txt`. ATI is one run; +-8% is noise.

Cost model: a port access ~7.6 us and flushes the L1; then each 16-byte line touched ~1.3 us.

## What XFree86 does everywhere

- **FIFO: room, never empty.** `WaitQueue(n)` (`ibm8514/reg8514.h:253`) spins on GP_STAT until n entries are
  free; the Mach8 assembly uses EXT_FIFO_STATUS masks the same way (`mach8/mach8imwrt.s:80` 0x80 = 9 free, `:207`
  0x2000 = 3 free, `:189` 0x8000 = 1 free). `WaitIdleEmpty` appears only before reads. One status read per
  command, polled before every command - more reads than ours, but it never waits for the engine to finish.
  ATIM8 does the same (`9AEE & F800`, free-slot budget). **Ours drains to empty**: `m8_fifo_drain`
  (m8hw.c:125) spins until EXT_FIFO_STATUS = 0 whenever the count runs out, and every uncounted routine starts
  with it (`m8_wait_fifo_empty`: plane_blit 1483, copy_keyed 1520, plane_mix 1557, polyline 1396,
  pattern_begin 572). A drain behind a queued blit waits for that blit's engine time.
- **Register state across commands.** The glyph loop (`mach8/mach8fc.c:Domach8CPolyText8:66`) writes CUR_Y once
  per font block (`:129`) and MAJ_AXIS_PCNT / MIN_AXIS_PCNT only when the glyph size changes (`:149-157`) - it
  relies on CUR_Y, MAJ_AXIS and MIN_AXIS surviving a BITBLT. Ours re-sends coordinates, MIN_AXIS and CMD on every
  command (m8hw.c:178 comment; `m8_copy_q` 1015). Not card-measured.
- **Pixmaps live in RAM.** Pixmap-to-pixmap is `cfbCopyArea` (`mach8blt.c:mach8CopyArea:95`); no off-screen
  pixmap cache except tiles, stipples and fonts (`cache/xf86bcache.c`, `xf86fcache.c`).
- **Cursor is software** (`mach8/mach8.c:629` `miDCInitialize`): save-under through GetImage/PutImage, i.e.
  screen reads over the bus. Worse than ours and ATIM8's; nothing to take.
- **Hardware features it uses that ATIM8 does not:** mono expansion from a VRAM plane (`RD_MASK` plane select +
  `PIX_CNTL MIXSEL_EXPBLT`) for glyphs and stipples (`mach8fc.c`, `mach8frect.c:mach8CImageStipple:482`);
  tile/stipple replication by doubling blits (`mach8frect.c:DoCacheExpandPixmap:95`). We already have plane
  expansion (`m8_hw_plane_mix`, the cursor). Neither uses polygon fill (CMD 5), short strokes or colour compare.

## Part A - the lagging tests

### mem_to_mem (0.63-0.86) - BitBlt 40x96 memory -> memory

XFree86: `mach8CopyArea:94-96` - neither drawable a window -> `cfbCopyArea`, a RAM copy. No port access.
Same as ATIM8 (`DIB_BITBLT`).
Ours: `m8_px_m2m` (screen.c:2012) when the destination (bmp_card2) is held on the card: `m8_px_find`,
`m8_px_pieces`, `m8_px_touch_rect` (source), `m8_bc_lookup` (hash ~3,840 B, ~0.3-0.4 ms), one 8514 copy (~10
writes). Card: no copy fails; ~60% of calls are RAM copies at ~1.1-1.2 ms, the card path ~2.3-2.5 ms. The
hash explains ~0.4 ms of the card path's extra ~1.2 ms; **the rest is unknown** (candidates: a first-sight
store in `m8_bc_lookup` (an upload), a slot eviction in `m8_px_pieces` -> `m8_px_new`, `m8_enter`).

Proposals:
1. **XFree86/ATIM8's rule: a RAM source goes to RAM.** If the source has no proxy (`ps == NULL`), write the
   destination's overlapping proxy back once (~14 ms for 71x96, then never again while the test runs, ~36 us/op
   over ~390 ops) and return 0 to `DIB_BitBlt`. Proxy-to-proxy copies (drag_frame) untouched. Saves
   ~0.4 x 1.2 ms = ~0.5 ms/op -> ~1.0x ATI. Confidence: medium-high (matches both references; a write-back on a
   RAM-sourced copy is a code path that already exists: `m8_px_touch_rect` with `M8_PX_WHY_MEM`).
2. Beyond ATI, only if the card path's unknown ~0.8 ms is removable: skip the hash when the source RAM is
   unchanged since it was hashed (a per-bitmap generation bumped wherever `m8_px_touch` runs). A card copy is
   then ~10 writes + CPU, ~0.2-0.3 ms against DIBENG's ~1.1 ms. Up to ~1.4-1.6x. Confidence: low - time the card
   path's parts first (one profile slot around `m8_bc_lookup`'s store and one around `m8_px_pieces`).

### cursor_move (0.88-0.95) - SetCursorPos

XFree86: software sprite (above) - no technique to borrow.
Ours: `MoveCursor` (screen.c:4738) -> `m8_cursor_hide` (copy save->screen, 8514 BITBLT) + `m8_cursor_show`
(copy screen->save, then `m8_hw_copy_keyed` m8hw.c:1515: drain, 5-6 set-up writes, 15 writes). ~34 writes.
ATIM8: ~78 port operations, yet ahead by 40-100 us. **Why: unknown.**
Static candidate: `m8_hw_copy_keyed` opens with `m8_wait_fifo_empty`, which waits until both queued 32x32 copies
have left the FIFO - i.e. until the hide copy has run in the engine. ATIM8 and XFree86 only wait for room.
Proposals:
1. Room-for-n wait (EXT_FIFO_STATUS mask, bit (16-n) clear) in place of the drain at the head of
   `m8_hw_copy_keyed`, `m8_hw_plane_mix`, `m8_hw_plane_blit`: saves the engine time of the copy ahead,
   est. 20-60 us/move (~2-6%). Confidence: low (engine time of a 32x32 VRAM copy not measured; the
   "uncounted routine" invariant (m8hw.c:103) must be kept by counting their writes instead).
2. Shadow MAJ_AXIS_PCNT / MIN_AXIS_PCNT for 8514 commands (both copies are the same size): -2 to -4 writes a
   move, 15-30 us. Confidence: medium on XFree86's evidence; needs one card probe (does an extended blit or
   LINEDRAW change them?).
3. Before either: the MoveCursor profile slot the comparison doc asked for.

### text_varied (0.82-0.99) - TextOut "Item n of m", ANSI_VAR_FONT, opaque, a new string every call

XFree86: `xf86ImageText8` (`cache/xf86text.c:186`) fills the background with one `PolyFillRect`, then
`mach8GlyphWrite` (`mach8fc.c`): 5 set-up writes, 4 scissors, then per glyph `WaitQueue(6)` + CUR_X,
DESTX_DIASTP, DESTY_AXSTP, [MAJ_AXIS], [MIN_AXIS], CMD BITBLT with PIX_CNTL = EXPBLT and RD_MASK = the
font's plane: **4 writes + 1 read a glyph**, glyphs expanded from 1-bpp planes in off-screen VRAM, uploaded
32 at a time on first use (`cache/xf86fcache.c:xf86loadFontBlock:222`). No DIB Engine layout at all.
ATIM8: DIBENG lays the string out (~440 us), then one mono expansion (~12 writes + ~90 words).
Ours: key (~100 us) + DIBENG layout/fill (~440 us) + `m8_mono` (~620 us incl. a trailing full idle wait).

Proposals:
1. **Glyph cache in VRAM planes (XFree86's method).** Per string: opaque fill (~5 writes), set-up (~8),
   ~12 visible glyphs x 4 writes = ~65 port operations, ~0.5 ms, plus ~0.1 ms CPU (one glyph table line a char).
   Removes the DIBENG layout that ATIM8 also pays. est. -0.4 to -0.5 ms/op -> ~1.15-1.25x ATI; first-sight text in
   every application gains. Fixed cell width per font (max width, background mix = leave) keeps MAJ_AXIS
   constant. Restrict to: no xform, no char/break extra, raster font <= 32 px, single clip rect via scissors;
   else today's path. Confidence: low-medium - plane expansion is card-proven (cursor, M8CONF), but CUR_Y
   persistence and exact GDI glyph placement (lpDx, overhang, opaque rect vs text box) must match pixel for pixel.
2. Lower-risk step to the same saving's larger half: lay the 1-bpp string out ourselves in assembly from the
   font's glyph bits (~12 glyphs x 13 rows of byte ORs into a ~180-byte buffer, ~30 lines touched, ~60-80 us)
   instead of `DIB_ExtTextOutExt` (~440 us), then today's single `rep outsw`. est. -0.3 ms/op -> ~1.05-1.1x.
   Confidence: medium (no new hardware state; the layout rules are the risk).
3. End `m8_mono` with a room wait instead of the full idle wait (both references): -15-30 us. Low.

### polygon (0.96-0.97) - Polygon, 6 random points, DKGRAY/LTGRAY brush

XFree86: `miFillPolygon` -> `mach8SolidFSpans` (`mach8fs.c:109`): `miClipSpans` in software, 3 set-up writes
(FRGD_COLOR, FRGD_MIX, WRT_MASK), per span `WaitQueue(5)` + 5 writes (CUR_X, CUR_Y, MAJ_AXIS, MIN_AXIS 0,
CMD RECT), 2 restores. 6 port operations a span; set-up ~4.
ATIM8: `DIB_OUTPUT` -> scanline callback, scissors once, ~6 port operations a span.
Ours: `spans.asm` 3 writes a span (CUR_Y on row change, CUR_X, SCAN_TO_X) - ahead per span - plus ~440 us of
set-up per Output call x 6.2 calls (s46-s48; s48 cut by `863906f`).
Proposals:
1. XFree86's set-up is 4 port operations and no per-call CPU beyond the clip. Cache the brush colour result
   (`m8_brush_colours`) and the data path by (brush, DRAWMODE) across the 6.2 calls of one polygon, so calls
   2-6 skip s46/s47 entirely: est. -0.2 to -0.3 ms/call x 5 = -1.0 to -1.5 ms/op -> ~1.05-1.1x. Confidence:
   medium on direction, size unknown until s46/s47 are timed (M8PSET never ran).
2. Beyond both references: SCAN_TO_X's fill flag (card-proven, M8PF) makes a scanline with k spans
   CUR_Y + CUR_X + 2k-1 writes; for these 6-point polygons most rows have 1-2 spans, so ~0-1 write a row.
   Small. Hardware polygon fill (CMD 5 / PIX_CNTL poly fill, `mach8_card_coverage.md:51`): one boundary pass
   + one fill blit, ~30-40 writes a polygon against ~200 now, but needs PC_POLYGON caps and GDI's fill rule
   pixel for pixel. Large saving (est. several ms/op), confidence low, cost high.

### dstinvert (0.85-1.07) - PatBlt 150x100 DSTINVERT

XFree86: `mach8PolyFillRect` FillSolid (`mach8frect.c:699`) with alu GXinvert: 3 set-up, `WaitQueue(5)` + 5
writes, 2 restores (~11 operations). ATIM8: ROP micro-op list (not traced). Ours: short path, one fill, 5
writes. Ahead of both on ports; the low readings are first-run-after-boot. Proposal: none beyond the MAJ/MIN
shadow (150x100 every call: -2 writes, ~15 us, ~2.5%). Confidence medium (probe as above).

### fill_small (0.94-1.01) - CreateSolidBrush, PatBlt 16x16, DeleteObject

XFree86: as dstinvert, ~11 operations. Ours: `m8_hw_fill_fast` 5-6 writes. ATIM8 ~10.
Proposals: (1) MAJ/MIN shadow: 16x16 every call, 5-6 -> 3-4 writes, ~15 us (1%). (2) The RealizeObject thunk
(thunks.asm:61) calls `M8PXREALIZE` for every object; only `-OBJ_BITMAP` and `OBJ_BRUSH`/`BS_PATTERN` do
anything (screen.c:1669-1684). Test iStyle in the thunk and jump straight to DIBENG otherwise: one far call,
`__loadds` and ~6-10 lines saved per realize, 2 realizes an op, ~20-30 us/op. Also applies to lines_dash
(pen realize + delete). Confidence: medium on direction, low on size.

## Part B - all 35 tests

Ratio = ATI/ours, four runs. "XF" = XFree86. "Idea" = one not yet used that could widen or secure the lead.

| # | test | ratio | ours | ATIM8 | XF | idea |
|---|---|---|---|---|---|---|
| 1 | text_sys | 2.1 | string cache (VRAM copy) | DIBENG layout + 1 expansion | glyph cache, 4 wr/glyph | none (cache hit) |
| 2 | text_same | 2.3 | string cache | same | same | none |
| 3 | text_gui | 2.1 | string cache, transparent | same | same | none |
| 4 | text_varied | 0.82-0.99 | key + DIBENG + mono | DIBENG + mono | glyph cache | **glyph cache / own layout (A)** |
| 5 | fill_small | 0.94-1.01 | fill_fast 5-6 wr | ~10 | ~11 | MAJ/MIN shadow; Realize thunk |
| 6 | fill_large | 0.97-0.99 | one fill | one fill | one fill | none: engine-bound (600x440); check the colour/realize path only |
| 7 | fill_hatch | 1.25 | pattern regs | per-scanline mono blit | stipple cached in plane, 1 expand blit/box | XF's: hatch once into a VRAM plane, then one EXPBLT per rect (~10 writes vs per-row) |
| 8 | fill_xor | 1.06 | pattern fill | PATT_DATA | tile cache + doubling blits | none |
| 9 | dstinvert | 0.85-1.07 | one fill, 5 wr | micro-ops | ~11 | MAJ/MIN shadow |
| 10 | blit_small | 1.2 | 8514 copy | ext. blit | 7 wr + read | MAJ/MIN shadow (32x32) |
| 11 | blit_large | 0.97-1.0 | 8514 copy | ext. blit | same | none: engine-bound |
| 12 | scroll | 1.03 | copy | copy | CopyWindow, 7 wr/rect | none: engine-bound |
| 13 | bmp_icon | 2.0-2.7 | bitmap cache | stream | ImageWrite stream | none |
| 14 | bmp_card | 11+ | bitmap cache | stream | stream | none |
| 15 | bmp_changing | 0.83-0.91 | proxy drop + upload + copy | one stream | one stream (`mach8imwrt.s`) | in hand (owner's call) |
| 16 | bmp_large | 32 | bitmap cache | stream | stream | none |
| 17 | dib_8bpp | 1.09 | asm translate, band outsw | asm 4px + outsw | n/a (no DIB) | DIB content cache (comparison doc #1) |
| 18 | stretch_dib | 1.9 | own path | seg14/DIBENG | n/a | none |
| 19 | stretchblt | 1.7 | own path | seg14 | n/a | none |
| 20 | mono_bmp | 3.2 | cached / mono | host expansion | CopyPlane -> host stipple | none |
| 21 | draw_icon | 1.3-1.55 | cache retry on rewrite | DIBENG + stream | n/a | none |
| 22 | scr_to_mem | 13 | proxy (no read-back) | rep insw + DIBENG | ImageRead | none |
| 23 | mem_to_mem | 0.63-0.86 | card copy via hash | RAM | RAM (`cfbCopyArea`) | **RAM source -> RAM (A)** |
| 24 | drag_frame | 8.5 | proxy-to-proxy copies | RAM + streams | RAM + streams | keep #23's change off proxy sources |
| 25 | lines | 1.3 | LINEDRAW 2 wr/pt | FEEE ~3 wr + read | LINEDRAW 2 wr/pt | none |
| 26 | lines_hv | 1.15 | SCAN_TO_X / LINEDRAW | CAEE | LINEDRAW | none |
| 27 | lines_dash | **0.79-0.92** | PATT_DATA 32-bit + LINEDRAW | same | per dash 2 wr (MAJ + CMD) | below ATI in all four runs (brief lists it >= 1.0): the line costs the same as ATI's; candidates are the pen realize/delete (Realize thunk) and `m8_hw_polyline`'s drain at 1396. Profile first |
| 28 | rubber_band | 1.1-1.3 | outline, mix NOT | DIBENG output | miPolyRectangle | none |
| 29 | setpixel | 0.92-1.09 | fill_fast 1x1, 5-6 wr | 1 read + 7 wr | 1-point LINE: CUR_X, CUR_Y, CMD (`mach8plypt.c:104-119`) | XF's point: MAJ_AXIS=0 shadowed, 3 wr + colour = 4 vs 5-6 (~10 us, ~2%) |
| 30 | getpixel | 0.93-1.11 | SCAN_TO_X read | same | ImageRead | none |
| 31 | polygon | 0.96-0.97 | spans 3 wr/span, ~440 us set-up/call | ~6/span | ~6/span, 4 set-up | **set-up cache across calls (A)**; CMD 5 long-term |
| 32 | rectangle | 1.15-1.23 | fill + outline | DIBENG | miPolyRectangle + FillRect | none |
| 33 | floodfill | 1.0-1.02 | ScanLR by SCAN_TO_X reads | same | n/a (mi, CPU) | none found; per-call fixed cost as polygon |
| 34 | cursor_move | 0.88-0.95 | 3 engine ops, ~34 wr | ~78 ops | software | **room wait; MAJ/MIN shadow; profile first (A)** |
| 35 | ellipse | 0.94-1.11 | spans + LINEDRAW outline | scissors + 5 wr/span | FillSpans 6/span | polygon's set-up cache (3.8 calls/op) |

## Unknowns that block a number

- The card path of mem_to_mem: ~0.8 ms beyond the hash, unattributed.
- Why cursor_move trails with fewer port operations; the engine time of a 32x32 VRAM copy.
- Whether CUR_Y / MAJ_AXIS_PCNT / MIN_AXIS_PCNT survive 8514 and extended commands on our card (XFree86's glyph
  loop says CUR_Y and the axis counts survive a BITBLT; one probe settles it for all the shadow proposals).
- s46 / s47 polygon set-up timers (M8PSET never ran).
- lines_dash: where its ~0.2 ms goes.

## Ranking (expected saving per op)

| # | test | proposal | saving | confidence |
|---|---|---|---|---|
| 1 | polygon (+ellipse) | brush/data-path set-up cached across the Output calls of one figure | ~1.0-1.5 ms (-> ~1.05-1.1x) | medium direction, size unknown |
| 2 | mem_to_mem | RAM-sourced copy -> write the destination back once, then DIBENG | ~0.5 ms (-> ~1.0x) | medium-high |
| 3 | text_varied | glyph cache in VRAM planes (or, first, own asm layout) | ~0.4-0.5 ms (~0.3 ms) | low-medium (medium) |
| 4 | cursor_move | room-for-n wait in place of drain; MAJ/MIN shadow | 35-90 us | low / medium |
| 5 | fill_small, setpixel, dstinvert, blit_small, lines_dash | MAJ/MIN shadow (one probe) + Realize thunk filter | 10-40 us each (1-3%) | medium |
