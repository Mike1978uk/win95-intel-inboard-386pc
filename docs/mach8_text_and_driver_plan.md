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
2. **Mono source from card memory - ANSWERED 2026-10-05: yes, on the Mach8.** ATI's *Programmer's Guide to
   the mach32 Registers* (REG688000-15, local copy `references/ati_mach32/`, not vendored) marks `RD_MASK`
   (AEE8) "Mach 32 | 8514/A | Mach 8" and `DP_CONFIG` (CEEE) "Mach 32 | Mach8". `DP_CONFIG[6:5]` MONO_SRC
   = 3 is "VRAM blit source"; each source pixel P counts as foreground when `(P | ~RD_MASK) == FFh`, so one
   `RD_MASK` bit selects a plane and **an 8-bpp area holds eight glyph sets**. ATI's text path writes
   `DP_CONFIG = 3251h` (MONO_SRC = 2, pixel transfer); the cache changes that field to 3 and adds the blit
   source. Unverified on the real card - first hardware test is one cached glyph.
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

**First hardware step: `tools/m8glyph/` (`M8GLYPH.COM`).** From a DOS boot it measures the `RD_MASK`
bit-to-plane map on all eight bits and expands one glyph from plane 5 with the rotated and the plain
mask, reading every result back through `PIX_TRANS` into `M8GLYPH.BIN`. All in Mach8 memory below
line 900; the screen does not change. Decode with `gen_m8glyph.py decode`.

**86Box bed, 2026-10-05** (`vm_magnaram_off`, `mach8_vga_isa`): every check passed - detection, both
controls, no timeouts; the bits map rotated (bit 0 -> plane 7) and only the rotated mask draws the F.
Capture `docs/captures/2026-10-05_m8glyph/`. This agrees with the documents by construction: 86Box
hard-codes the same rotation (`vid_8514a.c`, `rd_mask` rotate in the accel setup). Only the 5160 can
show the card does it; a byte-identical result there is what makes the bed trustworthy for this path.

