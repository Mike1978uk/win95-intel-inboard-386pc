# Overdraw and redundant-write audit (2026-11-19)

Read-only audit of the Windows driver (`drivers/mach8_display/src/`) and `tools/dos3d/m8dos.c` against the L6 findings
(`docs/l6_card_for_3d_2026_11_19.md`). Cost unit: one port write, ~7.6 us, about 500 pixels of plain fill (15 ns a pixel).
So **overdraw of pixels is cheap; overdraw that costs writes is not.** Candidates are ranked by writes saved.

"V" = verified by reading the code. "I" = inferred, needs a probe or a card run. Nothing here was run.

## Already clean (V)

- `m8d3d.c` joined patches, `m8d3d_join` ~3063-3288: one plane a colour, edges XOR-merged across triangles (a shared edge
  cancels), and with the default `shift=1` (line 3463) the right-bounding edges move one pixel (3117) and **no outline is
  drawn over the fill** (3198 skips it). This is the top-left idea, already built. The chained outline at 3198-3280 only
  runs for `shift=0`.
- `m8d3d_edges` 1237 and `m8_hw_poly` (`m8hw.c` 1306) send a closed scratch outline, 2 writes a corner.
- `m8d3d.c` stamp copy 2292-2300 sets its mode once; `m8d3d_join_fill_begin` 2780 writes colour-shared registers once a pass.
- 2D GDI path (`screen.c`, `m8hw.c`): `M8_SET` shadows skip unchanged registers (m8hw.c 239-250); opaque text is one
  mono pass with only the strips outside the box filled (`screen.c` 4402-4470); DIB blits send runs as fills (`screen.c` 388-399).
  Nothing found worth touching, and the parity risk is high. **Leave alone.**
- `m8dos.c`: `q_col` caches the colour (685), scratch clear is one rectangle a batch, page clear goes early (1147).

## Candidates, ranked

### 1. m8dos has no joined same-colour patches (V: absent)  - biggest

`m8dos.c` `batch_flush` 837-964 gives every polygon its own plane and its own scratch outline + fill (11 writes + ~12 of
outline), max 8 a batch (`PASS`). Windows merges same-colour neighbours into one plane, XORs the shared edges away and
fills the union once.
- Now: a quad sent as two triangles of one colour: ~2 x 25 = 50 writes. Merged: one closed outline of 4 corners
  (~12) + one fill (11) = ~23, the shared diagonal costing nothing. About -50% on any same-colour mesh (flat-shaded
  terrain, walls, floors, the usual two-triangle quads).
- Also lifts the 8-polygons-a-batch limit to 8 colours a pass: batch set-up and scratch clear (~2.5-9 writes) shared by
  many more polygons.
- Risk: the fill costs ~69 ns a pixel of the union's box (per band in Windows, `m8d3d_join_fill_bands` 2902), so a
  sparse union needs banding or a size cap; overlap must be excluded (`m8d3d_join_ok` 2690 does this with a cell map);
  with `m8_depth` active only same-colour **and same-z** polygons may merge (COLOR_CMP is per fill, 909).
- Order matters: the DOS layer's painter's order must survive (Windows runs it as "further runs painted after").

### 2. Flat triangles still draw an outline over the fill (V)

