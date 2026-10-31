# Mach8 hardware polygon fill - design and verdict (2026-11-01)

Question: can the polygon test (M8BENCH `t_polygon`, `tools/m8bench/M8BENCH.C:513`: 6 random points,
DKGRAY/LTGRAY stock brush, default pen) be drawn with CMD 5 + the poly-fill blit and give ATI's pixels?

**Verdict: not as a drop-in.** Two findings change the question; the second one decides it.

1. **DIBENG does not scan-convert polygons.** `DIB_OUTPUT` (`DIBENG.DLL` seg 3 `03FC-0437`) compares the
   style with 0x12 (polyline, to seg 7), 4 and 5 (scanlines, to seg 5), 0x50, 0x51 and returns `FFFFh`
   for everything else. Both ATIM8 and ours report `POLYGONALCAPS = 0x0008` (PC_SCANLINE only;
   `docs/captures/2026-10-23_card/m8_card_bench.txt:5812-5813`, `ati31` = ATIM8). So **GDI.EXE** converts
   the polygon and calls `Output` with style 5 (`OS_POLYSCANLINE`) for the fill and 0x12 for the pen
   (`docs/atim8_vs_ours_lagging_tests.md:186`, `docs/atim8_disassembly.md:110`). ATI's pixels follow
   GDI's fill rule, which no document or disassembly here has recorded.
2. **The driver never sees vertices.** `OS_POLYSCANLINE` carries `(n, top, bottom, n x values, n)` records
   (`docs/next_session_2026_10_21.md:7-12`). CMD 5 needs edges. Getting them means setting
   `PC_ALTPOLYGON`/`PC_WINDPOLYGON` in `dpPolygonals` (`enable.c:312` clears only `PC_POLYPOLYGON`), which
   takes GDI's scan converter out of the loop for **every** polygon. DIBENG then returns -1 for styles 20/22,
   so the driver must also draw the interior and pen of every polygon it cannot fill in hardware, with
   GDI's rule reproduced pixel for pixel. Whether GDI accepts a -1 return and simulates is not documented
   in the DDK files read (not determined).

Drawing the boundary from the spans we already have (a pixel at each span's left and right) gives no gain:
that is SCAN_TO_X's fill flag, already card-proven (M8PF, `docs/mach8_card_coverage.md:204,230`), at one
write per edge crossing per row.

## 1. Register sequence (if vertices were available)

Ports are the 8514/A/Mach8 ones in `m8.h:76-112`. Counts are cold (no shadow hits); estimates from
M8PF/M8PL2/`m8_hw_polyline`, not measured. A port write is ~7.64 us on the card
(`MEMORY.md`, `bed_misprices_io_vs_memory`).

Destination of the boundary pass: **off-screen scratch, not the screen.** Type A/C read the outline from the
destination's (or, for C, the blit source's) pixels with RD_MASK (`guide.txt:6947-6949`); drawing it on screen
would overwrite the picture. Type C (DP_CONFIG bit 1) reads the outline from the blit **source** and writes
a different destination (`guide.txt:6949,9986-10004`), which is what M8PF proved on the card. Use C.

| Step | Writes | Port / value |
|---|---|---|
| A. Clear mask (bbox w x h, scratch rows) | ~8 | WRT_MASK AAE8=FF, FRGD_COLOR A6E8=0, FRGD_MIX/DP_CONFIG, CUR_X 86E8, CUR_Y 82E8, MAJ_AXIS 96E8=w-1, MIN_AXIS BEE8 idx 0 =h-1, CMD 9AE8=40F3 |
| B. Boundary set-up | ~4 | PIX_CNTL BEE8=A000 (poly off for this pass), FRGD_COLOR=FF, FRGD_MIX=0027, scissors off |
| C. Boundary, per edge (x6) | 7 each = 42 | CUR_X, CUR_Y, ERR_TERM 92E8, DESTY_AXSTP 8AE8, DESTX_DIASTP 8EE8, MAJ_AXIS_PCNT 96E8, CMD 9AE8 = `A017h \| octant<<5` (M8PL2 used A013h: `tools/m8seq/M8PL2.ASM:6-8`; bit 2 = LAST_PEL_OFF, `guide.txt:8639`, is the addition and is **not measured on the card**) |
| C'. Alternative: LINEDRAW | 2 + 14 = 16 | LINEDRAW_OPT A2EE=0006 (POLY_MODE bit 1, LAST_PEL_OFF bit 2; `guide.txt:10325-10331`), LINEDRAW_INDEX 9AEE=0, then 7 points x (x,y) to FEEE (closing the figure). POLY_MODE through LINEDRAW is not card-proven (TS2 covers plain LINEDRAW) |
| D. Fill blit | 14 | M8PF's: RD_MASK AEE8=00FF, DP_CONFIG CEEE=2213 (2211 + bit 1), ALU_FG_FN BAEE=7, FRGD_COLOR, source 8EE8/8AE8, B2EE, BEEE, C2EE, CUR_X, A6EE, AAEE, CUR_Y, AEEE (starts) (`tools/m8seq/M8PF.ASM:72-85`) |
| E. Restore | ~2 | DP_CONFIG, WRT_MASK |
| **Total, CMD 5** | **~70** (~0.54 ms) | |
| **Total, LINEDRAW** | **~44** (~0.34 ms) | |