**Real 5160, 2026-10-05: the same.** Rotated map, glyph drawn with mask `40h` only, both controls OK, no
timeouts. The result file matches the bed's in all 784 bytes but one: `SUBSYS_STAT` reads `A8` on the card
and `AA` in 86Box - bit 1, `PICKFLAG`, an interrupt status the model leaves set after reset. The glyph
cache does not use it. **So 86Box models the engine path the glyph cache needs: develop the cache in the
bed, time it on the 5160.** Question 2 is answered on hardware; question 1 (ATI's own off-screen use) only matters for a patch to `ATIM8.DRV`.

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
  the Mach8 or 8514/A. The owner knows him; credited in the README. No licence on the repo - read his code, do not copy it without asking.
- **michaeldale, `velocity9x`** (Andrew, #49) - <https://github.com/michaeldale/velocity9x>. Win98,
  linear-framebuffer cards (Mach64 via VBE). Gave: the 8514/A-family colour-expand sequence for
  `DrawTextBitmap` on the Trio64 (`docs/decisions/2026-09-06-gdi-accel-005-text.md`: `PIX_CNTL=A080h`,
  `CMD=53B3h`, `rep outsw` to `E2E8`), which the Mach8 shares at the 8514/A level - check bit 9/12
  meanings against the Mach32 guide; Open Watcom as a 16-bit toolchain; and "`DP_MONO_SRC_BLIT` glyph
  caching in off-screen VRAM" listed as a Mach64 win - the same idea as ours. Not usable as code here:
  Mach64 is not register-compatible with the Mach8, and the driver needs an aperture.
  Re-read 2026-10-06 for Andrew's VGA/8514 switching idea (#49): **GPL-3.0** (this repo is MIT - read
  it, do not copy it). `docs/decisions/2026-08-10-dynamic-mode-switching.md`: live switching through
  `ReEnable` (`C1_REINIT_ABLE`, rebuild the PDEVICE in place between `DIB_BeginAccess`/`DIB_EndAccess`,
  re-register with the VDD), taken from vmdisp9x (<https://github.com/JHRobotics/vmdisp9x>, MIT);
  **same depth only** - Windows 9x never changes depth live (KB Q127139). `2026-08-16-vbe-tier0-family.md`:
  in a Win9x display driver, DPMI 0100h fails - get DOS buffers from `GlobalDosAlloc`.
- **VBEMP** (owner, 2026-10-06) - <https://bearwindows.zcm.com.au/vbemp.htm>. NT 3.1-XP only, no Win9x
  build, closed source (freeware, non-commercial), needs a VBE BIOS and mostly a linear framebuffer.
  Nothing for this card.
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
draw-engine tests. Andrew sees most 8514 command tests fail in 86Box. It was run on the 5160 before,
but the output was not kept; re-run it from a DOS boot (skip the startup) and photograph each screen.
That is the reference result, and the failing tests name what the emulator's Mach8 model lacks - the reason ATI's
accelerated mode does not run in the bed.

**Run 2026-10-05 on the 5160: every test passes** (register, FIFO, RAMDAC, video RAM, test sequences 1 and
2; 1M VRAM, 8-bit bus, PCLK 18810-2). The 640x480, 800x600 and 1024x768 patterns displayed; 1280x1024x4
87 Hz interlaced did not sync on the LCD. Transcript `docs/captures/2026-10-05_m8glyph/5160_TEST_COM.txt`.
**Bed, same night (`vm_magnaram_off`, `mach8_vga_isa`, 8-bit):** detection identical, then five of six stages fail
(`docs/captures/2026-10-05_m8glyph/bed_86box_TEST_COM.png`):

| stage | 5160 | 86Box |
|---|---|---|
| Register Integrity | pass | `82EE000A` / `14204 0602 0004 Graphics Subsystem Failure` |
| FIFO Integrity | pass | pass |
| RAMDAC Integrity | pass | `14205 1727 0005 RAMDAC Failure` |
| Video RAM | pass | `14206 74F1 0006 RAM Failure` |
| Test Sequence 1 | pass | `14216 27DB 0016 RAM Post Examination Failure` |
| Test Sequence 2 | pass | `14204 0602 0004 Graphics Subsystem Failure01` |

`82EE` is `PATT_DATA_INDEX`; the model returns it only for a word read (`vid_ati_mach8.c`, `case 0x82ee: if (len == 2)`).
Not investigated further: the Mach8 model is not the owner's code. What this means for the work: the bed is
proven for the path M8GLYPH tests (8514 fills, plane-select expand blits, `PIX_TRANS` reads) and **not** for
pattern registers, the RAMDAC or ATI's VRAM tests. Check anything outside the proven path on the 5160.

## Findings from the full `ATIM8.DRV` read, 2026-10-05 (`tools/nedis.py`, new)

- **`ATIM8.DRV` is a Windows 95 DIB Engine mini-driver** (imports `DIBENG`, 45 sites; `CreateDIBPDevice` at
  `2:083D` with **`lpBits = 0`**). Every mode's flags (`cs:1B83` table) carry `VRAM | NOT_FRAMEBUFFER` -
  DDK `DIBENG.INC`: "NOT_FRAMEBUFFER ... example: 8514/a". **This corrects route (b)'s caveat above:** an
  8514-class mini-driver over the DIB Engine needs neither an aperture nor a screen shadow - it is what ATI shipped.
- No aperture: the selector built at `2:3141` maps ATI's video BIOS ROM (`52EE` -> `C000h + n*80h`, DPMI 0800h);
  `6AEE` is `MAX_WAITSTATES` (bit 8 = `LINE_OPT_ENA`) on the Mach8.
- Mode table `cs:1B17` (320x200 .. 1280x1024) -> `[0x192]` width, `[0x194]` height; `[0x196]` pitch.
- **Off-screen use (question 1, partial):** `2:24E9` checks `pitch * (height + 5) + [0x1B5]` against VRAM size
  `[0x1A5]` - ATI appears to reserve **5 lines below the screen** plus a block of `[0x1B5]` bytes; `2:2641` asks the
  VDD (`lcall [0x62]`, function 80h/83h) for memory and stores lines in use in `[0x116E]`. Not yet read: what the
  5 lines and `[0x1B5]` hold, and the cursor path (DIBENG cursor calls 102-106). Continue there.

## XFree86 3.3.6 already did this (read 2026-10-05)

`XF86_Mach8` (Kevin E. Martin, MIT-style licence; local `references/xfree86_336_mach8/`, see
`docs/resources_and_sources.md` s4) ships an off-screen glyph cache for the Mach8. It is real code
that ran on real Mach8 cards, so it is the template for the cache, whichever route is chosen.

- **Layout (`mach8FontCache8Init`, `mach8fcach.c`):** everything below the visible screen. A 64x64 area
  at (0, virtualY) for expanding pixmaps; the rest, from x = 64 to 1023 and from virtualY to line 1023 on
  a 1 MB card, is the font cache, **registered once per bit plane - 8 pools over the same rectangle**.
  No cache unless at least two 6x13 fonts fit (w >= 192, h >= 26).
- **Draw (`mach8GlyphWrite`, `mach8fc.c`):** set once per string: `FRGD_COLOR`, `PIX_CNTL` =
  `MIXSEL_EXPBLT` (`MULTIFUNC_CNTL` A0C0h), `FRGD_MIX` = colour with the GC's ALU, `BKGD_MIX` = destination
  (transparent), `WRT_MASK`, scissors. Per font block: `CUR_Y` and `RD_MASK` (the plane), only when they
  change. **Per glyph: `CUR_X`, `DESTX_DIASTP`, `DESTY_AXSTP`, `CMD` = 4 word writes**, plus
  `MAJ_AXIS_PCNT`/`MIN_AXIS_PCNT` only when the glyph size changes, plus a FIFO poll (`WaitQueue`, an
  `inw` of 9AEEh). Glyphs sit 32 to a row, so the source X is `block.x + (char & 31) * width`.
- **This is plain 8514/A plane expansion** (`PIX_CNTL` mix select 3, "VRAM bitplane picks the mix"), not
  the Mach8-only `DP_CONFIG` MONO_SRC route in question 2. The `ibm8514` server does the same. Either
  should work; the 8514 route is the one that is known to have worked.
- **Against our estimate:** ~4 writes and a poll per character instead of the ~7 estimated above, and
  ~13 in ATI's path today. Still to be measured on the card.
- **Question 1, revised:** the map XFree86 uses is a choice, not something ATI's driver forces. Our
  own driver can take the same layout; a patch to `ATIM8.DRV` still has to avoid ATI's 5 lines and the
  `[0x1B5]` block.
### Licence terms - what we may do with it

Read from the file headers 2026-10-05. Stay inside these.

| Files | Holder | Terms |
|---|---|---|
| `mach8fc.c`, `mach8fcach.c`, `cache/xf86fcache.c`, `cache/xf86text.c` (and the rest of `mach8/`) | Copyright 1992 Kevin E. Martin, Chapel Hill, North Carolina | Use, copy, modify, distribute and sell, for any purpose, without fee - **provided** the copyright notice appears in all copies, the copyright and permission notice appear in supporting documentation, and **his name is not used in advertising or publicity** for the result without his written permission. As is, no warranty |
| `regmach8.h` | Copyright 1989, 1990 Panacea Inc. (written by Jake Richter); additions by Kevin E. Martin, Rickard E. Faith, Scott Laird, Tiago Gons | "May be freely incorporated in any program without royalty, as long as the copyright notice stays intact." No warranty |

What that means here:

- **Any source file of ours that adapts their code** opens with the original notice, verbatim, and one
  line naming the XFree86 3.3.6 file it came from. A register header adapted from `regmach8.h` keeps the
  Panacea notice intact.
- **Supporting documentation** - the driver's `dist/` README - carries the same notice text, not only
  a credit line.
- **Credit, not endorsement.** The README credits say the work is *based on* XFree86 3.3.6 by Kevin E.
  Martin, with the URL. Never "endorsed by", and his name is not used to promote the driver.
- Using the method alone (plane pools, `PIX_CNTL` expansion, the per-glyph write order) copies no code;
  credit it anyway, as above.
- **The owner accepted these terms on 2026-10-05** and will cite the work.

### `RD_MASK` is rotated by one plane

To read plane *n*, set `RD_MASK` bit *(n + 1) mod 8* - plane 0 is bit 1, plane 7 is bit 0. Two independent
sources: Richter and Smith's register chapter (AEE8h table: "bit 0 = plane 7, bit 1 = plane 0 ...") and
XFree86's `mach8cachemaskswapped[] = {02,04,08,10,20,40,80,01}` (same in `ibm8514/fc.c`). Applies to the
8514 `PIX_CNTL` planar route; whether the Mach8-only `DP_CONFIG` route (question 2) rotates too is unchecked.
The one-glyph hardware test must use the rotated mask, or it reads the wrong plane.
