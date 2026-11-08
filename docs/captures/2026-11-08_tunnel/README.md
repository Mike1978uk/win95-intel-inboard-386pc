# SDK tunnel on the card, 2026-11-08

`reference_host_gpu.png`: the unmodified DX 6.1 `tunneldp.exe` windowed on the host GPU, the look its author intended: grey-blue riveted metal, yellow and black hazard stripes, radiation symbols, a point light at the camera fading with distance. `tex2.bmp` is 256 x 256, 16 colours; every ring is 24 sides, each side one fixed slice of the texture.

Card, tunnel at normal priority, agent on:

| Build | Colours a frame | Writes | Ours | Engine | Frame | Owner's eye |
|---|---|---|---|---|---|---|
| `M8D3EL` joined, banded fills | 7 | 4,470 | 83 ms | 166 ms | 355 ms | no holes, no flicker, flat grey blocks |
| `M8D3EM` texture palette (16 colours x 16 light levels) | 38-41 | 8,600-10,000 | 144-162 ms | 232-265 ms | 470-548 ms | "colours infinitely better", depth from the light steps; still big flat triangles, yellow triangular and random where the original has rectangles |

Cause of the triangles: each triangle takes the texture colour it shows most, so a quad's two halves can differ. Next: the texture itself, stored once in card memory, lit by the palette (D5).

## M8D3EO (2 x 2 tessellation), evening

Card, normal priority, agent on, `tunnel_card.ps1 -Settle 90 -Gap 60`, label `eo1`, boot 288.

| Per flip | M8D3EO | M8D3EN |
|---|---|---|
| Ours | 643 ms | 90 ms |
| Engine | 338 ms | |
| Whole frame | 1,673 ms | 420 ms |
| Writes | 11,490 | 8,209 |
| Colours | 32.6 | 7.7 |

flipwhy: slow on 40 of 45 flips (boxes 37). Sizes per flip: 1-4 27.4, 5-16 192.2, 17-64 278.4, 65+ 19.6.

Owner's eye: started on the card, then after a few frames looked like software. The black/yellow stripe rectangles are back to triangles and look wrong.

Reading: 4x pieces overflow the triangle list (`M8DD_TRIS_MAX` 2048), so flips take the slow RAM path. Per-cell float maths on the 387 is the 7x in ours. One texel per cell turns diagonal stripe edges into small triangles.

Negative result: blanket tessellation is out. Patch kept at `drivers/mach8_display/wip/2026-11-08/tess_m8d3eo.patch`, not committed.