`m8d3d_tri` 1326-1400: scratch outline (14) + fill (15) + RD_MASK (1) + page outline (`m8d3d_outline` 1212, 14) = ~44.
Left-inclusive/right-exclusive fill from top row to bottom-1 with top-down chains (m8dos 871-888, `q_chains`) needs no page
outline: **-15 of ~44 (about a third)**. Same pattern, same saving: `m8d3d_mask_build` 1802-1840 (outline over the mask, 1839),
and `m8_hw_poly` `m8hw.c` 1376-1388 (the 16-bit driver's own polygon escape, `poly3d.c` 190, 5 writes a corner for n-gons).
- Risk: pixel set changes by the right column and bottom row of every triangle. Stamps and cached masks (`m8d3d_tri_cached`)
  must use the same rule as the joined path or one frame mixes two conventions (seams between a joined patch and a loose
  triangle). Not part of the 35 GDI tests (3D HAL and `poly3d` escape only), but the D3D visual comparison to software changes.
- Payoff caveat (V, from the 11-19 note): the twist bench is spent in describe/feed code; the Flip share is ~30 ms of 148 ms.
  Gain on frame rate is small until the CPU side shrinks.

### 3. Cost thresholds are stale after P2 and the top-left rule (V)

- `m8dos.c` `choose` 1015-1027: `fill_cost = 1 + 5*edges + 11 + 4 + 2n + 6` (per-edge scratch outline, page outline). Closed
  outline + top-left is about 2n+4 + 11 + 3 = 24 for a triangle; spans stay 3/row, so the crossover moves from ~14 rows to ~9.
- `m8d3d.c` 1257 `M8D3D_SPAN_ROWS 13` ("about 50 for an outline, a fill and a second outline"): same, ~44 now, ~29 after item 2.
- Saves: ~3 writes on each triangle of 9-13 rows (spans -> fill). Free to change; check with `M8DTEST costs` on the card.
- Risk: none for pixels if both routes give the same pixels (they do for the shifted rule; not yet for the outline rule's
  spans vs fill on apex rows: probe before moving the threshold under the outline rule).

### 4. Gradient triangles: end rows and per-edge outline (V)

`m8d3d_tri_bands` 2154-2274: `m8d3d_end_row` for top and bottom (2257, 4 writes each = 8 writes) because the fill starts at
t+1 and stops at b-1; `m8d3d_edges_grad` 1989-2042 sends 3 edges at 5 writes (15) where top-down chains are ~12.
With the fill starting at `t` the end rows go: **-8, plus ~-3 from chains**, of ~55-60 for a banded triangle.
- Risk: first row of each band is a fill at `a = t+1+rows*i/n` (2217); moving to `t` shifts band boundaries by a row,
  so band colours change by one row. Check the gradient triangle visually on the card (not 2D parity).

### 5. Unchanged registers re-sent (V for the three listed; I for the probe)

- `m8dos.c` 906-907 and 909: FRGD_COLOR and COLOR_CMP are sent for every fill, even when equal to the last (mesh of one colour,
  one depth). `q_col` already does this for the spans phase (685). **-1 to -2 a fill** (colour, depth) on same-colour runs.
  Subsumed by item 1 for colour.
- `m8d3d_join_fill` 2810-2825, `m8d3d_tri` 1377-1381, `m8dos` 916-918: `CUR_X` and `DEST_X_START` always carry the same value
  (they are the same x in every send). **I:** whether DEST_X_START must be written for the poly fill at all, and whether
  `SRC_X_END` / `DEST_X_END` survive a fill when the width is unchanged. P1 tested only DESTX_DIASTP, DESTY_AXSTP, SRC_X_START
  together (35,936 px differed): it does not say which one broke it, and the other registers were not tested. One card probe
  per register, same method as P1 (`M8DTEST probe`). Possible -1 to -3 a fill.
- Fixed order: `Q( M8_RD_MASK, 0x00FF )` + `M8_DP_CONFIG` + `LINEDRAW_OPT` at the end of a batch (m8dos 923-925) are written
  back to a state the next call rewrites anyway when only fills follow (I; needs the next-call analysis, ~-2 a batch).

### 6. Spans: equal consecutive rows (V: no merge)

`m8dos.c` `q_spans` 706-751 and `m8d3d_spans` 1261-1323 send 3 writes a row. Rows with the same lo and hi (a rectangle given as
a polygon, the vertical sides of `m8_trap` 1106, a quad with vertical edges) become one rectangle fill: 5 writes for k rows
instead of 3k (win at k >= 2). An axis-aligned 4-corner polygon could take `m8_rect` outright (5 writes, no scratch).
- Risk: none for pixels. Detect only exact equality. The Windows `M8_SPANS_SOLID` (spans.asm 15-16) already does this for GDI.

### 7. Mask plane cleared one by one before a rebuild (V)

`m8d3d_mask_build` 1783-1801: each rebuilt mask costs 5 set-up + 5 rect = 10 writes to clear its plane. Several masks
rebuilt in one frame could share one clear (union box, combined WRT_MASK of the planes being rebuilt): -8 each after the
first. Only on a cache miss; payoff depends on the hit rate (stat [21]).

### 8. Erase then redraw (V code, I payoff)

`m8d3d_replay` 3343-3369 fills the previous frame's triangle boxes black (bounding boxes, 2x the triangle's area), then
draws the new frame over them; the DirectDraw 8 bpp flush fills the background and then sends the non-background spans
over it (`m8dd32.c` ~2989-3020). Pixels written twice, but at 15 ns they cost ~1/500 of a write: only worth it if it saves
writes. Candidate: erase only boxes not covered by a new triangle's box (merge test already exists at 3354, 2500 px a box). Low.

### 9. Smaller sites

- `fb16.c` 611-614: an odd-width dithered block (w of 1 or 3) goes as up to 16 one-pixel fills (~80 writes). Rare; the
  2D driver, dither parity must hold, so pad to a word only if the pixel to the right is rewritten right after.
- `m8dd32.c` `m8dd_mono` 2699-2745: scissors written (4) and restored (4) around every mask, plus two idle waits. Several
  masks in a row could keep the scissors set: ~-5 a mask after the first. DirectDraw only.
- `poly3d.c` 168-170: a background rectangle fill (`m8_hw_fill`) before the polygons: inherent.
- Status polls in inner loops: none found in the paths above that wait per primitive (V for m8dos and `m8d3d.c`;
  `m8dd_dfr_*` polls FIFO status once a batch of three spans, 2990-3000: fine). The 779 status reads a frame (11-19 note) were
  waiting on the page clear, already fixed.

## Suggested order

1. Item 3 (thresholds): minutes, no pixel change; measure on the card.
2. Item 5 probes: one card boot each; may drop 1-3 writes from every fill in both layers.
3. Item 1 in m8dos (patch merge): largest, a build; mirror `m8d3d_join_ok` and `m8d3d_join`.
4. Items 2 and 4 in the Windows driver through `tools\m8_card_cycle.ps1` / the install_h flow, benched twice.
5. Item 6 in both layers.

Not recommended: any change in `screen.c` or `m8hw.c` GDI paths (already shadowed, parity with ATI in the 35 tests).

## Not checked

- `m8dd32.c` DirectDraw colour-key and overlay paths (`m8dd_ov_flip` 4448), `dd16.c`, `enable.c`, `blt.asm`, `pixel.asm`.
- Actual share of flat (non-joined) triangles in the twist and game workloads: decides items 2, 4 and 7.
- Whether `m8d3d_q_flush` inside `m8d3d_join_fill` (2827) costs an engine wait per fill run (not read through).