Today: ~96 span writes (32 spans x 3) plus 6.2 calls x set-up (`docs/next_session_2026_10_31.md:65-68`),
~200 in the sweep's estimate (`docs/xfree86_and_all_tests_sweep.md:115`).

Not covered by the counts: engine time to clear and blit the bbox (unmeasured; bbox is up to 200x200 = 40 KB
for the bench), software to derive Bresenham terms for 6 edges, and the pre-checks in section 3. Dithered
brushes need `MONO_SRC=1` (pattern) in the same DP_CONFIG as the poly bit; M8PF tested only a solid
FRGD_COLOR, so that combination, and the pattern phase in a blit, are unproven on the card
(bed gap: its SCAN_TO_X starts a mono pattern at bit 7, `docs/mach8_driver_method.md:110`).

## 2. Inclusion rules

Hardware (all from the card or the guide):

- Fill state clears at each row start and toggles at each outline pixel (source OR ~RD_MASK all ones)
  (`guide.txt:9994-10004`, `6922-6923`). Alternate algorithm only (`6922`).
- Types A and C: left outline edge inclusive, right exclusive. Type B: both inclusive
  (`guide.txt:6943-6945,8693,8707`). M8PF on the card: edges 10/20, 5/30, 10/20/30 fill left-inclusive,
  right-exclusive, with the flag toggling (`mach8_card_coverage.md:204`).
- Boundary pixel per row: a Bresenham polygon line writes the **last** pixel of each row it crosses
  (endpoint included); a horizontal one writes nothing (M8PL2 on the card, `docs/ati_test_com_notes.md:94-99`).
  Drawing direction therefore picks which end of a shallow run is the boundary pixel.
- One mask pixel is one toggle. Two edges whose boundary pixels land on the same pixel toggle once, so the
  fill runs on to the next outline pixel or the end of the blit row. (Inference from the toggle rule; the bed
  model does the same, `vid_8514a.c:2658-2660`. Not probed on the card.)
- IBM's 8514/A area fill (adapter firmware, not the chip) counts coincident lines twice and is top/left
  inclusive, bottom/right exclusive (`ibm_8514a_techref_198704.txt:1601-1612`). The chip alone does not.

GDI/DIBENG: **unverified.** GDI.EXE's rule was not disassembled. The conventional Windows rule (spans
`[xl, xr)`, top row in, bottom row out) is assumed only.

Pen: the default pen is drawn through `m8_line_on_engine` / `m8_hw_polyline` (`screen.c:3291-3305`), pixel
identical to ATI's lines (hash equal). Order of fill and pen inside one polygon is **not determined**
(the 6.2 calls have not been traced in order).

| Case | Same as GDI's fill? |
|---|---|
| Interior pixels between the edges | yes, if GDI's span ends lie on the line's pixels |
| Boundary pixel and the other pixels of the line's run, fill drawn before a 1-px opaque pen | **hidden by the pen**, provided GDI's span ends lie inside the pen's pixel set (unverified) |
| Same, pen NULL, wide or XOR | not hidden |
| Two boundary pixels on one pixel (self-crossing at a pixel, apex, narrow neck) | **no**: fill leaks to the next outline or the row end; the pen does not cover it |
| Local-minimum vertex (apex row) | needs the apex row handled in software, else the toggle is single |
| Winding (OS_WINDPOLYGON) | hardware has no winding; equals alternate only for simple polygons |

