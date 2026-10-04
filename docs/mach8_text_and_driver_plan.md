# Mach8 display driver: less bus traffic for text (and later bitmaps) - plan, 2026-10-05

Goal (owner): a display driver kinder to the 8-bit bus than ATI's `ATIM8.DRV`, so more bus time is
left for everything else. First target: text, via a glyph cache in spare card memory.

## What is measured

`TXTBENCH` on the 5160 (800x600x8, Small Fonts) - `docs/txtbench_2026_10_04.md`:
text ~81 us per character, blits ~16 MB/s and fills ~31 MB/s on the card. Wider glyphs cost more.

## What ATI's text path does (read 2026-10-05, `vxd-patches/ati/ATIM8.DRV`)

NE driver, 19 segments. `EXTTEXTOUT` is ordinal 14, segment 1 offset `15AFh` (`STRBLT` forwards into
it). Native Mach8 registers throughout (`xxEEh` ports). Per string: FIFO wait, clip box
(`DAEE/DEEE/E2EE/E6EE`), `DP_CONFIG=2211h` and colours, opaque background box; then `DP_CONFIG=3251h`
(colour-expand from the CPU). Per character (`17BAh`-`1865h`):

- 5 register writes: `86E8` cur X, `A6EE` dest X start, `AAEE` dest X end, `82E8` cur Y, `AEEE` dest Y
  end (starts the operation);
- the glyph's 1-bpp bits, packed 32 at a time, two word writes to `E2E8` per 32 bits (8x16 glyph = 8);
- a FIFO status read every few characters.

About 13 port writes per 8x16 character (~50 us of the 81 us); the rest is GDI. ATI already packs bits
and batches FIFO checks.

Bitmap paths (`SetDIBitsToDevice`/`BitBlt` from memory) live in segments 11-13 (28/28/18 `E2E8`
references, 32/32/18 `rep outsw`). That is what a Windows game hits, not text.

## The glyph cache

First draw of a glyph: upload its 1-bpp mask once to off-screen card memory. Later draws: a card-side
colour-expand blit from that mask - source X/Y plus the same 5 destination writes, ~7 per character
instead of ~13. Estimate: ~30% less bus traffic for text; more for large fonts (the saving grows with
glyph area). A full-screen text repaint ~0.3 s -> ~0.2 s.

## Open questions, in order

1. **Off-screen memory map.** At 800x600 there are ~424 spare lines of 1 MB. Which does ATI already use
   (cursor save, brush cache, save-under)? Read `ENABLE` (ordinal 5) and the cursor/brush code.
2. **Mono source from card memory.** Can the Mach8 colour-expand from a bit-plane in off-screen memory
   (`DP_CONFIG` mono source = video memory, `RD_MASK` selecting the plane)? Confirm against the register
   docs before designing storage. 8 glyph masks can share one 8-bpp area, one per plane.
3. **Route.** (a) Binary-patch `ATIM8.DRV`: a new NE segment for the cache code, `EXTTEXTOUT` jumping
   into it. (b) Our own driver: the Win95 DDK has an accelerated mini-driver sample over the DIB engine,
   `Windows95_ddk\DISPLAY\SAMPLES\MINI\XGA`, a template for a Mach8/8514 driver built with 2026 design
   choices. (b) is bigger but owns every path, including bitmaps. Needs a 16-bit linker - not found in the
   DDK yet (only the 32-bit `MSVC20\LINK.EXE`).
4. **Cache bookkeeping:** key by realized font + character; invalidate on font free, mode change, and
   whenever something else draws into the reserved area.

## Testing

Only on the 5160: 86Box's Mach8 model does not run ATI's accelerated mode (Windows falls back to VGA in
the bed, so the `MACH8_COUNT` counter saw nothing). Each test: `TXTBENCH` before and after, plus a look
at the desktop. Keep stock `ATIM8.DRV` beside any test build; a broken text path is recovered by
restoring it from DOS. Extend `TXTBENCH` with a bitmap test (`SetDIBitsToDevice`) before touching that path.
