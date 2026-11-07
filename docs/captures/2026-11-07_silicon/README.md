# Silicon probes, 2026-11-07

Card, boot 258, `M8D3DY` (pageable), 640x480x16 DirectDraw exclusive, agent on. Run: t1 of `tools/m8probe/M8TEX.C`. Log: `M8TEX.TXT` in this folder. Waits lost: 0. No hang.

Times are us per draw, fastest of 3 reps x 20 passes.

| Arm | 32 px | 100 px | 100 px vs flat |
|---|---|---|---|
| flat: scratch, edges, polygon fill | 409 | 1,011 | 1.00x |
| tcopy: texture copied to scratch, edges, 6213h fill | 463 | 1,577 | 1.56x |
| tatlas: edges XORed into plane 7 of the texture, 6213h fill, edges XORed out | 529 | 1,064 | 1.05x |
| avg, add, xor mix on the polygon fill | 409-410 | 1,011-1,012 | 1.00x |

100 x 100 box:

| Arm | us |
|---|---|
| replace | 190 |
| replace with WRT_MASK 0Fh | 347 |
| add 1Bh | 347 |
| add with WRT_MASK 0Fh | 347 |

Findings:

- tatlas leaves plane 7 clear (0 pixels set on the checked row): a texture in card memory takes its own outline and gives it back.
- 6213h pixels follow the atlas 1:1 on the checked row. The left edge pixel keeps bit 7 (9A for 1A, B4 for 34): 7-bit colours, as M8GRAD found; the driver must mask or draw over it.
- Mixes work on the polygon fill at no extra time. After 20 passes: add saturated at FF, xor cancelled back to background, avg converged on the ink colour.
- Field arithmetic: add 1 with WRT_MASK 0Fh, 20 passes, on 41h gave 4Fh on the card: it saturates inside the field. Bed gave 45h: the bed is wrong here.
- A copy wider than its source does not wrap in the row: at SRC_X_END the source continues on the next source row (40-4F then the row below). The bed agrees.
- SCAN_TO_X with DP_CONFIG 6211h steps along the source, one source pixel per destination pixel. The bed agrees.
- LINEDRAW with DP_CONFIG 6211h steps along the source (card: 43 44 45 ...). The bed drew nothing there. This answers M8TEXL, for a horizontal line only so far; other slopes are the next probe.
- Bed difference: in M8TEX the polygon-mode edges drew nothing in the bed (scratch stayed 00), and the triangle arms stayed background; the card drew them. Cause not found.

## Locked against pageable

Card, `M8D3DF` at high priority, agent on, fps. Locked = `M8D3DX` runs zl, zl2. Pageable = `M8D3DY` runs ya1, ya2, boot 258, FB16CHK 0 differ. `M8D3DY` also carries the desktop changes.

| Test | Locked | Pageable |
|---|---|---|
| hal_many | 31.2, 30.8 | 40.8, 39.6 |
| hal_large | 38.0, 38.3 | 43.0, 43.4 |
| hal_grad | 35.9, 36.2 | 40.7, 42.8 |
| hal_turn100 | 30.7, 30.6 | 32.5, 30.8 |

Locking slowed each test except hal_turn100.
