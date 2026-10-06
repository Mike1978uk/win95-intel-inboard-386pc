# Mach8 Windows 95 display driver - design, 2026-10-06 (#49)

Owner's brief: build the best of every technique we have found, not a copy of any one driver. Ideas and
the card's capabilities are in `docs/mach8_graphics_vision.md`; the text path and source notes in
`docs/mach8_text_and_driver_plan.md`. This file says how the choices are made and from what.

## The rule: the 5160 picks, per drawing job

The bus is the only scarce resource (~3.8 us per 16-bit port write). For every GDI job below there are
two to four ways to make the card do it. **Each candidate is timed on the real 5160 (M8TIME, below) and
the cheapest wins**, whichever driver it came from. Port writes per operation are counted as well as
time. A path is only used once 86Box's model matches the real card on it, so it can be developed in the
bed (`docs/ati_test_com_notes.md`; the 8514/A command path is ~80% measured, the Mach8 extended
`DP_CONFIG` path ~25%).

## Candidates per job

| job | candidate A | candidate B | candidate C |
|---|---|---|---|
| text (glyphs) | XFree86 glyph cache: glyph planes in spare memory, 8514/A plane-select blit, ~4 writes/glyph | ATI: mono expansion fed from the CPU (`DP_CONFIG 3251`), ~13 writes/char | DDK sample (Christian's XGA): DIB Engine lays out the string, one expansion per string, ~8 writes/char |
| solid fill | 8514/A `CMD` rect fill (`40F3`), ~5 writes | Mach8 `DEST_Y_END` fill (`2211`) | `SCAN_TO_X` per span (polygons) |
| screen-to-screen blit | 8514/A bitblt (`C0F3`) | Mach8 non-conforming blit (`6211`/`6011`) | - |
| pattern brush | 8514/A fixed pattern (8x1) | Mach8 colour pattern (`A211`) | brush cached in spare memory + blit |
| bitmap upload (`SetDIBitsToDevice`) | CPU data, 16-bit words (`rep outsw`) | per-scanline encoding: runs as fills, 2-colour spans as expansion, rest raw (vision doc) | - |
| lines | 8514/A Bresenham line | Mach8 `LINEDRAW` (point to point) | short-stroke vectors for short ones |
| cursor | software (DIB Engine) | save-under + blit from spare memory | - |
| screen modes | one mode per boot (ATI) | live same-depth switch via `ReEnable` (velocity9x / vmdisp9x), incl. 8514 <-> VGA for Andrew's DirectDraw idea | - |

## Measured: M8TIME on the 5160, 2026-10-06

`tools/m8seq/M8TIME` (capture `docs/captures/2026-10-06_m8seq/5160_M8TIME3.BIN`). Interrupts off, PIT timed,
a FIFO-room check before each operation and the engine's finish included. The raw upload works out at
7.6 us per 16-bit write, the same as the 2026-09-20 port measurement, so the timer scale holds.

| job | technique | cost | |
|---|---|---|---|
| text, 8x16 glyph | A: glyph cache, plane-select blit from spare memory (XFree86) | **51 us** | best |
| | C: one expansion per string, Mach8 `DP_CONFIG 3251` (DDK pattern) | 68 us | 1.3x |
| | B: one expansion per character, same path (ATI's `ExtTextOut`) | 121 us | 2.4x |
| fill | 64x16 rectangle, 8514/A `40F3` | 61 us | |
| blit | 64x16 card-to-card, 8514/A `C0F3` | 102 us | |
| raw bitmap | 64x16 pixels from the CPU, 16-bit words | 3,931 us = 3.84 us/pixel | |
| VGA memory | writes to the VGA side's memory, `rep stosw` | 2.07 us/byte | |

- **Text: the glyph cache wins.** First version of the text path: XFree86's layout, per-string expansion
  (C) as the fallback for glyphs not yet cached.
- **Bitmaps: never send what the card can make.** A 64x16 area costs 61 us as a fill and 3.9 ms as
  pixels, 64x. The bitmap path encodes per scanline: runs as fills, two-colour spans as expansion, raw
  only for the rest (vision doc).
- **Andrew's VGA point holds for writes:** 2.07 us/byte into the VGA's memory against 3.84 us/pixel through
  the engine. Reads were the slow direction (`docs/isa_memory_vs_io_2026_09_20.md`). The 8514 desktop has no
  memory window, so this only helps a VGA-mode path (DirectDraw, games) - a reason to keep live mode
  switching in the design.
- First attempts (`M8T1`, `M8T2`) sent the 1-bit text tests down the 8514/A path without the right
  command; the engine never finished. XFree86 never feeds 1-bit data from the CPU at all.

## What each source contributes, and on what terms

| source | takes | licence / terms |
|---|---|---|
| XFree86 3.3.6 `XF86_Mach8` (Kevin E. Martin) | glyph cache layout and draw sequence, 8514/A command usage | MIT-style: keep the notices, credit, no endorsement (`docs/mach8_text_and_driver_plan.md`) |
| velocity9x (Michael Dale) | DIB Engine glue, VDD registration, `ReEnable` mode switching, test tools (`V9XGDI`, `V9XMSW`), Open Watcom build | GPL-3.0; author's permission to use parts and to contribute, 2026-10-06. The driver is therefore GPL-3.0 |
| vmdisp9x (JHRobotics) | where velocity9x's switching came from; Win95 support claimed | MIT |
| xga-win9x (Christian Holzapfel) | how the DDK mini-driver sample builds and works, `DrawTextBitmap` | no licence: read, do not copy without asking |
| ATI `MACHW3.DRV`, Microsoft `ATIM8.DRV` | the reference for what works on this card: DIB Engine with `NOT_FRAMEBUFFER`, the extended path, init sequences | proprietary: study only |
| TEST.COM, ATI ROM | register behaviour, init tables | proprietary: study only |

GPL-3.0 and the DDK sample code do not mix: the driver is built on velocity9x's framework, not on DDK
sample source. DDK headers and documentation are fine.

## Open before the framework is fixed

- Whether velocity9x's framework fits a card with **no linear framebuffer** (`NOT_FRAMEBUFFER`, every
  pixel through the engine). Asked of Michael Dale; check its code too.
- Whether it runs on Windows 95 (author: "should"). Prove it in a Win95 bed before building on it.

## Order of work

1. **M8TIME** on the 5160: time candidates A/B/C for text, fill, blit and bitmap upload, with port-write
   counts, and writes into A0000h (Andrew's VGA point). One DOS run.
2. Finish matching 86Box on the paths M8TIME picks (the bed then develops the driver).
3. velocity9x review: clone, read, build; answer the two open questions.
4. The driver, primitive by primitive in the order M8TIME ranks them, each one measured with `TXTBENCH`
   before and after on the 5160.