**Conclusion: identical only with an opaque 1-px pen drawn after the fill, only for polygons passed by a
software pre-check (simple, no apex or coincident pixels), and only if GDI's span ends fall inside the pen's
pixels. Not identical in general.**

## 3. Pre-check and how often the bench passes

The pre-check is a vertex classification (O(6)) plus edge-pair intersection tests (15 pairs). Monte Carlo of
the bench's generator (6 points in 200x200, joined in order, 200,000 trials, scratch script): **13.2%
non-self-intersecting, 11.4% locally convex.** About 87% of the bench's polygons self-intersect and must
take the software scan-converter path. Edges that do not cross can still share a pixel in a row (narrow
neck), which needs a per-row test or a conservative gap rule (not designed).

Expected gain: if a passing polygon fell from ~10 ms to ~1-2 ms (unmeasured engine time) and 13% pass,
the mean saves ~1 ms of ~10 ms, offset by the pre-check. The remaining 87% need a driver-side scan
converter equal to GDI's, at a cost the GDI path currently pays for us. Not a win until that converter
exists and matches.

## 4. Alternate vs winding

OS_POLYGON/OS_ALTPOLYGON (22): matches the hardware's alternate fill. OS_WINDPOLYGON (20, `gdidefs.h:716`):
no hardware form; use it only for polygons the pre-check proves simple. The bench uses the default
(alternate) mode. `OS_POLYPOLYGON` (0x4000) is masked out in caps today (`enable.c:313`).

## 5. Test plan, cheapest first

0. **GDI's rule, no driver, no card.** A small Win16 program draws the bench's polygons (same `r()` seeds)
   on an 8bpp memory DC with NULL_PEN, then BLACK_PEN, and dumps the pixels with GetDIBits. A memory DC is
   DIBENG's, whose caps are scanline-only, so the pixels are GDI.EXE's rule. Compare offline with a Python
   model of section 2 (boundary last pixel, left-in/right-out). One Win95 boot in `vm_5160_mcp`. Settles
   the rule and the fill/pen order question (draw a pen that is a different colour). Gate: if the model
   cannot reproduce GDI's spans, stop here.
1. **Bed, engine only.** New DOS probe beside `M8PF.ASM`/`M8PL2.ASM`: clear mask, CMD 5 edges with
   LAST_PEL_OFF, type C blit, read back; compare with step 0 per pixel, including an apex, a coincident-pixel
   case and a dithered brush. Bed: `vm_5160_mcp` (`gfxcard = mach8_vga_isa`, the ATI Graphics Ultra model).
   The CMD 5 pixel rule fix (`3642d259d`) is scoped to `ATI_GRAPHICS_ULTRA`; confirm the bed build contains it.
2. **Card, same probe** over COMrade as M8PL2 was run; bed = card byte for byte. Needed for LAST_PEL_OFF on
   CMD 5, POLY_MODE via LINEDRAW, pattern plus poly bit, and the coincident-pixel leak.
3. **Driver.** Only if 0-2 hold: software scan converter hash-equal to ATI on the 10 bench polygons and on a
   wider random set; then M8BENCH `polygon` hash vs ATI's in the bed; then the card, twice, with the `idle:`
   line checked (`feedback_card_bench_prove_idle`).

## 6. Not determined

- GDI.EXE's polygon fill rule (edge rounding, top/bottom rows, horizontal edges).
- Order of the fill and pen `Output` calls within one `Polygon`, and what the 6.2 calls are.
- Whether GDI simulates a polygon when the driver's `Output` returns -1 for style 20/22.
- Behaviour of CMD 5 with LAST_PEL_OFF set, and of POLY_MODE via LINEDRAW, on the card.
- Poly-fill blit combined with a mono pattern source (pattern phase, two colours) on the card.
- Engine time for the mask clear and the poly blit at bench sizes.
- Which off-screen rectangle is free for the mask (cursor save at `m8.h:150-157`; the string cache's rows
  were not read).
- Whether the same-pixel double-toggle leak occurs on the card as in the bed model.
