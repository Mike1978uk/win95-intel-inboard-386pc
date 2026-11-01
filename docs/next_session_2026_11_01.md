# Next session - from 2026-11-01

Supersedes `docs/next_session_2026_10_31.md` (its open items 6 and the boot stall still stand).
Gate unchanged: every one of the 35 tests at or above ATIM8, better where possible (owner, 10-31).
Card log: `$TEMP\m8_card_bench.txt` (copy into `docs/captures/2026-11-01_card/` at the start).

## State

master `e8e95fb` and this handoff, pushed.
Card: `M8PATA.DRV` selected (= master). Last two runs (card_pp1, card_pp2): 1.798x / 1.806x ATI,
pixels = ATI in all 35, run 2 slower in **0** tests.

## Done this session (all card-benched twice, pixels = ATI in all 35)

| commit | change | effect |
|---|---|---|
| `29419f9` | Packed text laid out in our driver, not DIBENG: GDI passes raster text with ETO_BYTE_PACKED (0x100), lpFontInfo is then GDI's own glyph table and lpTextXForm the string's box (`docs/dibeng_font_layout_2026_10_31.md` s7); glyph rows OR-ed in `text.asm`, one mono expansion | text_varied 0.92 -> ~1.0 |
| `2430598` | Text key hashed in one `M8_HASH_PARTS` pass; strings seen twice cached from our own bitmap | text_varied 1.05/1.08 after the first run |
| `6405f4a` | Pattern rows that are rotations of row 0 keep one PATT_DATA, PATT_INDEX = (x + rot) & 7 | polygon 0.93 -> 0.97/0.99 (DKGRAY_BRUSH is a dither) |
| `e8e95fb` | Pattern spans walked in assembly (`M8_SPANS_PAT`, spans.asm) | polygon 1.05/1.07, ellipse 1.01-1.02, fill_hatch 1.14-1.28 |

## Disproved / closed (one line each)

- Per-glyph cache in card memory (7 writes a glyph, ~80 us): slower than one expansion of the whole string.
- Reading the FNT 3.0 table under ETO 0x100: wrong widths, missing dialog text on the card.
- Hardware polygon fill (#5): GDI scan-converts polygons itself; the card's fill leaks on self-intersecting
  polygons (~87% of the bench's) - `docs/mach8_poly_fill_design_2026_11_01.md`.
- Cursor off once per BEGINNSCAN batch plus brush reuse: no polygon gain (card_ns1/2), reverted.
- SCAN_TO_X does not step CUR_Y (guide 9-61): a CUR_Y write per row stays.

## Still below ATI in some runs - next, in order

1. **setpixel** (0.83 to 1.0+ run to run): code layout moves its short path across cache lines. Put Pixel's
   short path in aligned assembly (as spans.asm/text.asm), then bench twice.
2. **First run after a reboot** reads low in tests no change touched (text_varied 0.91-0.94, fill_small,
   dstinvert, getpixel ~0.9). Judge on runs 2+; consider M8BENCH doing one untimed warm-up pass.
3. **lines_dash** (0.89-0.93 in some runs, 1.01 in others; ATI's own runs 437 and 548/s): bench four times
   first. If real: wait for FIFO room instead of an empty FIFO (XFree86), engine steps the dash pattern.
4. **dstinvert / fill_small width-height trim** (owner approved, ~2.5%): first a card probe that MAJ_AXIS_PCNT
   (readback 96EE) and MIN_AXIS survive lines, SCAN_TO_X and blits - the guide does not say; 13 write sites
   in m8hw.c. Only then skip unchanged writes.
5. **#1 wait states** (static sweep, owner's next pick): M8UPRATE sweep of 6AEE bits 3:0 and 36EE bits 0-1 for
   PIX_TRANS writes, pixels checked; 3.00 us/B against 2.17 for an undecoded port.
6. Remaining sweep leads (`docs/mach8_static_sweep_2026_10_31.md`): string vs single OUT, short strokes with
   A2EE = 0708h, VRAM copy engine time.

## Method notes learned

- A fast path can be silently off: eligibility checks counted per reason (profile slots) found two wrong ones.
- Code layout: a C edit moves unrelated tests 5-12%; hot paths go into aligned assembly once correct.
- Research, git and docs go to cheaper agents (owner); the agents' docs this session were all used.
