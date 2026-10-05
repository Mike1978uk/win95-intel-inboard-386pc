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
   choices. (b) is bigger but owns every path, including bitmaps. 16-bit linker: MASM 6.11's segmented
   `LINK` (what holzachr builds the XGA sample with) or Open Watcom v2 `wlink` (velocity9x) - see below.
   ⚠ The DDK mini-driver model assumes the DIB Engine can draw into VRAM through a pointer. The Mach8
   has no aperture, so (b) means either a full driver like ATI's or a system-RAM shadow of the screen
   (800x600x8 = 469 KB locked; every software-drawn pixel crosses the bus once on upload).
4. **Cache bookkeeping:** key by realized font + character; invalidate on font free, mode change, and
   whenever something else draws into the reserved area.

## Testing

Only on the 5160: 86Box's Mach8 model does not run ATI's accelerated mode (Windows falls back to VGA in
the bed, so the `MACH8_COUNT` counter saw nothing). Each test: `TXTBENCH` before and after, plus a look
at the desktop. Keep stock `ATIM8.DRV` beside any test build; a broken text path is recovered by
restoring it from DOS. Extend `TXTBENCH` with a bitmap test (`SetDIBitsToDevice`) before touching that path.

## Sources reviewed 2026-10-05

- **holzachr, `xga-win9x`** - <https://github.com/holzachr/xga-win9x>. The Win95 DDK XGA mini sample,
  extended for XGA-2 (>1 MB, 16 bpp) and built. Gave: a working 16-bit build recipe (`buildall.bat`:
  MASM 6.11 + MSVC 1.5x/2.0, DDK `master.mk`), and the sample's text path (`MINI/STRBLT.ASM`):
  `ExtTextOut` jumps to `DIB_ExtTextOutExt`; the DIB Engine lays out the string into one 1-bpp buffer
  and calls back `DrawTextBitmap`, which copies it to off-screen memory and colour-expands once per
  string. No glyph cache. Depends on the XGA aperture. Also `XGA.NT35` (NT 3.5 XGA driver). Nothing on
  the Mach8 or 8514/A. He is a source, not a contact.
- **michaeldale, `velocity9x`** (Andrew, #49) - <https://github.com/michaeldale/velocity9x>. Win98,
  linear-framebuffer cards (Mach64 via VBE). Gave: the 8514/A-family colour-expand sequence for
  `DrawTextBitmap` on the Trio64 (`docs/decisions/2026-09-06-gdi-accel-005-text.md`: `PIX_CNTL=A080h`,
  `CMD=53B3h`, `rep outsw` to `E2E8`), which the Mach8 shares at the 8514/A level - check bit 9/12
  meanings against the Mach32 guide; Open Watcom as a 16-bit toolchain; and "`DP_MONO_SRC_BLIT` glyph
  caching in off-screen VRAM" listed as a Mach64 win - the same idea as ours. Not usable as code here:
  Mach64 is not register-compatible with the Mach8, and the driver needs an aperture.
- **Ardent Tool** (MCA community): <https://www.ardent-tool.com/video/8514.html> - an 8514/A register
  summary (`8514A_Registers.pdf`, from MIPS 1990; nothing beyond the Mach32 guide). `8514_Experience`
  and `8514A_Standard_Who_Wants_It` - history only (no aperture, AI too slow, Microsoft drove the
  registers directly). `WD_9510_MC` - Paradise 8514/A Plus, no programming detail.
  `ATI_mach8_Drivers` - Win95's bundled driver is ATI's last; `M8WR30.ZIP` (Win 3.1, v3.0, 07-94).
- **ATI 8514/ULTRA Installation Guide v1.0** (10-1990), <https://ardent-tool.com/video/ATI_8514_Ultra_Manual.pdf>:
  works in 8- or 16-bit slots; "8088 and 8086 systems: not supported"; 512 KB boards run a different
  "minimum mode"; `TEST.COM` runs the draw-engine tests. The v2.1 PDF is scanned (no text layer).
- **Not found anywhere above:** which off-screen memory ATI's driver uses, and whether the Mach8 can
  colour-expand from card memory. Questions 1 and 2 stay open.

## Two string-level options from the DDK pattern

Per string instead of per character: one colour-expand of the DIB Engine's buffer through `PIX_TRANS`
removes the 5 per-character register writes (~8 writes per 8x16 character, against ATI's ~13). The glyph
cache removes the bits instead (~7). They combine only if the Mach8 can expand from card memory.

## Cheap test from Andrew's M8UTL

`TEST.COM` in `M8UTL.ZIP` (<https://www.ardent-tool.com/video/ATI_mach8_Drivers.html>) runs ATI's
draw-engine tests. Andrew sees most 8514 command tests fail in 86Box. Running it on the 5160 gives the
reference result, and the failing tests name what the emulator's Mach8 model lacks - the reason ATI's
accelerated mode does not run in the bed.
