# 16-bit m8dos in the bed, 2026-11-18 (X30 step 2)

`tools/dos3d/bed_run.sh` on `vm_6695_w2` (486, Graphics Ultra, X22 build): `M8DTEST card 12`, `M8DTST16 card 12`,
then each bench (50 tris size 40, 100 frames; 30 tris size 120, 60 frames), 32-bit and 16-bit in turn.

| Check | 32-bit | 16-bit |
|---|---|---|
| Card: port writes, polygons fill/spans, rects, lines | 1450, 30/4, 12, 6 | same |
| Bench 50x40: writes a frame, fps | 2000.2, 60.69 | same |
| Bench 30x120: writes a frame, fps | 1360.3, 64.26 | same |
| Card screen (s10-s18 vs s22-s26) | pixel hash 074d50ece4 | same |

The bed draws outlines without fills (X27), so the shots prove the two builds equal, not the fill. The card is
the test of the 16-bit fill: `M8DTST16 card 15` and `M8DTST16 bench 50 40 200`, against the 32-bit numbers in
`2026-11-18_m8dtest_card/M8DTEST.TXT`. Free-standing object (no `M8_HOSTED`) calls only `__I4M`/`__I4D`; code about 5 KB.
